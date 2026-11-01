//============================================================================
// Name        : SceneSettings.cpp
// Author      : Duarte Peixinho
// Description : What the renderer takes of a scene's own settings - see SceneSettings.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/SceneSettings.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <map>

namespace p3d {

	namespace {
		f32 ToLinear(const f32 c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
		f32 First(const SceneMeta::PostEffectEntry &e, const char* name, const f32 otherwise)
		{
			std::map<std::string, std::vector<f32> >::const_iterator i = e.params.find(name);
			return i != e.params.end() && !i->second.empty() ? i->second[0] : otherwise;
		}
		Vec4 Four(const SceneMeta::PostEffectEntry &e, const char* name, const Vec4 &otherwise)
		{
			std::map<std::string, std::vector<f32> >::const_iterator i = e.params.find(name);
			if (i == e.params.end() || i->second.size() < 4) return otherwise;
			return Vec4(i->second[0], i->second[1], i->second[2], i->second[3]);
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
		const std::vector<SceneMeta::PostEffectEntry> &postEffects, const bool twoD, const std::string &assetRoot)
	{
		// The scene's own post effects (a .glsl each), in its chain's order: their text read here, kept by path
		{
			static std::map<std::string, std::string> sources;     // (read once a path: the chain is applied every frame)
			std::vector<SceneRenderer::AssetEffect> effects;
			for (const SceneMeta::PostEffectEntry &e : postEffects)
			{
				if (!e.enabled || e.asset.empty()) continue;
				const std::string path = e.asset[0] == '/' || assetRoot.empty() ? e.asset : assetRoot + "/" + e.asset;
				std::map<std::string, std::string>::iterator found = sources.find(path);
				if (found == sources.end())
				{
					std::string text;
					if (FILE* f = fopen(path.c_str(), "rb"))
					{
						fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
						text.resize(n > 0 ? (size_t)n : 0);
						if (n > 0 && fread(&text[0], 1, text.size(), f) != text.size()) text.clear();
						fclose(f);
					}
					else fprintf(stderr, "Renderer: the post effect %s could not be read\n", path.c_str());
					found = sources.insert(std::make_pair(path, text)).first;
				}
				if (found->second.empty()) continue;
				SceneRenderer::AssetEffect a;
				a.name = e.asset; a.source = found->second; a.params = e.params;
				effects.push_back(a);
			}
			renderer.SetAssetEffects(effects);
		}
		// (both picked colours: sRGB, decoded here)
		renderer.SetAmbient(Vec3(ToLinear(ambient.x) * k, ToLinear(ambient.y) * k, ToLinear(ambient.z) * k));
		renderer.SetBackground(Vec3(ToLinear(background.x), ToLinear(background.y), ToLinear(background.z)));
		bool sky = false, autoExposure = true, reflections = false;
		bool rayTraced = false, rtReflections = false; f32 sunAngle = 0.0093f;
		f32 scale = 1.f, shafts = 0.f, dofFocus = 10.f, dofRange = 4.f, dofBlur = 0.f, shutter = 0.f;
		f32 bloomThreshold = 1.f, bloomKnee = 0.5f; f32 antiAliasing = 2.f, bloom = 0.f,     /* (no bloom unless the scene has a Bloom effect: on by default, every brighter pixel flashed through it - Erangel asked for none) */ aoRadius = 1.f, aoStrength = 1.f, aoDirect = 0.2f;
		SceneRenderer::Grade grade;     // (off unless the scene has its Grade effect)
		SceneRenderer::VolumetricFog fog;     // (none unless it has its VolumetricLight effect)
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
			if (e.effect == "Output") { autoExposure = First(e, "uAuto", 1.f) > 0.5f; ev = First(e, "uExposure", ev); scale = First(e, "uRenderScale", 1.f); antiAliasing = First(e, "uAntiAliasing", 2.f); }
			if (e.effect == "SunShafts") shafts = First(e, "uStrength", 0.6f);
			if (e.effect == "Bloom") { bloom = First(e, "uIntensity", bloom); bloomThreshold = First(e, "uThreshold", bloomThreshold); bloomKnee = First(e, "uKnee", bloomKnee); }
			if (e.effect == "VolumetricLight")
			{
				fog.density = First(e, "uDensity", 0.01f); fog.anisotropy = First(e, "uAnisotropy", 0.6f); fog.falloff = First(e, "uFalloff", 0.05f);
				fog.baseHeight = First(e, "uBaseHeight", 0.f); fog.maxDistance = First(e, "uMaxDistance", 150.f); fog.steps = First(e, "uSteps", 32.f);
				fog.scattering = First(e, "uScattering", 1.f); fog.ambient = First(e, "uAmbient", 0.2f);
			}
			if (e.effect == "SSAO") { aoRadius = First(e, "uRadius", aoRadius); aoStrength = First(e, "uStrength", aoStrength); aoDirect = First(e, "uDirect", aoDirect); }
			if (e.effect == "Grade")
			{
				grade.on = true;
				grade.exposure = First(e, "uExposure", 0.f); grade.shoulder = First(e, "uShoulder", 0.f);
				grade.temperature = First(e, "uTemperature", 0.f); grade.tint = First(e, "uTint", 0.f);
				grade.contrast = First(e, "uContrast", 1.f); grade.pivot = First(e, "uPivot", 0.43f);
				grade.lift = Four(e, "uLift", grade.lift); grade.gamma = Four(e, "uGamma", grade.gamma); grade.gain = Four(e, "uGain", grade.gain);
				grade.shadowTint = Four(e, "uShadowTint", grade.shadowTint); grade.highlightTint = Four(e, "uHighlightTint", grade.highlightTint);
				grade.saturation = First(e, "uSaturation", 1.f); grade.vibrance = First(e, "uVibrance", 0.f);
				grade.vignette = First(e, "uVignette", 0.f); grade.grain = First(e, "uGrain", 0.f); grade.chromatic = First(e, "uChromatic", 0.f);
			}
			if (e.effect == "DepthOfField") { dofFocus = First(e, "uFocus", dofFocus); dofRange = First(e, "uRange", dofRange); dofBlur = First(e, "uBlur", 8.f); }
			if (e.effect == "MotionBlur") shutter = First(e, "uShutter", 0.5f);
			if (e.effect == "RayTracedReflections") rtReflections = true;
			if (e.effect == "RayTracedShadows") { rayTraced = true; sunAngle = First(e, "uSunAngle", 0.53f) * 0.017453293f; }
		}
		renderer.SetSky(sky);
		renderer.SetGrade(grade);
		renderer.SetBloomIntensity(bloom);
		renderer.SetBloomThreshold(bloomThreshold, bloomKnee);
		renderer.SetAmbientOcclusion(aoRadius, aoStrength, aoDirect);
		renderer.SetVolumetricFog(fog);
		// (the anti-aliasing: Output's uAntiAliasing - 0 none, 1 FXAA, 2 TAA, the default; PYROS_RENDERER_TAA=0 still wins)
		if (!getenv("PYROS_RENDERER_TAA") && !getenv("PYROS_RENDERER_AA")) renderer.SetAntiAliasing(antiAliasing < 0.5f ? SceneRenderer::AntiAliasing::None : antiAliasing < 1.5f ? SceneRenderer::AntiAliasing::FXAA : SceneRenderer::AntiAliasing::TAA);
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
