//============================================================================
// Name        : Foliage.cpp
// Author      : Duarte Peixinho
// Description : See Foliage.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/File/File.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/stb/stb_image.h>

#include <algorithm>
#include <map>
#include <mutex>
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

	namespace {
		// A patch image is one small picture shared by every tile of every
		// layer that names it, read from any thread that grows foliage.
		std::shared_ptr<const PaintableImage> PatchImage(const std::string &path)
		{
			static std::mutex guard;
			static std::map<std::string, std::shared_ptr<const PaintableImage> > loaded;
			std::lock_guard<std::mutex> lock(guard);
			std::map<std::string, std::shared_ptr<const PaintableImage> >::iterator it = loaded.find(path);
			if (it != loaded.end()) return it->second;
			std::shared_ptr<PaintableImage> image = std::make_shared<PaintableImage>();
			std::shared_ptr<const PaintableImage> kept;
			if (image->Load(path, 4)) kept = image;
			else echo("WARNING: Foliage - could not read the patch map " + path);
			loaded[path] = kept;
			return kept;
		}
		inline f32 Wrapped(const f32 v) { return v - std::floor(v); }
		inline f32 Smooth(const f32 a, const f32 b, const f32 x)
		{
			const f32 t = std::min(std::max((x - a) / std::max(b - a, 1e-6f), 0.f), 1.f);
			return t * t * (3.f - 2.f * t);
		}
	}

	void PreparedFoliageLayer::Generate(const HeightfieldData &ground, const FoliageLayerSpec &spec,
		const std::string &densityMapPath, PreparedFoliageLayer &out)
	{
		PaintableImage map;
		const bool haveMap = !densityMapPath.empty() && map.Load(densityMapPath, 1);
		Generate(ground, spec, haveMap ? &map : NULL, out);
	}

	void PreparedFoliageLayer::Generate(const HeightfieldData &ground, const FoliageLayerSpec &spec,
		const PaintableImage* densityMap, PreparedFoliageLayer &out)
	{
		out.blocks.clear();
		if (ground.samples < 2 || spec.density <= 0.f || spec.blockSize <= 0.f) return;

		const bool haveMap = densityMap && densityMap->width > 0;
		std::shared_ptr<const PaintableImage> patches;
		if (spec.patchSize > 0.f && !spec.patchMapPath.empty()) patches = PatchImage(spec.patchMapPath);
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
					if (haveMap && Unit(Hash(h0 + 2u)) >= densityMap->Sample(x / ground.size, z / ground.size, 0)) continue;
					if (ground.IsHoleAt(x, z)) continue;	// nothing grows over a hole
					f32 patchShade = -1.f;
					if (patches)
					{
						const f32 u = Wrapped((spec.patchOriginX + x) / spec.patchSize), v = Wrapped((spec.patchOriginZ + z) / spec.patchSize);
						const f32 keep = 1.f - (1.f - spec.patchKeep) * Smooth(spec.patchFrom, spec.patchTo, patches->Sample(u, v, 1));
						if (keep < 1.f && Unit(Hash(h0 + 5u)) >= keep) continue;
						patchShade = Smooth(0.25f, 0.75f, patches->Sample(u, v, 0));
					}
					const f32 y = ground.HeightAt(x, z);
					if (y < spec.minHeight || y > spec.maxHeight) continue;
					const Vec3 n = GroundNormal(ground, x, z);
					if (n.y < cosMaxSlope) continue;
					const f32 r = Unit(Hash(h0 + 3u));
					positions.push_back(Vec3(x, y - spec.sink, z));
					normals.push_back(n);
					scales.push_back(spec.minScale + r * (spec.maxScale - spec.minScale));
					yaws.push_back(Unit(Hash(h0 + 4u)) * 2.f * (f32)M_PI);
					shades.push_back(patchShade < 0.f ? r : r + (patchShade - r) * spec.patchTint);
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

	static bool g_foliageHeadless = false;
	void FoliageComponent::SetHeadless(const bool on) { g_foliageHeadless = on; }
	static bool g_thinning = true;
	void FoliageComponent::SetThinning(const bool on) { g_thinning = on; }
	bool FoliageComponent::GetThinning() { return g_thinning; }

	f32 FoliageComponent::ThinningAt(const FoliageLayerSpec &spec, const f32 d)
	{
		if (!g_thinning || spec.thinDensity >= 1.f || d <= spec.thinFrom) return 1.f;
		const f32 least = std::max(0.05f, spec.thinDensity);
		if (d >= spec.thinTo || spec.thinTo <= spec.thinFrom) return least;
		const f32 t = (d - spec.thinFrom) / (spec.thinTo - spec.thinFrom);
		return 1.f + (least - 1.f) * (t * t * (3.f - 2.f * t));
	}

	void FoliageComponent::BuildBlocks(GameObject* owner, Layer &layer, PreparedFoliageLayer &prepared,
		std::vector<std::shared_ptr<GameObject> >* created)
	{
		if (g_foliageHeadless) { prepared.blocks.clear(); return; }
		const f32 meshRadius = layer.mesh ? layer.mesh->GetBoundingSphereRadius() * std::max(layer.spec.maxScale, layer.spec.minScale) : 0.f;
		const bool tinted = !(layer.spec.tintLow == Vec4(1.f, 1.f, 1.f, 1.f) && layer.spec.tintHigh == Vec4(1.f, 1.f, 1.f, 1.f));
		const uint32 modelOptions = ShaderUsage::Diffuse | ShaderUsage::InstancedRendering;
		for (size_t b = 0; b < prepared.blocks.size(); b++)
		{
			FoliageBlock &block = prepared.blocks[b];
			std::shared_ptr<GameObject> child = std::make_shared<GameObject>();
			child->SetName(layer.spec.name + "_block" + std::to_string(b));
			child->SetTransient(true);
			child->SetPosition(block.origin);
			std::shared_ptr<RenderingInstancedComponent> ic = layer.material
				? std::make_shared<RenderingInstancedComponent>(layer.mesh, layer.material, (uint32)block.transforms.size(), block.radius + meshRadius)
				: std::make_shared<RenderingInstancedComponent>(layer.mesh, modelOptions, (uint32)block.transforms.size(), block.radius + meshRadius);
			ic->transform = block.transforms;
			ic->UpdateTransforms();
			if (tinted)
			{
				ic->EnableInstanceColors();
				ic->instanceColor = block.tints;
				ic->UpdateInstanceColors();
			}
			if (layer.lodMesh)
			{
				ic->SetFirstLODDistance(layer.spec.lodDistance);
				if (layer.lodMaterial) ic->AddLOD(layer.lodMesh, 1e9f, layer.lodMaterial);
				else ic->AddLOD(layer.lodMesh, 1e9f, modelOptions);
			}
			if (layer.spec.castShadows) ic->EnableCastShadows(); else ic->DisableCastShadows();
			child->AddComponent(ic);
			if (owner) owner->Add(child);
			if (created) created->push_back(child);
			layer.blocks.push_back(ic);
			layer.blockObjects.push_back(child);
			layer.centres.push_back(block.origin);
			layer.counts.push_back((uint32)block.transforms.size());
		}
	}

	void FoliageComponent::Regrow(const HeightfieldData &ground, const int32 layerIndex)
	{
		GameObject* owner = GetOwner();
		for (size_t l = 0; l < layers.size(); l++)
		{
			if (layerIndex >= 0 && (size_t)layerIndex != l) continue;
			Layer &layer = layers[l];
			for (size_t b = 0; b < layer.blockObjects.size(); b++)
			{
				if (owner) owner->Remove(layer.blockObjects[b]);
				Retired r;
				r.object = layer.blockObjects[b];
				r.updatesLeft = 4;
				retired.push_back(r);
			}
			layer.blocks.clear();
			layer.blockObjects.clear();
			layer.centres.clear();
			layer.counts.clear();
			if (!layer.mesh) continue;
			PreparedFoliageLayer prepared;
			// The map being painted, else the saved one - growing at full
			// density here would undo every stroke that ever thinned it.
			if (layer.densityMap) PreparedFoliageLayer::Generate(ground, layer.spec, layer.densityMap.get(), prepared);
			else PreparedFoliageLayer::Generate(ground, layer.spec, layer.densityMapPath, prepared);
			BuildBlocks(owner, layer, prepared);
		}
		// The new blocks start at full count; the next Update() thins them.
		Update(0.0);
	}

	void FoliageComponent::Update(const f64 time)
	{
		// Blocks retired by Regrow() outlive the frames that may still draw them.
		for (size_t i = 0; i < retired.size();)
		{
			if (retired[i].updatesLeft == 0) { retired[i] = retired.back(); retired.pop_back(); }
			else { retired[i].updatesLeft--; i++; }
		}

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
				// (thinned by where the block's middle is - its plants are as
				// many nearer as further than that - and those left grown to
				// cover what the others did: wider more than taller, so that a
				// field far off does not stand higher than one near.)
				const f32 kept = ThinningAt(layer.spec, d + half);
				const uint32 n = (uint32)(layer.counts[b] * DensityAt(layer.spec, d) * kept);
				if (rc->NumberOfInstances() != n) rc->SetNumberInstances(n);
				const f32 area = 1.f / kept;
				rc->SetInstanceGrowth(powf(area, 0.75f), powf(area, 0.25f));
				if (layer.spec.castShadows && d <= layer.spec.shadowDistance) rc->EnableCastShadows();
				else rc->DisableCastShadows();
			}
		}
	}

}
