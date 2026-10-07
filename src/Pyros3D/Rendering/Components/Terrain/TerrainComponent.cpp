//============================================================================
// Name        : TerrainComponent.cpp
// Author      : Duarte Peixinho
// Description : See TerrainComponent.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Assets/Renderable/Terrains/CaveVolume.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>

namespace p3d {

	using json = nlohmann::json;
	namespace fs = std::filesystem;

	namespace {
		// Updates a dropped tile waits before it is destroyed - see
		// WorldStreamer's graveyard.
		const uint32 kGraveUpdates = 4;
		// Distant tiles made or remade per Update: a terrain's worth
		// spreads over a few frames instead of hitching one.
		const uint32 kDistantPerUpdate = 48;
		const char* const kTileToken = "{tile}";

		std::map<const SceneGraph*, std::vector<Vec3> > &Viewers()
		{
			static std::map<const SceneGraph*, std::vector<Vec3> > v;
			return v;
		}
		std::vector<TerrainComponent*> &Live()
		{
			static std::vector<TerrainComponent*> v;
			return v;
		}
		std::set<const GameObject*> &DistantObjects()
		{
			static std::set<const GameObject*> s;
			return s;
		}

		void DestroyAssets(LoadedSceneAssets &a)
		{
			a.gameObjects.clear();
			a.skeletonAnimations.clear();
			a.textureAnimations.clear();
			a.materials.clear();
			a.textures.clear();
			a.renderables.clear();
		}

		void RefreshSubtree(GameObject* go)
		{
			if (!go) return;
			go->RefreshTransformation();
			for (size_t i = 0; i < go->GetChildren().size(); i++) RefreshSubtree(go->GetChildren()[i].get());
		}

		std::string ReplaceAll(std::string text, const std::string &what, const std::string &with)
		{
			size_t at = 0;
			while ((at = text.find(what, at)) != std::string::npos)
			{
				text.replace(at, what.size(), with);
				at += with.size();
			}
			return text;
		}

		std::string Join(const std::string &root, const std::string &rel)
		{
			if (root.empty() || fs::path(rel).is_absolute()) return rel;
			return (fs::path(root) / rel).lexically_normal().generic_string();
		}

		// The first heightfield renderable under a tile node.
		const json* FindHeightfield(const json &node)
		{
			if (!node.is_object()) return NULL;
			if (node.contains("components") && node["components"].is_array())
				for (const auto &c : node["components"])
					if (c.is_object() && c.contains("renderable") && c["renderable"].is_object()
						&& c["renderable"].value("kind", std::string()) == "heightfield") return &c["renderable"];
			if (node.contains("children") && node["children"].is_array())
				for (const auto &k : node["children"])
					if (const json* r = FindHeightfield(k)) return r;
			return NULL;
		}

		uint32 OverviewSamples(const uint32 wanted)
		{
			// A power of two plus one, 3..257.
			uint32 n = 2;
			while (n * 2 <= std::max(2u, wanted - 1) && n < 256) n *= 2;
			return n + 1;
		}
	}

	TerrainComponent::TerrainComponent(const Settings &s, const std::string &root, IPhysics* physics, sol::state* lua)
		: settings(s), assetRoot(root), physics(physics), lua(lua)
	{
		settings.tilesX = std::max(1, settings.tilesX);
		settings.tilesZ = std::max(1, settings.tilesZ);
		settings.tileSize = std::max(1.f, settings.tileSize);
		settings.unloadRadius = std::max(settings.loadRadius, settings.unloadRadius);
		settings.overviewSamples = OverviewSamples(settings.overviewSamples);
		tiles.resize((size_t)settings.tilesX * settings.tilesZ);
		// A whole number of voxels to the tile, so lattices meet at borders.
		caveCells = std::max(4, (int32)std::lround(settings.tileSize / std::max(0.25f, settings.caveVoxel)));
		caveVoxel = settings.tileSize / (f32)caveCells;
		ReadTemplate();
		Live().push_back(this);
	}

	TerrainComponent::~TerrainComponent()
	{
		// The owner may already be gone: nothing here reaches for it. The
		// tiles are its children and die with it; what is still loading
		// must not finish into a dead terrain.
		for (size_t i = 0; i < tiles.size(); i++)
		{
			if (tiles[i].state == Tile::Loading && tiles[i].ticket) AssetStreamer::Instance().Cancel(tiles[i].ticket);
			if (tiles[i].distant) DistantObjects().erase(tiles[i].distant.get());
		}
		std::vector<TerrainComponent*> &live = Live();
		live.erase(std::remove(live.begin(), live.end(), this), live.end());
	}

	void TerrainComponent::ReadTemplate()
	{
		templateText = std::make_shared<const std::string>(settings.tileTemplate);
		heightScale = 1.f;
		heightOffset = 0.f;
		tileSkirt = 2.f;
		try
		{
			const json t = json::parse(settings.tileTemplate);
			if (const json* hf = t.contains("root") ? FindHeightfield(t["root"]) : NULL)
			{
				heightScale = hf->value("heightScale", heightScale);
				heightOffset = hf->value("heightOffset", heightOffset);
				tileSkirt = hf->value("skirt", tileSkirt);
			}
		}
		catch (const std::exception&)
		{
			echo("ERROR: TerrainComponent - the tile template is not valid JSON");
		}
	}

	void TerrainComponent::SetAssetRoot(const std::string &root)
	{
		if (root == assetRoot) return;
		assetRoot = root;
		ReloadTiles();
		distantMaterial.reset();
		overviewTried = false;
	}

	void TerrainComponent::SetRadii(const f32 load, const f32 unload)
	{
		settings.loadRadius = std::max(0.f, load);
		settings.unloadRadius = std::max(settings.loadRadius, unload);
	}

	void TerrainComponent::SetTileTemplate(const std::string &tileTemplate, const bool reload)
	{
		settings.tileTemplate = tileTemplate;
		ReadTemplate();
		if (reload) ReloadTiles();
	}

	void TerrainComponent::ReloadTiles()
	{
		for (int32 z = 0; z < settings.tilesZ; z++)
			for (int32 x = 0; x < settings.tilesX; x++)
				if (At(x, z).state != Tile::Unloaded) Unload(x, z);
	}

	void TerrainComponent::SetViewers(const SceneGraph* scene, const std::vector<Vec3> &viewers)
	{
		Viewers()[scene] = viewers;
	}

	const std::vector<TerrainComponent*> &TerrainComponent::Instances()
	{
		return Live();
	}

	bool TerrainComponent::IsDistantTile(const GameObject* object)
	{
		return object && DistantObjects().count(object) > 0;
	}

	std::string TerrainComponent::TileStem(const int32 x, const int32 z)
	{
		return std::to_string(x) + "_" + std::to_string(z);
	}

	std::string TerrainComponent::HeightmapPath(const int32 x, const int32 z) const
	{
		return settings.directory + "/" + TileStem(x, z) + ".png";
	}

	void TerrainComponent::Register(SceneGraph* Scene)
	{
		scene = Scene;
		Registered = true;
	}

	void TerrainComponent::Unregister(SceneGraph* Scene)
	{
		Registered = false;
		scene = NULL;
		Viewers().erase(Scene);
	}

	bool TerrainComponent::TileAt(const Vec3 &world, int32 &x, int32 &z) const
	{
		const Vec3 origin = (Owner ? Owner->GetWorldPosition() : Vec3()) + Corner();
		x = (int32)std::floor((world.x - origin.x) / settings.tileSize);
		z = (int32)std::floor((world.z - origin.z) / settings.tileSize);
		return x >= 0 && z >= 0 && x < settings.tilesX && z < settings.tilesZ;
	}

	Vec3 TerrainComponent::TileOrigin(const int32 x, const int32 z) const
	{
		return (Owner ? Owner->GetWorldPosition() : Vec3()) + Corner() + Vec3(x * settings.tileSize, 0.f, z * settings.tileSize);
	}

	bool TerrainComponent::TileOf(const GameObject* tile, int32 &x, int32 &z) const
	{
		if (!tile) return false;
		for (size_t i = 0; i < tiles.size(); i++)
			if (tiles[i].root.get() == tile)
			{
				x = (int32)(i % (size_t)settings.tilesX);
				z = (int32)(i / (size_t)settings.tilesX);
				return true;
			}
		return false;
	}

	GameObject* TerrainComponent::GetTile(const int32 x, const int32 z) const
	{
		if (x < 0 || z < 0 || x >= settings.tilesX || z >= settings.tilesZ) return NULL;
		return At(x, z).root.get();
	}

	uint32 TerrainComponent::LoadedCount() const
	{
		uint32 n = 0;
		for (size_t i = 0; i < tiles.size(); i++) if (tiles[i].state == Tile::Loaded && tiles[i].root) n++;
		return n;
	}

	uint32 TerrainComponent::LoadingCount() const
	{
		uint32 n = 0;
		for (size_t i = 0; i < tiles.size(); i++) if (tiles[i].state == Tile::Loading) n++;
		return n;
	}

	uint32 TerrainComponent::DistantCount() const
	{
		uint32 n = 0;
		for (size_t i = 0; i < tiles.size(); i++) if (tiles[i].distantShown) n++;
		return n;
	}

	f32 TerrainComponent::DistanceToTile(const Vec3 &local, const int32 x, const int32 z) const
	{
		const f32 minX = x * settings.tileSize, minZ = z * settings.tileSize;
		const f32 dx = std::max(0.f, std::max(minX - local.x, local.x - (minX + settings.tileSize)));
		const f32 dz = std::max(0.f, std::max(minZ - local.z, local.z - (minZ + settings.tileSize)));
		return std::sqrt(dx * dx + dz * dz);
	}

	std::string TerrainComponent::BuildTileJson(const Settings &s, const std::string &tileTemplate, const std::string &assetRoot,
		const int32 x, const int32 z)
	{
		const std::string stem = TileStem(x, z);
		json tile;
		try { tile = json::parse(ReplaceAll(tileTemplate, kTileToken, stem)); }
		catch (const std::exception&) { return std::string(); }
		if (!tile.is_object() || !tile.contains("root") || !tile["root"].is_object()) return std::string();
		json &root = tile["root"];
		root["name"] = "Tile_" + stem;
		root["position"] = { (x - 0.5f * s.tilesX) * s.tileSize, 0.f, (z - 0.5f * s.tilesZ) * s.tileSize };
		root["rotation"] = { 0, 0, 0 };
		root["scale"] = { 1, 1, 1 };
		root["static"] = true;
		if (root.contains("components") && root["components"].is_array())
			for (auto &c : root["components"])
			{
				// A tile with holes cut has its mask next to its heightmap.
				if (c.is_object() && c.contains("renderable") && c["renderable"].is_object()
					&& c["renderable"].value("kind", std::string()) == "heightfield")
				{
					const std::string rel = s.directory + "/" + stem + "_holes.png";
					std::error_code ec;
					if (fs::exists(Join(assetRoot, rel), ec)) c["renderable"]["holes"] = rel;
					else c["renderable"].erase("holes");
				}
				if (!c.is_object() || c.value("type", std::string()) != "Foliage" || !c.contains("layers") || !c["layers"].is_array()) continue;
				uint32 index = 0;
				for (auto &layer : c["layers"])
				{
					// The same field on every tile would read as a pattern.
					layer["seed"] = layer.value("seed", 1u) + (uint32)(z * s.tilesX + x) * 7919u;
					// A layer painted on this tile has its map next to the
					// tile's heightmap; one never painted grows everywhere.
					const std::string name = layer.value("name", std::string());
					const std::string rel = s.directory + "/" + stem + "_" + (name.empty() ? std::to_string(index) : name) + "_density.png";
					std::error_code ec;
					if (fs::exists(Join(assetRoot, rel), ec)) layer["densityMap"] = rel;
					else layer.erase("densityMap");
					index++;
				}
			}
		return tile.dump();
	}

	void TerrainComponent::RequestLoad(const int32 x, const int32 z, const f32 distance)
	{
		Tile &t = At(x, z);
		std::shared_ptr<std::shared_ptr<SceneSerializer::PreparedSubtree> > slot =
			std::make_shared<std::shared_ptr<SceneSerializer::PreparedSubtree> >();
		const Settings s = settings;
		const std::shared_ptr<const std::string> text = templateText;
		const std::string root = assetRoot;
		// The destructor cancels every ticket, so the steps never outlive
		// this terrain and may capture it. One texture upload per step,
		// then the objects - see WorldStreamer::RequestLoad.
		const AssetStreamer::Ticket ticket = AssetStreamer::Instance().SubmitSteps(
			[slot, s, text, root, x, z] {
				Settings bare = s;
				bare.tileTemplate.clear();
				const std::string tileJson = BuildTileJson(bare, *text, root, x, z);
				if (!tileJson.empty())
					*slot = SceneSerializer::PrepareSubtreeText(tileJson, root, "terrain tile " + TileStem(x, z));
			},
			[this, slot, x, z]() -> bool {
				if (*slot && SceneSerializer::UploadNextPrepared(**slot)) return false;
				Finish(x, z, std::static_pointer_cast<void>(*slot));
				return true;
			},
			distance);
		t.ticket = ticket;
		t.state = ticket ? Tile::Loading : Tile::Unloaded;
	}

	void TerrainComponent::Finish(const int32 x, const int32 z, const std::shared_ptr<void> &preparedVoid)
	{
		Tile &t = At(x, z);
		t.ticket = 0;
		// Loaded with no root is a tile whose maps failed: not retried
		// until the viewers leave and come back.
		t.state = Tile::Loaded;
		std::shared_ptr<SceneSerializer::PreparedSubtree> prepared = std::static_pointer_cast<SceneSerializer::PreparedSubtree>(preparedVoid);
		if (!prepared || !Owner) return;
		t.assets = std::make_shared<LoadedSceneAssets>();
		t.root = SceneSerializer::InstantiatePrepared(*prepared, physics, lua, t.assets.get());
		if (!t.root)
		{
			echo("ERROR: TerrainComponent - tile " + TileStem(x, z) + " built nothing");
			return;
		}
		// Not the scene file's: the terrain makes it again on every load.
		t.root->SetTransient(true);
		Owner->Add(t.root);
		// It has not been through a scene update: whoever asks where it is
		// before the next one must get the right answer.
		RefreshSubtree(t.root.get());
		// What was dug under it.
		{
			std::error_code ec;
			if (fs::exists(CavePath(x, z), ec))
			{
				t.caves = std::make_shared<CaveVolume>(caveVoxel, caveCells);
				if (!t.caves->Load(CavePath(x, z)))
				{
					echo("WARNING: TerrainComponent - " + TileStem(x, z) + "_caves.bin does not fit this terrain's cave grid; ignored");
					t.caves.reset();
				}
				else RebuildCave(x, z, true);
			}
		}
		SyncDistant(t);
		if (onLoaded) onLoaded(t.root.get());
	}

	void TerrainComponent::Unload(const int32 x, const int32 z)
	{
		Tile &t = At(x, z);
		if (t.state == Tile::Loading && t.ticket) AssetStreamer::Instance().Cancel(t.ticket);
		if (t.root)
		{
			if (onUnloading) onUnloading(t.root.get());
			if (Owner) Owner->Remove(t.root);
		}
		if (t.assets)
		{
			Grave g;
			g.assets = t.assets;
			g.updatesLeft = kGraveUpdates;
			graveyard.push_back(g);
		}
		t.state = Tile::Unloaded;
		t.ticket = 0;
		// The walls are the tile's child: they left with it, and die with
		// what is in the graveyard no sooner than it does.
		if (t.caveObject)
		{
			Grave g;
			g.object = t.caveObject;
			g.updatesLeft = kGraveUpdates;
			graveyard.push_back(g);
		}
		t.caveObject.reset();
		t.caves.reset();
		t.root.reset();
		t.assets.reset();
		SyncDistant(t);
	}

	void TerrainComponent::CollectGraveyard(const bool all)
	{
		for (size_t i = 0; i < graveyard.size(); i++)
			if (graveyard[i].updatesLeft > 0) graveyard[i].updatesLeft--;
		while (!graveyard.empty() && (all || graveyard.front().updatesLeft == 0))
		{
			if (graveyard.front().assets) DestroyAssets(*graveyard.front().assets);
			graveyard.pop_front();
		}
	}

	void TerrainComponent::Stream(const std::vector<Vec3> &foci)
	{
		if (!Owner) return;
		const Vec3 origin = Owner->GetWorldPosition() + Corner();
		std::vector<Vec3> local(foci.size());
		for (size_t f = 0; f < foci.size(); f++) local[f] = foci[f] - origin;
		for (int32 z = 0; z < settings.tilesZ; z++)
			for (int32 x = 0; x < settings.tilesX; x++)
			{
				Tile &t = At(x, z);
				// The nearest viewer decides: wanted by one is wanted.
				f32 d = 1e30f;
				for (size_t f = 0; f < local.size(); f++) d = std::min(d, DistanceToTile(local[f], x, z));
				switch (t.state)
				{
				case Tile::Unloaded:
					if (d <= settings.loadRadius) RequestLoad(x, z, d);
					break;
				case Tile::Loading:
					if (d > settings.unloadRadius) Unload(x, z);
					else AssetStreamer::Instance().SetPriority(t.ticket, d);
					break;
				case Tile::Loaded:
					if (d > settings.unloadRadius && !(unloadVeto && t.root && unloadVeto(t.root.get()))) Unload(x, z);
					break;
				}
			}
	}

	void TerrainComponent::Update(const f64 time)
	{
		(void)time;
		CollectGraveyard(false);
		if (!Registered || !Owner) return;
		std::map<const SceneGraph*, std::vector<Vec3> >::const_iterator v = Viewers().find(scene);
		static const std::vector<Vec3> none;
		const std::vector<Vec3> &foci = (v != Viewers().end()) ? v->second : none;
		// No viewers: nothing is known about where anyone is, so what is
		// loaded stays and nothing new comes in.
		if (!foci.empty()) Stream(foci);
		UpdateDistant(foci);
	}

	void TerrainComponent::LoadAround(const std::vector<Vec3> &foci)
	{
		if (!Owner) return;
		Owner->RefreshTransformation();
		Stream(foci);
		while (LoadingCount() > 0) AssetStreamer::Instance().Flush();
	}

	// ---- caves -----------------------------------------------------------

	std::string TerrainComponent::CavePath(const int32 x, const int32 z) const
	{
		return Join(assetRoot, settings.directory + "/" + TileStem(x, z) + "_caves.bin");
	}

	const HeightfieldData* TerrainComponent::TileGround(const int32 x, const int32 z) const
	{
		GameObject* root = GetTile(x, z);
		if (!root) return NULL;
		for (size_t c = 0; c < root->GetComponents().size(); c++)
			if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(root->GetComponents()[c].get()))
				if (Heightfield* hf = dynamic_cast<Heightfield*>(rc->GetRenderable())) return hf->GetData().get();
		return NULL;
	}

	CaveVolume* TerrainComponent::TileCaves(const int32 x, const int32 z, const bool create)
	{
		if (!GetTile(x, z)) return NULL;
		Tile &t = At(x, z);
		if (!t.caves && create) t.caves = std::make_shared<CaveVolume>(caveVoxel, caveCells);
		return t.caves.get();
	}

	bool TerrainComponent::CarveTile(const int32 x, const int32 z, const Vec3 &world, const f32 radius, const bool air)
	{
		// Filling a tile that has no caves has nothing to fill.
		CaveVolume* v = TileCaves(x, z, air);
		if (!v) return false;
		return v->Carve(world - TileOrigin(x, z), radius, air);
	}

	bool TerrainComponent::BrushTile(const int32 x, const int32 z, const int mode, const Vec3 &world, const f32 radius, const f32 amount,
		const f32 hardness, const f32 level)
	{
		// Only digging makes caves where there were none.
		const bool makes = mode == CaveVolume::Dig || mode == CaveVolume::Level;
		CaveVolume* v = TileCaves(x, z, makes);
		if (!v) return false;
		const Vec3 origin = TileOrigin(x, z);
		// One pass of smoothing only reaches a point's nearest neighbours;
		// a few in a row take the larger bumps down too.
		bool changed = false;
		const int passes = mode == CaveVolume::Smooth ? 4 : 1;
		for (int i = 0; i < passes; i++)
			changed = v->Brush((CaveVolume::BrushMode)mode, world - origin, radius, amount, hardness, level - origin.y) || changed;
		return changed;
	}

	bool TerrainComponent::CutCaveHole(const int32 x, const int32 z, const Vec3 &world, const f32 radius, const bool open)
	{
		CaveVolume* v = TileCaves(x, z, false);
		if (!v || v->Empty()) return false;
		return v->CutHole(world - TileOrigin(x, z), radius, open);
	}

	f32 TerrainComponent::GroundHeight(const f32 wx, const f32 wz, const f32 fallback) const
	{
		int32 x, z;
		if (!TileAt(Vec3(wx, 0.f, wz), x, z)) return fallback;
		const HeightfieldData* d = TileGround(x, z);
		if (!d) return fallback;
		const Vec3 o = TileOrigin(x, z);
		return o.y + d->HeightAt(wx - o.x, wz - o.z);
	}

	f32 TerrainComponent::OpeningAt(const Vec3 &surface) const
	{
		int32 x, z;
		if (!TileAt(surface, x, z)) return 0.f;
		const Tile &t = At(x, z);
		if (!t.caves || !t.root) return 0.f;
		return t.caves->OpeningAt(surface - TileOrigin(x, z));
	}

	f32 TerrainComponent::AirAt(const Vec3 &world) const
	{
		int32 x, z;
		if (!TileAt(world, x, z)) return 0.f;
		const Tile &t = At(x, z);
		if (!t.caves || !t.root) return 0.f;
		return t.caves->AirAt(world - TileOrigin(x, z));
	}

	std::vector<uchar> TerrainComponent::CaveBlob(const int32 x, const int32 z) const
	{
		if (x < 0 || z < 0 || x >= settings.tilesX || z >= settings.tilesZ || !At(x, z).caves) return std::vector<uchar>();
		return At(x, z).caves->ToBlob();
	}

	void TerrainComponent::SetCaveBlob(const int32 x, const int32 z, const std::vector<uchar> &blob)
	{
		if (!GetTile(x, z)) return;
		Tile &t = At(x, z);
		if (blob.empty()) t.caves.reset();
		else
		{
			t.caves = std::make_shared<CaveVolume>(caveVoxel, caveCells);
			if (!t.caves->FromBlob(blob)) t.caves.reset();
		}
		RebuildCave(x, z, true);
	}

	bool TerrainComponent::SaveCaves(const int32 x, const int32 z)
	{
		if (x < 0 || z < 0 || x >= settings.tilesX || z >= settings.tilesZ) return false;
		const Tile &t = At(x, z);
		std::error_code ec;
		if (!t.caves || t.caves->Empty())
		{
			fs::remove(CavePath(x, z), ec);
			return true;
		}
		return t.caves->Save(CavePath(x, z));
	}

	void TerrainComponent::SetCaveMaterial(const std::string &materialJson)
	{
		settings.caveMaterial = materialJson;
		caveMaterial.reset();
		for (int32 z = 0; z < settings.tilesZ; z++)
			for (int32 x = 0; x < settings.tilesX; x++)
				if (At(x, z).caveObject) RebuildCave(x, z, true);
	}

	void TerrainComponent::RebuildCave(const int32 x, const int32 z, const bool collision)
	{
		Tile &t = At(x, z);
		if (!t.root) return;
		if (t.caveObject)
		{
			t.root->Remove(t.caveObject);
			Grave g;
			g.object = t.caveObject;
			g.updatesLeft = kGraveUpdates;
			graveyard.push_back(g);
			t.caveObject.reset();
		}
		if (!t.caves || t.caves->Empty()) return;
		CaveMeshData mesh;
		// The ground as the mesher needs it: this tile's, and - for the
		// voxel or two it looks past each border - the neighbour's, so two
		// tiles make the same ring of ground along the border they share.
		const HeightfieldData* own = TileGround(x, z);
		const Vec3 origin = TileOrigin(x, z);
		const f32 base = Owner ? Owner->GetWorldPosition().y : 0.f;
		CaveVolume::GroundFn ground;
		if (own)
			ground = [this, own, origin, base](const f32 lx, const f32 lz) -> f32 {
				if (lx >= 0.f && lz >= 0.f && lx <= own->size && lz <= own->size) return own->HeightAt(lx, lz);
				const f32 clamped = own->HeightAt(std::min(std::max(lx, 0.f), own->size), std::min(std::max(lz, 0.f), own->size));
				return GroundHeight(origin.x + lx, origin.z + lz, base + clamped) - base;
			};
		t.caves->BuildMesh(ground, x == settings.tilesX - 1, z == settings.tilesZ - 1, mesh);
		if (mesh.index.empty()) return;

		std::shared_ptr<GameObject> go = std::make_shared<GameObject>(true);
		go->SetName("Caves_" + TileStem(x, z));
		go->SetTransient(true);
		// Collision first: the renderable takes the arrays.
		if (collision && physics)
			if (std::shared_ptr<IPhysicsComponent> body = physics->CreateTriangleMesh(mesh.index, mesh.vertex, 0.f, false))
				go->AddComponent(body);
		// A process that draws nothing keeps only the collision.
		if (!HeightfieldMesh::IsHeadless())
		{
			if (!caveMaterial)
			{
				std::string def = settings.caveMaterial;
				if (def.empty())
					def = "{\"id\":0,\"kind\":\"generic\",\"options\":16384,\"color\":[0.42,0.40,0.37,1.0],\"roughness\":0.95,\"castingShadows\":false}";
				caveMaterial = SceneSerializer::BuildMaterialFromText(def, assetRoot);
			}
			if (caveMaterial)
			{
				std::shared_ptr<CaveMesh> walls = std::make_shared<CaveMesh>(std::move(mesh));
				std::shared_ptr<RenderingComponent> rc = std::make_shared<RenderingComponent>(std::static_pointer_cast<Renderable>(walls), caveMaterial);
				rc->DisableCastShadows();
				go->AddComponent(rc);
			}
		}
		t.root->Add(go);
		RefreshSubtree(go.get());
		t.caveObject = go;
	}

	// ---- the overview ----------------------------------------------------

	std::string TerrainComponent::OverviewPath(const bool colour) const
	{
		return Join(assetRoot, settings.directory + (colour ? "/overview_color.png" : "/overview.png"));
	}

	bool TerrainComponent::LoadOverview()
	{
		overviewTried = true;
		overviewLoaded = false;
		const uint32 n = settings.overviewSamples;
		const uint32 side = (uint32)std::max(settings.tilesX, settings.tilesZ) * (n - 1) + 1;
		std::error_code ec;
		if (!fs::exists(OverviewPath(false), ec)) return false;
		HeightfieldData grid;
		if (!HeightfieldData::LoadFile(OverviewPath(false), 1.f, heightScale, heightOffset, grid) || grid.samples != side)
		{
			echo("WARNING: TerrainComponent - " + settings.directory + "/overview.png does not match this terrain; bake it again");
			return false;
		}
		overviewSide = side;
		overviewHeights.swap(grid.heights);
		overviewLoaded = true;
		overviewVersion++;
		return true;
	}

	void TerrainComponent::EnsureDistantMaterial()
	{
		if (distantMaterial) return;
		json m;
		m["id"] = 0;
		m["kind"] = "generic";
		m["options"] = 16386;	// diffuse | texture
		m["color"] = { 1, 1, 1, 1 };
		m["roughness"] = 0.95;
		m["clampMaps"] = true;
		m["castingShadows"] = false;
		std::error_code ec;
		if (fs::exists(OverviewPath(true), ec)) m["colorMap"] = settings.directory + "/overview_color.png";
		else { m["options"] = 16384; m["color"] = { 0.35, 0.45, 0.25, 1 }; }
		distantMaterial = SceneSerializer::BuildMaterialFromText(m.dump(), assetRoot);
	}

	void TerrainComponent::DropDistant(Tile &t)
	{
		if (!t.distant) return;
		DistantObjects().erase(t.distant.get());
		if (t.distantShown && Owner) Owner->Remove(t.distant);
		Grave g;
		g.object = t.distant;
		g.updatesLeft = kGraveUpdates;
		graveyard.push_back(g);
		t.distant.reset();
		t.distantShown = false;
	}

	void TerrainComponent::SyncDistant(Tile &t)
	{
		if (!t.distant || !Owner) return;
		// In the scene exactly while the full tile is not.
		const bool show = !(t.state == Tile::Loaded && t.root);
		if (show == t.distantShown) return;
		if (show) { Owner->Add(t.distant); RefreshSubtree(t.distant.get()); }
		else Owner->Remove(t.distant);
		t.distantShown = show;
	}

	// How far off, in tiles, a distant tile goes to every second sample and to every fourth.
	static const f32 kDistantHalfFrom = 5.f, kDistantQuarterFrom = 12.f;

	void TerrainComponent::BuildDistant(const int32 x, const int32 z)
	{
		Tile &t = At(x, z);
		DropDistant(t);
		t.distantVersion = overviewVersion;
		EnsureDistantMaterial();
		if (!distantMaterial || !Owner) return;

		const uint32 n = settings.overviewSamples;
		const uint32 gx0 = (uint32)x * (n - 1), gz0 = (uint32)z * (n - 1);
		const f32 spacing = settings.tileSize / (f32)(n - 1);
		// The terrain's own extent in the (square, padded) grid: normals
		// stop at the terrain's edge, not at the padding.
		const uint32 maxX = (uint32)settings.tilesX * (n - 1), maxZ = (uint32)settings.tilesZ * (n - 1);
		const std::vector<f32> &H = overviewHeights;
		const uint32 side = overviewSide;

		std::shared_ptr<HeightfieldData> d = std::make_shared<HeightfieldData>();
		d->samples = n;
		d->size = settings.tileSize;
		d->rangeMin = std::min(heightOffset, heightOffset + heightScale);
		d->rangeMax = std::max(heightOffset, heightOffset + heightScale);
		d->heights.resize((size_t)n * n);
		d->normals.resize((size_t)n * n);
		d->minHeight = 1e30f;
		d->maxHeight = -1e30f;
		for (uint32 r = 0; r < n; r++)
			for (uint32 c = 0; c < n; c++)
			{
				const uint32 gx = gx0 + c, gz = gz0 + r;
				const f32 h = H[(size_t)gz * side + gx];
				d->heights[(size_t)r * n + c] = h;
				d->minHeight = std::min(d->minHeight, h);
				d->maxHeight = std::max(d->maxHeight, h);
				// Central differences over the whole terrain: this tile's
				// edge reads its neighbour's ground, and both agree.
				const uint32 xa = gx > 0 ? gx - 1 : gx, xb = std::min(gx + 1, maxX);
				const uint32 za = gz > 0 ? gz - 1 : gz, zb = std::min(gz + 1, maxZ);
				const f32 dx = (H[(size_t)gz * side + xb] - H[(size_t)gz * side + xa]) / (spacing * (f32)(xb - xa));
				const f32 dz = (H[(size_t)zb * side + gx] - H[(size_t)za * side + gx]) / (spacing * (f32)(zb - za));
				d->normals[(size_t)r * n + c] = Vec3(-dx, 1.f, -dz).normalize();
			}
		// This tile's rectangle of the one colour image.
		d->uvScale = Vec2(1.f / settings.tilesX, 1.f / settings.tilesZ);
		d->uvOffset = Vec2((f32)x / settings.tilesX, (f32)z / settings.tilesZ);

		// A skirt deep enough to cover the step down to a full tile's
		// coarsest level next door.
		const f32 skirt = std::max(tileSkirt, settings.tileSize / 16.f);
		HeightfieldMesh mesh;
		HeightfieldMesh::Build(*d, 1, skirt, mesh);
		std::shared_ptr<Heightfield> hf = std::make_shared<Heightfield>(std::move(mesh), d, 1);
		hf->source.skirt = skirt;
		// Far enough off, a distant tile is drawn from every second sample of
		// its ground and then every fourth: on a terrain of a thousand tiles
		// most of what is drawn is these, kilometres away, and each was the
		// same two thousand triangles as the one next to the loaded ground.
		// (Their skirts are deeper by as much, to cover the coarser edge.)
		const bool half = n >= 9 && (n - 1) % 2 == 0, quarter = n >= 17 && (n - 1) % 4 == 0;
		std::shared_ptr<RenderingComponent> rc = std::make_shared<RenderingComponent>(std::static_pointer_cast<Renderable>(hf), distantMaterial,
			half ? settings.tileSize * kDistantHalfFrom : 0.f);
		if (half)
		{
			HeightfieldMesh coarse;
			HeightfieldMesh::Build(*d, 2, skirt * 2.f, coarse);
			std::shared_ptr<Heightfield> level = std::make_shared<Heightfield>(std::move(coarse), d, 2);
			level->source.skirt = skirt * 2.f;
			rc->AddLOD(std::static_pointer_cast<Renderable>(level), quarter ? settings.tileSize * kDistantQuarterFrom : 1e9f, distantMaterial);
		}
		if (quarter)
		{
			HeightfieldMesh coarse;
			HeightfieldMesh::Build(*d, 4, skirt * 4.f, coarse);
			std::shared_ptr<Heightfield> level = std::make_shared<Heightfield>(std::move(coarse), d, 4);
			level->source.skirt = skirt * 4.f;
			rc->AddLOD(std::static_pointer_cast<Renderable>(level), 1e9f, distantMaterial);
		}
		rc->DisableCastShadows();

		std::shared_ptr<GameObject> go = std::make_shared<GameObject>(true);
		go->SetName("Distant_" + TileStem(x, z));
		go->SetTransient(true);
		go->SetPosition(Corner() + Vec3(x * settings.tileSize, 0.f, z * settings.tileSize));
		go->AddComponent(rc);
		t.distant = go;
		DistantObjects().insert(go.get());
		SyncDistant(t);
	}

	void TerrainComponent::UpdateDistant(const std::vector<Vec3> &foci)
	{
		// A process that draws nothing has no use for them.
		if (HeightfieldMesh::IsHeadless() || !Owner) return;
		if (!overviewTried) LoadOverview();
		if (!overviewLoaded) return;
		const Vec3 origin = Owner->GetWorldPosition() + Corner();
		const size_t count = tiles.size();
		uint32 built = 0;
		for (size_t step = 0; step < count && built < kDistantPerUpdate; step++)
		{
			const size_t i = (distantCursor + step) % count;
			Tile &t = tiles[i];
			const int32 x = (int32)(i % (size_t)settings.tilesX), z = (int32)(i / (size_t)settings.tilesX);
			bool wanted = true;
			if (settings.viewDistance > 0.f)
			{
				f32 d = foci.empty() ? 0.f : 1e30f;
				for (size_t f = 0; f < foci.size(); f++) d = std::min(d, DistanceToTile(foci[f] - origin, x, z));
				wanted = d <= settings.viewDistance;
				if (!wanted && t.distant && d > settings.viewDistance * 1.1f) DropDistant(t);
			}
			if (wanted && (!t.distant || t.distantVersion != overviewVersion))
			{
				BuildDistant(x, z);
				built++;
				if (built == kDistantPerUpdate) distantCursor = (uint32)((i + 1) % count);
			}
		}
		if (built < kDistantPerUpdate) distantCursor = 0;
	}

	bool TerrainComponent::BakeOverview(const std::vector<std::pair<int32, int32> >* only, std::string &error)
	{
		const uint32 n = settings.overviewSamples;
		const uint32 side = (uint32)std::max(settings.tilesX, settings.tilesZ) * (n - 1) + 1;
		// Colour texels per tile side: two per grid cell, fewer when the
		// image would outgrow what every device takes.
		uint32 cn = (n - 1) * 2;
		while (cn > 2 && cn * (uint32)std::max(settings.tilesX, settings.tilesZ) > 4096) cn /= 2;
		const uint32 cw = cn * (uint32)settings.tilesX, ch = cn * (uint32)settings.tilesZ;

		// What is there already, when only some tiles change.
		std::vector<f32> heights((size_t)side * side, heightOffset);
		std::vector<uchar> colour((size_t)cw * ch * 4, 255);
		bool partial = only != NULL;
		if (partial)
		{
			HeightfieldData old;
			PaintableImage oldColour;
			std::error_code ec;
			if (fs::exists(OverviewPath(false), ec) && HeightfieldData::LoadFile(OverviewPath(false), 1.f, heightScale, heightOffset, old)
				&& old.samples == side && oldColour.Load(OverviewPath(true), 4) && (uint32)oldColour.width == cw && (uint32)oldColour.height == ch)
			{
				heights.swap(old.heights);
				colour.swap(oldColour.pixels);
			}
			else partial = false;	// nothing usable to keep: bake it all
		}

		// What the tiles' material shows: a splat map over four ground
		// textures, each reduced to its average colour, or a colour map.
		Vec3 layerColour[4] = { Vec3(0.3f, 0.45f, 0.2f), Vec3(0.4f, 0.32f, 0.22f), Vec3(0.45f, 0.45f, 0.43f), Vec3(0.75f, 0.7f, 0.5f) };
		std::string splatPattern, colourPattern;
		try
		{
			const json t = json::parse(settings.tileTemplate);
			if (t.contains("materials") && t["materials"].is_array())
				for (const auto &m : t["materials"])
				{
					if (!m.is_object()) continue;
					if (m.contains("samplers") && m["samplers"].is_array())
						for (const auto &s : m["samplers"])
						{
							const std::string name = s.value("name", std::string()), file = s.value("texture", std::string());
							if (name == "splatMap") splatPattern = file;
							else if (name.size() == 6 && name.compare(0, 5, "layer") == 0 && name[5] >= '0' && name[5] <= '3' && !file.empty())
							{
								PaintableImage img;
								if (img.Load(Join(assetRoot, file), 4) && !img.pixels.empty())
								{
									f64 r = 0, g = 0, b = 0;
									const size_t px = img.pixels.size() / 4;
									for (size_t i = 0; i < px; i++) { r += img.pixels[i * 4]; g += img.pixels[i * 4 + 1]; b += img.pixels[i * 4 + 2]; }
									layerColour[name[5] - '0'] = Vec3((f32)(r / px / 255.0), (f32)(g / px / 255.0), (f32)(b / px / 255.0));
								}
							}
						}
					else if (splatPattern.empty() && colourPattern.empty() && m.contains("colorMap") && m["colorMap"].is_string()
						&& m["colorMap"].get<std::string>().find(kTileToken) != std::string::npos)
						colourPattern = m["colorMap"].get<std::string>();
				}
		}
		catch (const std::exception&) { error = "the tile template is not valid JSON"; return false; }

		std::vector<std::pair<int32, int32> > todo;
		if (partial) todo = *only;
		else
			for (int32 z = 0; z < settings.tilesZ; z++)
				for (int32 x = 0; x < settings.tilesX; x++) todo.push_back(std::make_pair(x, z));

		for (size_t i = 0; i < todo.size(); i++)
		{
			const int32 x = todo[i].first, z = todo[i].second;
			if (x < 0 || z < 0 || x >= settings.tilesX || z >= settings.tilesZ) continue;
			HeightfieldData tile;
			if (!HeightfieldData::LoadFile(Join(assetRoot, HeightmapPath(x, z)), settings.tileSize, heightScale, heightOffset, tile))
			{
				error = "could not read " + HeightmapPath(x, z);
				return false;
			}
			for (uint32 r = 0; r < n; r++)
				for (uint32 c = 0; c < n; c++)
					heights[(size_t)((uint32)z * (n - 1) + r) * side + (uint32)x * (n - 1) + c] =
						tile.HeightAt((f32)c / (n - 1) * tile.size, (f32)r / (n - 1) * tile.size);

			PaintableImage splat, tileColour;
			const bool haveSplat = !splatPattern.empty() && splat.Load(Join(assetRoot, ReplaceAll(splatPattern, kTileToken, TileStem(x, z))), 4);
			const bool haveColour = !haveSplat && !colourPattern.empty()
				&& tileColour.Load(Join(assetRoot, ReplaceAll(colourPattern, kTileToken, TileStem(x, z))), 4);
			for (uint32 r = 0; r < cn; r++)
				for (uint32 c = 0; c < cn; c++)
				{
					const f32 u = (c + 0.5f) / cn, v = (r + 0.5f) / cn;
					Vec3 rgb(0.35f, 0.45f, 0.25f);
					if (haveSplat)
					{
						f32 w[4], sum = 0.f;
						for (uint32 k = 0; k < 4; k++) { w[k] = splat.Sample(u, v, k); sum += w[k]; }
						if (sum > 1e-4f)
						{
							rgb = Vec3();
							for (int k = 0; k < 4; k++) rgb = rgb + layerColour[k] * (w[k] / sum);
						}
					}
					else if (haveColour) rgb = Vec3(tileColour.Sample(u, v, 0), tileColour.Sample(u, v, 1), tileColour.Sample(u, v, 2));
					uchar* p = &colour[((size_t)((uint32)z * cn + r) * cw + (uint32)x * cn + c) * 4];
					p[0] = (uchar)std::lround(std::min(std::max(rgb.x, 0.f), 1.f) * 255.f);
					p[1] = (uchar)std::lround(std::min(std::max(rgb.y, 0.f), 1.f) * 255.f);
					p[2] = (uchar)std::lround(std::min(std::max(rgb.z, 0.f), 1.f) * 255.f);
					p[3] = 255;
				}
		}

		const f32 scale = heightScale != 0.f ? heightScale : 1.f;
		std::vector<uint16> px(heights.size());
		for (size_t i = 0; i < px.size(); i++)
			px[i] = (uint16)std::lround(std::min(std::max((heights[i] - heightOffset) / scale, 0.f), 1.f) * 65535.f);
		if (!PaintableImage::WritePNG16(OverviewPath(false), (int32)side, (int32)side, &px[0])
			|| !PaintableImage::WritePNG(OverviewPath(true), (int32)cw, (int32)ch, 4, &colour[0]))
		{
			error = "could not write the overview under " + settings.directory;
			return false;
		}

		// The picture changed on disk: the material is built again from
		// it, and every distant tile is cut again - the ones just baked
		// (and their neighbours, whose edge normals read them) right now,
		// the rest over the next few updates, each staying up until its
		// replacement exists.
		Texture::ForgetShared(OverviewPath(true));
		distantMaterial.reset();
		if (HeightfieldMesh::IsHeadless()) return true;
		if (!LoadOverview()) { error = "the overview was written but could not be read back"; return false; }
		if (partial && Owner)
		{
			std::set<std::pair<int32, int32> > now;
			for (size_t i = 0; i < todo.size(); i++)
				for (int32 dz = -1; dz <= 1; dz++)
					for (int32 dx = -1; dx <= 1; dx++)
					{
						const int32 x = todo[i].first + dx, z = todo[i].second + dz;
						if (x >= 0 && z >= 0 && x < settings.tilesX && z < settings.tilesZ) now.insert(std::make_pair(x, z));
					}
			for (std::set<std::pair<int32, int32> >::const_iterator it = now.begin(); it != now.end(); ++it)
				BuildDistant(it->first, it->second);
		}
		return true;
	}

}
