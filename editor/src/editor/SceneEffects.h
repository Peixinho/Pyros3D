//============================================================================
// Name        : SceneEffects.h
// Description : The effects a scene can list (SceneMeta::postEffects) that the
//               renderer draws - p3d::ApplySceneSettings reads them - and their
//               parameters, as the editor offers them.
//============================================================================

#ifndef SCENEEFFECTS_H
#define SCENEEFFECTS_H

#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <string>
#include <vector>

struct SceneEffectParam
{
	const char* name;       // as saved, and as the renderer reads it
	const char* label;
	p3d::f32 value;         // what it is when the scene does not say
	p3d::f32 min, max;
	bool toggle = false;    // 0 or 1: a checkbox
};

struct SceneEffect
{
	const char* name;
	const char* about;
	std::vector<SceneEffectParam> params;
};

inline const std::vector<SceneEffect>& SceneEffects()
{
	static const std::vector<SceneEffect> all = {
		{ "Sky", "The physical sky, its sun and its clouds - lighting the scene as well as behind it",
			{ { "uCloudCover", "Cloud cover", 0.52f, 0.f, 1.f }, { "uCloudShadow", "Cloud shadow", 0.45f, 0.f, 1.f },
			  { "uCloudSteps", "Cloud detail (steps along a ray)", 40.f, 8.f, 128.f } } },
		{ "SSR", "Reflections of what is on screen, on surfaces smooth enough to show them",
			{ { "uMaxDistance", "Reach (metres)", 150.f, 5.f, 1000.f } } },
		{ "Output", "How the picture is exposed: automatically, from what is in view, or fixed",
			{ { "uAuto", "Automatic exposure", 1.f, 0.f, 1.f, true }, { "uExposure", "Exposure (EV)", 0.f, -6.f, 6.f },
			  { "uRenderScale", "Render scale (drawn at this share, resolved up by the TAA)", 1.f, 0.5f, 1.f } } },
		{ "SunShafts", "Light shafts from the sun, streaming past what stands against the sky (with the Sky)",
			{ { "uStrength", "Strength", 0.6f, 0.f, 2.f } } },
		{ "DepthOfField", "A lens focused at one distance: nearer and further, out of focus",
			{ { "uFocus", "In focus at (metres)", 10.f, 0.1f, 500.f }, { "uRange", "Sharp depth (metres)", 4.f, 0.f, 200.f }, { "uBlur", "Most blur (pixels)", 8.f, 0.f, 24.f } } },
		{ "RayTracedShadows", "The sun's shadows traced through the scene's triangles: soft as the sun's size makes them, in place of the shadow maps",
			{ { "uSunAngle", "The sun's angular radius (degrees)", 0.53f, 0.05f, 3.f } } },
		{ "RayTracedReflections", "Reflections traced through the scene's triangles, in place of the screen's: what the screen does not show is mirrored too", {} },
		{ "MotionBlur", "What moves on screen smeared along its move, as a camera's shutter does",
			{ { "uShutter", "Shutter (1: a whole frame)", 0.5f, 0.f, 2.f } } },
	};
	return all;
}

inline const SceneEffect* FindSceneEffect(const std::string &name)
{
	for (const SceneEffect &e : SceneEffects()) if (name == e.name) return &e;
	return nullptr;
}

// An entry's parameters (none for one the renderer does not draw)
inline const std::vector<SceneEffectParam>& SceneEffectParams(const p3d::SceneMeta::PostEffectEntry &entry)
{
	static const std::vector<SceneEffectParam> none;
	const SceneEffect* e = FindSceneEffect(entry.effect);
	return e ? e->params : none;
}

#endif
