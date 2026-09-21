//============================================================================
// Name        : SceneGI.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See SceneGI.h.
//============================================================================

#include <Pyros3D/Rendering/GI/SceneGI.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <cfloat>

namespace p3d {

	void CollectRayLights(SceneGraph *scene, std::vector<RayLight> &outLights)
	{
		outLights.clear();
		if (scene == NULL)
			return;

		std::vector<IComponent*> &lights = ILightComponent::GetLightsOnScene(scene);
		for (size_t i = 0; i < lights.size(); i++)
		{
			ILightComponent *lc = dynamic_cast<ILightComponent*>(lights[i]);
			if (lc == NULL || lc->GetOwner() == NULL)
				continue;

			RayLight rl;
			const Vec4 c = lc->GetLightColor();
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
				rl.range = 0.f;
				break;
			}
			outLights.push_back(rl);
		}
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

		std::vector<RayLight> lights;
		CollectRayLights(scene, lights);
		if (lights.empty())
			echo("BakeSceneGI: the scene has no lights - indirect light will be sky only.");

		// hysteresis 0 on the first pass so the volume starts from the
		// traced values rather than blending up from its initial state,
		// then a partial blend afterwards to average successive rotated
		// ray sets together instead of replacing them.
		for (uint32 p = 0; p < std::max(1u, settings.passes); p++)
			outVolume.Update(rays, lights, settings.raysPerProbe, p, p == 0 ? 0.f : 0.5f);

		echo("BakeSceneGI: " + std::to_string(outVolume.ProbeCount()) + " probes, "
			+ std::to_string(rays.TriangleCount()) + " triangles, "
			+ std::to_string(settings.raysPerProbe) + " rays x "
			+ std::to_string(std::max(1u, settings.passes)) + " passes.");
		return true;
	}

};
