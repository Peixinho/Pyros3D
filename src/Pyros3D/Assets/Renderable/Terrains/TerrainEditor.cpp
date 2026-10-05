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
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Assets/Renderable/Terrains/CaveVolume.h>
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

	namespace {
		// The Terrain a tile belongs to, and which of its tiles it is.
		TerrainComponent* TerrainOf(const TerrainTile &tile, int32 &x, int32 &z)
		{
			GameObject* parent = tile.owner ? tile.owner->GetParent() : NULL;
			if (!parent) return NULL;
			for (size_t c = 0; c < parent->GetComponents().size(); c++)
				if (TerrainComponent* tc = dynamic_cast<TerrainComponent*>(parent->GetComponents()[c].get()))
					return tc->TileOf(tile.owner, x, z) ? tc : NULL;
			return NULL;
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
			tile.distant = TerrainComponent::IsDistantTile(go);
			tiles.push_back(tile);
		}
		return tiles;
	}

	std::vector<TerrainTile> TerrainEditor::EditableTiles(SceneGraph* scene) const
	{
		std::vector<TerrainTile> tiles = FindTiles(scene);
		std::vector<TerrainTile> out;
		for (size_t i = 0; i < tiles.size(); i++)
			if (!tiles[i].distant && (!editable || editable(tiles[i].owner))) out.push_back(tiles[i]);
		return out;
	}

	namespace {
		// The scene's tiles, found once a frame. FindTiles() walks every
		// object in the scene and asks each component what it is, which on
		// a world of a few thousand objects is over a millisecond - and a
		// script asking the height of the ground at thirty points round the
		// player, as one that listens for the shore does, paid it thirty
		// times: a 40-60 ms frame twice a second. Tiles come and go in the
		// scene's Update(), so within a frame the list cannot change.
		const std::vector<TerrainTile> &TilesThisFrame(SceneGraph* scene)
		{
			static SceneGraph* forScene = NULL;
			static f64 forTime = -1.0;
			static uint32 forGeneration = 0;
			static std::vector<TerrainTile> tiles;
			// ... except when a script swaps the scene for another: the new
			// scene is the same object at the same time, and its tiles are new
			// ones - the list held pointers to the old scene's, already
			// destroyed, and the first height asked for in the new scene read
			// through one (a crash going from a title screen into the game).
			// Any tile made or destroyed since the list was made makes it stale.
			const uint32 generation = Heightfield::Generation();
			// The list is kept for as long as it is right, not just for the frame:
			// finding the tiles walks the whole scene, and with the scene's time
			// as the key it was found again every frame (and more than once in a
			// frame wherever the time differs between callers) - a tenth of the
			// frame in a game whose scripts ask for heights. Tiles made or
			// destroyed make it stale at once; a tile that was made before it was
			// added to the scene is picked up by the look every half second.
			const f64 now = scene ? scene->GetUpdateTime() : -1.0;
			if (scene != forScene || generation != forGeneration || now < forTime || now - forTime > 0.5)
			{
				tiles = TerrainEditor::FindTiles(scene);
				forScene = scene;
				forTime = now;
				forGeneration = generation;
			}
			return tiles;
		}
	}

	bool TerrainEditor::HeightAt(SceneGraph* scene, const f32 x, const f32 z, f32 &height)
	{
		const std::vector<TerrainTile> &tiles = TilesThisFrame(scene);
		for (size_t i = 0; i < tiles.size(); i++)
			if (tiles[i].Contains(x, z))
			{
				const HeightfieldData* data = tiles[i].Data();
				if (!data) continue;
				height = tiles[i].origin.y + data->HeightAt(x - tiles[i].origin.x, z - tiles[i].origin.z);
				return true;
			}
		return false;
	}

	bool TerrainEditor::SplatAt(SceneGraph* scene, const f32 x, const f32 z, f32 weights[4])
	{
		const std::vector<TerrainTile> tiles = FindTiles(scene);
		for (size_t i = 0; i < tiles.size(); i++)
		{
			if (tiles[i].distant || !tiles[i].Contains(x, z)) continue;
			std::shared_ptr<PaintableImage> img = SplatImage(tiles[i], State(tiles[i]));
			if (!img || img->channels < 4) continue;
			const f32 size = tiles[i].Size();
			for (uint32 c = 0; c < 4; c++)
				weights[c] = img->Sample((x - tiles[i].origin.x) / size, (z - tiles[i].origin.z) / size, c);
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
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		std::vector<TerrainTile> touched;
		for (size_t i = 0; i < tiles.size(); i++)
			if (Overlaps(tiles[i], x, z, radius)) touched.push_back(tiles[i]);
		if (touched.empty()) return 0;
		for (size_t i = 0; i < touched.size(); i++) Touch(touched[i]);

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
			{
				// Caves under it stop at the ground, wherever that is now.
				int32 tx, tz;
				TerrainComponent* tc = TerrainOf(tile, tx, tz);
				if (tc && tc->TileCaves(tx, tz))
				{
					// And the ground's openings are where the caves reach
					// the surface, which has just moved: raise it over a
					// pit and the pit is roofed, lower it onto a cave and
					// the cave opens.
					SyncHolesToCaves(tile, x, z, radius + s * 2.f);
					tc->RebuildCave(tx, tz, false);
					state.caveCollisionDirty = true;
				}
			}
			bool listed = false;
			for (size_t i = 0; i < strokeTiles.size(); i++) listed = listed || strokeTiles[i].owner == tile.owner;
			if (!listed) strokeTiles.push_back(tile);
		}
		return changedTiles;
	}

	uint32 TerrainEditor::CutHoles(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const bool open)
	{
		if (radius <= 0.f) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changedTiles = 0;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			const TerrainTile &tile = tiles[t];
			if (!Overlaps(tile, x, z, radius)) continue;
			HeightfieldData &d = *tile.Data();
			if (d.samples < 2) continue;
			const f32 s = d.Spacing();
			// The field ramps from ground to hole across two cells around
			// the brush's rim, linear in distance - so where it passes one
			// half, which is where the mesh is cut, is the brush's circle.
			const f32 reach = radius + s;
			const int32 c0 = std::max(0, (int32)std::floor((x - reach - tile.origin.x) / s));
			const int32 c1 = std::min((int32)d.samples - 1, (int32)std::ceil((x + reach - tile.origin.x) / s));
			const int32 r0 = std::max(0, (int32)std::floor((z - reach - tile.origin.z) / s));
			const int32 r1 = std::min((int32)d.samples - 1, (int32)std::ceil((z + reach - tile.origin.z) / s));
			bool touched = false, changed = false;
			for (int32 r = r0; r <= r1; r++)
				for (int32 c = c0; c <= c1; c++)
				{
					const f32 wx = tile.origin.x + c * s, wz = tile.origin.z + r * s;
					const f32 dist = std::sqrt((wx - x) * (wx - x) + (wz - z) * (wz - z));
					const f32 inside = std::min(std::max(0.5f + (radius - dist) / (2.f * s), 0.f), 1.f);
					if (inside <= 0.f) continue;
					const f32 was = d.HoleAt((uint32)c, (uint32)r);
					const f32 now = open ? std::max(was, inside) : std::min(was, 1.f - inside);
					const uchar q = (uchar)std::lround(now * 255.f);
					if (!d.holes.empty() && q == d.holes[(size_t)r * d.samples + c]) continue;
					if (d.holes.empty() && q == 0) continue;
					if (!touched) { Touch(tile); touched = true; }
					if (d.holes.empty()) d.holes.assign((size_t)d.samples * d.samples, 0);
					d.holes[(size_t)r * d.samples + c] = q;
					changed = true;
				}
			if (!changed) continue;
			changedTiles++;
			for (size_t l = 0; l < tile.levels.size(); l++) tile.levels[l]->Rebuild();
			TileState &state = State(tile);
			state.holesDirty = true;
			state.collisionDirty = true;
			bool listed = false;
			for (size_t i = 0; i < strokeTiles.size(); i++) listed = listed || strokeTiles[i].owner == tile.owner;
			if (!listed) strokeTiles.push_back(tile);
		}
		return changedTiles;
	}

	void TerrainEditor::SyncHolesToCaves(const TerrainTile &tile, const f32 x, const f32 z, const f32 reach)
	{
		int32 tx, tz;
		TerrainComponent* tc = TerrainOf(tile, tx, tz);
		if (!tc) return;
		HeightfieldData &d = *tile.Data();
		const f32 s = d.Spacing();
		const int32 c0 = std::max(0, (int32)std::floor((x - reach - tile.origin.x) / s));
		const int32 c1 = std::min((int32)d.samples - 1, (int32)std::ceil((x + reach - tile.origin.x) / s));
		const int32 r0 = std::max(0, (int32)std::floor((z - reach - tile.origin.z) / s));
		const int32 r1 = std::min((int32)d.samples - 1, (int32)std::ceil((z + reach - tile.origin.z) / s));
		bool changed = false;
		for (int32 r = r0; r <= r1; r++)
			for (int32 c = c0; c <= c1; c++)
			{
				// How open the surface is there: the terrain ends a little
				// outside the cave's rim, on the ring of ground the cave's
				// own mesh carries round every opening.
				const Vec3 at(tile.origin.x + c * s, tile.origin.y + d.At((uint32)c, (uint32)r), tile.origin.z + r * s);
				const uchar q = (uchar)std::lround(std::min(std::max(tc->OpeningAt(at), 0.f), 1.f) * 255.f);
				const uchar was = d.holes.empty() ? 0 : d.holes[(size_t)r * d.samples + c];
				if (q == was) continue;
				if (d.holes.empty()) d.holes.assign((size_t)d.samples * d.samples, 0);
				d.holes[(size_t)r * d.samples + c] = q;
				changed = true;
			}
		if (!changed) return;
		for (size_t l = 0; l < tile.levels.size(); l++) tile.levels[l]->Rebuild();
		TileState &state = State(tile);
		state.holesDirty = true;
		state.collisionDirty = true;
	}

	void TerrainEditor::MarkCaveEdit(const TerrainTile &tile)
	{
		TileState &state = State(tile);
		state.cavesDirty = true;
		state.caveCollisionDirty = true;
		bool listed = false;
		for (size_t i = 0; i < strokeTiles.size(); i++) listed = listed || strokeTiles[i].owner == tile.owner;
		if (!listed) strokeTiles.push_back(tile);
	}

	uint32 TerrainEditor::Dig(SceneGraph* scene, const Vec3 &centre, const f32 radius, const bool air)
	{
		if (radius <= 0.f) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changedTiles = 0;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			const TerrainTile &tile = tiles[t];
			int32 tx, tz;
			TerrainComponent* tc = TerrainOf(tile, tx, tz);
			if (!tc) continue;
			// The tile's lattice reaches a voxel past each border.
			const f32 reach = radius + tc->CaveVoxel() * (CaveVolume::kApron + 1.f);
			if (!Overlaps(tile, centre.x, centre.z, reach)) continue;
			Touch(tile);
			if (!tc->CarveTile(tx, tz, centre, radius, air)) continue;
			changedTiles++;
			// The walls now, their collision when the stroke ends.
			SyncHolesToCaves(tile, centre.x, centre.z, reach);
			tc->RebuildCave(tx, tz, false);
			MarkCaveEdit(tile);
		}
		return changedTiles;
	}

	uint32 TerrainEditor::CaveBrush(SceneGraph* scene, const int mode, const Vec3 &centre, const f32 radius, const f32 amount,
		const f32 hardness, const f32 level)
	{
		if (radius <= 0.f || amount <= 0.f) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changedTiles = 0;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			const TerrainTile &tile = tiles[t];
			int32 tx, tz;
			TerrainComponent* tc = TerrainOf(tile, tx, tz);
			if (!tc) continue;
			const f32 reach = radius + tc->CaveVoxel() * (CaveVolume::kApron + 1.f);
			if (!Overlaps(tile, centre.x, centre.z, reach)) continue;
			Touch(tile);
			if (!tc->BrushTile(tx, tz, mode, centre, radius, amount, hardness, level)) continue;
			changedTiles++;
			SyncHolesToCaves(tile, centre.x, centre.z, reach);
			tc->RebuildCave(tx, tz, false);
			MarkCaveEdit(tile);
		}
		return changedTiles;
	}

	uint32 TerrainEditor::CutCaveHoles(SceneGraph* scene, const Vec3 &centre, const f32 radius, const bool open)
	{
		if (radius <= 0.f) return 0;
		strokeScene = scene;
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changedTiles = 0;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			const TerrainTile &tile = tiles[t];
			int32 tx, tz;
			TerrainComponent* tc = TerrainOf(tile, tx, tz);
			if (!tc || !tc->TileCaves(tx, tz)) continue;
			if (!Overlaps(tile, centre.x, centre.z, radius + tc->CaveVoxel() * (CaveVolume::kApron + 1.f))) continue;
			Touch(tile);
			if (!tc->CutCaveHole(tx, tz, centre, radius, open)) continue;
			changedTiles++;
			tc->RebuildCave(tx, tz, false);
			MarkCaveEdit(tile);
		}
		return changedTiles;
	}

	uint32 TerrainEditor::GenerateCaves(SceneGraph* scene, const Vec3 &centre, const f32 radius, const uint32 seed, const f32 size,
		const f32 width, const f32 minDepth, const f32 maxDepth)
	{
		strokeScene = scene;
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changedTiles = 0;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			const TerrainTile &tile = tiles[t];
			int32 tx, tz;
			TerrainComponent* tc = TerrainOf(tile, tx, tz);
			if (!tc) continue;
			if (radius > 0.f && !Overlaps(tile, centre.x, centre.z, radius + tc->CaveVoxel() * (CaveVolume::kApron + 1.f))) continue;
			Touch(tile);
			CaveVolume* v = tc->TileCaves(tx, tz, true);
			if (!v) continue;
			CaveVolume::Noise noise;
			noise.seed = seed;
			noise.size = size;
			noise.width = width;
			noise.minDepth = minDepth;
			noise.maxDepth = maxDepth;
			// The noise is sampled in the terrain's own space, so tunnels
			// carry on from one tile into the next.
			const Vec3 inTerrain = tile.origin - tc->TileOrigin(0, 0);
			if (!v->Generate(*tile.Data(), Vec3(inTerrain.x, tile.origin.y, inTerrain.z), centre - tile.origin, radius, noise)) continue;
			changedTiles++;
			SyncHolesToCaves(tile, tile.origin.x + tile.Size() * 0.5f, tile.origin.z + tile.Size() * 0.5f, tile.Size());
			tc->RebuildCave(tx, tz, false);
			MarkCaveEdit(tile);
		}
		return changedTiles;
	}

	uint32 TerrainEditor::ResyncCaves(SceneGraph* scene)
	{
		strokeScene = scene;
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 count = 0;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			const TerrainTile &tile = tiles[t];
			int32 tx, tz;
			TerrainComponent* tc = TerrainOf(tile, tx, tz);
			if (!tc || !tc->TileCaves(tx, tz)) continue;
			Touch(tile);
			SyncHolesToCaves(tile, tile.origin.x + tile.Size() * 0.5f, tile.origin.z + tile.Size() * 0.5f, tile.Size());
			tc->RebuildCave(tx, tz, false);
			MarkCaveEdit(tile);
			count++;
		}
		return count;
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
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changed = 0;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			if (!Overlaps(tiles[i], x, z, radius)) continue;
			TileState &state = State(tiles[i]);
			std::shared_ptr<PaintableImage> img = SplatImage(tiles[i], state);
			if (!img) continue;
			Touch(tiles[i]);
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
		std::vector<TerrainTile> tiles = EditableTiles(scene);
		uint32 changed = 0;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			const TerrainTile &tile = tiles[i];
			if (!tile.foliage || layer >= tile.foliage->GetLayers().size() || !Overlaps(tile, x, z, radius)) continue;
			FoliageComponent::Layer &fl = tile.foliage->GetLayers()[layer];
			if (!fl.densityMap)
			{
				fl.densityMap = std::make_shared<PaintableImage>();
				const std::string saved = !fl.densityMapPath.empty() ? fl.densityMapPath : Resolve(fl.spec.densityMap);
				if (fl.spec.densityMap.empty() || !fl.densityMap->Load(saved, 1))
				{
					// None yet: everywhere at full density, a pixel per metre
					// (edges on the tile's edges, like its heights).
					const int32 px = std::min(1025, std::max(2, (int32)tile.Size() + 1));
					fl.densityMap->Create(px, px, 1, 255);
					// Named now rather than at Save(): the layer's saved
					// form names its map, and an object rebuilt from that
					// form before a save must still find it.
					if (fl.spec.densityMap.empty())
					{
						std::filesystem::path hp(tile.levels[0]->source.heightmap);
						fl.spec.densityMap = (hp.parent_path() / (hp.stem().string() + "_" + (fl.spec.name.empty() ? std::to_string(layer) : fl.spec.name) + "_density.png")).generic_string();
					}
				}
			}
			Touch(tile);
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
			if (state.caveCollisionDirty)
			{
				int32 tx, tz;
				if (TerrainComponent* tc = TerrainOf(tile, tx, tz)) tc->RebuildCave(tx, tz, true);
				state.caveCollisionDirty = false;
			}
			if (tile.foliage)
			{
				if (state.heightsDirty || state.holesDirty) tile.foliage->Regrow(*tile.Data());
				else
					for (size_t l = 0; l < state.foliageDirty.size(); l++)
						if (state.foliageDirty[l]) tile.foliage->Regrow(*tile.Data(), (int32)l);
			}
		}
		strokeTiles.clear();
		if (recording)
		{
			recording = false;
			lastBefore.swap(strokeBefore);
			lastAfter.clear();
			for (size_t i = 0; i < strokeTouched.size(); i++) lastAfter.push_back(Capture(strokeTouched[i]));
			strokeBefore.clear();
			strokeTouched.clear();
		}
	}

	TerrainEditor::TileSnapshot TerrainEditor::Capture(const TerrainTile &tile)
	{
		TileSnapshot snap;
		snap.heightmap = tile.levels[0]->source.heightmap;
		snap.heights = tile.Data()->heights;
		snap.holes = tile.Data()->holes;
		{
			int32 tx, tz;
			if (TerrainComponent* tc = TerrainOf(tile, tx, tz)) snap.caves = tc->CaveBlob(tx, tz);
		}
		for (size_t i = 0; i < states.size(); i++)
			if (states[i].owner == tile.owner && states[i].splat) snap.splat = states[i].splat->pixels;
		if (tile.foliage)
		{
			const std::vector<FoliageComponent::Layer> &layers = tile.foliage->GetLayers();
			snap.density.resize(layers.size());
			for (size_t l = 0; l < layers.size(); l++)
				if (layers[l].densityMap) snap.density[l] = layers[l].densityMap->pixels;
		}
		return snap;
	}

	void TerrainEditor::Touch(const TerrainTile &tile)
	{
		if (!recording) return;
		for (size_t i = 0; i < strokeTouched.size(); i++)
			if (strokeTouched[i].owner == tile.owner)
			{
				// Already recorded - but a map loaded since (this stroke's
				// first dab on it) is still as it was: record it now.
				TileSnapshot &snap = strokeBefore[i];
				const TileSnapshot now = Capture(tile);
				if (snap.splat.empty()) snap.splat = now.splat;
				if (snap.density.size() < now.density.size()) snap.density.resize(now.density.size());
				for (size_t l = 0; l < now.density.size(); l++)
					if (snap.density[l].empty()) snap.density[l] = now.density[l];
				return;
			}
		strokeTouched.push_back(tile);
		strokeBefore.push_back(Capture(tile));
	}

	void TerrainEditor::BeginStroke()
	{
		recording = true;
		strokeBefore.clear();
		strokeTouched.clear();
	}

	bool TerrainEditor::TakeStrokeUndo(std::vector<TileSnapshot> &before, std::vector<TileSnapshot> &after)
	{
		before.swap(lastBefore);
		after.swap(lastAfter);
		lastBefore.clear();
		lastAfter.clear();
		return !before.empty();
	}

	void TerrainEditor::Restore(SceneGraph* scene, const std::vector<TileSnapshot> &snapshots)
	{
		strokeScene = scene;
		std::vector<TerrainTile> tiles = FindTiles(scene);
		for (size_t s = 0; s < snapshots.size(); s++)
		{
			const TileSnapshot &snap = snapshots[s];
			for (size_t t = 0; t < tiles.size(); t++)
			{
				const TerrainTile &tile = tiles[t];
				if (tile.levels[0]->source.heightmap != snap.heightmap) continue;
				TileState &state = State(tile);
				HeightfieldData &d = *tile.Data();
				bool regrowAll = false;
				if (snap.heights.size() == d.heights.size() && snap.heights != d.heights)
				{
					d.heights = snap.heights;
					for (size_t l = 0; l < tile.levels.size(); l++) tile.levels[l]->Rebuild();
					if (tile.rendering) tile.rendering->RefreshBounds();
					if (tile.collision)
					{
						tile.collision->Unregister(scene);
						tile.collision->Register(scene);
					}
					state.heightsDirty = true;
					regrowAll = true;
				}
				if (snap.holes != d.holes)
				{
					d.holes = snap.holes;
					for (size_t l = 0; l < tile.levels.size(); l++) tile.levels[l]->Rebuild();
					if (tile.collision)
					{
						tile.collision->Unregister(scene);
						tile.collision->Register(scene);
					}
					state.holesDirty = true;
					regrowAll = true;
				}
				{
					int32 tx, tz;
					TerrainComponent* tc = TerrainOf(tile, tx, tz);
					if (tc && tc->CaveBlob(tx, tz) != snap.caves)
					{
						tc->SetCaveBlob(tx, tz, snap.caves);
						state.cavesDirty = true;
					}
					// The caves did not change but the ground over them did:
					// their walls stop at the ground and carry a ring of it,
					// so they are made again for the ground that is back.
					else if (tc && regrowAll && tc->TileCaves(tx, tz)) tc->RebuildCave(tx, tz, true);
				}
				if (!snap.splat.empty())
				{
					std::shared_ptr<PaintableImage> img = SplatImage(tile, state);
					if (img && img->pixels.size() == snap.splat.size())
					{
						img->pixels = snap.splat;
						img->Upload();
						state.splatDirty = true;
					}
				}
				if (tile.foliage)
				{
					std::vector<FoliageComponent::Layer> &layers = tile.foliage->GetLayers();
					if (state.foliageDirty.size() < layers.size()) state.foliageDirty.resize(layers.size(), false);
					for (size_t l = 0; l < layers.size(); l++)
					{
						const bool had = l < snap.density.size() && !snap.density[l].empty();
						if (had && layers[l].densityMap && layers[l].densityMap->pixels.size() == snap.density[l].size())
						{
							if (layers[l].densityMap->pixels == snap.density[l]) continue;
							layers[l].densityMap->pixels = snap.density[l];
						}
						else if (!had && layers[l].densityMap) layers[l].densityMap.reset();	// back to the file
						else continue;
						state.foliageDirty[l] = true;
						if (!regrowAll) tile.foliage->Regrow(d, (int32)l);
					}
					if (regrowAll) tile.foliage->Regrow(d);
				}
				break;
			}
		}
	}

	bool TerrainEditor::HasUnsaved(const GameObject* owner) const
	{
		for (size_t i = 0; i < states.size(); i++)
		{
			if (states[i].owner != owner) continue;
			if (states[i].heightsDirty || states[i].splatDirty || states[i].holesDirty || states[i].cavesDirty) return true;
			for (size_t l = 0; l < states[i].foliageDirty.size(); l++) if (states[i].foliageDirty[l]) return true;
		}
		return false;
	}

	std::vector<const GameObject*> TerrainEditor::UnsavedOwners() const
	{
		std::vector<const GameObject*> out;
		for (size_t i = 0; i < states.size(); i++)
			if (states[i].owner && HasUnsaved(states[i].owner)) out.push_back(states[i].owner);
		return out;
	}

	void TerrainEditor::Forget(const GameObject* owner)
	{
		for (size_t i = states.size(); i-- > 0;) if (states[i].owner == owner) states.erase(states.begin() + i);
		for (size_t i = strokeTiles.size(); i-- > 0;) if (strokeTiles[i].owner == owner) strokeTiles.erase(strokeTiles.begin() + i);
		for (size_t i = strokeTouched.size(); i-- > 0;)
			if (strokeTouched[i].owner == owner)
			{
				strokeTouched.erase(strokeTouched.begin() + i);
				strokeBefore.erase(strokeBefore.begin() + i);
			}
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
			if (state.cavesDirty)
			{
				int32 tx, tz;
				TerrainComponent* tc = TerrainOf(*tile, tx, tz);
				if (!tc || tc->SaveCaves(tx, tz)) state.cavesDirty = false;
				else ok = false;
			}
			if (state.holesDirty)
			{
				// Next to the heightmap: "<heightmap>_holes.png", a pixel per
				// grid point, white where there is no ground. Written even
				// when the last hole was filled, so the file says so.
				const HeightfieldData &d = *tile->Data();
				const uint32 cells = d.samples;
				std::filesystem::path hp(hf->source.heightmap);
				const std::string rel = !hf->source.holes.empty() ? hf->source.holes
					: (hp.parent_path() / (hp.stem().string() + "_holes.png")).generic_string();
				std::vector<uchar> px((size_t)cells * cells, 0);
				for (size_t i = 0; i < px.size() && i < d.holes.size(); i++) px[i] = d.holes[i];
				if (PaintableImage::WritePNG(Resolve(rel), (int32)cells, (int32)cells, 1, &px[0]))
				{
					state.holesDirty = false;
					for (size_t l = 0; l < tile->levels.size(); l++) tile->levels[l]->source.holes = rel;
				}
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
					// None in memory: the file is already what it should be.
					if (!fl.densityMap) { state.foliageDirty[l] = false; continue; }
					if (fl.spec.densityMap.empty())
					{
						// Next to the heightmap: "<heightmap>_<layer>_density.png".
						std::filesystem::path hp(hf->source.heightmap);
						fl.spec.densityMap = (hp.parent_path() / (hp.stem().string() + "_" + (fl.spec.name.empty() ? std::to_string(l) : fl.spec.name) + "_density.png")).generic_string();
					}
					if (fl.densityMap->Save(Resolve(fl.spec.densityMap)))
					{
						state.foliageDirty[l] = false;
						fl.densityMapPath = Resolve(fl.spec.densityMap);
					}
					else ok = false;
				}
		}
		return ok;
	}

}
