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
		  radianceLevels(0), multiBounce(1.f),
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

	void DDGIVolume::SetMultiBounce(const f32 strength)
	{
		multiBounce = std::min(std::max(strength, 0.f), 1.f);
	}

	f32 DDGIVolume::GetFeedbackNormalBias() const
	{
		const f32 s = std::min(std::min(spacing.x, spacing.y), spacing.z);
		return s * 0.25f;
	}

	f32 DDGIVolume::MinRoughness()
	{
		// Chosen against the ray count, not by taste: at 128 rays the
		// mean angle between neighbours is about 9 degrees, and a GGX
		// lobe at roughness 0.08 has a comparable width. Narrower and
		// the prefilter starts returning individual rays.
		return 0.08f;
	}

	f32 DDGIVolume::LevelRoughness(const uint32 level, const uint32 levels)
	{
		if (levels <= 1)
			return 1.f;
		const f32 t = (f32)level / (f32)(levels - 1);
		return MinRoughness() + (1.f - MinRoughness()) * t;
	}

	bool DDGIVolume::Allocate(const Vec3 &o, const Vec3 &s,
		const uint32 nx, const uint32 ny, const uint32 nz,
		const uint32 irradianceRes, const uint32 visibilityRes,
		const uint32 radianceRes, const uint32 levels)
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

		// Specular is opt-out. The atlas is levels times the size of
		// the irradiance one, so a scene that only wants bounce light
		// should not be paying for it.
		radianceLevels = levels;
		radiance.Clear();
		if (radianceLevels > 0)
		{
			if (!radiance.Allocate(total * radianceLevels, radianceRes, 4))
				return false;
		}

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

		// Multi-bounce, for the cost of one lookup. See SetMultiBounce.
		//
		// Offset along the normal first - see GetFeedbackNormalBias.
		// Sampling at the hit point itself leaks through thin walls and
		// then compounds that leak once per update, which is how a
		// feedback term turns a small error into an obvious one.
		if (multiBounce > 0.f)
			outgoing += albedo * multiBounce *
				SampleIrradianceIn(feedback, point + n * GetFeedbackNormalBias(), n);

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
		const uint32 radRes = radiance.GetResolution();
		// Golden-ratio rotation per frame: successive frames land
		// between each other's directions rather than repeating.
		const f32 rotation = (f32)frame * 0.618033988749895f;

		std::vector<Vec3> rayDir(raysPerProbe);
		std::vector<Vec3> rayRadiance(raysPerProbe);
		std::vector<f32> rayDistance(raysPerProbe);

		const uint32 total = ProbeCount();
		const uint32 wanted = (probeBudget == 0 || probeBudget > total) ? total : probeBudget;

		// Freeze what multi-bounce feeds back from, before any probe in
		// this update has written. See the `feedback` member.
		if (multiBounce > 0.f)
			SnapshotFeedback();

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

			// ---- prefiltered radiance: GGX lobe per texel, per level --
			//
			// The same rays, gathered through a different filter. The
			// irradiance pass above convolves with a cosine lobe, which
			// is the diffuse BRDF; this convolves with the GGX
			// distribution, which is the specular one. Nothing is
			// re-traced - a second set of rays for specular would cost
			// as much again and buy very little, because the rays are
			// the same rays and only the weighting differs.
			for (uint32 level = 0; level < radianceLevels; level++)
			{
				const f32 rough = LevelRoughness(level, radianceLevels);
				const f32 a = rough * rough;
				const f32 a2 = a * a;
				const uint32 tile = RadianceTile(probe, level);

				for (uint32 ty = 0; ty < radRes; ty++)
				for (uint32 tx = 0; tx < radRes; tx++)
				{
					// N = V = R, Karis' approximation. It is what makes
					// a single prefiltered map serve every view
					// direction; the error it introduces is the loss of
					// lobe stretching at grazing angles, which is the
					// trade every engine doing split-sum accepts.
					const Vec3 n = ProbeAtlas::TexelDirection(tx, ty, radRes);
					Vec3 sum(0.f, 0.f, 0.f);
					f32 weightSum = 0.f;
					f32 bestDot = -1.f;
					uint32 bestRay = 0;

					for (uint32 r = 0; r < raysPerProbe; r++)
					{
						const f32 nDotL = n.dotProduct(rayDir[r]);
						if (nDotL > bestDot) { bestDot = nDotL; bestRay = r; }
						if (nDotL <= 0.f) continue;
						Vec3 h = rayDir[r] + n;
						const f32 hlen = h.magnitude();
						if (hlen < 1e-6f) continue;
						h = h * (1.f / hlen);
						const f32 nDotH = std::max(0.f, n.dotProduct(h));
						const f32 d = (nDotH * nDotH) * (a2 - 1.f) + 1.f;
						const f32 ndf = a2 / std::max(kPi * d * d, 1e-8f);
						const f32 w = ndf * nDotL;
						if (w <= 1e-8f) continue;
						sum += rayRadiance[r] * w;
						weightSum += w;
					}

					if (weightSum > 1e-8f)
						sum = sum * (1.f / weightSum);
					else if (bestDot > 0.f)
						// The lobe fell between rays. Returning black
						// here would punch a hole in the reflection;
						// the closest ray is a poor estimate but it is
						// an estimate of the right thing.
						sum = rayRadiance[bestRay];

					f32 *dst = radiance.At(tile, tx, ty);
					dst[0] = dst[0] * hysteresis + sum.x * (1.f - hysteresis);
					dst[1] = dst[1] * hysteresis + sum.y * (1.f - hysteresis);
					dst[2] = dst[2] * hysteresis + sum.z * (1.f - hysteresis);
				}
			}
		}

		if (updateCursor >= total)
			updateCursor = 0;

		// Borders are refilled for the whole atlas rather than per probe
		// touched: it is a cheap pass over data already in cache, and
		// tracking which tiles are dirty would cost more than it saves.
		irradiance.FillBorders();
		visibility.FillBorders();
		radiance.FillBorders();
	}

	uint32 DDGIVolume::GatherProbes(const Vec3 &worldPosition, const Vec3 &normal,
		uint32 *outProbes, f32 *outWeights) const
	{
		if (!IsValid())
			return 0;

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

		const uint32 visRes = visibility.GetResolution();
		uint32 found = 0;

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

			outProbes[found] = probe;
			outWeights[found] = weight;
			found++;
		}
		return found;
	}

	Vec3 DDGIVolume::SampleIrradiance(const Vec3 &worldPosition, const Vec3 &normal) const
	{
		return SampleIrradianceIn(irradiance, worldPosition, normal);
	}

	Vec3 DDGIVolume::SampleIrradianceIn(const ProbeAtlas &atlas,
		const Vec3 &worldPosition, const Vec3 &normal) const
	{
		if (atlas.GetResolution() == 0)
			return Vec3(0.f, 0.f, 0.f);
		uint32 probes[8];
		f32 weights[8];
		const uint32 n = GatherProbes(worldPosition, normal, probes, weights);
		if (n == 0)
			return Vec3(0.f, 0.f, 0.f);

		const uint32 irrRes = atlas.GetResolution();
		// Irradiance in the surface's normal direction - the same texel
		// for every probe, because it is the surface's normal and not
		// the probe's.
		const Vec2 oct = OctEncode(normal);
		const uint32 ix = (uint32)std::min((f32)(irrRes - 1),
			std::max(0.f, (oct.x * 0.5f + 0.5f) * (f32)irrRes));
		const uint32 iy = (uint32)std::min((f32)(irrRes - 1),
			std::max(0.f, (oct.y * 0.5f + 0.5f) * (f32)irrRes));

		Vec3 sum(0.f, 0.f, 0.f);
		f32 weightSum = 0.f;
		for (uint32 i = 0; i < n; i++)
		{
			const f32 *c = atlas.At(probes[i], ix, iy);
			sum += Vec3(c[0], c[1], c[2]) * weights[i];
			weightSum += weights[i];
		}
		if (weightSum <= 1e-6f)
			return Vec3(0.f, 0.f, 0.f);
		return sum * (1.f / weightSum);
	}

	Vec3 DDGIVolume::SampleRadiance(const Vec3 &worldPosition, const Vec3 &normal,
		const Vec3 &reflection, const f32 roughness) const
	{
		if (radianceLevels == 0 || radiance.GetResolution() == 0)
			return Vec3(0.f, 0.f, 0.f);

		uint32 probes[8];
		f32 weights[8];
		const uint32 n = GatherProbes(worldPosition, normal, probes, weights);
		if (n == 0)
			return Vec3(0.f, 0.f, 0.f);

		// Where this roughness falls between two prefiltered levels.
		// Linear in roughness rather than in the GGX alpha, matching
		// how LevelRoughness lays the levels out - the two have to be
		// inverses or a surface samples a lobe it was not filtered for.
		f32 t = 0.f;
		if (radianceLevels > 1)
		{
			const f32 minR = MinRoughness();
			t = (std::min(std::max(roughness, minR), 1.f) - minR) / (1.f - minR);
			t *= (f32)(radianceLevels - 1);
		}
		const uint32 lo = (uint32)std::min((f32)(radianceLevels - 1), floorf(t));
		const uint32 hi = std::min(lo + 1, radianceLevels - 1);
		const f32 lerp = std::min(std::max(t - (f32)lo, 0.f), 1.f);

		Vec3 dir = reflection;
		const f32 len = dir.magnitude();
		if (len < 1e-6f)
			return Vec3(0.f, 0.f, 0.f);
		dir = dir * (1.f / len);

		const uint32 radRes = radiance.GetResolution();
		const Vec2 oct = OctEncode(dir);
		const uint32 rx = (uint32)std::min((f32)(radRes - 1),
			std::max(0.f, (oct.x * 0.5f + 0.5f) * (f32)radRes));
		const uint32 ry = (uint32)std::min((f32)(radRes - 1),
			std::max(0.f, (oct.y * 0.5f + 0.5f) * (f32)radRes));

		Vec3 sum(0.f, 0.f, 0.f);
		f32 weightSum = 0.f;
		for (uint32 i = 0; i < n; i++)
		{
			const f32 *a = radiance.At(RadianceTile(probes[i], lo), rx, ry);
			const f32 *b = radiance.At(RadianceTile(probes[i], hi), rx, ry);
			const Vec3 v(a[0] + (b[0] - a[0]) * lerp,
						 a[1] + (b[1] - a[1]) * lerp,
						 a[2] + (b[2] - a[2]) * lerp);
			sum += v * weights[i];
			weightSum += weights[i];
		}
		if (weightSum <= 1e-6f)
			return Vec3(0.f, 0.f, 0.f);
		return sum * (1.f / weightSum);
	}

};
