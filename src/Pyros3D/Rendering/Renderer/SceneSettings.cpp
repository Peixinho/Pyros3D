//============================================================================
// Name        : SceneSettings.cpp
// Author      : Duarte Peixinho
// Description : What the renderer takes of a scene's own settings - see SceneSettings.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/SceneSettings.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace p3d {

	namespace {
		f32 ToLinear(const f32 c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
		f32 First(const SceneMeta::PostEffectEntry &e, const char* name, const f32 otherwise)
		{
			std::map<std::string, std::vector<f32> >::const_iterator i = e.params.find(name);
			return i != e.params.end() && !i->second.empty() ? i->second[0] : otherwise;
		}
	}

	void ApplyAmbientEnvironment(SceneRenderer &renderer, const int mode, const Vec4 &sky, const Vec4 &equator, const Vec4 &ground,
		const Vec4 sh[9], const f32 k)
	{
		auto lin = [k](const Vec4 &c) { return Vec3(ToLinear(c.x) * k, ToLinear(c.y) * k, ToLinear(c.z) * k); };
		Vec3 coefficients[9];
		for (int i = 0; i < 9; i++) coefficients[i] = Vec3(sh[i].x * k, sh[i].y * k, sh[i].z * k);
		renderer.SetAmbientEnvironment(mode == 1 || mode == 2 ? mode : 0, lin(sky), lin(equator), lin(ground), coefficients);
	}

	void ApplySceneSettings(SceneRenderer &renderer, const Vec4 &ambient, const f32 k, const Vec4 &background,
		const std::vector<SceneMeta::PostEffectEntry> &postEffects, const bool twoD)
	{
		// (both picked colours: sRGB, as the old renderer takes them - IRenderer::ScaleAmbient)
		renderer.SetAmbient(Vec3(ToLinear(ambient.x) * k, ToLinear(ambient.y) * k, ToLinear(ambient.z) * k));
		renderer.SetBackground(Vec3(ToLinear(background.x), ToLinear(background.y), ToLinear(background.z)));
		bool sky = false, autoExposure = true, reflections = false;
		bool rayTraced = false, rtReflections = false; f32 sunAngle = 0.0093f;
			f32 scale = 1.f, shafts = 0.f, dofFocus = 10.f, dofRange = 4.f, dofBlur = 0.f, shutter = 0.f;
		f32 ev = 0.f, cloudCover = 0.52f, cloudSteps = 40.f, cloudShadow = 0.45f, reach = 150.f;
		for (const SceneMeta::PostEffectEntry &e : postEffects)
		{
			if (!e.enabled) continue;
			if (e.effect == "SSR") { reflections = true; reach = First(e, "uMaxDistance", reach); }
			if (e.effect == "Sky")
			{
				// (its clouds, as the scene set them: the cover - 0.52 unless it says - their shadow, the steps along a ray)
				sky = true;
				cloudCover = First(e, "uCloudCover", cloudCover); cloudShadow = First(e, "uCloudShadow", cloudShadow); cloudSteps = First(e, "uCloudSteps", cloudSteps);
			}
			if (e.effect == "Output") { autoExposure = First(e, "uAuto", 1.f) > 0.5f; ev = First(e, "uExposure", ev); scale = First(e, "uRenderScale", 1.f); }
			if (e.effect == "SunShafts") shafts = First(e, "uStrength", 0.6f);
			if (e.effect == "DepthOfField") { dofFocus = First(e, "uFocus", dofFocus); dofRange = First(e, "uRange", dofRange); dofBlur = First(e, "uBlur", 8.f); }
			if (e.effect == "MotionBlur") shutter = First(e, "uShutter", 0.5f);
			if (e.effect == "RayTracedReflections") rtReflections = true;
			if (e.effect == "RayTracedShadows") { rayTraced = true; sunAngle = First(e, "uSunAngle", 0.53f) * 0.017453293f; }
		}
		renderer.SetSky(sky);
		if (!getenv("PYROS_RENDER_SCALE")) renderer.SetRenderScale(scale);
		renderer.SetRayTracedShadows(rayTraced, sunAngle);
		renderer.SetRayTracedReflections(rtReflections);
		renderer.SetSunShafts(shafts); renderer.SetDepthOfField(dofFocus, dofRange, dofBlur); renderer.SetMotionBlur(shutter);
		renderer.SetReflections(reflections, reach);
		renderer.SetClouds(cloudCover, (uint32)std::max(8.f, cloudSteps), cloudShadow);
		if (const char* haze = getenv("PYROS_RENDERER_HAZE")) renderer.SetHaze((f32)atof(haze));     // (to try one; a script sets its own: renderer:setHaze)
		renderer.SetAutoExposure(autoExposure && !twoD, ev);
		renderer.SetToneCurve(!twoD);
	}

}
