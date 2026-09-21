//============================================================================
// Name        : DDGIVolume.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See DDGIVolume.h.
//============================================================================

#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {
		const f32 kPi = 3.14159265358979323846f;
	}

	DDGIVolume::DDGIVolume()
		: origin(0.f,0.f,0.f), spacing(1.f,1.f,1.f),
		  skyColor(0.f,0.f,0.f), maxRayDistance(100.f), updateCursor(0)
	{
		counts[0] = counts[1] = counts[2] = 0;
	}

	bool DDGIVolume::IsValid() const
	{
		return counts[0] >= 2 && counts[1] >= 2 && counts[2] >= 2
			&& spacing.x > 0.f && spacing.y > 0.f && spacing.z > 0.f
			&& irradiance.GetResolution() > 0 && visibility.GetResolution() > 0;
	}

	Vec3 DDGIVolume::ProbePosition(const uint32 x, const uint32 y, const uint32 z) const
	{
		return Vec3(origin.x + spacing.x * (f32)x,
					origin.y + spacing.y * (f32)y,
					origin.z + spacing.z * (f32)z);
	}

	bool DDGIVolume::Allocate(const Vec3 &o, const Vec3 &s,
		const uint32 nx, const uint32 ny, const uint32 nz,
		const uint32 irradianceRes, const uint32 visibilityRes)
	{
		if (nx < 2 || ny < 2 || nz < 2) return false;
		if (s.x <= 0.f || s.y <= 0.f || s.z <= 0.f) return false;

		origin = o; spacing = s;
		counts[0] = nx; counts[1] = ny; counts[2] = nz;
		const uint32 total = nx * ny * nz;
		// FOUR channels for three channels of data. Metal has no
		// three-component 32-bit float pixel format at all - there is
		// MTLPixelFormatR32Float, RG32Float and RGBA32Float and nothing
		// between - so an RGB32F upload is rejected outright ("AGX:
		// Texture read/write assertion failed: bytes_per_row >=
		// used_bytes_per_row", which names the symptom and not the
		// cause). The alpha is unused and costs 25% of this atlas;
		// packing something into it later is free.
		if (!irradiance.Allocate(total, irradianceRes, 4)) return false;
		if (!visibility.Allocate(total, visibilityRes, 2)) return false;

		// A ray that escapes should be treated as sky at roughly the
		// scale of the volume, not at infinity: the visibility moments
		// are compared against distances within the grid, and an
		// infinite mean makes every Chebyshev test pass.
		const Vec3 extent(spacing.x * (f32)(nx-1), spacing.y * (f32)(ny-1), spacing.z * (f32)(nz-1));
		maxRayDistance = extent.magnitude();
		if (maxRayDistance <= 0.f) maxRayDistance = 100.f;

		// Visibility starts at maxRayDistance rather than zero. Zero
		// means "geometry is right here in every direction", which makes
		// every probe fail its own Chebyshev test before it has ever
		// been traced - the volume would be black until fully converged
		// instead of merely inaccurate.
		std::vector<f32> &vd = visibility.GetData();
		for (size_t i = 0; i + 1 < vd.size(); i += 2)
		{
			vd[i] = maxRayDistance;
			vd[i+1] = maxRayDistance * maxRayDistance;
		}
		return true;
	}

	Vec3 DDGIVolume::SphericalFibonacci(const uint32 index, const uint32 count, const f32 rotation)
	{
		// Evenly spread directions with no clustering at the poles, and
		// cheap - no sorting, no rejection. The rotation offset is what
		// makes successive frames sample DIFFERENT directions; without
		// it the same fixed set is retraced forever and the estimate
		// converges to whatever those particular directions saw rather
		// than to the true integral.
		const f32 n = (f32)count;
		const f32 i = (f32)index + 0.5f;
		const f32 phi = 2.f * kPi * (i * 0.618033988749895f + rotation);
		const f32 cosTheta = 1.f - 2.f * i / n;
		const f32 sinTheta = sqrtf(std::max(0.f, 1.f - cosTheta * cosTheta));
		return Vec3(cosf(phi) * sinTheta, sinf(phi) * sinTheta, cosTheta);
	}

	Vec3 DDGIVolume::ShadeHit(const RayScene &scene, const RayHit &hit,
		const Vec3 &rayOrigin, const Vec3 &rayDir,
		const std::vector<RayLight> &lights) const
	{
		const RayTriangle &tri = scene.triangles[hit.triangle];
		const Vec3 point = rayOrigin + rayDir * hit.t;
		// Barycentric normal. w = 1-u-v is the weight of v0.
		Vec3 n = tri.n0 * (1.f - hit.u - hit.v) + tri.n1 * hit.u + tri.n2 * hit.v;
		n.normalizeSelf();
		// Face the ray. A probe ray can strike either side of a wall and
		// the lighting has to be computed for the side it actually hit,
		// or a probe inside a room is lit through the wall behind it.
		if (n.dotProduct(rayDir) > 0.f)
			n = n * -1.f;

		Vec3 albedo(0.8f, 0.8f, 0.8f);
		Vec3 emissive(0.f, 0.f, 0.f);
		if (tri.materialIndex < scene.materials.size())
		{
			albedo = scene.materials[tri.materialIndex].albedo;
			emissive = scene.materials[tri.materialIndex].emissive;
		}

		Vec3 outgoing = emissive;
		for (size_t l = 0; l < lights.size(); l++)
		{
			const RayLight &light = lights[l];
			Vec3 toLight;
			f32 distance;
			if (light.isPoint > 0.5f)
			{
				toLight = light.positionOrDirection - point;
				distance = toLight.magnitude();
				if (distance < 1e-5f) continue;
				toLight = toLight * (1.f / distance);
			}
			else
			{
				toLight = light.positionOrDirection * -1.f;
				toLight.normalizeSelf();
				distance = maxRayDistance;
			}

			const f32 ndotl = n.dotProduct(toLight);
			if (ndotl <= 0.f) continue;

			// Shadow ray. Offset along the normal, not along the ray:
			// offsetting along the ray leaves a grazing hit still inside
			// its own triangle and the surface shadows itself, which
			// reads as uniform darkening rather than as an error.
			RayHit shadow;
			if (scene.Intersect(point + n * 1e-3f, toLight, 1e-4f, distance - 1e-3f, shadow))
				continue;

			f32 attenuation = 1.f;
			if (light.isPoint > 0.5f && light.range > 0.f)
			{
				const f32 k = std::max(0.f, 1.f - distance / light.range);
				attenuation = k * k;
			}
			// Lambert, energy-conserving. albedo/PI times irradiance;
			// the engine's convention is that a light colour is already
			// irradiance/PI (see CalculatePBRLighting), so the two PIs
			// cancel and this is a plain product.
			outgoing += albedo * light.color * (ndotl * attenuation);
		}
		return outgoing;
	}

	void DDGIVolume::Update(const RayScene &scene, const std::vector<RayLight> &lights,
		const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis,
		const uint32 probeBudget)
	{
		if (!IsValid() || raysPerProbe == 0)
			return;

		const uint32 irrRes = irradiance.GetResolution();
		const uint32 visRes = visibility.GetResolution();
		// Golden-ratio rotation per frame: successive frames land
		// between each other's directions rather than repeating.
		const f32 rotation = (f32)frame * 0.618033988749895f;

		std::vector<Vec3> rayDir(raysPerProbe);
		std::vector<Vec3> rayRadiance(raysPerProbe);
		std::vector<f32> rayDistance(raysPerProbe);

		const uint32 total = ProbeCount();
		const uint32 wanted = (probeBudget == 0 || probeBudget > total) ? total : probeBudget;

		for (uint32 n = 0; n < wanted; n++)
		{
			if (updateCursor >= total)
				updateCursor = 0;
			const uint32 probe = updateCursor++;
			// Inverse of Index(), which is row-major with X fastest. The
			// two must agree or probes are written into the wrong cells,
			// which looks like plausible lighting shifted by an axis.
			const uint32 x = probe % counts[0];
			const uint32 y = (probe / counts[0]) % counts[1];
			const uint32 z = probe / (counts[0] * counts[1]);
			const Vec3 p = ProbePosition(x, y, z);

			for (uint32 r = 0; r < raysPerProbe; r++)
			{
				const Vec3 d = SphericalFibonacci(r, raysPerProbe, rotation);
				rayDir[r] = d;
				RayHit hit;
				if (scene.Intersect(p, d, 1e-4f, maxRayDistance, hit))
				{
					rayRadiance[r] = ShadeHit(scene, hit, p, d, lights);
					rayDistance[r] = hit.t;
				}
				else
				{
					rayRadiance[r] = skyColor;
					// A miss is "nothing out to here", which is what the
					// visibility moments should record - not zero, which
					// would claim a wall at the probe itself.
					rayDistance[r] = maxRayDistance;
				}
			}

			// ---- irradiance: cosine-weighted gather per texel ----------
			for (uint32 ty = 0; ty < irrRes; ty++)
			for (uint32 tx = 0; tx < irrRes; tx++)
			{
				const Vec3 texelDir = ProbeAtlas::TexelDirection(tx, ty, irrRes);
				Vec3 sum(0.f, 0.f, 0.f);
				f32 weightSum = 0.f;
				for (uint32 r = 0; r < raysPerProbe; r++)
				{
					const f32 w = std::max(0.f, texelDir.dotProduct(rayDir[r]));
					if (w <= 0.f) continue;
					sum += rayRadiance[r] * w;
					weightSum += w;
				}
				if (weightSum > 1e-6f)
					sum = sum * (1.f / weightSum);

				f32 *dst = irradiance.At(probe, tx, ty);
				// Exponential blend. Replacing outright makes every frame
				// a fresh low-sample-count estimate and the lighting
				// visibly boils; blending trades response time for
				// stability, which for indirect light is the right way
				// round.
				dst[0] = dst[0] * hysteresis + sum.x * (1.f - hysteresis);
				dst[1] = dst[1] * hysteresis + sum.y * (1.f - hysteresis);
				dst[2] = dst[2] * hysteresis + sum.z * (1.f - hysteresis);
			}

			// ---- visibility: distance moments --------------------------
			for (uint32 ty = 0; ty < visRes; ty++)
			for (uint32 tx = 0; tx < visRes; tx++)
			{
				const Vec3 texelDir = ProbeAtlas::TexelDirection(tx, ty, visRes);
				f32 mean = 0.f, mean2 = 0.f, weightSum = 0.f;
				for (uint32 r = 0; r < raysPerProbe; r++)
				{
					// A much tighter cosine lobe than irradiance uses.
					// Visibility has to resolve which side of a wall
					// edge a direction is on; averaging over a wide lobe
					// smears the wall's distance into the open direction
					// beside it, and the leak comes back.
					f32 w = std::max(0.f, texelDir.dotProduct(rayDir[r]));
					w = w * w * w * w;
					if (w <= 1e-6f) continue;
					const f32 d = std::min(rayDistance[r], maxRayDistance);
					mean += d * w;
					mean2 += d * d * w;
					weightSum += w;
				}
				if (weightSum > 1e-6f)
				{
					mean /= weightSum;
					mean2 /= weightSum;
				}
				else
				{
					mean = maxRayDistance;
					mean2 = maxRayDistance * maxRayDistance;
				}
				f32 *dst = visibility.At(probe, tx, ty);
				dst[0] = dst[0] * hysteresis + mean * (1.f - hysteresis);
				dst[1] = dst[1] * hysteresis + mean2 * (1.f - hysteresis);
			}
		}

		if (updateCursor >= total)
			updateCursor = 0;

		// Borders are refilled for the whole atlas rather than per probe
		// touched: it is a cheap pass over data already in cache, and
		// tracking which tiles are dirty would cost more than it saves.
		irradiance.FillBorders();
		visibility.FillBorders();
	}

	Vec3 DDGIVolume::SampleIrradiance(const Vec3 &worldPosition, const Vec3 &normal) const
	{
		if (!IsValid())
			return Vec3(0.f, 0.f, 0.f);

		f32 g[3] = {
			(worldPosition.x - origin.x) / spacing.x,
			(worldPosition.y - origin.y) / spacing.y,
			(worldPosition.z - origin.z) / spacing.z
		};
		uint32 base[3];
		f32 frac[3];
		for (uint32 a = 0; a < 3; a++)
		{
			const f32 maxC = (f32)(counts[a] - 1);
			g[a] = std::min(std::max(g[a], 0.f), maxC);
			const f32 fl = floorf(g[a]);
			base[a] = (uint32)fl;
			if (base[a] + 1 >= counts[a] && base[a] > 0) base[a] = counts[a] - 2;
			frac[a] = g[a] - (f32)base[a];
		}

		const uint32 irrRes = irradiance.GetResolution();
		const uint32 visRes = visibility.GetResolution();
		Vec3 sum(0.f, 0.f, 0.f);
		f32 weightSum = 0.f;

		for (uint32 corner = 0; corner < 8; corner++)
		{
			const uint32 cx = base[0] + ((corner & 1) ? 1 : 0);
			const uint32 cy = base[1] + ((corner & 2) ? 1 : 0);
			const uint32 cz = base[2] + ((corner & 4) ? 1 : 0);
			if (cx >= counts[0] || cy >= counts[1] || cz >= counts[2])
				continue;

			const f32 tx = (corner & 1) ? frac[0] : (1.f - frac[0]);
			const f32 ty = (corner & 2) ? frac[1] : (1.f - frac[1]);
			const f32 tz = (corner & 4) ? frac[2] : (1.f - frac[2]);
			f32 weight = tx * ty * tz;
			if (weight <= 0.f)
				continue;

			const uint32 probe = Index(cx, cy, cz);
			const Vec3 probePos = ProbePosition(cx, cy, cz);
			Vec3 toProbe = probePos - worldPosition;
			const f32 distToProbe = toProbe.magnitude();
			if (distToProbe > 1e-5f)
				toProbe = toProbe * (1.f / distToProbe);

			// Backface rejection. A probe behind the surface cannot be
			// lighting it, and including it is how a wall's far side
			// bleeds through. Smoothed rather than binary so a surface
			// rotating past the threshold does not pop.
			const f32 facing = (toProbe.dotProduct(normal) + 1.f) * 0.5f;
			weight *= facing * facing + 0.2f;

			// Chebyshev visibility. Given the mean and mean square of
			// distance in this direction, the one-sided Chebyshev
			// inequality bounds the probability that a surface at
			// distToProbe is visible. This is the entire reason the
			// visibility atlas exists and the entire difference between
			// this and IrradianceProbeGrid.
			{
				const Vec2 oct = OctEncode(toProbe * -1.f);
				const uint32 vx = (uint32)std::min((f32)(visRes - 1),
					std::max(0.f, (oct.x * 0.5f + 0.5f) * (f32)visRes));
				const uint32 vy = (uint32)std::min((f32)(visRes - 1),
					std::max(0.f, (oct.y * 0.5f + 0.5f) * (f32)visRes));
				const f32 *m = visibility.At(probe, vx, vy);
				const f32 mean = m[0];
				const f32 variance = std::max(0.f, m[1] - mean * mean);
				// Bias along the normal so a surface does not occlude
				// itself from its own probe.
				const f32 d = distToProbe;
				if (d > mean)
				{
					const f32 diff = d - mean;
					const f32 cheb = variance / (variance + diff * diff);
					// Cubed: the raw Chebyshev bound is loose, and
					// sharpening it is what actually removes the leak
					// rather than merely dimming it.
					weight *= std::max(cheb * cheb * cheb, 0.f);
				}
			}

			if (weight <= 1e-6f)
				continue;

			// Irradiance in the surface's normal direction.
			const Vec2 oct = OctEncode(normal);
			const uint32 ix = (uint32)std::min((f32)(irrRes - 1),
				std::max(0.f, (oct.x * 0.5f + 0.5f) * (f32)irrRes));
			const uint32 iy = (uint32)std::min((f32)(irrRes - 1),
				std::max(0.f, (oct.y * 0.5f + 0.5f) * (f32)irrRes));
			const f32 *c = irradiance.At(probe, ix, iy);
			sum += Vec3(c[0], c[1], c[2]) * weight;
			weightSum += weight;
		}

		if (weightSum <= 1e-6f)
			return Vec3(0.f, 0.f, 0.f);
		return sum * (1.f / weightSum);
	}

};
