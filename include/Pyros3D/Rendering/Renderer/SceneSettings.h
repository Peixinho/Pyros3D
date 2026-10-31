//============================================================================
// Name        : SceneSettings.h
// Author      : Duarte Peixinho
// Description : What the renderer takes of a scene's own settings (SceneMeta - what the editor edits and a game's
//               scene file holds): its ambient, its background, the sky (an enabled Sky effect, with its clouds),
//               the reflections (SSR, how far they look) and the exposure (an Output effect: uAuto 0 a fixed
//               2^uExposure, else auto, compensated by uExposure). One place - the player's games and the
//               editor's Scene View see a scene the same.
//============================================================================

#ifndef PYROS_RENDERER_SCENESETTINGS_H
#define PYROS_RENDERER_SCENESETTINGS_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>

namespace p3d {

	// ambient: the scene's ambient colour (linear) and its intensity; background: sRGB, as the scene keeps it
	// twoD: a 2D scene - its art shown as it was painted: a fixed exposure, no filmic curve (the old renderer's 2D picture)
	PYROS3D_API void ApplySceneSettings(SceneRenderer &renderer, const Vec4 &ambient, const f32 ambientIntensity, const Vec4 &background,
		const std::vector<SceneMeta::PostEffectEntry> &postEffects, const bool twoD = false);
	// Its ambient where there is no sky, by its mode (SceneMeta::ambientMode): 0 flat (the ambient colour), 1 a gradient
	// from the ground through the horizon to the sky (sRGB, as picked), 2 spherical harmonics (linear: a skybox baked
	// into them) - each times its intensity, as the old renderer takes them. A sky, when the scene has one, lights it
	// instead; 3 (DDGI) is SetGlobalIllumination's
	PYROS3D_API void ApplyAmbientEnvironment(SceneRenderer &renderer, const int mode, const Vec4 &sky, const Vec4 &equator, const Vec4 &ground,
		const Vec4 sh[9], const f32 ambientIntensity);

}

#endif
