//============================================================================
// Name        : TerrainTools.cpp
// Author      : Duarte Peixinho
// Description : See TerrainTools.h.
//============================================================================

#include "TerrainTools.h"
#include "MaterialCodegen.h"
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>

using namespace p3d;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
	const char* kToolNames[TerrainTools::ToolCount] = { "raise", "lower", "smooth", "flatten", "paint", "foliage" };

	// Ground height at a world position from a tile list, false off every tile.
	bool GroundAt(const std::vector<TerrainTile> &tiles, const f32 x, const f32 z, f32 &h)
	{
		for (size_t i = 0; i < tiles.size(); i++)
			if (tiles[i].Contains(x, z))
			{
				h = tiles[i].origin.y + tiles[i].Data()->HeightAt(x - tiles[i].origin.x, z - tiles[i].origin.z);
				return true;
			}
		return false;
	}

	void CollectSubtree(const GameObject* go, std::vector<const GameObject*> &out)
	{
		if (!go) return;
		out.push_back(go);
		for (size_t i = 0; i < go->GetChildren().size(); i++) CollectSubtree(go->GetChildren()[i].get(), out);
	}

	// A tileable ground texture: periodic value noise over a base colour,
	// so a fresh terrain reads as ground rather than as four flat colours.
	bool WriteGroundTexture(const std::string &path, const Vec3 &base, const f32 contrast, const uint32 seed)
	{
		const int32 n = 256;
		auto lattice = [seed](int32 x, int32 y, const int32 period) -> f32 {
			x = ((x % period) + period) % period;
			y = ((y % period) + period) % period;
			uint32 h = (uint32)x * 374761393u + (uint32)y * 668265263u + seed * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return (f32)((h ^ (h >> 16)) & 0xffff) / 65535.f;
		};
		auto noise = [&](const f32 u, const f32 v, const int32 period) -> f32 {
			const f32 x = u * period, y = v * period;
			const int32 x0 = (int32)std::floor(x), y0 = (int32)std::floor(y);
			f32 fx = x - x0, fy = y - y0;
			fx = fx * fx * (3.f - 2.f * fx);
			fy = fy * fy * (3.f - 2.f * fy);
			const f32 a = lattice(x0, y0, period), b = lattice(x0 + 1, y0, period);
			const f32 c = lattice(x0, y0 + 1, period), d = lattice(x0 + 1, y0 + 1, period);
			return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
		};
		std::vector<uchar> px((size_t)n * n * 4);
		for (int32 y = 0; y < n; y++)
			for (int32 x = 0; x < n; x++)
			{
				const f32 u = (f32)x / n, v = (f32)y / n;
				const f32 v0 = noise(u, v, 4) * 0.45f + noise(u, v, 16) * 0.35f + noise(u, v, 64) * 0.2f;
				const f32 k = 1.f + (v0 - 0.5f) * 2.f * contrast;
				uchar* p = &px[((size_t)y * n + x) * 4];
				p[0] = (uchar)std::min(255.f, std::max(0.f, base.x * k * 255.f));
				p[1] = (uchar)std::min(255.f, std::max(0.f, base.y * k * 255.f));
				p[2] = (uchar)std::min(255.f, std::max(0.f, base.z * k * 255.f));
				p[3] = 255;
			}
		return PaintableImage::WritePNG(path, n, n, 4, &px[0]);
	}

	// The splat shader's surface: four ground textures tiled in world
	// space (so they run on across tile borders), weighted by the tile's
	// splat map.
	const char* kSplatBody =
		"vec4 w = texture_2D(splatMap, vTexcoord);\n"
		"float sum = max(w.r + w.g + w.b + w.a, 0.0001);\n"
		"vec2 uv = vWorldPos.xz / 6.0;\n"
		"vec3 Albedo = (texture_2D(layer0, uv).rgb * w.r + texture_2D(layer1, uv).rgb * w.g\n"
		"    + texture_2D(layer2, uv).rgb * w.b + texture_2D(layer3, uv).rgb * w.a) / sum;\n"
		"float Metallic = 0.0;\n"
		"float Roughness = 0.9;\n";
}

const char* TerrainTools::ToolName(const Tool t)
{
	return (t >= 0 && t < ToolCount) ? kToolNames[t] : "";
}

bool TerrainTools::ToolFromName(const std::string &name, Tool &out)
{
	for (int i = 0; i < ToolCount; i++)
		if (name == kToolNames[i]) { out = (Tool)i; return true; }
	return false;
}

bool TerrainTools::Raycast(const std::vector<TerrainTile> &tiles, const Vec3 &origin, const Vec3 &direction, Vec3 &hit) const
{
	if (tiles.empty()) return false;
	// March until the ray is below the ground, then bisect. The step grows
	// with the height above the ground, so a camera far above the terrain
	// does not crawl to it - a slope under ~60 degrees cannot be skipped.
	f32 t = 0.f, prevT = 0.f;
	bool wasAbove = false;
	for (int i = 0; i < 4000 && t < 20000.f; i++)
	{
		const Vec3 p = origin + direction * t;
		f32 h;
		if (GroundAt(tiles, p.x, p.z, h))
		{
			if (p.y < h)
			{
				if (!wasAbove) return false;	// started under the ground
				f32 lo = prevT, hi = t;
				for (int k = 0; k < 16; k++)
				{
					const f32 mid = (lo + hi) * 0.5f;
					const Vec3 q = origin + direction * mid;
					f32 hq;
					if (GroundAt(tiles, q.x, q.z, hq) && q.y < hq) hi = mid; else lo = mid;
				}
				hit = origin + direction * hi;
				return true;
			}
			wasAbove = true;
			prevT = t;
			t += std::max(0.1f, (p.y - h) * 0.5f);
		}
		else
		{
			wasAbove = true;
			prevT = t;
			t += 1.f;
		}
	}
	return false;
}

void TerrainTools::Update(SceneGraph* scene, const bool rayValid, const Vec3 &origin, const Vec3 &direction, const float dt)
{
	hoverValid = false;
	if (!active || !scene || !rayValid) return;
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	hoverValid = Raycast(tiles, origin, direction, hover);
	if (stroking && hoverValid) ApplyAt(scene, hover.x, hover.z, dt);
}

bool TerrainTools::BeginStroke(SceneGraph* scene)
{
	if (!active || !hoverValid || !scene) return false;
	stroking = true;
	flattenTarget = hover.y;	// Flatten levels to where the stroke began
	editor.BeginStroke();
	return true;
}

bool TerrainTools::ApplyAt(SceneGraph* scene, const float x, const float z, const float dt)
{
	if (!scene || radius <= 0.f) return false;
	const float s = std::min(std::max(strength, 0.f), 1.f);
	// Per-second rates at strength 1: 20 m/s of raise, and most of the way
	// to smooth / flat / painted in about a fifth of a second.
	const float blend = std::min(1.f, s * 6.f * dt);
	switch (tool)
	{
	case Raise: return editor.Sculpt(scene, x, z, radius, s * 20.f * dt, hardness, TerrainEditor::Raise) > 0;
	case Lower: return editor.Sculpt(scene, x, z, radius, s * 20.f * dt, hardness, TerrainEditor::Lower) > 0;
	case Smooth: return editor.Sculpt(scene, x, z, radius, blend, hardness, TerrainEditor::Smooth) > 0;
	case Flatten: return editor.Sculpt(scene, x, z, radius, blend, hardness, TerrainEditor::Flatten, flattenTarget) > 0;
	case PaintTexture: return editor.PaintSplat(scene, x, z, radius, (uint32)std::max(0, std::min(3, layer)), blend, hardness) > 0;
	case PaintFoliage: return editor.PaintFoliage(scene, x, z, radius, (uint32)std::max(0, layer), std::min(std::max(density, 0.f), 1.f), blend, hardness) > 0;
	default: return false;
	}
}

bool TerrainTools::EndStroke(std::vector<TerrainEditor::TileSnapshot> &before, std::vector<TerrainEditor::TileSnapshot> &after)
{
	if (!stroking) return false;
	stroking = false;
	editor.FinishStroke();
	return editor.TakeStrokeUndo(before, after);
}

bool TerrainTools::HasUnsaved(const GameObject* root) const
{
	std::vector<const GameObject*> all;
	CollectSubtree(root, all);
	for (size_t i = 0; i < all.size(); i++) if (editor.HasUnsaved(all[i])) return true;
	return false;
}

uint32_t TerrainTools::UnsavedCount(SceneGraph* scene) const
{
	uint32_t n = 0;
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	for (size_t i = 0; i < tiles.size(); i++) if (editor.HasUnsaved(tiles[i].owner)) n++;
	return n;
}

void TerrainTools::Forget(GameObject* root)
{
	std::vector<const GameObject*> all;
	CollectSubtree(root, all);
	for (size_t i = 0; i < all.size(); i++) editor.Forget(all[i]);
}

void TerrainTools::DrawOverlay(DebugRenderer* debug, SceneGraph* scene) const
{
	if (!active || !hoverValid || !debug || !scene) return;
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	const Vec4 colours[ToolCount] = {
		Vec4(0.3f, 1.f, 0.4f, 1.f), Vec4(1.f, 0.45f, 0.3f, 1.f), Vec4(0.4f, 0.8f, 1.f, 1.f),
		Vec4(1.f, 0.9f, 0.3f, 1.f), Vec4(1.f, 0.4f, 1.f, 1.f), Vec4(0.6f, 1.f, 0.2f, 1.f) };
	const Vec4 outer = colours[tool];
	const Vec4 inner(outer.x, outer.y, outer.z, 0.5f);
	// Draped: each segment end sits on the ground, a little above it.
	auto ring = [&](const float r, const Vec4 &col) {
		const int n = 64;
		Vec3 prev;
		for (int i = 0; i <= n; i++)
		{
			const float a = (float)i / n * 6.2831853f;
			Vec3 p(hover.x + std::cos(a) * r, hover.y, hover.z + std::sin(a) * r);
			f32 h;
			if (GroundAt(tiles, p.x, p.z, h)) p.y = h;
			p.y += 0.15f;
			if (i > 0) debug->drawLine(prev, p, col);
			prev = p;
		}
	};
	ring(radius, outer);
	if (hardness > 0.02f && hardness < 0.98f) ring(radius * hardness, inner);
	debug->drawLine(hover, hover + Vec3(0.f, std::max(1.f, radius * 0.25f), 0.f), outer);
}

bool TerrainTools::CreateTerrain(const CreateParams &params, const std::string &projectRoot, std::string &subtreeJson, std::string &error)
{
	if (projectRoot.empty()) { error = "no project open"; return false; }
	if (params.tilesX < 1 || params.tilesZ < 1 || params.tilesX * params.tilesZ > 1024) { error = "tiles must be 1..1024 in all"; return false; }
	if (params.tileSize < 1.f) { error = "tile size must be at least 1 m"; return false; }
	if (params.samples < 3 || params.samples > 4097) { error = "samples must be 3..4097"; return false; }
	if (params.heightRange <= 0.f) { error = "height range must be positive"; return false; }
	std::string safe;
	for (size_t i = 0; i < params.name.size(); i++)
	{
		const char c = params.name[i];
		safe += (std::isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
	}
	if (safe.empty()) safe = "Terrain";

	const fs::path root(projectRoot);
	const std::string relDir = "assets/terrain/" + safe;
	std::error_code ec;
	fs::create_directories(root / relDir, ec);
	fs::create_directories(root / "assets/terrain/layers", ec);
	if (ec) { error = "could not create " + (root / relDir).string(); return false; }

	// Ground textures and the splat shader, shared by every terrain in the
	// project - written once, then the user's to change.
	const char* layerFiles[4] = { "grass.png", "dirt.png", "rock.png", "sand.png" };
	const Vec3 layerColours[4] = { Vec3(0.30f, 0.44f, 0.17f), Vec3(0.42f, 0.32f, 0.21f), Vec3(0.47f, 0.46f, 0.44f), Vec3(0.78f, 0.71f, 0.52f) };
	for (int i = 0; i < 4; i++)
	{
		const fs::path p = root / "assets/terrain/layers" / layerFiles[i];
		if (!fs::exists(p, ec) && !WriteGroundTexture(p.string(), layerColours[i], i == 2 ? 0.5f : 0.3f, (uint32)(i + 1)))
		{ error = "could not write " + p.string(); return false; }
	}
	const std::string shaderRel = "assets/terrain/splat_terrain.glsl";
	if (!fs::exists(root / shaderRel, ec))
	{
		const MaterialCodegenResult gen = GenerateGLSLFromSimpleText(kSplatBody, { "splatMap", "layer0", "layer1", "layer2", "layer3" });
		if (!gen.error.empty()) { error = "splat shader: " + gen.error; return false; }
		std::ofstream out((root / shaderRel).string().c_str(), std::ios::binary | std::ios::trunc);
		out << gen.glsl;
		if (!out) { error = "could not write " + (root / shaderRel).string(); return false; }
	}

	// Heights: 0 m is a quarter of the way up the range, so there is room
	// to dig as well as to raise.
	const float offset = -params.heightRange * 0.25f;
	const int32 n = params.samples;
	const std::vector<uint16> flat((size_t)n * n, (uint16)std::lround(0.25 * 65535.0));
	const int32 splatPx = std::min(1024, std::max(16, (int32)params.tileSize + 1));
	std::vector<uchar> splat((size_t)splatPx * splatPx * 4, 0);
	for (size_t i = 0; i < splat.size(); i += 4) splat[i] = 255;

	json tree;
	json rootObj;
	rootObj["name"] = params.name.empty() ? std::string("Terrain") : params.name;
	rootObj["position"] = { params.origin.x, params.origin.y, params.origin.z };
	rootObj["rotation"] = { 0, 0, 0 };
	rootObj["scale"] = { 1, 1, 1 };
	rootObj["components"] = json::array();
	json children = json::array();
	json materials = json::array();
	for (int32 tz = 0; tz < params.tilesZ; tz++)
		for (int32 tx = 0; tx < params.tilesX; tx++)
		{
			const std::string stem = std::to_string(tx) + "_" + std::to_string(tz);
			const std::string heightRel = relDir + "/" + stem + ".png";
			const std::string splatRel = relDir + "/" + stem + "_splat.png";
			if (fs::exists(root / heightRel, ec)) { error = heightRel + " already exists - pick another name"; return false; }
			if (!PaintableImage::WritePNG16((root / heightRel).string(), n, n, &flat[0])
				|| !PaintableImage::WritePNG((root / splatRel).string(), splatPx, splatPx, 4, &splat[0]))
			{ error = "could not write the maps under " + relDir; return false; }

			const uint32 matId = (uint32)materials.size();
			json m;
			m["id"] = matId;
			m["kind"] = "custom";
			m["shaderFile"] = shaderRel;
			json samplers = json::array();
			samplers.push_back({ { "name", "splatMap" }, { "texture", splatRel }, { "clampMaps", true } });
			for (int i = 0; i < 4; i++)
				samplers.push_back({ { "name", "layer" + std::to_string(i) }, { "texture", std::string("assets/terrain/layers/") + layerFiles[i] } });
			m["samplers"] = samplers;
			m["castingShadows"] = true;
			materials.push_back(m);

			json lods = json::array();
			const float d = params.tileSize * 1.2f;
			lods.push_back({ { "step", 1 }, { "distance", d } });
			lods.push_back({ { "step", 2 }, { "distance", d * 2.f } });
			lods.push_back({ { "step", 4 }, { "distance", d * 4.f } });
			lods.push_back({ { "step", 8 }, { "distance", 0 } });

			json tile;
			tile["name"] = "Tile_" + stem;
			tile["position"] = { tx * params.tileSize, 0, tz * params.tileSize };
			tile["rotation"] = { 0, 0, 0 };
			tile["scale"] = { 1, 1, 1 };
			tile["static"] = true;
			tile["children"] = json::array();
			json rc;
			rc["type"] = "RenderingComponent";
			rc["material"] = matId;
			rc["castingShadows"] = true;
			rc["cullTest"] = true;
			rc["renderable"] = { { "kind", "heightfield" }, { "heightmap", heightRel }, { "size", params.tileSize },
				{ "heightScale", params.heightRange }, { "heightOffset", offset }, { "skirt", 2.0 }, { "lods", lods } };
			json phys;
			phys["type"] = "Physics";
			phys["shape"] = "HeightField";
			phys["mass"] = 0.0;
			phys["ghost"] = false;
			tile["components"] = json::array({ rc, phys });
			children.push_back(tile);
		}
	rootObj["children"] = children;
	tree["root"] = rootObj;
	tree["materials"] = materials;
	subtreeJson = tree.dump();
	return true;
}

namespace {
	// Blades rising from the bottom edge, on transparent, for alpha-clipped
	// crossed cards.
	bool WriteGrassBlades(const std::string &path)
	{
		const int32 n = 128;
		std::vector<uchar> px((size_t)n * n * 4, 0);
		uint32 rng = 12345u;
		auto rnd = [&rng]() { rng = rng * 1664525u + 1013904223u; return (f32)(rng >> 8) / 16777216.f; };
		for (int b = 0; b < 22; b++)
		{
			const f32 baseX = 6.f + rnd() * (n - 12.f);
			const f32 height = n * (0.45f + rnd() * 0.53f);
			const f32 width = 3.f + rnd() * 4.f;
			const f32 lean = (rnd() - 0.5f) * 36.f;
			const f32 shade = 0.8f + rnd() * 0.35f;
			for (int32 row = 0; row < (int32)height; row++)
			{
				const f32 t = (f32)row / height;				// 0 at the root, 1 at the tip
				const f32 cx = baseX + lean * t * t;
				const f32 half = width * 0.5f * (1.f - t);
				const int32 y = n - 1 - row;
				for (int32 x = (int32)std::floor(cx - half); x <= (int32)std::ceil(cx + half); x++)
				{
					if (x < 0 || x >= n || std::fabs(x - cx) > half + 0.5f) continue;
					uchar* p = &px[((size_t)y * n + x) * 4];
					p[0] = (uchar)std::min(255.f, (0.20f + 0.32f * t) * shade * 255.f);
					p[1] = (uchar)std::min(255.f, (0.34f + 0.40f * t) * shade * 255.f);
					p[2] = (uchar)std::min(255.f, (0.09f + 0.14f * t) * shade * 255.f);
					p[3] = 255;
				}
			}
		}
		return PaintableImage::WritePNG(path, n, n, 4, &px[0]);
	}
}

int TerrainTools::AddGrassLayer(json &subtree, const std::string &projectRoot, std::string &error)
{
	if (!subtree.is_object() || !subtree.contains("root")) { error = "not a subtree"; return 0; }
	const std::string texRel = "assets/terrain/layers/grass_blades.png";
	std::error_code ec;
	const fs::path texAbs = fs::path(projectRoot) / texRel;
	fs::create_directories(texAbs.parent_path(), ec);
	if (!fs::exists(texAbs, ec) && !WriteGrassBlades(texAbs.string())) { error = "could not write " + texAbs.string(); return 0; }

	if (!subtree.contains("materials") || !subtree["materials"].is_array()) subtree["materials"] = json::array();
	json &materials = subtree["materials"];
	uint32 matId = 0;
	for (size_t i = 0; i < materials.size(); i++) matId = std::max(matId, materials[i].value("id", 0u) + 1);
	json m;
	m["id"] = matId;
	m["kind"] = "generic";
	// Diffuse | texture | alpha clip | wind - the options the scene's own
	// grass uses (GenericShaderMaterial's ShaderUsage bits).
	m["options"] = 122699778;
	m["color"] = { 1, 1, 1, 1 };
	m["colorMap"] = texRel;
	m["alphaCutoff"] = 0.5;
	m["cullFace"] = 2;
	m["roughness"] = 0.85;
	m["wind"] = { 0.16, 1.6, 0.14 };
	m["castingShadows"] = false;

	json layer;
	layer["name"] = "grass";
	layer["mesh"] = { { "kind", "primitive" }, { "shape", "Plane" }, { "width", 0.9 }, { "height", 0.7 }, { "smooth", false }, { "flip", false } };
	layer["material"] = matId;
	layer["density"] = 1.0;
	layer["blockSize"] = 32;
	layer["minScale"] = 0.7;
	layer["maxScale"] = 1.4;
	layer["tintLow"] = { 0.6, 0.72, 0.42, 1 };
	layer["tintHigh"] = { 1.05, 1.14, 0.72, 1 };
	layer["maxSlope"] = 32;
	layer["sink"] = -0.6;
	layer["fullDistance"] = 45;
	layer["fadeDistance"] = 110;
	layer["shadowDistance"] = 0;
	layer["castShadows"] = false;

	int tiles = 0;
	std::function<void(json &)> visit = [&](json &node) {
		if (!node.is_object()) return;
		if (node.contains("components") && node["components"].is_array())
		{
			json &comps = node["components"];
			bool isTile = false;
			json* foliage = NULL;
			for (size_t c = 0; c < comps.size(); c++)
			{
				const json &cj = comps[c];
				if (cj.value("type", std::string()) == "RenderingComponent" && cj.contains("renderable")
					&& cj["renderable"].is_object() && cj["renderable"].value("kind", std::string()) == "heightfield") isTile = true;
				if (cj.value("type", std::string()) == "Foliage") foliage = &comps[c];
			}
			if (isTile)
			{
				json l = layer;
				l["seed"] = 1 + tiles * 7919;
				if (foliage)
				{
					if (!(*foliage).contains("layers") || !(*foliage)["layers"].is_array()) (*foliage)["layers"] = json::array();
					(*foliage)["layers"].push_back(l);
				}
				else comps.push_back({ { "type", "Foliage" }, { "layers", json::array({ l }) } });
				tiles++;
			}
		}
		if (node.contains("children") && node["children"].is_array())
			for (size_t i = 0; i < node["children"].size(); i++) visit(node["children"][i]);
	};
	visit(subtree["root"]);
	if (tiles > 0) materials.push_back(m);
	else error = "no terrain tiles in it";
	return tiles;
}

json TerrainTools::State(SceneGraph* scene) const
{
	json r;
	r["active"] = active;
	r["tool"] = ToolName(tool);
	r["radius"] = radius;
	r["strength"] = strength;
	r["hardness"] = hardness;
	r["layer"] = layer;
	r["density"] = density;
	r["stroking"] = stroking;
	r["hoverValid"] = hoverValid;
	if (hoverValid) r["hover"] = { hover.x, hover.y, hover.z };
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	r["tiles"] = (uint32_t)tiles.size();
	uint32_t unsaved = 0;
	for (size_t i = 0; i < tiles.size(); i++) if (editor.HasUnsaved(tiles[i].owner)) unsaved++;
	r["unsaved"] = unsaved;
	return r;
}
