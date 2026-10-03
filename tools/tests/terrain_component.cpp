// TerrainComponent::BuildTileJson: a tile's subtree from the terrain's
// template - "{tile}" becomes "<x>_<z>", the tile is placed so the terrain
// is centred on its object, every tile's foliage gets its own seed, and a
// painted layer's density map is picked up only when its file exists.
// Pure JSON and files: no render device.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/terrain_component.cpp -o /tmp/terrain_component \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/terrain_component
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Utils/Json/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace p3d;
using json = nlohmann::json;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	if (!cond) { failures++; printf("FAIL: %s\n", what.c_str()); }
}

int main()
{
	const fs::path root = fs::temp_directory_path() / "pyros_terrain_component_test";
	fs::remove_all(root);
	fs::create_directories(root / "assets/terrain/T");
	// Tile 2_1 has a painted "grass" layer; no other tile does.
	{ std::ofstream f((root / "assets/terrain/T/2_1_grass_density.png").string()); f << "x"; }

	const json tmpl = {
		{ "materials", json::array({ { { "id", 0 }, { "kind", "custom" },
			{ "samplers", json::array({ { { "name", "splatMap" }, { "texture", "assets/terrain/T/{tile}_splat.png" } } }) } } }) },
		{ "root", { { "name", "Tile" }, { "position", { 0, 0, 0 } }, { "children", json::array() },
			{ "components", json::array({
				{ { "type", "RenderingComponent" }, { "material", 0 },
					{ "renderable", { { "kind", "heightfield" }, { "heightmap", "assets/terrain/T/{tile}.png" }, { "size", 64.0 } } } },
				{ { "type", "Foliage" }, { "layers", json::array({ { { "name", "grass" }, { "seed", 5 } } }) } } }) } } } };

	TerrainComponent::Settings s;
	s.directory = "assets/terrain/T";
	s.tilesX = 4;
	s.tilesZ = 2;
	s.tileSize = 64.f;

	const json a = json::parse(TerrainComponent::BuildTileJson(s, tmpl.dump(), root.string(), 2, 1));
	check(a["root"]["name"] == "Tile_2_1", "the tile is named after its place");
	check(a["root"]["components"][0]["renderable"]["heightmap"] == "assets/terrain/T/2_1.png", "{tile} in the heightmap path");
	check(a["materials"][0]["samplers"][0]["texture"] == "assets/terrain/T/2_1_splat.png", "{tile} in the material's splat map");
	// 4 x 2 tiles of 64 m: the object is at the centre, so tile (2, 1)'s
	// corner is at (0, 0, 0) and tile (0, 0)'s at (-128, 0, -64).
	check(a["root"]["position"][0].get<float>() == 0.f && a["root"]["position"][2].get<float>() == 0.f, "tile (2,1) starts at the centre");
	const json b = json::parse(TerrainComponent::BuildTileJson(s, tmpl.dump(), root.string(), 0, 0));
	check(b["root"]["position"][0].get<float>() == -128.f && b["root"]["position"][2].get<float>() == -64.f, "tile (0,0) is half the extent back");
	const json &la = a["root"]["components"][1]["layers"][0], &lb = b["root"]["components"][1]["layers"][0];
	check(la["seed"] != lb["seed"], "each tile's foliage has its own seed");
	check(lb["seed"] == 5, "tile (0,0) keeps the template's seed");
	check(la.contains("densityMap") && la["densityMap"] == "assets/terrain/T/2_1_grass_density.png", "a painted layer finds its density map");
	check(!lb.contains("densityMap"), "an unpainted layer names none");
	check(TerrainComponent::BuildTileJson(s, "not json", root.string(), 0, 0).empty(), "a broken template builds nothing");

	// The component itself, off any scene: settings are sanitised and the
	// template's heightfield tells it the height mapping.
	json withHeights = tmpl;
	withHeights["root"]["components"][0]["renderable"]["heightScale"] = 400.0;
	withHeights["root"]["components"][0]["renderable"]["heightOffset"] = -50.0;
	s.tileTemplate = withHeights.dump();
	s.overviewSamples = 40;
	{
		TerrainComponent tc(s, root.string());
		check(tc.HeightScale() == 400.f && tc.HeightOffset() == -50.f, "height mapping read from the template");
		check(tc.GetSettings().overviewSamples == 33, "overview samples round down to a power of two plus one");
		check(tc.Corner().x == -128.f && tc.Corner().z == -64.f, "the corner is half the extent from the object");
		check(tc.HeightmapPath(3, 1) == "assets/terrain/T/3_1.png", "heightmap path");
		check(TerrainComponent::Instances().size() == 1, "a live terrain is listed");
	}
	check(TerrainComponent::Instances().empty(), "and unlisted once gone");

	fs::remove_all(root);
	printf(failures ? "%d FAILED\n" : "ALL PASS\n", failures);
	return failures ? 1 : 0;
}
