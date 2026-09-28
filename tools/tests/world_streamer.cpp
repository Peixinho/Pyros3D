// WorldStreamer: cells come in within loadRadius, go out past unloadRadius
// and not before (hysteresis), a cell left mid-load is cancelled, a missing
// cell file is reported once rather than retried every frame, and the scene
// file's "world" block round-trips. Cells hold plain GameObjects, so no
// render device is needed.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/world_streamer.cpp -o /tmp/world_streamer \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/world_streamer && PYROS_STREAM_WORKERS=0 /tmp/world_streamer
#include <cmath>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace p3d;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	fflush(stdout);
	if (!cond) failures++;
}

static void write(const fs::path &p, const std::string &text)
{
	std::ofstream out(p.string().c_str());
	out << text;
}

// Updates until nothing is loading (bounded), as a game loop would.
static void settle(WorldStreamer &w, const Vec3 &focus)
{
	for (int i = 0; i < 2000; i++)
	{
		w.Update(focus, 100.0);
		if (w.LoadingCount() == 0) { w.Update(focus, 100.0); return; }
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

static bool inScene(SceneGraph &s, const std::string &name)
{
	std::vector<std::shared_ptr<GameObject> > &all = s.GetAllGameObjectList();
	for (size_t i = 0; i < all.size(); i++)
		if (all[i]->GetName() == name) return true;
	return false;
}

int main()
{
	const fs::path root = fs::temp_directory_path() / "pyros_world_streamer_test";
	fs::remove_all(root);
	fs::create_directories(root / "scenes" / "World.cells");

	// A 5x1 strip of 100 m cells along +X: cells (0..4, 0). (5,0) is listed
	// but has no file.
	std::string cellsJson;
	for (int x = 0; x <= 5; x++)
	{
		cellsJson += (x ? "," : "") + std::string("[") + std::to_string(x) + ",0]";
		if (x == 5) continue;
		const std::string name = "Cell_" + std::to_string(x);
		write(root / "scenes" / "World.cells" / WorldStreamer::CellFileName(x, 0),
			"{\"root\":{\"name\":\"" + name + "\",\"position\":[" + std::to_string(x * 100 + 50) + ",0,50],"
			"\"children\":[{\"name\":\"" + name + "_child\"}]}}");
	}
	const std::string scenePath = (root / "scenes" / "World.json").string();
	write(scenePath, "{\"version\":1,\"roots\":[{\"name\":\"Player\"}],"
		"\"world\":{\"cellSize\":100,\"loadRadius\":60,\"unloadRadius\":120,\"cellsDir\":\"World.cells\",\"cells\":[" + cellsJson + "]}}");

	SceneGraph scene;
	SceneMeta meta;
	check(SceneSerializer::LoadScene(&scene, scenePath, NULL, NULL, NULL, &meta), "scene loads");
	check(meta.world.enabled && meta.world.cellSize == 100.f && meta.world.cells.size() == 6, "world block parsed");

	// Round trip the world block through SaveScene.
	{
		const std::string again = (root / "scenes" / "Again.json").string();
		SceneSerializer::SaveScene(&scene, again, NULL, &meta);
		SceneGraph s2;
		SceneMeta m2;
		SceneSerializer::LoadScene(&s2, again, NULL, NULL, NULL, &m2);
		check(m2.world.enabled && m2.world.cells == meta.world.cells && m2.world.cellsDir == "World.cells"
			&& m2.world.loadRadius == 60.f && m2.world.unloadRadius == 120.f, "world block round-trips");
	}

	{
		WorldStreamer w(&scene, scenePath, meta.world);

		// Focus in the middle of cell 0: it, and cell 1 (whose square
		// starts 50 m away), come in; cell 2 (150 m) does not.
		settle(w, Vec3(50, 0, 50));
		check(w.IsLoaded(0, 0) && w.IsLoaded(1, 0), "cells within loadRadius load");
		check(!w.IsLoaded(2, 0), "a cell beyond loadRadius does not");
		check(inScene(scene, "Cell_0") && inScene(scene, "Cell_1"), "cell roots are in the scene");
		std::shared_ptr<GameObject> c0 = w.GetCellRoot(0, 0);
		check(c0 && c0->GetChildren().size() == 1, "a cell's children come with it");

		// Move to x=190: cell 0 is 90 m away - past loadRadius, inside
		// unloadRadius - so it stays. Cell 2 (10 m) comes in.
		settle(w, Vec3(190, 0, 50));
		check(w.IsLoaded(0, 0), "hysteresis: a cell between the radii stays");
		check(w.IsLoaded(2, 0), "the cell ahead loads");

		// x=250: cell 0 is 150 m away - out.
		settle(w, Vec3(250, 0, 50));
		check(!w.IsLoaded(0, 0) && !inScene(scene, "Cell_0"), "past unloadRadius a cell leaves the scene");
		std::weak_ptr<GameObject> gone = c0;
		c0.reset();
		check(!gone.expired(), "...but is not destroyed while frames may be in flight");
		for (int i = 0; i < 8; i++) w.Update(Vec3(250, 0, 50), 1.0);
		check(gone.expired(), "...and is destroyed a few updates later");

		// Cell 5 is listed but has no file: reported, not retried each frame.
		settle(w, Vec3(540, 0, 50));
		check(!w.IsLoaded(5, 0) && w.LoadingCount() == 0, "a missing cell file stays unloaded without retrying");
		check(w.IsLoaded(4, 0), "its neighbour still loads");

		// Leave a cell mid-load: queue it, then jump away before pumping.
		settle(w, Vec3(-1000, 0, 50));
		check(w.LoadedCount() == 0, "everything unloads far away");
		w.Update(Vec3(50, 0, 50), 0.0);	// requests cells 0 and 1, pumps one at most
		w.Update(Vec3(-1000, 0, 50), 0.0);
		AssetStreamer::Instance().Flush();
		settle(w, Vec3(-1000, 0, 50));
		check(w.LoadedCount() == 0 && !inScene(scene, "Cell_1"), "a cell left mid-load never arrives");

		// LoadAround blocks until the ring is in.
		w.LoadAround(Vec3(350, 0, 50));
		check(w.IsLoaded(3, 0) && w.IsLoaded(2, 0) && w.IsLoaded(4, 0), "LoadAround loads the ring before returning");
	}
	check(!inScene(scene, "Cell_3") && inScene(scene, "Player"), "destroying the streamer removes its cells, not the scene's own objects");

	// ---- far versions ----
	// Every cell but 5 gets a far file; out to 400 m a cell shows it while
	// its full version is not loaded, and the swap leaves no gap.
	for (int x = 0; x <= 4; x++)
		write(root / "scenes" / "World.cells" / WorldStreamer::FarFileName(x, 0),
			"{\"root\":{\"name\":\"Far_" + std::to_string(x) + "\",\"position\":[" + std::to_string(x * 100) + ",0,0]}}");
	{
		SceneMeta::World fw = meta.world;
		fw.farRadius = 400.f;
		for (int x = 0; x <= 4; x++) fw.farCells.push_back(std::make_pair(x, 0));
		{
			// Round trip through a saved scene.
			SceneMeta m = meta;
			m.world = fw;
			const std::string again = (root / "scenes" / "Far.json").string();
			SceneSerializer::SaveScene(&scene, again, NULL, &m);
			SceneGraph s2;
			SceneMeta m2;
			SceneSerializer::LoadScene(&s2, again, NULL, NULL, NULL, &m2);
			check(m2.world.farRadius == 400.f && m2.world.farCells == fw.farCells, "far radius and far cells round-trip");
		}
		WorldStreamer w(&scene, scenePath, fw);
		// At x=50: cells 0 and 1 are in full; 2..4 (150..350 m) show far.
		settle(w, Vec3(50, 0, 50));
		check(w.IsLoaded(0, 0) && !inScene(scene, "Far_0") && !inScene(scene, "Far_1"), "a loaded cell hides its far version");
		check(inScene(scene, "Far_2") && inScene(scene, "Far_3") && inScene(scene, "Far_4"), "cells past loadRadius show far");
		check(w.FarShownCount() == 3, "and only those");
		// Walk to x=450: cells 3 and 4 come in full, 0 goes far, and at no
		// Update is a cell neither loaded nor showing its far version.
		bool gap = false;
		for (int step = 0; step <= 40; step++)
		{
			const Vec3 at(50.f + step * 10.f, 0, 50);
			w.Update(at, 100.0);
			for (int x = 0; x <= 4; x++)
			{
				const std::string full = "Cell_" + std::to_string(x), farName = "Far_" + std::to_string(x);
				// A cell within the far radius must be one or the other,
				// once it has had a chance to load.
				if (step > 2 && !inScene(scene, full) && !inScene(scene, farName) && std::fabs(at.x - (x * 100 + 50)) < 350.f) gap = true;
				if (inScene(scene, full) && inScene(scene, farName)) gap = true;	// both at once is a z-fight
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		settle(w, Vec3(450, 0, 50));
		check(!gap, "walking the strip, every cell is exactly one of full or far");
		check(w.IsLoaded(4, 0) && !inScene(scene, "Far_4") && inScene(scene, "Far_0"), "the swap follows the focus both ways");
		// Past the far radius, nothing.
		settle(w, Vec3(5000, 0, 50));
		check(w.FarShownCount() == 0 && w.LoadedCount() == 0, "beyond the far radius nothing is kept");
	}
	check(!inScene(scene, "Far_2"), "destroying the streamer removes far versions too");
	check(AssetStreamer::Instance().PendingCount() == 0, "nothing left pending");

	fs::remove_all(root);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
