//============================================================================
// Name        : TerrainEditor.cpp
// Author      : Duarte Peixinho
// Description : See TerrainEditor.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace p3d {

	namespace {
		bool Overlaps(const TerrainTile &t, const f32 x, const f32 z, const f32 r)
		{
			const f32 s = t.Size();
			return x + r >= t.origin.x && z + r >= t.origin.z && x - r <= t.origin.x + s && z - r <= t.origin.z + s;
		}
	}

	std::vector<TerrainTile> TerrainEditor::FindTiles(SceneGraph* scene)
	{
		std::vector<TerrainTile> tiles;
		if (!scene) return tiles;
		std::vector<GameObject*> all;
		scene->CollectGameObjectsRecursive(all);
		for (size_t i = 0; i < all.size(); i++)
		{
			GameObject* go = all[i];
			if (!go) continue;
			TerrainTile tile;
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t c = 0; c < comps.size(); c++)
			{
				IComponent* comp = comps[c].get();
				if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(comp))
				{
					if (!dynamic_cast<Heightfield*>(rc->GetRenderable())) continue;
					tile.rendering = rc;
					const std::vector<std::shared_ptr<Renderable> > levels = rc->GetLODRenderables();
					for (size_t l = 0; l < levels.size(); l++)
						if (Heightfield* hf = dynamic_cast<Heightfield*>(levels[l].get())) tile.levels.push_back(hf);
				}
				else if (FoliageComponent* fc = dynamic_cast<FoliageComponent*>(comp)) tile.foliage = fc;
				else if (IPhysicsComponent* pc = dynamic_cast<IPhysicsComponent*>(comp))
				{
					if (pc->GetShape() == CollisionShapes::HeightFieldTerrain) tile.collision = pc;
				}
			}
			if (tile.levels.empty() || !tile.Data()) continue;
			tile.owner = go;
			tile.origin = go->GetWorldPosition();
			tiles.push_back(tile);
		}
		return tiles;
	}

	bool TerrainEditor::HeightAt(SceneGraph* scene, const f32 x, const f32 z, f32 &height)
	{
		const std::vector<TerrainTile> tiles = FindTiles(scene);
		for (size_t i = 0; i < tiles.size(); i++)
			if (tiles[i].Contains(x, z))
			{
				height = tiles[i].origin.y + tiles[i].Data()->HeightAt(x - tiles[i].origin.x, z - tiles[i].origin.z);
				return true;
			}
		return false;
	}

	TerrainEditor::TileState &TerrainEditor::State(const TerrainTile &tile)
	{
		for (size_t i = 0; i < states.size(); i++)
			if (states[i].owner == tile.owner) return states[i];
		TileState s;
		s.owner = tile.owner;
		states.push_back(s);
		return states.back();
	}

	std::string TerrainEditor::Resolve(const std::string &path) const
	{
		if (path.empty() || std::filesystem::path(path).is_absolute() || assetRoot.empty()) return path;
		return (std::filesystem::path(assetRoot) / path).lexically_normal().string();
	}

	uint32 TerrainEditor::Sculpt(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const f32 amount,
		const f32 hardness, const SculptMode mode, const f32 target)
	{
		if (radius <= 0.f) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = FindTiles(scene);
		std::vector<TerrainTile> touched;
		for (size_t i = 0; i < tiles.size(); i++)
			if (Overlaps(tiles[i], x, z, radius)) touched.push_back(tiles[i]);
		if (touched.empty()) return 0;

		// Smoothing reads its neighbours from how the ground was BEFORE this
		// step, in world space and across tile borders - reading a tile
		// already smoothed this step would give its neighbour a different
		// edge, and the border would open.
		std::vector<HeightfieldData> before;
		if (mode == Smooth)
			for (size_t i = 0; i < touched.size(); i++) before.push_back(*touched[i].Data());
		const auto worldHeight = [&](const f32 wx, const f32 wz, const f32 fallback) -> f32 {
			for (size_t i = 0; i < touched.size(); i++)
				if (touched[i].Contains(wx, wz))
					return touched[i].origin.y + before[i].HeightAt(wx - touched[i].origin.x, wz - touched[i].origin.z);
			return fallback;
		};

		uint32 changedTiles = 0;
		for (size_t t = 0; t < touched.size(); t++)
		{
			const TerrainTile &tile = touched[t];
			HeightfieldData &d = *tile.Data();
			const f32 s = d.Spacing();
			const int32 c0 = std::max(0, (int32)std::floor((x - radius - tile.origin.x) / s));
			const int32 c1 = std::min((int32)d.samples - 1, (int32)std::ceil((x + radius - tile.origin.x) / s));
			const int32 r0 = std::max(0, (int32)std::floor((z - radius - tile.origin.z) / s));
			const int32 r1 = std::min((int32)d.samples - 1, (int32)std::ceil((z + radius - tile.origin.z) / s));
			bool changed = false;
			for (int32 r = r0; r <= r1; r++)
				for (int32 c = c0; c <= c1; c++)
				{
					const f32 wx = tile.origin.x + c * s, wz = tile.origin.z + r * s;
					const f32 w = PaintableImage::Falloff(std::sqrt((wx - x) * (wx - x) + (wz - z) * (wz - z)) / radius, hardness);
					if (w <= 0.f) continue;
					f32 &h = d.heights[(size_t)r * d.samples + c];
					const f32 old = h;
					switch (mode)
					{
					case Raise: h += amount * w; break;
					case Lower: h -= amount * w; break;
					case Flatten: h += (target - tile.origin.y - h) * std::min(1.f, amount * w); break;
					case Smooth:
					{
						const f32 own = before[t].heights[(size_t)r * d.samples + c];
						f32 sum = 0.f;
						for (int32 dz = -1; dz <= 1; dz++)
							for (int32 dx = -1; dx <= 1; dx++)
								sum += worldHeight(wx + dx * s, wz + dz * s, tile.origin.y + own) - tile.origin.y;
						h = own + (sum / 9.f - own) * std::min(1.f, amount * w);
					}
					break;
					}
					// Only what the heightmap can store, so what is sculpted is
					// what is saved.
					h = std::min(std::max(h, d.rangeMin), d.rangeMax);
					if (h != old) changed = true;
				}
			if (!changed) continue;
			changedTiles++;
			for (size_t l = 0; l < tile.levels.size(); l++) tile.levels[l]->Rebuild();
			if (tile.rendering) tile.rendering->RefreshBounds();
			TileState &state = State(tile);
			state.heightsDirty = true;
			state.collisionDirty = true;
			bool listed = false;
			for (size_t i = 0; i < strokeTiles.size(); i++) listed = listed || strokeTiles[i].owner == tile.owner;
			if (!listed) strokeTiles.push_back(tile);
		}
		return changedTiles;
	}

	std::shared_ptr<PaintableImage> TerrainEditor::SplatImage(const TerrainTile &tile, TileState &state)
	{
		if (state.splat) return state.splat;
		if (!tile.rendering || tile.rendering->GetMeshes().empty()) return nullptr;
		IMaterial* material = tile.rendering->GetMeshes()[0]->Material.get();
		Texture* map = NULL;
		// A graph material's "splatMap" sampler, else a plain material's
		// colour map (painting colour straight onto the tile).
		if (CustomShaderMaterial* cm = dynamic_cast<CustomShaderMaterial*>(material))
		{
			const std::vector<std::string> &names = cm->GetSamplerNames();
			for (size_t i = 0; i < names.size() && i < cm->textures.size(); i++)
				if (names[i] == "splatMap" || names[i] == "uSplatMap") map = cm->textures[i].get();
		}
		else if (GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(material))
			map = gm->GetColorMap();
		if (!map || map->GetFilename().empty()) return nullptr;
		std::shared_ptr<PaintableImage> img = std::make_shared<PaintableImage>();
		if (!img->Load(map->GetFilename(), 4)) return nullptr;
		// The very Texture the material samples - LoadShared hands back the
		// cached object for the same file and wrap mode.
		img->texture = Texture::LoadShared(map->GetFilename(), TextureType::Texture, true, map->IsClampedToEdge());
		state.splat = img;
		state.splatPath = map->GetFilename();
		return img;
	}

	uint32 TerrainEditor::PaintSplat(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const uint32 layer,
		const f32 strength, const f32 hardness)
	{
		if (radius <= 0.f || layer > 3) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = FindTiles(scene);
		uint32 changed = 0;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			if (!Overlaps(tiles[i], x, z, radius)) continue;
			TileState &state = State(tiles[i]);
			std::shared_ptr<PaintableImage> img = SplatImage(tiles[i], state);
			if (!img) continue;
			const f32 size = tiles[i].Size();
			if (img->Paint((x - tiles[i].origin.x) / size, (z - tiles[i].origin.z) / size, radius / size, radius / size,
					layer, 1.f, strength, hardness, true))
			{
				img->Upload();
				state.splatDirty = true;
				changed++;
			}
		}
		return changed;
	}

	uint32 TerrainEditor::PaintFoliage(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const uint32 layer,
		const f32 target, const f32 strength, const f32 hardness)
	{
		if (radius <= 0.f) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = FindTiles(scene);
		uint32 changed = 0;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			const TerrainTile &tile = tiles[i];
			if (!tile.foliage || layer >= tile.foliage->GetLayers().size() || !Overlaps(tile, x, z, radius)) continue;
			FoliageComponent::Layer &fl = tile.foliage->GetLayers()[layer];
			if (!fl.densityMap)
			{
				fl.densityMap = std::make_shared<PaintableImage>();
				if (fl.spec.densityMap.empty() || !fl.densityMap->Load(Resolve(fl.spec.densityMap), 1))
				{
					// None yet: everywhere at full density, a pixel per metre
					// (edges on the tile's edges, like its heights).
					const int32 px = std::min(1025, std::max(2, (int32)tile.Size() + 1));
					fl.densityMap->Create(px, px, 1, 255);
				}
			}
			const f32 size = tile.Size();
			if (fl.densityMap->Paint((x - tile.origin.x) / size, (z - tile.origin.z) / size, radius / size, radius / size,
					0, target, strength, hardness, false))
			{
				TileState &state = State(tile);
				if (state.foliageDirty.size() <= layer) state.foliageDirty.resize(layer + 1, false);
				state.foliageDirty[layer] = true;
				bool listed = false;
				for (size_t s = 0; s < strokeTiles.size(); s++) listed = listed || strokeTiles[s].owner == tile.owner;
				if (!listed) strokeTiles.push_back(tile);
				changed++;
			}
		}
		return changed;
	}

	void TerrainEditor::FinishStroke()
	{
		for (size_t i = 0; i < strokeTiles.size(); i++)
		{
			const TerrainTile &tile = strokeTiles[i];
			TileState &state = State(tile);
			if (state.collisionDirty && tile.collision && strokeScene)
			{
				// Box3D copied the heights into its own compressed shape at
				// creation; a new body is how it sees the edit.
				tile.collision->Unregister(strokeScene);
				tile.collision->Register(strokeScene);
				state.collisionDirty = false;
			}
			if (tile.foliage)
			{
				if (state.heightsDirty) tile.foliage->Regrow(*tile.Data());
				else
					for (size_t l = 0; l < state.foliageDirty.size(); l++)
						if (state.foliageDirty[l]) tile.foliage->Regrow(*tile.Data(), (int32)l);
			}
		}
		strokeTiles.clear();
	}

	bool TerrainEditor::Save()
	{
		bool ok = true;
		std::vector<TerrainTile> tiles = FindTiles(strokeScene);
		for (size_t s = 0; s < states.size(); s++)
		{
			TileState &state = states[s];
			const TerrainTile* tile = NULL;
			for (size_t t = 0; t < tiles.size() && !tile; t++) if (tiles[t].owner == state.owner) tile = &tiles[t];
			if (!tile) continue;
			const Heightfield* hf = tile->levels[0];
			if (state.heightsDirty)
			{
				const HeightfieldData &d = *tile->Data();
				const f32 scale = hf->source.heightScale != 0.f ? hf->source.heightScale : 1.f;
				std::vector<uint16> px(d.heights.size());
				for (size_t i = 0; i < px.size(); i++)
				{
					const f32 u = (d.heights[i] - hf->source.heightOffset) / scale;
					px[i] = (uint16)std::lround(std::min(std::max(u, 0.f), 1.f) * 65535.f);
				}
				if (PaintableImage::WritePNG16(Resolve(hf->source.heightmap), (int32)d.samples, (int32)d.samples, &px[0])) state.heightsDirty = false;
				else ok = false;
			}
			if (state.splatDirty && state.splat)
			{
				if (state.splat->Save(state.splatPath)) state.splatDirty = false;
				else ok = false;
			}
			if (tile->foliage)
				for (size_t l = 0; l < state.foliageDirty.size() && l < tile->foliage->GetLayers().size(); l++)
				{
					FoliageComponent::Layer &fl = tile->foliage->GetLayers()[l];
					if (!fl.densityMap) continue;
					if (fl.spec.densityMap.empty())
					{
						// Next to the heightmap: "<heightmap>_<layer>_density.png".
						std::filesystem::path hp(hf->source.heightmap);
						fl.spec.densityMap = (hp.parent_path() / (hp.stem().string() + "_" + (fl.spec.name.empty() ? std::to_string(l) : fl.spec.name) + "_density.png")).generic_string();
					}
					if (fl.densityMap->Save(Resolve(fl.spec.densityMap))) state.foliageDirty[l] = false;
					else ok = false;
				}
		}
		return ok;
	}

}
