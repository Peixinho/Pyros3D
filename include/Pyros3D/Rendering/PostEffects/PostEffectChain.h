//============================================================================
// Name        : PostEffectChain.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Turns a scene's saved post-effect list into a live chain.
//============================================================================

#ifndef POSTEFFECTCHAIN_H
#define POSTEFFECTCHAIN_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Rendering/PostEffects/Effects/CustomEffect.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	// The one place that knows how to go from "what the scene file says" to
	// "effects in a PostEffectsManager". The editor's viewport and the player
	// both call Build(), so what you see while authoring is what ships - the
	// alternative being two lists of effect names that drift.
	namespace PostEffectChain {

		// Every built-in effect that can be named in a scene file, in the
		// order the editor should offer them. Chained effects read LastRTT,
		// so the order in the chain matters far more than this one.
		PYROS3D_API const std::vector<std::string> &ListBuiltIn();

		// What a built-in exposes, in the same shape an asset effect's
		// `//! param` lines produce - so the editor draws both with one piece
		// of code and a scene stores both overrides the same way. Empty for
		// the effects that genuinely have nothing to tune.
		PYROS3D_API const std::vector<CustomEffect::Param> &ListBuiltInParams(const std::string &name);

		// NULL for a name that is not in ListBuiltIn(). `params` overrides
		// what ListBuiltInParams() reports as defaults; unknown names in it
		// are ignored, since a scene may name a parameter an older or newer
		// build of the effect does not have. The caller owns the result only
		// until it hands it to PostEffectsManager::AddEffect(), which takes
		// ownership (see RemoveAllEffects).
		PYROS3D_API IEffect* CreateBuiltIn(const std::string &name, const uint32 width, const uint32 height,
			const std::map<std::string, std::vector<f32> > &params = std::map<std::string, std::vector<f32> >());

		// Reads one .glsl asset. The engine has no idea where a project keeps
		// its files, so the caller supplies the reading: return false and the
		// entry is skipped with a log line naming it.
		typedef bool (*ReadAssetFn)(const std::string &path, std::string &sourceOut, void *user);

		// Replaces whatever the manager currently has with `entries`, in
		// order. Disabled entries are skipped - kept in the file so turning
		// one off in the editor does not lose its parameters. Anything that
		// fails to build says so in the log and does not stop the rest of the
		// chain: half a chain is a better answer than a black screen.
		PYROS3D_API void Build(PostEffectsManager &manager,
			const std::vector<SceneMeta::PostEffectEntry> &entries,
			const uint32 width, const uint32 height,
			ReadAssetFn readAsset = NULL, void *readAssetUser = NULL);

	}

}

#endif /* POSTEFFECTCHAIN_H */
