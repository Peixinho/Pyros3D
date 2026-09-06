//============================================================================
// Name        : PostEffectChain.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See PostEffectChain.h.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/PostEffectChain.h>
#include <Pyros3D/Rendering/PostEffects/Effects/CustomEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/GammaEncodeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/VignetteEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOCompositeEffect.h>
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	namespace PostEffectChain {

		// Deliberately not every IEffect subclass. Depth of field, motion
		// blur and SSR each need something the chain has no way to supply -
		// two external blur-stage textures, a velocity render pass, a
		// G-buffer normal target - so naming one here would produce an effect
		// that compiles and then draws wrong. They stay script-driven until
		// the chain can feed them (see buildMotionBlurPostChain in
		// PyrosLuaPostFX.cpp for what that plumbing looks like today).
		// SSAO used to be in that list and no longer is: it needed a view
		// matrix per frame, which PostEffectsManager::SetViewMatrix() now
		// delivers, and it needed a depth buffer with something in it, which
		// it now gets (AxisHelper's full-target depth clear was erasing the
		// editor's captured scene depth every frame - see SceneEditor.cpp's
		// scissor around it).
		const std::vector<std::string> &ListBuiltIn()
		{
			static std::vector<std::string> names;
			if (names.empty())
			{
				names.push_back("Bloom");
				names.push_back("BlurX");
				names.push_back("BlurY");
				names.push_back("Tonemap");
				names.push_back("Vignette");
				names.push_back("GammaEncode");
				// Three passes behind one name - see AppendBuiltIn().
				names.push_back("SSAO");
			}
			return names;
		}

		namespace {
			CustomEffect::Param MakeParam(const char* name, const char* label, const f32 value,
				const f32 lo, const f32 hi)
			{
				CustomEffect::Param p;
				p.name = name;
				p.label = label;
				p.type = Uniforms::DataType::Float;
				p.value[0] = value;
				p.min = lo; p.max = hi; p.hasRange = true;
				return p;
			}
			f32 ParamOr(const std::map<std::string, std::vector<f32> > &params, const char* name, const f32 fallback)
			{
				std::map<std::string, std::vector<f32> >::const_iterator it = params.find(name);
				return (it != params.end() && !it->second.empty()) ? it->second[0] : fallback;
			}
		}

		// Only what an effect's constructor actually takes. Adding a knob here
		// that the effect does not read would put a slider in the editor that
		// does nothing, which is worse than no slider.
		const std::vector<CustomEffect::Param> &ListBuiltInParams(const std::string &name)
		{
			static std::map<std::string, std::vector<CustomEffect::Param> > table;
			static const std::vector<CustomEffect::Param> none;
			if (table.empty())
			{
				std::vector<CustomEffect::Param> vignette;
				vignette.push_back(MakeParam("uRADIUS", "Radius", 0.5f, 0.f, 1.5f));
				vignette.push_back(MakeParam("uSOFTNESS", "Softness", 0.2f, 0.f, 1.f));
				table["Vignette"] = vignette;

				std::vector<CustomEffect::Param> ssao;
				ssao.push_back(MakeParam("uRadius", "Radius", 0.2f, 0.01f, 2.f));
				ssao.push_back(MakeParam("uStrength", "Strength", 1.5f, 0.f, 5.f));
				ssao.push_back(MakeParam("uTreshOld", "Threshold", 2.f, 0.f, 10.f));
				// 100, not 1 - that is what SSAOEffect's constructor sets, and
				// it is the tiling rate of the 4x4 noise texture, not a strength.
				// At 1 the noise barely varies across the screen and the kernel
				// stops being randomly rotated, which shows up as banded blotches.
				ssao.push_back(MakeParam("uScale", "Noise scale", 100.f, 1.f, 400.f));
				table["SSAO"] = ssao;
			}
			std::map<std::string, std::vector<CustomEffect::Param> >::const_iterator it = table.find(name);
			return (it != table.end()) ? it->second : none;
		}

		IEffect* CreateBuiltIn(const std::string &name, const uint32 width, const uint32 height,
			const std::map<std::string, std::vector<f32> > &params)
		{
			// Every one of these reads LastRTT, not Color: an effect in a
			// chain works on what the previous one produced. The manager
			// resolves LastRTT to the captured frame for the first effect, so
			// a one-effect chain still gets the scene.
			if (name == "Bloom")       return new BloomEffect(RTT::LastRTT, width, height);
			if (name == "BlurX")       return new BlurXEffect(RTT::LastRTT, width, height);
			if (name == "BlurY")       return new BlurYEffect(RTT::LastRTT, width, height);
			if (name == "Tonemap")     return new TonemapEffect(RTT::LastRTT, width, height);
			if (name == "Vignette")    return new VignetteEffect(RTT::LastRTT, width, height,
										   ParamOr(params, "uRADIUS", 0.5f), ParamOr(params, "uSOFTNESS", 0.2f));
			if (name == "GammaEncode") return new GammaEncodeEffect(RTT::LastRTT, width, height);
			return NULL;
		}

		bool AppendBuiltIn(PostEffectsManager &manager, const std::string &name,
			const uint32 width, const uint32 height,
			const std::map<std::string, std::vector<f32> > &params)
		{
			if (name == "SSAO")
			{
				// Depth in, occlusion out; blur it; then multiply the original
				// colour by it. The middle and last passes read LastRTT, so
				// this is an ordinary run of three chain entries - it is one
				// name only so that the three cannot be ordered wrongly or
				// half-added.
				SSAOEffect* ssao = new SSAOEffect(RTT::Depth, width, height);
				ssao->SetRadius(ParamOr(params, "uRadius", 0.2f));
				ssao->SetStrength(ParamOr(params, "uStrength", 1.5f));
				ssao->SetTreshOld(ParamOr(params, "uTreshOld", 2.f));
				ssao->SetScale(ParamOr(params, "uScale", 100.f));
				manager.AddEffect(ssao);
				manager.AddEffect(new BlurSSAOEffect(RTT::LastRTT, width, height));
				manager.AddEffect(new SSAOCompositeEffect(RTT::Color, RTT::LastRTT, width, height));
				return true;
			}
			IEffect* fx = CreateBuiltIn(name, width, height, params);
			if (fx == NULL)
				return false;
			manager.AddEffect(fx);
			return true;
		}

		void Build(PostEffectsManager &manager,
			const std::vector<SceneMeta::PostEffectEntry> &entries,
			const uint32 width, const uint32 height,
			ReadAssetFn readAsset, void *readAssetUser)
		{
			manager.RemoveAllEffects();
			if (width == 0 || height == 0)
				return;

			for (size_t i = 0; i < entries.size(); i++)
			{
				const SceneMeta::PostEffectEntry &e = entries[i];
				if (!e.enabled)
					continue;

				if (!e.effect.empty())
				{
					if (!AppendBuiltIn(manager, e.effect, width, height, e.params))
						echo("ERROR: post effect '" + e.effect + "' is not a built-in effect - skipped");
					continue;
				}

				if (readAsset == NULL)
				{
					echo("ERROR: post effect asset '" + e.asset + "' cannot be read here - skipped");
					continue;
				}
				std::string source;
				if (!readAsset(e.asset, source, readAssetUser))
				{
					echo("ERROR: post effect asset not found: " + e.asset);
					continue;
				}
				std::string error;
				CustomEffect* fx = CustomEffect::CreateFromSource(source, width, height, error);
				if (fx == NULL)
				{
					echo("ERROR: post effect " + e.asset + ": " + error);
					continue;
				}
				// Saved overrides last, over whatever the file's own defaults
				// are - so editing a default in the .glsl changes every scene
				// that has not overridden it, and none that have.
				for (std::map<std::string, std::vector<f32> >::const_iterator k = e.params.begin(); k != e.params.end(); k++)
				{
					if (!fx->SetParam(k->first, &k->second[0], (uint32)k->second.size()))
						echo("WARNING: post effect " + e.asset + " has no parameter '" + k->first + "' - override ignored");
				}
				manager.AddEffect(fx);
			}
		}

	}

}
