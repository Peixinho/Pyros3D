//============================================================================
// Name        : WorldStreamer.cpp
// Author      : Duarte Peixinho
// Description : See WorldStreamer.h.
//============================================================================

#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <Pyros3D/Utils/Streaming/LoadStats.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace p3d {

	namespace {
		// Updates a dropped cell waits before it is destroyed: more than the
		// frames any backend keeps in flight (Vulkan and Metal use two or
		// three), so nothing still recording or executing can reference it.
		const uint32 kGraveUpdates = 4;

		// UnloadScene's order: objects first, so components let go of the
		// materials, textures and meshes before those are dropped.
		void Destroy(LoadedSceneAssets &a)
		{
			a.gameObjects.clear();
			a.skeletonAnimations.clear();
			a.textureAnimations.clear();
			a.materials.clear();
			a.textures.clear();
			a.renderables.clear();
		}
	}

	WorldStreamer::WorldStreamer(SceneGraph* scene, const std::string &scenePath, const SceneMeta::World &world,
		IPhysics* physics, sol::state* lua)
		: scene(scene), physics(physics), lua(lua), scenePath(scenePath), world(world)
	{
		std::filesystem::path dir = std::filesystem::path(scenePath).parent_path();
		cellsDir = (dir / world.cellsDir).generic_string();
		if (!cellsDir.empty() && cellsDir.back() != '/') cellsDir += "/";
		for (size_t i = 0; i < world.cells.size(); i++)
			cells[world.cells[i]] = Cell();
		for (size_t i = 0; i < world.farCells.size(); i++)
			cells[world.farCells[i]].hasFar = true;
	}

	std::string WorldStreamer::FarFileName(const int32 x, const int32 z)
	{
		return std::to_string(x) + "_" + std::to_string(z) + ".far.json";
	}

	void WorldStreamer::AddFarCell(const int32 x, const int32 z)
	{
		const CellKey key(x, z);
		Cell &c = cells[key];
		if (!c.hasFar)
		{
			c.hasFar = true;
			world.farCells.push_back(key);
		}
		// A new far version replaces the one in memory.
		if (c.farState != Cell::Unloaded) UnloadFar(key);
	}

	uint32 WorldStreamer::FarShownCount() const
	{
		uint32 n = 0;
		for (std::map<CellKey, Cell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
			if (it->second.farShown) n++;
		return n;
	}

	bool WorldStreamer::IsFarRoot(const GameObject* root) const
	{
		if (!root) return false;
		for (std::map<CellKey, Cell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
			if (it->second.farRoot.get() == root) return true;
		return false;
	}

	void WorldStreamer::SyncFarVisibility(Cell &c)
	{
		const bool show = c.farRoot && !(c.state == Cell::Loaded && c.root);
		if (show == c.farShown) return;
		if (show) scene->Add(c.farRoot);
		else scene->Remove(c.farRoot);
		c.farShown = show;
	}

	void WorldStreamer::Bury(const std::shared_ptr<LoadedSceneAssets> &assets)
	{
		if (!assets) return;
		Grave g;
		g.assets = assets;
		g.updatesLeft = kGraveUpdates;
		graveyard.push_back(g);
	}

	void WorldStreamer::UnloadFar(const CellKey &key)
	{
		Cell &c = cells[key];
		if (c.farState == Cell::Loading && c.farTicket)
			AssetStreamer::Instance().Cancel(c.farTicket);
		if (c.farRoot && c.farShown) scene->Remove(c.farRoot);
		Bury(c.farAssets);
		c.farState = Cell::Unloaded;
		c.farTicket = 0;
		c.farRoot.reset();
		c.farAssets.reset();
		c.farShown = false;
	}

	WorldStreamer::~WorldStreamer()
	{
		UnloadAll();
	}

	std::string WorldStreamer::CellFileName(const int32 x, const int32 z)
	{
		return std::to_string(x) + "_" + std::to_string(z) + ".json";
	}

	void WorldStreamer::CellOf(const Vec3 &p, const f32 cellSize, int32 &x, int32 &z)
	{
		x = (int32)std::floor(p.x / cellSize);
		z = (int32)std::floor(p.z / cellSize);
	}

	f32 WorldStreamer::DistanceToCell(const Vec3 &p, const CellKey &key) const
	{
		const f32 minX = key.first * world.cellSize, minZ = key.second * world.cellSize;
		const f32 dx = std::max(0.f, std::max(minX - p.x, p.x - (minX + world.cellSize)));
		const f32 dz = std::max(0.f, std::max(minZ - p.z, p.z - (minZ + world.cellSize)));
		return std::sqrt(dx * dx + dz * dz);
	}

	void WorldStreamer::RequestLoad(const CellKey &key, const f32 distance, const bool farVersion)
	{
		Cell &c = cells[key];
		const std::string path = cellsDir + (farVersion ? FarFileName(key.first, key.second) : CellFileName(key.first, key.second));
		const std::string sceneFile = scenePath;
		std::shared_ptr<std::shared_ptr<SceneSerializer::PreparedSubtree> > slot =
			std::make_shared<std::shared_ptr<SceneSerializer::PreparedSubtree> >();
		const std::chrono::steady_clock::time_point requested = std::chrono::steady_clock::now();
		std::shared_ptr<f64> workMs = std::make_shared<f64>(0.0);
		// The destructor cancels every ticket, so finish never outlives
		// this streamer and may capture it.
		// Stepped: one texture upload per step, then the objects, so a cell
		// full of new textures spreads over frames instead of hitching one.
		std::shared_ptr<f64> uploadMs = std::make_shared<f64>(0.0);
		std::shared_ptr<uint32> uploads = std::make_shared<uint32>(0);
		const AssetStreamer::Ticket ticket = AssetStreamer::Instance().SubmitSteps(
			[slot, path, sceneFile, workMs] {
				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				*slot = SceneSerializer::PrepareSubtreeFile(path, sceneFile);
				*workMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
			},
			[this, key, slot, requested, workMs, uploadMs, uploads, farVersion]() -> bool {
				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				if (*slot && SceneSerializer::UploadNextPrepared(**slot))
				{
					*uploadMs += std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
					(*uploads)++;
					return false;
				}
				const f64 mat0 = LoadStats::Ms(LoadStats::MaterialBuild), obj0 = LoadStats::Ms(LoadStats::ObjectBuild);
				Finish(key, *slot, farVersion);
				if (LoadStats::TraceEnabled() && !farVersion)
				{
					const f64 finishMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
					const f64 totalMs = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - requested).count();
					std::fprintf(stderr, "[stream] cell %d_%d: %.1f ms off-thread; main thread: %u upload(s) %.1f ms, then %.1f ms"
						" (materials %.1f, objects %.1f); %.1f ms from request\n",
						key.first, key.second, *workMs, *uploads, *uploadMs, finishMs,
						LoadStats::Ms(LoadStats::MaterialBuild) - mat0, LoadStats::Ms(LoadStats::ObjectBuild) - obj0, totalMs);
				}
				return true;
			},
			distance);
		// 0 when the streamer has been shut down: leave it unloaded rather
		// than waiting on a request that will never come back.
		if (farVersion)
		{
			c.farTicket = ticket;
			c.farState = ticket ? Cell::Loading : Cell::Unloaded;
			return;
		}
		c.ticket = ticket;
		c.state = ticket ? Cell::Loading : Cell::Unloaded;
	}

	void WorldStreamer::Finish(const CellKey &key, const std::shared_ptr<SceneSerializer::PreparedSubtree> &prepared, const bool farVersion)
	{
		Cell &c = cells[key];
		if (farVersion)
		{
			c.farTicket = 0;
			c.farState = Cell::Loaded;
			if (!prepared) return;
			c.farAssets = std::make_shared<LoadedSceneAssets>();
			// No physics: nobody walks on the horizon. Transient: never
			// saved with the scene it is shown in.
			c.farRoot = SceneSerializer::InstantiatePrepared(*prepared, NULL, NULL, c.farAssets.get());
			if (c.farRoot) c.farRoot->SetTransient(true);
			SyncFarVisibility(c);
			return;
		}
		c.ticket = 0;
		c.state = Cell::Loaded;
		if (!prepared) return;	// already logged; stays empty until left
		c.assets = std::make_shared<LoadedSceneAssets>();
		c.root = SceneSerializer::InstantiatePrepared(*prepared, physics, lua, c.assets.get());
		if (c.root)
		{
			scene->Add(c.root);
			if (onLoaded) onLoaded(c.root);
		}
		else echo("ERROR: WorldStreamer - cell " + CellFileName(key.first, key.second) + " built nothing");
		SyncFarVisibility(c);
	}

	void WorldStreamer::Unload(const CellKey &key)
	{
		Cell &c = cells[key];
		if (c.state == Cell::Loading && c.ticket)
			AssetStreamer::Instance().Cancel(c.ticket);
		if (c.root)
		{
			if (onUnloading) onUnloading(c.root);
			scene->Remove(c.root);
		}
		Bury(c.assets);
		// The far version outlives its cell: keep it, and show it again.
		c.state = Cell::Unloaded;
		c.ticket = 0;
		c.root.reset();
		c.assets.reset();
		SyncFarVisibility(c);
	}

	void WorldStreamer::CollectGraveyard(const bool all)
	{
		while (!graveyard.empty())
		{
			Grave &g = graveyard.front();
			if (!all && g.updatesLeft > 0) break;	// FIFO: the rest are younger
			Destroy(*g.assets);
			graveyard.pop_front();
		}
		for (size_t i = 0; i < graveyard.size(); i++)
			if (graveyard[i].updatesLeft > 0) graveyard[i].updatesLeft--;
	}

	void WorldStreamer::Update(const Vec3 &focus, const f64 pumpBudgetMs)
	{
		Update(std::vector<Vec3>(1, focus), pumpBudgetMs);
	}

	void WorldStreamer::Update(const std::vector<Vec3> &foci, const f64 pumpBudgetMs)
	{
		CollectGraveyard(false);

		for (std::map<CellKey, Cell>::iterator it = cells.begin(); it != cells.end(); ++it)
		{
			Cell &c = it->second;
			// The nearest focus decides: wanted by one is wanted.
			f32 d = 1e30f;
			for (size_t f = 0; f < foci.size(); f++) d = std::min(d, DistanceToCell(foci[f], it->first));
			switch (c.state)
			{
			case Cell::Unloaded:
				if (d <= world.loadRadius) RequestLoad(it->first, d);
				break;
			case Cell::Loading:
				if (d > world.unloadRadius) Unload(it->first);
				else AssetStreamer::Instance().SetPriority(c.ticket, d);
				break;
			case Cell::Loaded:
				if (d > world.unloadRadius && !(unloadVeto && c.root && unloadVeto(c.root))) Unload(it->first);
				break;
			}

			// The far version: wanted out to farRadius (dropped a tenth
			// past it), queued behind every full cell - the ground under the
			// camera matters more than the horizon.
			if (c.hasFar && world.farRadius > 0.f)
			{
				const f32 farUnload = world.farRadius * 1.1f;
				if (c.farState == Cell::Unloaded && d <= world.farRadius && d > world.loadRadius * 0.5f)
					RequestLoad(it->first, d + world.farRadius * 2.f, true);
				else if (c.farState != Cell::Unloaded && d > farUnload)
					UnloadFar(it->first);
				else if (c.farState == Cell::Loading)
					AssetStreamer::Instance().SetPriority(c.farTicket, d + world.farRadius * 2.f);
			}
			else if (c.farState != Cell::Unloaded) UnloadFar(it->first);
		}

		AssetStreamer::Instance().Pump(pumpBudgetMs);
	}

	void WorldStreamer::LoadAround(const Vec3 &focus)
	{
		LoadAround(std::vector<Vec3>(1, focus));
	}

	void WorldStreamer::LoadAround(const std::vector<Vec3> &foci)
	{
		Update(foci, 0.0);
		while (LoadingCount() > 0)
		{
			AssetStreamer::Instance().Flush();
			// A Flush can finish a cell that another request was waiting
			// on; loop until nothing this streamer asked for is pending.
		}
	}

	void WorldStreamer::UnloadAll()
	{
		for (std::map<CellKey, Cell>::iterator it = cells.begin(); it != cells.end(); ++it)
		{
			if (it->second.state != Cell::Unloaded) Unload(it->first);
			if (it->second.farState != Cell::Unloaded) UnloadFar(it->first);
		}
		if (!graveyard.empty() && IsActiveRenderDeviceSet())
			GetActiveRenderDevice().WaitIdle();
		CollectGraveyard(true);
	}

	bool WorldStreamer::FindCell(const GameObject* root, int32 &x, int32 &z) const
	{
		for (std::map<CellKey, Cell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
			if (it->second.root.get() == root) { x = it->first.first; z = it->first.second; return true; }
		return false;
	}

	std::vector<std::shared_ptr<GameObject> > WorldStreamer::LoadedRoots() const
	{
		std::vector<std::shared_ptr<GameObject> > out;
		for (std::map<CellKey, Cell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
			if (it->second.root) out.push_back(it->second.root);
		return out;
	}

	uint32 WorldStreamer::LoadedCount() const
	{
		uint32 n = 0;
		for (std::map<CellKey, Cell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
			if (it->second.state == Cell::Loaded) n++;
		return n;
	}

	uint32 WorldStreamer::LoadingCount() const
	{
		uint32 n = 0;
		for (std::map<CellKey, Cell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
			if (it->second.state == Cell::Loading || it->second.farState == Cell::Loading) n++;
		return n;
	}

	bool WorldStreamer::IsLoaded(const int32 x, const int32 z) const
	{
		std::map<CellKey, Cell>::const_iterator it = cells.find(CellKey(x, z));
		return it != cells.end() && it->second.state == Cell::Loaded && it->second.root;
	}

	std::shared_ptr<GameObject> WorldStreamer::GetCellRoot(const int32 x, const int32 z) const
	{
		std::map<CellKey, Cell>::const_iterator it = cells.find(CellKey(x, z));
		return it != cells.end() ? it->second.root : std::shared_ptr<GameObject>();
	}

}
