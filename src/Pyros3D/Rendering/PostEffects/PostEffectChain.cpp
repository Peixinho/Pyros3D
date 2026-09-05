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
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	namespace PostEffectChain {

		// Deliberately not every IEffect subclass. SSAO, depth of field,
		// motion blur and SSR each need something the chain cannot supply on
		// its own - a view matrix per frame, a velocity render pass, a
		// G-buffer normal target - so naming one here would produce an effect
		// that compiles and then draws wrong. They stay script-driven until
		// the chain can feed them (see buildMotionBlurPostChain in
		// PyrosLuaPostFX.cpp for what that plumbing looks like today).
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
			}
			return names;
		}

		IEffect* CreateBuiltIn(const std::string &name, const uint32 width, const uint32 height)
		{
			// Every one of these reads LastRTT, not Color: an effect in a
			// chain works on what the previous one produced. The manager
			// resolves LastRTT to the captured frame for the first effect, so
			// a one-effect chain still gets the scene.
			if (name == "Bloom")       return new BloomEffect(RTT::LastRTT, width, height);
			if (name == "BlurX")       return new BlurXEffect(RTT::LastRTT, width, height);
			if (name == "BlurY")       return new BlurYEffect(RTT::LastRTT, width, height);
			if (name == "Tonemap")     return new TonemapEffect(RTT::LastRTT, width, height);
			if (name == "Vignette")    return new VignetteEffect(RTT::LastRTT, width, height);
			if (name == "GammaEncode") return new GammaEncodeEffect(RTT::LastRTT, width, height);
			return NULL;
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
					IEffect* fx = CreateBuiltIn(e.effect, width, height);
					if (fx == NULL)
					{
						echo("ERROR: post effect '" + e.effect + "' is not a built-in effect - skipped");
						continue;
					}
					manager.AddEffect(fx);
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
