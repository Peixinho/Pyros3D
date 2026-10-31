//============================================================================
// Name        : SceneGI.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See SceneGI.h.
//============================================================================

#include <Pyros3D/Rendering/ColourSpace.h>
#include <Pyros3D/Utils/LoadProgress/LoadProgress.h>
#include <Pyros3D/Rendering/GI/SceneGI.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <cfloat>

namespace p3d {
namespace {
	// The GI cache file (see SceneGISettings::cachePath): a header, the grid, the sky, then each
	// atlas's shape and floats, then the probes' offsets. Read back only if every shape matches
	// what the volume allocates for the same grid - else it is someone else's, and re-solved.
	// (the atlases as half floats: light needs nothing finer, and a long street's volume was 383 MB
	// in floats - most of it reflections, which are left out when the sky gives them: n 0)
	const char kGICacheMagic[8] = { 'P', '3', 'D', 'G', 'I', 'C', '0', '2' };
	template <class T> void Put(std::ofstream &o, const T &v) { o.write((const char*)&v, sizeof(T)); }
	template <class T> bool Take(std::ifstream &i, T &v) { return (bool)i.read((char*)&v, sizeof(T)); }
	uint16 ToHalf(const f32 f)
	{
		uint32 x; std::memcpy(&x, &f, 4);
		const uint32 sign = (x >> 16) & 0x8000u; int32 e = (int32)((x >> 23) & 0xff) - 127 + 15; uint32 m = x & 0x7fffffu;
		if (e <= 0) return (uint16)sign;                                   // (too small: zero)
		if (e >= 31) return (uint16)(sign | 0x7bffu);                      // (too large: the largest half)
		return (uint16)(sign | ((uint32)e << 10) | ((m + 0x1000u) >> 13 > 0x3ffu ? 0x3ffu : (m + 0x1000u) >> 13));
	}
	f32 FromHalf(const uint16 h)
	{
		const uint32 sign = (uint32)(h & 0x8000u) << 16; const uint32 e = (h >> 10) & 0x1fu, m = h & 0x3ffu;
		uint32 x = (e == 0) ? sign : (sign | ((e - 15 + 127) << 23) | (m << 13));
		f32 f; std::memcpy(&f, &x, 4); return f;
	}
	void PutAtlas(std::ofstream &o, const ProbeAtlas &a, const bool keep = true)
	{
		Put(o, a.GetResolution()); Put(o, a.GetChannels());
		const uint64 n = keep ? a.GetData().size() : 0; Put(o, n);
		std::vector<uint16> h((size_t)n);
		for (size_t k = 0; k < (size_t)n; k++) h[k] = ToHalf(a.GetData()[k]);
		if (n) o.write((const char*)&h[0], (std::streamsize)(n * sizeof(uint16)));
	}
	bool TakeAtlas(std::ifstream &i, ProbeAtlas &a)
	{
		uint32 res = 0, ch = 0; uint64 n = 0;
		if (!Take(i, res) || !Take(i, ch) || !Take(i, n)) return false;
		if (res != a.GetResolution() || ch != a.GetChannels() || (n != a.GetData().size() && n != 0)) return false;
		if (n == 0) return true;
		std::vector<uint16> h((size_t)n);
		if (!i.read((char*)&h[0], (std::streamsize)(n * sizeof(uint16)))) return false;
		for (size_t k = 0; k < (size_t)n; k++) a.GetData()[k] = FromHalf(h[k]);
		return true;
	}
}


	bool ReadSceneGICache(const SceneGISettings &settings, DDGIVolume &volume)
	{
		if (settings.cachePath.empty()) return false;
		std::ifstream in(settings.cachePath, std::ios::binary);
		char magic[8] = { 0 }; uint64 key = 0;
		Vec3 origin, spacing; uint32 counts[3] = { 0, 0, 0 };
		bool ok = in && in.read(magic, 8) && std::memcmp(magic, kGICacheMagic, 8) == 0 && Take(in, key) && key == settings.cacheKey
			&& Take(in, origin) && Take(in, spacing) && Take(in, counts[0]) && Take(in, counts[1]) && Take(in, counts[2]);
		if (ok && volume.Allocate(origin, spacing, counts[0], counts[1], counts[2]))
		{
			Vec3 sky; f32 maxRay = 0.f, bounce = 0.f; uint8 fromSH = 0; SphericalHarmonicsL2 sh;
			ok = Take(in, sky) && Take(in, maxRay) && Take(in, bounce) && Take(in, fromSH);
			for (uint32 c = 0; ok && c < SphericalHarmonicsL2::kCoefficientCount; c++) ok = Take(in, sh.coefficients[c]);
			ok = ok && TakeAtlas(in, volume.GetIrradianceAtlasMutable()) && TakeAtlas(in, volume.GetVisibilityAtlasMutable())
				&& TakeAtlas(in, volume.GetRadianceAtlasMutable());
			uint64 np = 0;
			ok = ok && Take(in, np) && np == volume.GetProbeData().size()
				&& (np == 0 || (bool)in.read((char*)&volume.GetProbeDataMutable()[0], (std::streamsize)(np * sizeof(Vec4))));
			if (ok)
			{
				volume.SetSkyColor(sky); volume.SetMaxRayDistance(maxRay); volume.SetMultiBounce(bounce);
				if (fromSH) volume.SetSkySH(sh); else volume.ClearSkySH();
				echo("BakeSceneGI: " + std::to_string(volume.ProbeCount()) + " probes read from " + settings.cachePath + " - nothing traced.");
				return true;
			}
		}
		if (in) echo("BakeSceneGI: " + settings.cachePath + " is for another scene or settings - solving again.");
		return false;
	}

	bool WriteSceneGICache(const SceneGISettings &settings, const DDGIVolume &volume, const bool keepRadiance)
	{
		if (settings.cachePath.empty()) return false;
		std::ofstream o(settings.cachePath, std::ios::binary | std::ios::trunc);
		if (!o) return false;
		o.write(kGICacheMagic, 8); Put(o, settings.cacheKey);
		Put(o, volume.origin); Put(o, volume.spacing);
		Put(o, volume.counts[0]); Put(o, volume.counts[1]); Put(o, volume.counts[2]);
		Put(o, volume.GetSkyColor()); Put(o, volume.GetMaxRayDistance()); Put(o, volume.GetMultiBounce());
		const uint8 fromSH = volume.IsSkyFromSH() ? 1 : 0; Put(o, fromSH);
		for (uint32 c = 0; c < SphericalHarmonicsL2::kCoefficientCount; c++) Put(o, volume.GetSkySH().coefficients[c]);
		PutAtlas(o, volume.GetIrradianceAtlas()); PutAtlas(o, volume.GetVisibilityAtlas());
		PutAtlas(o, volume.GetRadianceAtlas(), keepRadiance);
		const uint64 np = volume.GetProbeData().size(); Put(o, np);
		if (np) o.write((const char*)&volume.GetProbeData()[0], (std::streamsize)(np * sizeof(Vec4)));
		echo("BakeSceneGI: the solve kept in " + settings.cachePath);
		return (bool)o;
	}

	bool SolveSceneGI(SceneGraph *scene, const SceneGISettings &settings, DDGIVolume &volume)
	{
		if (ReadSceneGICache(settings, volume)) return true;
		if (!BakeSceneGI(scene, settings, volume)) return false;
		WriteSceneGICache(settings, volume, true);
		return true;
	}


	void CollectRayLights(SceneGraph *scene, std::vector<RayLight> &outLights)
	{
		outLights.clear();
		if (scene == NULL)
			return;

		std::vector<IComponent*> lights = ILightComponent::GetLightsOnScene(scene);
		// A scene's registries fill on its first update, and a bake made at load comes before that:
		// with nothing registered yet, the lights are found among the objects themselves (or the
		// sun bounced nowhere - "the scene has no lights", sky only).
		if (lights.empty())
		{
			std::vector<GameObject*> todo;
			for (size_t i = 0; i < scene->GetAllGameObjectList().size(); i++) todo.push_back(scene->GetAllGameObjectList()[i].get());
			while (!todo.empty())
			{
				GameObject *go = todo.back(); todo.pop_back();
				if (go == NULL) continue;
				for (size_t c = 0; c < go->GetComponents().size(); c++)
					if (dynamic_cast<ILightComponent*>(go->GetComponents()[c].get()) != NULL) lights.push_back(go->GetComponents()[c].get());
				for (size_t k = 0; k < go->GetChildren().size(); k++) todo.push_back(go->GetChildren()[k].get());
			}
		}
		for (size_t i = 0; i < lights.size(); i++)
		{
			ILightComponent *lc = dynamic_cast<ILightComponent*>(lights[i]);
			if (lc == NULL || lc->GetOwner() == NULL)
				continue;

			RayLight rl;
			// (the light it gives, as the direct light's: the colour decoded times the intensity - and a
			// sun's tint by the hour. The colour alone was taken: the bounce never followed the intensity,
			// and a lamp turned down or off went on lighting the room by its bounce)
			DirectionalLight *sunLight = lc->GetLightType() == LIGHT_TYPE::DIRECTIONAL ? dynamic_cast<DirectionalLight*>(lc) : NULL;
			const Vec4 c = sunLight != NULL ? sunLight->GetLightRadianceNow() : lc->GetLightRadiance();
			rl.color = Vec3(c.x, c.y, c.z);

			switch (lc->GetLightType())
			{
			case LIGHT_TYPE::DIRECTIONAL:
			{
				DirectionalLight *dl = dynamic_cast<DirectionalLight*>(lc);
				if (dl == NULL) continue;
				rl.isPoint = 0.f;
				rl.positionOrDirection = dl->GetLightDirection();
				break;
			}
			case LIGHT_TYPE::POINT:
			{
				PointLight *pl = dynamic_cast<PointLight*>(lc);
				if (pl == NULL) continue;
				rl.isPoint = 1.f;
				rl.positionOrDirection = pl->GetOwner()->GetWorldPosition();
				rl.range = pl->GetLightRadius();
				break;
			}
			default:
				// Spot lights are treated as points for the bounce.
				// Their cone shapes the DIRECT lighting, which the
				// forward/deferred path already handles; indirect light
				// from a spot is dominated by what its pool illuminates,
				// and approximating the cone as a point overestimates
				// the bounce behind it rather than missing it entirely.
				// Worth revisiting; not worth blocking on.
				rl.isPoint = 1.f;
				rl.positionOrDirection = lc->GetOwner()->GetWorldPosition();
				// (its reach, for the falloff: with none, its bounce was as strong at any distance)
				{ SpotLight *sl = dynamic_cast<SpotLight*>(lc); rl.range = sl != NULL ? sl->GetLightRadius() : 0.f; }
				break;
			}
			outLights.push_back(rl);
		}
		// Every sun, but only the strongest few lamps: each light is a shadow ray at every hit, and
		// a street of shop lamps (74) made the bake longer than the GPU would run it - a black
		// frame. A small lamp's bounce is next to nothing; it still lights the scene directly.
		const size_t kMostLamps = 8;
		std::vector<RayLight> suns, lamps;
		for (size_t i = 0; i < outLights.size(); i++) (outLights[i].isPoint > 0.5f ? lamps : suns).push_back(outLights[i]);
		if (lamps.size() > kMostLamps)
		{
			std::sort(lamps.begin(), lamps.end(), [](const RayLight &a, const RayLight &b) {
				const f32 wa = (a.color.x + a.color.y + a.color.z) * std::max(a.range, 1.f) * std::max(a.range, 1.f);
				const f32 wb = (b.color.x + b.color.y + b.color.z) * std::max(b.range, 1.f) * std::max(b.range, 1.f);
				return wa > wb; });
			lamps.resize(kMostLamps);
		}
		outLights = suns;
		outLights.insert(outLights.end(), lamps.begin(), lamps.end());
	}

	void UpdateSceneGI(SceneGraph *scene, const RayScene &rays, DDGIVolume &volume,
		const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis,
		const uint32 probeBudget)
	{
		if (scene == NULL || !volume.IsValid())
			return;
		std::vector<RayLight> lights;
		CollectRayLights(scene, lights);
		volume.Update(rays, lights, raysPerProbe, frame, hysteresis, probeBudget);
	}

	bool BakeSceneGI(SceneGraph *scene, const SceneGISettings &settings, DDGIVolume &outVolume)
	{
		if (scene == NULL || !settings.enabled)
			return false;

		RayScene rays;
		if (!rays.BuildFromScene(scene))
		{
			echo("BakeSceneGI: the scene has no renderable geometry.");
			return false;
		}
		rays.Build(4);
		return BakeSceneGI(scene, settings, outVolume, rays, true);
	}

	bool BakeSceneGI(SceneGraph *scene, const SceneGISettings &settings, DDGIVolume &outVolume, const RayScene &rays, const bool trace)
	{
		if (scene == NULL || !settings.enabled)
			return false;
		if (rays.triangles.empty())
		{
			echo("BakeSceneGI: the scene has no renderable geometry.");
			return false;
		}

		// Volume bounds from the geometry that was just extracted, which
		// is the same set the rays will hit - deriving them from
		// anything else risks a volume that does not contain the scene
		// it is lighting.
		Vec3 mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (size_t i = 0; i < rays.triangles.size(); i++)
		{
			const RayTriangle &t = rays.triangles[i];
			for (uint32 v = 0; v < 3; v++)
			{
				const Vec3 &p = (v == 0) ? t.v0 : (v == 1) ? t.v1 : t.v2;
				mn = Vec3(std::min(mn.x,p.x), std::min(mn.y,p.y), std::min(mn.z,p.z));
				mx = Vec3(std::max(mx.x,p.x), std::max(mx.y,p.y), std::max(mx.z,p.z));
			}
		}
		const Vec3 pad = (mx - mn) * settings.padding;
		mn = mn - pad;
		mx = mx + pad;
		if (settings.hasBounds) { mn = settings.boundsMin; mx = settings.boundsMax; }

		const uint32 nx = std::max(2u, settings.counts[0]);
		const uint32 ny = std::max(2u, settings.counts[1]);
		const uint32 nz = std::max(2u, settings.counts[2]);
		const Vec3 extent = mx - mn;
		const Vec3 spacing(extent.x / (f32)(nx - 1), extent.y / (f32)(ny - 1), extent.z / (f32)(nz - 1));

		if (!outVolume.Allocate(mn, spacing, nx, ny, nz))
		{
			echo("BakeSceneGI: invalid volume dimensions.");
			return false;
		}
		outVolume.SetSkyColor(settings.skyColor);
		if (settings.skyFromSH) outVolume.SetSkySH(settings.skySH); else outVolume.ClearSkySH();

		std::vector<RayLight> lights;
		CollectRayLights(scene, lights);
		if (lights.empty())
			echo("BakeSceneGI: the scene has no lights - indirect light will be sky only.");
		if (!trace) return true;

		// hysteresis 0 on the first pass so the volume starts from the
		// traced values rather than blending up from its initial state,
		// then a partial blend afterwards to average successive rotated
		// ray sets together instead of replacing them.
		for (uint32 p = 0; p < std::max(1u, settings.passes); p++)
		{
			LoadProgress::Report((float)p / (float)std::max(1u, settings.passes));
			outVolume.Update(rays, lights, settings.raysPerProbe, p, p == 0 ? 0.f : 0.5f);
		}
		LoadProgress::Report(1.f);

		echo("BakeSceneGI: " + std::to_string(outVolume.ProbeCount()) + " probes, "
			+ std::to_string(rays.TriangleCount()) + " triangles, "
			+ std::to_string(settings.raysPerProbe) + " rays x "
			+ std::to_string(std::max(1u, settings.passes)) + " passes.");
		return true;
	}

};
