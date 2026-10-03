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
#include <Pyros3D/Rendering/PostEffects/Effects/DepthOfFieldEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	namespace PostEffectChain {

		// The effects a scene can name. Three of them are more than one pass
		// and AppendBuiltIn() expands them; the rest map one-to-one.
		//
		// This list used to exclude SSAO, depth of field and motion blur,
		// each for something the chain could not supply. All three are gone:
		// SSAO needed a view matrix per frame (PostEffectsManager::
		// SetViewMatrix) and a depth buffer with something in it (the axis
		// widget was clearing it - see SceneEditor.cpp's scissor); depth of
		// field needed two blurred copies at different resolutions, which is
		// just five more chain entries plus IEffect::SetResizeScale so the
		// small ones stay small; motion blur needed a velocity map, which
		// the manager now owns and the caller drives.
		//
		// Screen-space reflection is not here and has no effect class any
		// more. The DeferredRenderer traces reflections in its lighting pass
		// from the G-buffer it already has, per material - see
		// DeferredRenderer::EnableSSR() and GenericShaderMaterial::
		// SetSSREnabled(). A post-effect pass could only work from what the
		// chain can see, which is colour and depth and no normals, so it was
		// the strictly worse of the two and nothing used it.
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
				// Six passes behind one name - see AppendBuiltIn().
				names.push_back("DepthOfField");
				// Needs the caller to drive PostEffectsManager::
				// RenderVelocityPass() every frame; without that it is inert
				// rather than wrong, so it is safe to offer.
				names.push_back("MotionBlur");
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
				ssao.push_back(MakeParam("uRadius", "Radius", 0.5f, 0.01f, 2.f));
				ssao.push_back(MakeParam("uStrength", "Strength", 2.f, 0.f, 5.f));
				// How far past the radius an occluder's vote fades out.
				ssao.push_back(MakeParam("uTreshOld", "Falloff", 0.5f, 0.f, 10.f));
				// Deferred only: the share of occlusion direct lights take too.
				// 1 by default, like the post effect: at 0.5 a scene lit mostly
				// by direct light showed next to no SSAO at all.
				// There was a "Noise scale" here; the noise is tiled in screen
				// space now and has no scale, so a saved value is ignored.
				ssao.push_back(MakeParam("uDirect", "Direct light", 1.f, 0.f, 1.f));
				table["SSAO"] = ssao;

				std::vector<CustomEffect::Param> dof;
				dof.push_back(MakeParam("uFocalPosition", "Focal distance", 20.f, 0.f, 500.f));
				dof.push_back(MakeParam("uFocalRange", "Focal range", 2.f, 0.01f, 100.f));
				dof.push_back(MakeParam("uRatioL", "Far blur", 3.1f, 0.f, 8.f));
				dof.push_back(MakeParam("uRatioH", "Near blur", 1.f, 0.f, 8.f));
				table["DepthOfField"] = dof;

				std::vector<CustomEffect::Param> mb;
				mb.push_back(MakeParam("uTargetFPS", "Target FPS", 60.f, 15.f, 240.f));
				table["MotionBlur"] = mb;

				std::vector<CustomEffect::Param> bloom;
				bloom.push_back(MakeParam("uThreshold", "Threshold", 0.8f, 0.f, 4.f));
				bloom.push_back(MakeParam("uKnee", "Knee", 0.35f, 0.f, 1.f));
				bloom.push_back(MakeParam("uIntensity", "Intensity", 1.f, 0.f, 4.f));
				table["Bloom"] = bloom;
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
				ssao->SetRadius(ParamOr(params, "uRadius", 0.5f));
				ssao->SetStrength(ParamOr(params, "uStrength", 2.f));
				ssao->SetTreshOld(ParamOr(params, "uTreshOld", 0.5f));
				ssao->SetScale(ParamOr(params, "uScale", 100.f));
				manager.AddEffect(ssao);
				manager.AddEffect(new BlurSSAOEffect(RTT::LastRTT, width, height));
				manager.AddEffect(new SSAOCompositeEffect(RTT::Color, RTT::LastRTT, width, height));
				return true;
			}
			if (name == "DepthOfField")
			{
				// Two blurred copies of the frame plus the sharp one, mixed
				// per pixel by how far that pixel is from the focal plane.
				// The quarter-resolution pair is the far/bokeh blur - cheap
				// and wide - and the full-resolution pair is the near one.
				// SetResizeScale keeps that quarter through a viewport
				// resize; without it PostEffectsManager::Resize() would
				// promote the low stages to full resolution and the two
				// inputs would become the same image.
				BlurXEffect* blurX = new BlurXEffect(RTT::Color, width, height);
				BlurYEffect* blurY = new BlurYEffect(RTT::LastRTT, width, height);
				Texture* fullRes = blurY->GetTexture();

				const uint32 lw = width / 4 > 0 ? width / 4 : 1;
				const uint32 lh = height / 4 > 0 ? height / 4 : 1;
				ResizeEffect* resize = new ResizeEffect(RTT::Color, lw, lh);
				BlurXEffect* blurXLow = new BlurXEffect(RTT::LastRTT, lw, lh);
				BlurYEffect* blurYLow = new BlurYEffect(RTT::LastRTT, lw, lh);
				resize->SetResizeScale(0.25f);
				blurXLow->SetResizeScale(0.25f);
				blurYLow->SetResizeScale(0.25f);
				Texture* lowRes = blurYLow->GetTexture();

				DepthOfFieldEffect* dof = new DepthOfFieldEffect(lowRes, fullRes, width, height);
				dof->SetFocalPosition(ParamOr(params, "uFocalPosition", 20.f));
				dof->SetFocalRange(ParamOr(params, "uFocalRange", 2.f));
				dof->SetRatioLow(ParamOr(params, "uRatioL", 3.1f));
				dof->SetRatioHigh(ParamOr(params, "uRatioH", 1.f));

				manager.AddEffect(blurX);
				manager.AddEffect(blurY);
				manager.AddEffect(resize);
				manager.AddEffect(blurXLow);
				manager.AddEffect(blurYLow);
				manager.AddEffect(dof);
				return true;
			}
			if (name == "Bloom")
			{
				AppendBloom(manager, width, height,
					ParamOr(params, "uThreshold", 0.8f),
					ParamOr(params, "uKnee", 0.35f),
					ParamOr(params, "uIntensity", 1.f));
				return true;
			}
			if (name == "MotionBlur")
			{
				// Reads LastRTT, so it composes with whatever came before -
				// unlike SSAO's and depth of field's composites, which need
				// the untouched scene and belong at the head of a chain.
				// The velocity map comes from the manager because producing
				// it is a render pass, not a fullscreen quad.
				Texture* velocity = manager.EnsureVelocityMap();
				MotionBlurEffect* mb = new MotionBlurEffect(RTT::LastRTT, velocity, manager.GetVelocityDepth(), width, height);
				mb->SetTargetFPS(ParamOr(params, "uTargetFPS", 60.f));
				// A sane starting value for the first frame; the caller
				// replaces it every frame through RenderVelocityPass().
				mb->SetCurrentFPS(ParamOr(params, "uTargetFPS", 60.f));
				manager.AddEffect(mb);
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
			ReadAssetFn readAsset, void *readAssetUser,
			DeferredRenderer *deferred)
		{
			manager.RemoveAllEffects();
			if (deferred)
				deferred->DisableSSAO();
			if (width == 0 || height == 0)
				return;

			for (size_t i = 0; i < entries.size(); i++)
			{
				const SceneMeta::PostEffectEntry &e = entries[i];
				if (!e.enabled)
					continue;

				if (!e.effect.empty() && e.effect == "SSAO" && deferred)
				{
					// Same parameters, same defaults and the same meaning as
					// the post effect's (uTreshOld is the falloff past the
					// radius in both), so a scene looks alike either way.
					deferred->SetSSAOParams(ParamOr(e.params, "uRadius", 0.5f),
						ParamOr(e.params, "uStrength", 2.f), ParamOr(e.params, "uTreshOld", 0.5f));
					deferred->SetSSAODirectStrength(ParamOr(e.params, "uDirect", 1.f));
					deferred->EnableSSAO();
					continue;
				}
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
