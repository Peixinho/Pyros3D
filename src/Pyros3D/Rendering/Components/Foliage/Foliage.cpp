//============================================================================
// Name        : Foliage.cpp
// Author      : Duarte Peixinho
// Description : See Foliage.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/File/File.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/stb/stb_image.h>

#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {
		// Integer hash, not the sin() one the grass demo uses: sin() is not
		// bit-identical across compilers and platforms, and a tile must grow
		// the same field on every machine that loads it.
		inline uint32 Hash(uint32 x)
		{
			x ^= x >> 16; x *= 0x7feb352dU;
			x ^= x >> 15; x *= 0x846ca68bU;
			x ^= x >> 16;
			return x;
		}
		inline uint32 Hash(const uint32 a, const uint32 b, const uint32 c, const uint32 d)
		{
			return Hash(a ^ Hash(b ^ Hash(c ^ Hash(d))));
		}
		inline f32 Unit(const uint32 h) { return (f32)(h >> 8) / 16777216.f; }

		struct DensityMap
		{
			int w = 0, h = 0;
			std::vector<f32> v;
			bool Load(const std::string &path)
			{
				File file;
				if (!file.Open(path) || file.Size() == 0) return false;
				int c = 0;
				stbi_uc* px = stbi_load_from_memory(&file.GetData()[0], (int)file.Size(), &w, &h, &c, 1);
				file.Close();
				if (!px) { echo("ERROR: Foliage - could not decode density map " + path); return false; }
				v.resize((size_t)w * h);
				for (size_t i = 0; i < v.size(); i++) v[i] = px[i] / 255.f;
				stbi_image_free(px);
				return true;
			}
			// u, v in 0..1 across the tile, bilinear.
			f32 At(const f32 u, const f32 t) const
			{
				const f32 fx = std::min(std::max(u, 0.f), 1.f) * (w - 1), fy = std::min(std::max(t, 0.f), 1.f) * (h - 1);
				const int x0 = std::min((int)fx, w - 2 < 0 ? 0 : w - 2), y0 = std::min((int)fy, h - 2 < 0 ? 0 : h - 2);
				const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
				const f32 a = fx - x0, b = fy - y0;
				const f32 top = v[(size_t)y0 * w + x0] * (1 - a) + v[(size_t)y0 * w + x1] * a;
				const f32 bot = v[(size_t)y1 * w + x0] * (1 - a) + v[(size_t)y1 * w + x1] * a;
				return top * (1 - b) + bot * b;
			}
		};

		Vec3 GroundNormal(const HeightfieldData &g, const f32 x, const f32 z)
		{
			// One-sided at the tile's edges: HeightAt clamps outside the tile,
			// so a centred difference there sees half a flat step and read a
			// ramp along the edge as level ground.
			const f32 e = std::max(g.Spacing(), 0.01f);
			const f32 xa = std::max(x - e, 0.f), xb = std::min(x + e, g.size);
			const f32 za = std::max(z - e, 0.f), zb = std::min(z + e, g.size);
			const f32 dx = xb > xa ? (g.HeightAt(xb, z) - g.HeightAt(xa, z)) / (xb - xa) : 0.f;
			const f32 dz = zb > za ? (g.HeightAt(x, zb) - g.HeightAt(x, za)) / (zb - za) : 0.f;
			return Vec3(-dx, 1.f, -dz).normalize();
		}
	}

	void PreparedFoliageLayer::Generate(const HeightfieldData &ground, const FoliageLayerSpec &spec,
		const std::string &densityMapPath, PreparedFoliageLayer &out)
	{
		out.blocks.clear();
		if (ground.samples < 2 || spec.density <= 0.f || spec.blockSize <= 0.f) return;

		DensityMap map;
		const bool haveMap = !densityMapPath.empty() && map.Load(densityMapPath);
		const f32 cosMaxSlope = std::cos(spec.maxSlopeDegrees * (f32)M_PI / 180.f);
		const uint32 perSide = (uint32)std::ceil(ground.size / spec.blockSize);

		for (uint32 bz = 0; bz < perSide; bz++)
			for (uint32 bx = 0; bx < perSide; bx++)
			{
				const f32 x0 = bx * spec.blockSize, z0 = bz * spec.blockSize;
				const f32 w = std::min(spec.blockSize, ground.size - x0), d = std::min(spec.blockSize, ground.size - z0);
				if (w <= 0.f || d <= 0.f) continue;
				// Candidates before filtering; the fraction is settled by the
				// hash so a density of 0.3 over 10 m2 gives 3 or 4, not always 3.
				const f32 expected = spec.density * w * d;
				uint32 candidates = (uint32)expected;
				if (Unit(Hash(spec.seed, bx, bz, 0xFFFFFFFFu)) < expected - (f32)candidates) candidates++;

				FoliageBlock block;
				std::vector<Vec3> positions;
				std::vector<f32> scales, yaws, shades;
				std::vector<Vec3> normals;
				f32 minY = 1e30f, maxY = -1e30f;
				for (uint32 k = 0; k < candidates; k++)
				{
					const uint32 h0 = Hash(spec.seed, bx, bz, k * 5u);
					const f32 x = x0 + Unit(h0) * w;
					const f32 z = z0 + Unit(Hash(h0 + 1u)) * d;
					if (haveMap && Unit(Hash(h0 + 2u)) >= map.At(x / ground.size, z / ground.size)) continue;
					const f32 y = ground.HeightAt(x, z);
					if (y < spec.minHeight || y > spec.maxHeight) continue;
					const Vec3 n = GroundNormal(ground, x, z);
					if (n.y < cosMaxSlope) continue;
					const f32 r = Unit(Hash(h0 + 3u));
					positions.push_back(Vec3(x, y - spec.sink, z));
					normals.push_back(n);
					scales.push_back(spec.minScale + r * (spec.maxScale - spec.minScale));
					yaws.push_back(Unit(Hash(h0 + 4u)) * 2.f * (f32)M_PI);
					shades.push_back(r);
					minY = std::min(minY, y);
					maxY = std::max(maxY, y);
				}
				if (positions.empty()) continue;

				// The block's GameObject sits at its centre, so the component's
				// bounding sphere (centred on it) covers the block.
				block.origin = Vec3(x0 + w * 0.5f, (minY + maxY) * 0.5f, z0 + d * 0.5f);
				block.centreY = block.origin.y;
				block.radius = std::sqrt(w * w * 0.25f + d * d * 0.25f + (maxY - minY) * (maxY - minY) * 0.25f);
				block.transforms.resize(positions.size());
				block.tints.resize(positions.size());
				for (size_t i = 0; i < positions.size(); i++)
				{
					const f32 c = std::cos(yaws[i]), s = std::sin(yaws[i]), sc = scales[i];
					Vec3 up(0.f, 1.f, 0.f);
					if (spec.alignToGround > 0.f)
						up = (up * (1.f - spec.alignToGround) + normals[i] * spec.alignToGround).normalize();
					// Yawed x, bent onto the plane of `up`; z completes a
					// right-handed frame (x cross y = z).
					Vec3 xa(c, 0.f, -s);
					xa = (xa - up * xa.dotProduct(up)).normalize();
					const Vec3 za = xa.cross(up).normalize();
					Matrix &m = block.transforms[i];
					m.identity();
					m.m[0] = xa.x * sc; m.m[1] = xa.y * sc; m.m[2] = xa.z * sc;
					m.m[4] = up.x * sc; m.m[5] = up.y * sc; m.m[6] = up.z * sc;
					m.m[8] = za.x * sc; m.m[9] = za.y * sc; m.m[10] = za.z * sc;
					const Vec3 local = positions[i] - block.origin;
					m.m[12] = local.x; m.m[13] = local.y; m.m[14] = local.z;
					const f32 t = shades[i];
					block.tints[i] = spec.tintLow + (spec.tintHigh - spec.tintLow) * t;
				}
				out.blocks.push_back(block);
			}
	}

	namespace {
		Vec3 g_viewer;
	}

	void FoliageComponent::SetViewer(const Vec3 &worldPosition) { g_viewer = worldPosition; }
	const Vec3 &FoliageComponent::GetViewer() { return g_viewer; }

	f32 FoliageComponent::DensityAt(const FoliageLayerSpec &spec, const f32 d)
	{
		if (d <= spec.fullDistance) return 1.f;
		if (d >= spec.fadeDistance || spec.fadeDistance <= spec.fullDistance) return 0.f;
		// Held near full most of the way, then cut: thinning removes whole
		// plants, and a long linear fade reads as dots on bare ground.
		const f32 t = (d - spec.fullDistance) / (spec.fadeDistance - spec.fullDistance);
		return 1.f - t * t;
	}

	void FoliageComponent::Update(const f64 time)
	{
		GameObject* owner = GetOwner();
		if (!owner) return;
		const Vec3 base = owner->GetWorldPosition();
		for (size_t l = 0; l < layers.size(); l++)
		{
			Layer &layer = layers[l];
			for (size_t b = 0; b < layer.blocks.size(); b++)
			{
				RenderingInstancedComponent* rc = layer.blocks[b].get();
				if (!rc) continue;
				// Distance to the block, not its centre: standing in a block
				// must draw all of it.
				const f32 half = layer.spec.blockSize * 0.7072f;
				const f32 d = std::max(0.f, (base + layer.centres[b]).distance(g_viewer) - half);
				const uint32 n = (uint32)(layer.counts[b] * DensityAt(layer.spec, d));
				if (rc->NumberOfInstances() != n) rc->SetNumberInstances(n);
				if (layer.spec.castShadows && d <= layer.spec.shadowDistance) rc->EnableCastShadows();
				else rc->DisableCastShadows();
			}
		}
	}

}
