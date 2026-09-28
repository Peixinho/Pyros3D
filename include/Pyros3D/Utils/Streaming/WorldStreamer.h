//============================================================================
// Name        : WorldStreamer.h
// Author      : Duarte Peixinho
// Description : Keeps the cells of a streamed world (SceneMeta::World)
//               loaded around a focus point - normally the camera.
//
//               Each Update() decides, from the focus, which cells should
//               be in: a cell starts loading once the focus is within
//               loadRadius of its square, and is dropped once the focus is
//               beyond unloadRadius. Loading goes through AssetStreamer: a
//               loader thread reads, parses and decodes the cell
//               (SceneSerializer::PrepareSubtreeFile), then the main thread
//               instantiates it and adds its root to the scene. Nearer
//               cells go first, and a cell the focus leaves before it
//               finished is cancelled.
//
//               A dropped cell leaves the scene at once but its objects are
//               destroyed a few Updates later, once no frame in flight can
//               still be drawing them - the alternative, UnloadScene's
//               WaitIdle, would stall the GPU every time the player crossed
//               a border.
//
//               Far versions (SceneMeta::World::farRadius): a cell that
//               has one shows it from loadRadius out to farRadius - loaded
//               like a cell, at a lower priority, never with physics - and
//               hides it the moment its full version is in, so the swap
//               leaves no gap.
//
//               Main thread only.
//============================================================================

#ifndef WORLDSTREAMER_H
#define WORLDSTREAMER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace sol { class state; }

namespace p3d {

	class SceneGraph;
	class IPhysics;
	class GameObject;

	class PYROS3D_API WorldStreamer
	{
	public:
		// scenePath is the scene file the world belongs to: cellsDir is
		// relative to its directory, and asset paths resolve against it.
		WorldStreamer(SceneGraph* scene, const std::string &scenePath, const SceneMeta::World &world,
			IPhysics* physics = NULL, sol::state* lua = NULL);
		// Cancels what is loading and destroys every cell it loaded.
		~WorldStreamer();

		// Once per frame. Also pumps AssetStreamer with pumpBudgetMs, so an
		// app using a WorldStreamer must not pump it again.
		void Update(const Vec3 &focus, const f64 pumpBudgetMs = 4.0);
		// Several foci - a server keeping the world around every player: a
		// cell is wanted while any focus is within loadRadius of it, and
		// dropped only when all are past unloadRadius.
		void Update(const std::vector<Vec3> &foci, const f64 pumpBudgetMs = 4.0);

		// Blocks until every cell Update() would want at focus is in - for a
		// loading screen, or spawning the player into a world.
		void LoadAround(const Vec3 &focus);

		// Everything out, now. Stalls for the GPU, like UnloadScene.
		void UnloadAll();

		// Called with a cell's root right after it joins the scene, and
		// right before it leaves - for what a game does to objects once
		// they exist (start their sounds, emitters, autoplay clips) and
		// must stop doing before they go.
		typedef std::function<void(const std::shared_ptr<GameObject> &root)> CellCallback;
		void SetOnCellLoaded(const CellCallback &cb) { onLoaded = cb; }
		void SetOnCellUnloading(const CellCallback &cb) { onUnloading = cb; }

		// Asked before a cell leaves: true keeps it loaded. The editor keeps
		// a cell with unsaved edits in memory until it is saved.
		typedef std::function<bool(const std::shared_ptr<GameObject> &root)> UnloadVeto;
		void SetUnloadVeto(const UnloadVeto &veto) { unloadVeto = veto; }

		// New load/unload radii, from the next Update() on.
		void SetRadii(const f32 load, const f32 unload) { world.loadRadius = load; world.unloadRadius = std::max(load, unload); }

		// Unloads one cell now, veto or not (the editor ending play mode).
		void UnloadCell(const int32 x, const int32 z) { Unload(CellKey(x, z)); }

		// A cell made after the world was opened (the editor placing an
		// object where no cell was): it loads like the rest from the next
		// Update(). The caller writes its file first.
		void AddCell(const int32 x, const int32 z)
		{
			const CellKey key(x, z);
			if (cells.find(key) != cells.end()) return;
			cells[key] = Cell();
			world.cells.push_back(key);
		}
		bool HasCell(const int32 x, const int32 z) const { return cells.find(CellKey(x, z)) != cells.end(); }
		// Read from disk right now: its file must not change under it.
		bool IsLoading(const int32 x, const int32 z) const
		{
			std::map<CellKey, Cell>::const_iterator it = cells.find(CellKey(x, z));
			return it != cells.end() && it->second.state == Cell::Loading;
		}
		f32 CellSize() const { return world.cellSize; }
		const SceneMeta::World &GetWorld() const { return world; }
		// Which cell a loaded root belongs to.
		bool FindCell(const GameObject* root, int32 &x, int32 &z) const;
		// Every loaded cell's root.
		std::vector<std::shared_ptr<GameObject> > LoadedRoots() const;
		// The file a cell lives in.
		std::string CellPath(const int32 x, const int32 z) const { return cellsDir + CellFileName(x, z); }
		// Its far version's file.
		std::string FarPath(const int32 x, const int32 z) const { return cellsDir + FarFileName(x, z); }
		static std::string FarFileName(const int32 x, const int32 z);
		// A cell whose far version was (re)written: loaded, or reloaded if
		// it was showing, from the next Update().
		void AddFarCell(const int32 x, const int32 z);
		void SetFarRadius(const f32 r) { world.farRadius = std::max(0.f, r); }
		// Far versions in the scene right now.
		uint32 FarShownCount() const;

		uint32 LoadedCount() const;
		uint32 LoadingCount() const;
		bool IsLoaded(const int32 x, const int32 z) const;
		// The cell's root, NULL unless loaded.
		std::shared_ptr<GameObject> GetCellRoot(const int32 x, const int32 z) const;

		// "<x>_<z>.json" - what the editor writes and this reads.
		static std::string CellFileName(const int32 x, const int32 z);
		// The cell whose square holds p.
		static void CellOf(const Vec3 &p, const f32 cellSize, int32 &x, int32 &z);

	private:
		typedef std::pair<int32, int32> CellKey;

		struct Cell
		{
			// Loaded with no root is a cell whose file failed: it is not
			// retried until the focus leaves and comes back.
			enum State { Unloaded, Loading, Loaded };
			State state = Unloaded;
			AssetStreamer::Ticket ticket = 0;
			std::shared_ptr<GameObject> root;
			std::shared_ptr<LoadedSceneAssets> assets;
			// The far version, loaded on its own and shown only while the
			// full cell is not.
			bool hasFar = false;
			State farState = Unloaded;
			AssetStreamer::Ticket farTicket = 0;
			std::shared_ptr<GameObject> farRoot;
			std::shared_ptr<LoadedSceneAssets> farAssets;
			bool farShown = false;
		};

		// Distance on XZ from p to the cell's square, 0 inside it.
		f32 DistanceToCell(const Vec3 &p, const CellKey &key) const;
		void RequestLoad(const CellKey &key, const f32 distance, const bool far = false);
		void Finish(const CellKey &key, const std::shared_ptr<SceneSerializer::PreparedSubtree> &prepared, const bool far = false);
		void Unload(const CellKey &key);
		void UnloadFar(const CellKey &key);
		void Bury(const std::shared_ptr<LoadedSceneAssets> &assets);
		// Far in the scene exactly when it is loaded and the cell is not.
		void SyncFarVisibility(Cell &c);
		void CollectGraveyard(const bool all);

		SceneGraph* scene;
		IPhysics* physics;
		sol::state* lua;
		std::string scenePath;
		std::string cellsDir;
		SceneMeta::World world;
		std::map<CellKey, Cell> cells;

		// Dropped cells waiting out the frames in flight.
		struct Grave
		{
			std::shared_ptr<LoadedSceneAssets> assets;
			uint32 updatesLeft;
		};
		std::deque<Grave> graveyard;

		CellCallback onLoaded, onUnloading;
		UnloadVeto unloadVeto;
	};

}

#endif /* WORLDSTREAMER_H */
