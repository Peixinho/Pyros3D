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
//               Main thread only.
//============================================================================

#ifndef WORLDSTREAMER_H
#define WORLDSTREAMER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
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
		};

		// Distance on XZ from p to the cell's square, 0 inside it.
		f32 DistanceToCell(const Vec3 &p, const CellKey &key) const;
		void RequestLoad(const CellKey &key, const f32 distance);
		void Finish(const CellKey &key, const std::shared_ptr<SceneSerializer::PreparedSubtree> &prepared);
		void Unload(const CellKey &key);
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
	};

}

#endif /* WORLDSTREAMER_H */
