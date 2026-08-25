// Round-trip check for prefab references. Standalone on purpose - the repo
// has no test framework, and wiring one up for a single file would be a
// bigger change than the thing being tested. Build and run it against an
// existing engine build:
//
//   c++ -std=c++17 -I include -I shared -I /opt/homebrew/include/freetype2 \
//       tools/tests/prefab_roundtrip.cpp -o /tmp/prefab_roundtrip \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   mkdir -p /tmp/p/assets/prefabs /tmp/p/scenes && cd /tmp/p && /tmp/prefab_roundtrip .
//
// Covers the resolver (shared/PrefabResolver.h) and its two ends: a scene
// whose roots are references expands into something the engine loads, and a
// scene the engine wrote collapses back into references. The engine itself
// knows nothing about prefabs, which is precisely what makes this testable
// without a window or a render device.
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include "PrefabResolver.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace p3d;
using json = nlohmann::json;

static int failures = 0;
static void check(bool cond, const std::string& what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}

static std::string slurp(const std::string& p)
{
	std::ifstream in(p.c_str());
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static void spit(const std::string& p, const json& j)
{
	std::ofstream out(p.c_str());
	out << j.dump(4);
}

int main(int argc, char** argv)
{
	const std::string root = argc > 1 ? argv[1] : ".";
	const std::string prefabAbs = root + "/assets/prefabs/Turret.prefab";
	const std::string prefabRel = "assets/prefabs/Turret.prefab";
	const std::string sceneAbs = root + "/scenes/Test.json";

	prefab::LoadFn load = [&](const std::string& rel) {
		return prefab::ReadPrefabFile(root + "/" + rel);
	};

	// -------- author a prefab from a live subtree --------
	{
		std::shared_ptr<GameObject> src = std::make_shared<GameObject>();
		src->SetName("Turret");
		src->SetPosition(Vec3(0, 1, 0));
		src->AddTag("turret");
		std::shared_ptr<GameObject> barrel = std::make_shared<GameObject>();
		barrel->SetName("Barrel");
		src->Add(barrel);

		const std::string subtree = SceneSerializer::SerializeSubtree(src.get(), sceneAbs);
		check(!subtree.empty(), "SerializeSubtree captures the object");
		check(prefab::WritePrefabFile(json::parse(subtree), prefabAbs), "WritePrefabFile writes it");
		check(json::parse(slurp(prefabAbs)).value("prefabVersion", 0) == 1, "the file is marked as a prefab");
	}

	// -------- a scene of two references --------
	{
		json scene;
		scene["version"] = 1;
		scene["materials"] = json::array();
		json roots = json::array();
		json a;
		a["prefab"] = prefabRel;
		a["name"] = "TurretLeft";
		a["position"] = { -5.0, 0.0, 0.0 };
		roots.push_back(a);
		json b;
		b["prefab"] = prefabRel;
		b["name"] = "TurretRight";
		b["position"] = { 5.0, 0.0, 0.0 };
		b["scale"] = { 2.0, 2.0, 2.0 };
		b["tags"] = json::array({ "boss" });
		roots.push_back(b);
		scene["roots"] = roots;
		spit(sceneAbs, scene);
	}

	// -------- expand, and load the result with the engine --------
	{
		json scene = json::parse(slurp(sceneAbs));
		std::vector<prefab::Link> links;
		std::vector<std::string> errors;
		prefab::ExpandScene(scene, load, links, errors);
		check(errors.empty() && links.size() == 2, "ExpandScene resolved both references");

		SceneGraph graph;
		check(SceneSerializer::LoadSceneFromText(&graph, scene.dump(), sceneAbs),
			"the engine loads the expanded scene");

		std::vector<std::shared_ptr<GameObject>>& all = graph.GetAllGameObjectList();
		check(all.size() == 2, "two instances came back");
		GameObject *left = NULL, *right = NULL;
		for (size_t i = 0; i < all.size(); i++)
		{
			if (all[i]->GetName() == "TurretLeft") left = all[i].get();
			if (all[i]->GetName() == "TurretRight") right = all[i].get();
		}
		check(left && right, "both instances kept their overridden names");
		if (left && right)
		{
			check(left->GetPosition().x == -5.f && right->GetPosition().x == 5.f, "positions survived");
			check(right->GetScale().x == 2.f, "scale override survived");
			check(right->HaveTag("boss") && right->HaveTag("turret"), "instance tag added, prefab tag kept");
			check(left->GetChildren().size() == 1, "the prefab's child was rebuilt");
			check(left->GetChildren()[0]->GetName() == "Barrel", "and it is the right one");
		}
	}

	// -------- editing the prefab reaches every instance --------
	{
		json p = json::parse(slurp(prefabAbs));
		p["root"]["children"][0]["name"] = "BarrelMk2";
		spit(prefabAbs, p);

		json scene = json::parse(slurp(sceneAbs));
		std::vector<prefab::Link> links;
		std::vector<std::string> errors;
		prefab::ExpandScene(scene, load, links, errors);

		SceneGraph graph;
		SceneSerializer::LoadSceneFromText(&graph, scene.dump(), sceneAbs);
		std::vector<std::shared_ptr<GameObject>>& all = graph.GetAllGameObjectList();
		bool allUpdated = all.size() == 2;
		for (size_t i = 0; i < all.size(); i++)
			if (all[i]->GetChildren().empty() || all[i]->GetChildren()[0]->GetName() != "BarrelMk2")
				allUpdated = false;
		check(allUpdated, "editing the prefab updated BOTH instances");
	}

	// -------- an engine-written scene collapses back to references --------
	{
		// What the engine writes: every root in full, no idea any of them
		// came from anywhere.
		json scene = json::parse(slurp(sceneAbs));
		std::vector<prefab::Link> links;
		std::vector<std::string> errors;
		prefab::ExpandScene(scene, load, links, errors);
		scene["roots"][0].erase("prefab");
		scene["roots"][1].erase("prefab");

		std::vector<std::string> paths = { prefabRel, prefabRel };
		std::vector<std::string> modified, missing;
		prefab::CollapseScene(scene, paths, load, modified, missing);

		check(modified.empty() && missing.empty(), "both instances still matched their source");
		check(scene["roots"][0].contains("prefab") && !scene["roots"][0].contains("children"),
			"the scene stores a reference, not a copy");
		check(scene["roots"][0].value("name", std::string()) == "TurretLeft"
			&& scene["roots"][0]["position"][0] == -5.0, "the overrides were kept");
		check(scene.dump().find("BarrelMk2") == std::string::npos,
			"the prefab's contents are NOT duplicated into the scene");
		spit(sceneAbs, scene);
	}

	// -------- a modified instance is written out in full, not discarded --------
	{
		json scene = json::parse(slurp(sceneAbs));
		std::vector<prefab::Link> links;
		std::vector<std::string> errors;
		prefab::ExpandScene(scene, load, links, errors);

		json extra;
		extra["name"] = "BoltedOnPart";
		extra["components"] = json::array();
		extra["children"] = json::array();
		scene["roots"][0]["children"].push_back(extra);
		scene["roots"][0].erase("prefab");
		scene["roots"][1].erase("prefab");

		std::vector<std::string> paths = { prefabRel, prefabRel };
		std::vector<std::string> modified, missing;
		prefab::CollapseScene(scene, paths, load, modified, missing);

		check(modified.size() == 1, "the edited instance was detected as modified");
		check(scene.dump().find("BoltedOnPart") != std::string::npos,
			"the edit was written out in full, not silently discarded");
		check(scene["roots"][0].value("prefab", std::string()) == prefabRel,
			"and it still knows which prefab it came from");
		check(!scene["roots"][1].contains("children"),
			"the untouched instance is still stored as a reference");
	}

	// -------- a missing prefab costs its contents, not the object --------
	{
		json scene;
		scene["version"] = 1;
		scene["materials"] = json::array();
		json ghost;
		ghost["prefab"] = "assets/prefabs/Gone.prefab";
		ghost["name"] = "Ghost";
		ghost["position"] = { 1.0, 2.0, 3.0 };
		scene["roots"] = json::array({ ghost });

		std::vector<prefab::Link> links;
		std::vector<std::string> errors;
		prefab::ExpandScene(scene, load, links, errors);
		check(errors.size() == 1, "a missing prefab is reported");

		SceneGraph graph;
		SceneSerializer::LoadSceneFromText(&graph, scene.dump(), sceneAbs);
		std::vector<std::shared_ptr<GameObject>>& all = graph.GetAllGameObjectList();
		check(all.size() == 1 && all[0]->GetName() == "Ghost" && all[0]->GetPosition().y == 2.f,
			"the instance still loads with its name and transform");
	}

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
