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
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>

using namespace p3d;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
	const char* kToolNames[TerrainTools::ToolCount] = { "raise", "lower", "smooth", "flatten", "paint", "foliage", "place", "hole", "fill", "dig", "pack", "cavesmooth", "cavelevel" };

	// Ground height at a world position from a tile list, false off every tile.
	// `hint` is the tile the last query landed on: a ray march or a brush
	// ring asks about neighbouring points hundreds of times, and a streamed
	// world has a thousand tiles to look through for each.
	bool GroundAt(const std::vector<TerrainTile> &tiles, const f32 x, const f32 z, f32 &h, size_t* hint = NULL)
	{
		if (hint && *hint < tiles.size() && tiles[*hint].Contains(x, z))
		{
			const TerrainTile &t = tiles[*hint];
			h = t.origin.y + t.Data()->HeightAt(x - t.origin.x, z - t.origin.z);
			return true;
		}
		for (size_t i = 0; i < tiles.size(); i++)
			if (tiles[i].Contains(x, z))
			{
				h = tiles[i].origin.y + tiles[i].Data()->HeightAt(x - tiles[i].origin.x, z - tiles[i].origin.z);
				if (hint) *hint = i;
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

namespace {
	// Whether a point under the ground's surface is in a dug cave.
	// `open` is how much air counts as a cave: one half is where the wall
	// is drawn.
	bool InCaveAir(const Vec3 &p, const f32 open)
	{
		const std::vector<TerrainComponent*> &terrains = TerrainComponent::Instances();
		for (size_t i = 0; i < terrains.size(); i++)
			if (terrains[i]->AirAt(p) > open) return true;
		return false;
	}
}

namespace {
	// The side of a cave voxel, from whichever terrain there is.
	f32 CaveVoxelSize()
	{
		const std::vector<TerrainComponent*> &terrains = TerrainComponent::Instances();
		f32 v = 0.f;
		for (size_t i = 0; i < terrains.size(); i++) v = std::max(v, terrains[i]->CaveVoxel());
		return v > 0.f ? v : 1.f;
	}
}

float TerrainTools::CaveRadius() const
{
	return std::max(radius, 1.5f * CaveVoxelSize());
}

bool TerrainTools::Raycast(const std::vector<TerrainTile> &tiles, const Vec3 &origin, const Vec3 &direction, Vec3 &hit) const
{
	if (tiles.empty()) return false;
	// While the dig brush is held, the point it works at stays put until
	// the rock there is all but gone - not merely past the half-way mark
	// where the wall is drawn. The brush is strongest at its centre: left
	// to follow the half-way mark it ran ahead down a thread of air no
	// wider than a voxel, and never opened anything.
	// Filling is the same the other way round: the point stays until
	// the rock there is all but whole.
	//
	// How gone is "all but gone" depends on the brush: a dab can only take a
	// point as far as the brush's own soft sphere reaches there, and under
	// the cursor - between lattice points - a small brush never gets to
	// all air. Asking for more than it can give stalled the dig at a dimple.
	const f32 voxel = CaveVoxelSize();
	const f32 reachable = std::min(std::max(0.5f + (CaveRadius() - 0.6f * voxel) / (2.f * voxel), 0.5f), 1.f);
	const f32 cleared = 0.5f + 0.6f * (reachable - 0.5f);
	const f32 open = !stroking ? 0.5f : (tool == Dig ? cleared : (tool == Pack ? 1.f - cleared : 0.5f));
	// March until the ray is in rock, then bisect. Rock is under the
	// surface and not dug out: a ray that goes down a cave mouth keeps
	// going until it meets a wall. Above the ground the step grows with
	// the height, so a camera far above the terrain does not crawl to it -
	// a slope under ~60 degrees cannot be skipped; inside a cave it is a
	// fraction of a metre.
	f32 t = 0.f, prevT = 0.f;
	bool wasFree = false;
	size_t hint = 0;
	auto rock = [&](const Vec3 &p, f32 &h, bool &overTerrain) {
		overTerrain = GroundAt(tiles, p.x, p.z, h, &hint);
		return overTerrain && p.y < h && !InCaveAir(p, open);
	};
	// Off every tile the march has no ground to pace itself by; it steps by
	// a fraction of how far it has come, so a ray from kilometres out still
	// reaches the terrain's edge within the iteration budget.
	for (int i = 0; i < 6000 && t < 200000.f; i++)
	{
		const Vec3 p = origin + direction * t;
		f32 h = 0.f;
		bool over = false;
		if (rock(p, h, over))
		{
			if (!wasFree) return false;	// started inside the rock
			f32 lo = prevT, hi = t;
			for (int k = 0; k < 16; k++)
			{
				const f32 mid = (lo + hi) * 0.5f;
				f32 hq;
				bool oq;
				if (rock(origin + direction * mid, hq, oq)) hi = mid; else lo = mid;
			}
			hit = origin + direction * hi;
			return true;
		}
		wasFree = true;
		prevT = t;
		if (!over) t += std::max(1.f, t * 0.01f);
		else if (p.y >= h) t += std::max(0.1f, (p.y - h) * 0.5f);
		else t += 0.4f;	// under the surface, in a cave
	}
	return false;
}

void TerrainTools::Update(SceneGraph* scene, const bool rayValid, const Vec3 &origin, const Vec3 &direction, const float dt)
{
	hoverValid = false;
	hoverEditable = false;
	hoverOnCave = false;
	if (!active || !scene || !rayValid) return;
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	hoverValid = Raycast(tiles, origin, direction, hover);
	if (!hoverValid) return;
	for (size_t i = 0; i < tiles.size() && !hoverEditable; i++)
		if (tiles[i].Contains(hover.x, hover.z)) hoverEditable = !tiles[i].distant && editor.IsEditable(tiles[i].owner);
	{
		// Under the ground by more than a step: a cave's wall or floor.
		size_t hint = 0;
		f32 ground;
		hoverOnCave = GroundAt(tiles, hover.x, hover.z, ground, &hint) && hover.y < ground - 0.75f;
	}
	if (!stroking) return;
	// A cave tool works where the cursor points in depth - on the ground,
	// or on a cave's wall - and a little way in, so that holding it bores
	// a tunnel along the view rather than a dimple.
	if (IsHoleTool() && hoverOnCave) { ApplyAt3D(scene, hover, dt); return; }
	if (IsCaveTool())
	{
		caveDt += dt;
		if (caveDt < 1.f / 30.f) return;
		// At the cursor's point: as the wall gives way the point follows
		// it in, at the rate the strength sets. The floor tool stays at
		// the height its stroke began at.
		ApplyAt3D(scene, tool == CaveLevel ? Vec3(hover.x, flattenTarget, hover.z) : hover, std::min(caveDt, 0.1f));
		caveDt = 0.f;
	}
	else ApplyAt(scene, hover.x, hover.z, dt);
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
	case Place: return false;	// SceneEditor places: it owns the objects
	case Hole: return editor.CutHoles(scene, x, z, radius, true) > 0;
	case Fill: return editor.CutHoles(scene, x, z, radius, false) > 0;
	case Dig:
	case Pack:
	case CaveSmooth:
	case CaveLevel:
	{
		// With no depth given: at the surface, half sunk - an entrance.
		float h;
		Vec3 n;
		if (!GroundPoint(scene, x, z, h, n)) return false;
		return ApplyAt3D(scene, Vec3(x, h - radius * 0.5f, z), dt);
	}
	case PaintFoliage: return editor.PaintFoliage(scene, x, z, radius, (uint32)std::max(0, layer), std::min(std::max(density, 0.f), 1.f), blend, hardness) > 0;
	default: return false;
	}
}

bool TerrainTools::ApplyAt3D(SceneGraph* scene, const Vec3 &centre, const float dt)
{
	if (!scene || radius <= 0.f) return false;
	const float radius = CaveRadius();	// no smaller than the voxels can hold
	// The hole tools, pointed at a cave: an opening in its wall.
	if (IsHoleTool()) return editor.CutCaveHoles(scene, centre, radius, tool == Hole) > 0;
	if (!IsCaveTool()) return false;
	// At full strength a second of brush is the whole of its effect; the
	// wall under the cursor then gives way at about the radius a second
	// and a half. Smoothing and levelling are gentler things and go faster.
	const float s = std::min(std::max(strength, 0.f), 1.f);
	const int mode = tool == Dig ? 0 : (tool == Pack ? 1 : (tool == CaveSmooth ? 2 : 3));
	const float rate = (tool == CaveSmooth || tool == CaveLevel) ? 3.f : 1.f;
	return editor.CaveBrush(scene, mode, centre, radius, s * rate * dt, hardness, flattenTarget) > 0;
}

bool TerrainTools::EndStroke(std::vector<TerrainEditor::TileSnapshot> &before, std::vector<TerrainEditor::TileSnapshot> &after)
{
	if (!stroking) return false;
	stroking = false;
	editor.FinishStroke();
	return editor.TakeStrokeUndo(before, after);
}

bool TerrainTools::GroundPoint(SceneGraph* scene, const float x, const float z, float &height, Vec3 &normal)
{
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	if (!::GroundAt(tiles, x, z, height)) return false;
	const float d = 0.5f;
	float hx0 = height, hx1 = height, hz0 = height, hz1 = height;
	::GroundAt(tiles, x - d, z, hx0); ::GroundAt(tiles, x + d, z, hx1);
	::GroundAt(tiles, x, z - d, hz0); ::GroundAt(tiles, x, z + d, hz1);
	normal = Vec3(-(hx1 - hx0) / (2.f * d), 1.f, -(hz1 - hz0) / (2.f * d)).normalize();
	return true;
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
		Vec4(1.f, 0.9f, 0.3f, 1.f), Vec4(1.f, 0.4f, 1.f, 1.f), Vec4(0.6f, 1.f, 0.2f, 1.f), Vec4(1.f, 1.f, 1.f, 1.f),
		Vec4(1.f, 0.2f, 0.2f, 1.f), Vec4(0.2f, 0.9f, 0.9f, 1.f), Vec4(1.f, 0.55f, 0.1f, 1.f), Vec4(0.5f, 0.7f, 1.f, 1.f),
		Vec4(0.4f, 0.8f, 1.f, 1.f), Vec4(1.f, 0.9f, 0.3f, 1.f) };
	// Grey over a far version: the cell is still streaming in.
	const Vec4 outer = hoverEditable ? colours[tool] : Vec4(0.55f, 0.55f, 0.55f, 1.f);
	const Vec4 inner(outer.x, outer.y, outer.z, 0.5f);
	size_t hint = 0;
	// Draped: each segment end sits on the ground, a little above it.
	auto ring = [&](const float r, const Vec4 &col) {
		const int n = 64;
		Vec3 prev;
		for (int i = 0; i <= n; i++)
		{
			const float a = (float)i / n * 6.2831853f;
			Vec3 p(hover.x + std::cos(a) * r, hover.y, hover.z + std::sin(a) * r);
			f32 h;
			if (GroundAt(tiles, p.x, p.z, h, &hint)) p.y = h;
			p.y += 0.15f;
			if (i > 0) debug->drawLine(prev, p, col);
			prev = p;
		}
	};
	if (IsCaveTool() || (IsHoleTool() && hoverOnCave))
	{
		// A sphere, not a disc: three circles round the cursor's point - the
		// size the tool will really work at.
		const float radius = CaveRadius();
		const int n = 48;
		for (int plane = 0; plane < 3; plane++)
		{
			Vec3 prev;
			for (int i = 0; i <= n; i++)
			{
				const float a = (float)i / n * 6.2831853f, c = std::cos(a) * radius, s = std::sin(a) * radius;
				const Vec3 at = (tool == CaveLevel && stroking) ? Vec3(hover.x, flattenTarget, hover.z) : hover;
				const Vec3 p = at + (plane == 0 ? Vec3(c, 0.f, s) : (plane == 1 ? Vec3(c, s, 0.f) : Vec3(0.f, c, s)));
				if (i > 0) debug->drawLine(prev, p, outer);
				prev = p;
			}
		}
		return;
	}
	ring(radius, outer);
	if (hardness > 0.02f && hardness < 0.98f) ring(radius * hardness, inner);
	debug->drawLine(hover, hover + Vec3(0.f, std::max(1.f, radius * 0.25f), 0.f), outer);
}

namespace {
	// Gradient (Perlin) noise over the plane, about -1..1, the lattice
	// hashed with a seed.
	float Perlin2(const float x, const float y, const uint32_t seed)
	{
		auto grad = [seed](const int32_t ix, const int32_t iy, const float dx, const float dy) {
			uint32_t h = (uint32_t)ix * 374761393u + (uint32_t)iy * 668265263u + seed * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			h ^= h >> 16;
			const float a = (float)(h & 0xffff) / 65536.f * 6.2831853f;
			return std::cos(a) * dx + std::sin(a) * dy;
		};
		const float fx = std::floor(x), fy = std::floor(y);
		const int32_t x0 = (int32_t)fx, y0 = (int32_t)fy;
		const float tx = x - fx, ty = y - fy;
		const float u = tx * tx * tx * (tx * (tx * 6.f - 15.f) + 10.f), v = ty * ty * ty * (ty * (ty * 6.f - 15.f) + 10.f);
		const float a = grad(x0, y0, tx, ty), b = grad(x0 + 1, y0, tx - 1.f, ty);
		const float c = grad(x0, y0 + 1, tx, ty - 1.f), d = grad(x0 + 1, y0 + 1, tx - 1.f, ty - 1.f);
		return ((a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v) * 1.4142135f;
	}

	// Fractal sum of it, 0..1. ridged folds each layer at zero, which
	// turns the noise's zero crossings into crests.
	float FractalNoise(const float x, const float z, const TerrainTools::CreateParams &p, const bool ridged)
	{
		float sum = 0.f, norm = 0.f, amp = 1.f;
		float freq = 1.f / std::max(1.f, p.featureSize);
		for (int o = 0; o < std::max(1, std::min(12, p.octaves)); o++)
		{
			float n = Perlin2(x * freq, z * freq, p.seed + (uint32_t)o * 101u);
			if (ridged) { n = 1.f - std::fabs(n); n = n * n * 2.f - 1.f; }
			sum += n * amp;
			norm += amp;
			amp *= std::min(std::max(p.roughness, 0.f), 1.f);
			freq *= 2.f;
		}
		// A sum of layers crowds the middle of its range; stretched so the
		// hills use the height they were given.
		// Folded noise sits high (a crest is 1, a valley rarely -1), so it
		// gets its own mapping - or every summit is clipped to a plateau.
		const float v = sum / std::max(norm, 1e-6f);
		return std::min(std::max(ridged ? v * 0.58f + 0.4f : v * 0.9f + 0.5f, 0.f), 1.f);
	}
}

bool TerrainTools::CreateTerrain(const CreateParams &params, const std::string &projectRoot, std::string &subtreeJson, std::string &error)
{
	if (projectRoot.empty()) { error = "no project open"; return false; }
	if (params.tilesX < 1 || params.tilesZ < 1 || params.tilesX * params.tilesZ > 1024) { error = "tiles must be 1..1024 in all"; return false; }
	if (params.tileSize < 1.f) { error = "tile size must be at least 1 m"; return false; }
	if (params.samples < 3 || params.samples > 4097) { error = "samples must be 3..4097"; return false; }
	if (params.heightRange <= 0.f) { error = "height range must be positive"; return false; }
	if (params.generator != "flat" && params.generator != "perlin" && params.generator != "ridged")
	{ error = "generator must be flat, perlin or ridged"; return false; }
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
	// to dig as well as to raise. An import maps its image onto the range
	// from baseHeight instead.
	// Shaped ground - an image, or noise - sits on baseHeight with the whole
	// range above it; flat ground keeps a quarter of the range below.
	const bool fromImage = !params.importPath.empty();
	const bool generated = !fromImage && params.generator != "flat";
	const bool ridged = params.generator == "ridged";
	const bool importing = fromImage || generated;
	const float offset = importing ? params.baseHeight : -params.heightRange * 0.25f;
	const int32 n = params.samples;
	const std::vector<uint16> flat((size_t)n * n, (uint16)std::lround(0.25 * 65535.0));
	const int32 splatPx = std::min(1024, std::max(16, (int32)params.tileSize + 1));
	std::vector<uchar> splat((size_t)splatPx * splatPx * 4, 0);
	for (size_t i = 0; i < splat.size(); i += 4) splat[i] = 255;

	// The source, as 0..1 over the whole terrain's extent.
	HeightfieldData source;
	const float extentX = params.tilesX * params.tileSize, extentZ = params.tilesZ * params.tileSize;
	if (fromImage)
	{
		if (params.tilesX != params.tilesZ) { error = "an imported heightmap is square: use as many tiles along X as along Z"; return false; }
		if (!HeightfieldData::LoadFile(params.importPath, extentX, 1.f, 0.f, source))
		{ error = "could not read " + params.importPath + " as a square heightmap"; return false; }
	}
	// Normalized source height at terrain-local (x, z), clamped to the edge.
	// Noise is a function of position, so two tiles sample their shared
	// edge alike; it leaves a tenth of the range under the lowest valley.
	const float amount = std::min(std::max(params.amount, 0.f), 0.9f);
	auto sourceAt = [&](const float x, const float z) {
		if (generated) return 0.1f + amount * FractalNoise(x, z, params, ridged);
		return source.HeightAt(std::min(std::max(x, 0.f), extentX), std::min(std::max(z, 0.f), extentZ));
	};
	std::vector<uint16> imported;
	std::vector<uchar> importedSplat;

	for (int32 tz = 0; tz < params.tilesZ; tz++)
		for (int32 tx = 0; tx < params.tilesX; tx++)
		{
			const std::string stem = std::to_string(tx) + "_" + std::to_string(tz);
			const std::string heightRel = relDir + "/" + stem + ".png";
			const std::string splatRel = relDir + "/" + stem + "_splat.png";
			if (fs::exists(root / heightRel, ec)) { error = heightRel + " already exists - pick another name"; return false; }
			const uint16* heightPixels = &flat[0];
			const uchar* splatPixels = &splat[0];
			if (importing)
			{
				// This tile's square of the source, edges shared with its
				// neighbours (the same positions sample the same heights).
				const float x0 = tx * params.tileSize, z0 = tz * params.tileSize;
				imported.assign((size_t)n * n, 0);
				for (int32 r = 0; r < n; r++)
					for (int32 c = 0; c < n; c++)
					{
						const float h = sourceAt(x0 + c * params.tileSize / (n - 1), z0 + r * params.tileSize / (n - 1));
						imported[(size_t)r * n + c] = (uint16)std::lround(std::min(std::max(h, 0.f), 1.f) * 65535.f);
					}
				// Rock where the ground is steeper than ~25 degrees, fully
				// by ~40: layer 0 (grass) elsewhere.
				importedSplat.assign((size_t)splatPx * splatPx * 4, 0);
				const float d = params.tileSize / (splatPx - 1);
				for (int32 r = 0; r < splatPx; r++)
					for (int32 c = 0; c < splatPx; c++)
					{
						const float x = x0 + c * d, z = z0 + r * d;
						const float dx = (sourceAt(x + d, z) - sourceAt(x - d, z)) * params.heightRange / (2.f * d);
						const float dz = (sourceAt(x, z + d) - sourceAt(x, z - d)) * params.heightRange / (2.f * d);
						const float slope = std::atan(std::sqrt(dx * dx + dz * dz)) * 57.29578f;
						const float rock = std::min(std::max((slope - 25.f) / 15.f, 0.f), 1.f);
						uchar* p = &importedSplat[((size_t)r * splatPx + c) * 4];
						p[0] = (uchar)std::lround((1.f - rock) * 255.f);
						p[2] = (uchar)std::lround(rock * 255.f);
					}
				heightPixels = &imported[0];
				splatPixels = &importedSplat[0];
			}
			if (!PaintableImage::WritePNG16((root / heightRel).string(), n, n, heightPixels)
				|| !PaintableImage::WritePNG((root / splatRel).string(), splatPx, splatPx, 4, splatPixels))
			{ error = "could not write the maps under " + relDir; return false; }

		}

	// One object: the terrain. Its tiles are made from this template,
	// "{tile}" standing for each one's "<x>_<z>".
	json m;
	m["id"] = 0;
	m["kind"] = "custom";
	m["shaderFile"] = shaderRel;
	json samplers = json::array();
	samplers.push_back({ { "name", "splatMap" }, { "texture", relDir + "/{tile}_splat.png" }, { "clampMaps", true } });
	for (int i = 0; i < 4; i++)
		samplers.push_back({ { "name", "layer" + std::to_string(i) }, { "texture", std::string("assets/terrain/layers/") + layerFiles[i] } });
	m["samplers"] = samplers;
	m["castingShadows"] = true;

	json lods = json::array();
	const float d = params.tileSize * 1.2f;
	lods.push_back({ { "step", 1 }, { "distance", d } });
	lods.push_back({ { "step", 2 }, { "distance", d * 2.f } });
	lods.push_back({ { "step", 4 }, { "distance", d * 4.f } });
	lods.push_back({ { "step", 8 }, { "distance", 0 } });

	json tile;
	tile["name"] = "Tile";
	tile["position"] = { 0, 0, 0 };
	tile["rotation"] = { 0, 0, 0 };
	tile["scale"] = { 1, 1, 1 };
	tile["static"] = true;
	tile["children"] = json::array();
	json rc;
	rc["type"] = "RenderingComponent";
	rc["material"] = 0;
	rc["castingShadows"] = true;
	rc["cullTest"] = true;
	rc["renderable"] = { { "kind", "heightfield" }, { "heightmap", relDir + "/{tile}.png" }, { "size", params.tileSize },
		{ "heightScale", params.heightRange }, { "heightOffset", offset }, { "skirt", 2.0 }, { "lods", lods } };
	json phys;
	phys["type"] = "Physics";
	phys["shape"] = "HeightField";
	phys["mass"] = 0.0;
	phys["ghost"] = false;
	tile["components"] = json::array({ rc, phys });

	json terrain;
	terrain["type"] = "Terrain";
	terrain["directory"] = relDir;
	terrain["tilesX"] = params.tilesX;
	terrain["tilesZ"] = params.tilesZ;
	terrain["tileSize"] = params.tileSize;
	terrain["loadRadius"] = std::max(512.f, params.tileSize * 2.f);
	terrain["unloadRadius"] = std::max(512.f, params.tileSize * 2.f) * 1.25f;
	terrain["viewDistance"] = 0.0;
	terrain["overviewSamples"] = std::min(33, params.samples);
	// Caves as fine as the ground they open into.
	terrain["caveVoxel"] = std::max(1.f, params.tileSize / (float)(params.samples - 1));
	terrain["tileTemplate"] = { { "materials", json::array({ m }) }, { "root", tile } };

	json rootObj;
	rootObj["name"] = params.name.empty() ? std::string("Terrain") : params.name;
	rootObj["position"] = { params.origin.x, params.origin.y, params.origin.z };
	rootObj["rotation"] = { 0, 0, 0 };
	rootObj["scale"] = { 1, 1, 1 };
	rootObj["static"] = true;
	rootObj["tags"] = json::array();
	rootObj["components"] = json::array({ terrain });
	rootObj["children"] = json::array();
	json tree;
	tree["root"] = rootObj;
	tree["materials"] = json::array();
	subtreeJson = tree.dump();
	return true;
}

namespace {
	// ---- ConvertSceneTerrains ------------------------------------------

	bool IsTileNode(const json &node, const json** renderable = NULL)
	{
		if (!node.is_object() || !node.contains("components") || !node["components"].is_array()) return false;
		for (const auto &c : node["components"])
			if (c.is_object() && c.value("type", std::string()) == "RenderingComponent" && c.contains("renderable")
				&& c["renderable"].is_object() && c["renderable"].value("kind", std::string()) == "heightfield")
			{
				if (renderable) *renderable = &c["renderable"];
				return true;
			}
		return false;
	}

	// "assets/terrain/Name/3_7.png" -> ("assets/terrain/Name", 3, 7).
	bool ParseTilePath(const std::string &heightmap, std::string &dir, int &x, int &z)
	{
		const size_t slash = heightmap.find_last_of('/');
		const size_t dot = heightmap.find_last_of('.');
		if (slash == std::string::npos || dot == std::string::npos || dot < slash) return false;
		const std::string stem = heightmap.substr(slash + 1, dot - slash - 1);
		const size_t us = stem.find('_');
		if (us == std::string::npos || us == 0 || us + 1 >= stem.size()) return false;
		for (size_t i = 0; i < stem.size(); i++)
			if (i != us && !std::isdigit((unsigned char)stem[i])) return false;
		dir = heightmap.substr(0, slash);
		x = std::atoi(stem.substr(0, us).c_str());
		z = std::atoi(stem.substr(us + 1).c_str());
		return true;
	}

	Vec3 NodePosition(const json &node)
	{
		if (node.contains("position") && node["position"].is_array() && node["position"].size() >= 3)
			return Vec3(node["position"][0].get<float>(), node["position"][1].get<float>(), node["position"][2].get<float>());
		return Vec3();
	}

	struct FoundTile
	{
		json node;				// the tile, as written
		json materials;			// the pool its material ids index
		Vec3 world;
		std::string dir;
		int x = 0, z = 0;
		float size = 0.f;
	};

	// Takes every tile out of `node`'s descendants, into `found`.
	void ExtractTiles(json &node, const Vec3 &parentWorld, const json &pool, std::vector<FoundTile> &found)
	{
		if (!node.is_object() || !node.contains("children") || !node["children"].is_array()) return;
		const Vec3 world = parentWorld + NodePosition(node);
		json &kids = node["children"];
		for (size_t i = 0; i < kids.size();)
		{
			const json* renderable = NULL;
			FoundTile t;
			if (IsTileNode(kids[i], &renderable)
				&& ParseTilePath(renderable->value("heightmap", std::string()), t.dir, t.x, t.z))
			{
				t.node = kids[i];
				t.materials = pool;
				t.world = world + NodePosition(kids[i]);
				t.size = renderable->value("size", 256.f);
				found.push_back(t);
				kids.erase(kids.begin() + i);
				continue;
			}
			ExtractTiles(kids[i], world, pool, found);
			i++;
		}
	}

	// The tile's node and the materials it uses as a template: ids
	// renumbered from 0 (an id is an index into the pool), paths naming
	// the tile turned into "{tile}", per-tile things dropped.
	json MakeTemplate(const FoundTile &t)
	{
		json node = t.node;
		json pool = json::array();
		std::map<uint32_t, uint32_t> remap;
		std::function<void(json &)> walk = [&](json &j) {
			if (j.is_object())
				for (json::iterator it = j.begin(); it != j.end(); ++it)
				{
					if ((it.key() == "material" || it.key() == "lodMaterial") && it.value().is_number_unsigned())
					{
						const uint32_t id = it.value().get<uint32_t>();
						if (!remap.count(id) && t.materials.is_array() && id < t.materials.size())
						{
							json m = t.materials[id];
							m["id"] = (uint32_t)pool.size();
							// Unset memory in files saved before these were
							// initialised - not a difference between tiles.
							if (!m.value("depthBias", false)) { m["depthBiasFactor"] = 0.0; m["depthBiasUnits"] = 0.0; }
							remap[id] = (uint32_t)pool.size();
							pool.push_back(m);
						}
						if (remap.count(id)) it.value() = remap[id];
					}
					else walk(it.value());
				}
			else if (j.is_array())
				for (size_t i = 0; i < j.size(); i++) walk(j[i]);
		};
		walk(node);
		node["name"] = "Tile";
		node["position"] = { 0, 0, 0 };
		node["children"] = json::array();
		if (node.contains("components") && node["components"].is_array())
			for (auto &c : node["components"])
				if (c.is_object() && c.value("type", std::string()) == "Foliage" && c.contains("layers") && c["layers"].is_array())
					for (auto &l : c["layers"]) { l.erase("densityMap"); l["seed"] = 1; }
		json tmpl = { { "materials", pool }, { "root", node } };
		// "<dir>/<x>_<z>" wherever it names one of the tile's files.
		std::string text = tmpl.dump();
		const std::string from = t.dir + "/" + std::to_string(t.x) + "_" + std::to_string(t.z);
		const std::string to = t.dir + "/{tile}";
		size_t at = 0;
		while ((at = text.find(from, at)) != std::string::npos)
		{
			const char next = at + from.size() < text.size() ? text[at + from.size()] : '\0';
			if (next == '.' || next == '_') { text.replace(at, from.size(), to); at += to.size(); }
			else at += from.size();
		}
		return json::parse(text);
	}

	bool ReadJson(const std::string &path, json &out)
	{
		std::ifstream in(path.c_str(), std::ios::binary);
		if (!in) return false;
		try { in >> out; }
		catch (const std::exception &) { return false; }
		return true;
	}

	bool WriteJson(const std::string &path, const json &j)
	{
		std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
		out << j.dump(4);
		return (bool)out;
	}
}

bool TerrainTools::ConvertSceneTerrains(const std::string &scenePath, const std::string &projectRoot, const bool force,
	json &report, std::string &error)
{
	(void)projectRoot;
	json sceneJson;
	if (!ReadJson(scenePath, sceneJson) || !sceneJson.is_object()) { error = "could not read " + scenePath; return false; }
	const json scenePool = sceneJson.value("materials", json::array());
	std::vector<FoundTile> found;

	// Tiles under the scene's own objects (a "Terrain" root with a tile per
	// child, as terrain_create used to make outside a streamed world).
	if (sceneJson.contains("roots") && sceneJson["roots"].is_array())
		for (auto &rootNode : sceneJson["roots"]) ExtractTiles(rootNode, Vec3(), scenePool, found);

	// Tiles in a streamed world's cells.
	const bool world = sceneJson.contains("world") && sceneJson["world"].is_object();
	const fs::path cellsDir = world ? fs::path(scenePath).parent_path() / sceneJson["world"].value("cellsDir", std::string()) : fs::path();
	std::map<std::string, json> cellFiles;			// path -> rewritten content
	std::vector<std::pair<int, int> > emptiedCells;
	if (world && sceneJson["world"].contains("cells") && sceneJson["world"]["cells"].is_array())
		for (const auto &cell : sceneJson["world"]["cells"])
		{
			if (!cell.is_array() || cell.size() < 2) continue;
			const int cx = cell[0].get<int>(), cz = cell[1].get<int>();
			const std::string path = (cellsDir / (std::to_string(cx) + "_" + std::to_string(cz) + ".json")).string();
			json cellJson;
			if (!ReadJson(path, cellJson) || !cellJson.contains("root")) continue;
			const size_t before = found.size();
			ExtractTiles(cellJson["root"], Vec3(), cellJson.value("materials", json::array()), found);
			if (found.size() == before) continue;
			cellFiles[path] = cellJson;
			const json &r = cellJson["root"];
			if ((!r.contains("children") || r["children"].empty()) && (!r.contains("components") || r["components"].empty()))
				emptiedCells.push_back(std::make_pair(cx, cz));
		}
	if (found.empty()) { error = "this scene has no terrain tiles to convert"; return false; }

	// One terrain per maps directory.
	std::map<std::string, std::vector<size_t> > byDir;
	for (size_t i = 0; i < found.size(); i++) byDir[found[i].dir].push_back(i);
	json terrains = json::array();
	json newRoots = json::array();
	for (std::map<std::string, std::vector<size_t> >::const_iterator g = byDir.begin(); g != byDir.end(); ++g)
	{
		const std::vector<size_t> &ids = g->second;
		int maxX = 0, maxZ = 0;
		const FoundTile* first = &found[ids[0]];
		for (size_t i = 0; i < ids.size(); i++)
		{
			const FoundTile &t = found[ids[i]];
			maxX = std::max(maxX, t.x);
			maxZ = std::max(maxZ, t.z);
			if (t.z < first->z || (t.z == first->z && t.x < first->x)) first = &t;
		}
		const int tilesX = maxX + 1, tilesZ = maxZ + 1;
		const float size = first->size;
		const Vec3 origin = first->world - Vec3(first->x * size, 0.f, first->z * size);
		std::vector<char> have((size_t)tilesX * tilesZ, 0);
		const json tmpl = MakeTemplate(*first);
		int differ = 0;
		for (size_t i = 0; i < ids.size(); i++)
		{
			const FoundTile &t = found[ids[i]];
			have[(size_t)t.z * tilesX + t.x] = 1;
			const Vec3 expect = origin + Vec3(t.x * size, 0.f, t.z * size);
			if (std::fabs(t.size - size) > 0.01f || (t.world - expect).magnitude() > 0.05f)
			{
				error = g->first + ": tile " + std::to_string(t.x) + "_" + std::to_string(t.z) + " is not where a regular grid of "
					+ std::to_string((int)size) + " m tiles would put it";
				return false;
			}
			if (MakeTemplate(t) != tmpl) differ++;
		}
		for (size_t i = 0; i < have.size(); i++)
			if (!have[i])
			{
				error = g->first + ": tile " + std::to_string(i % tilesX) + "_" + std::to_string(i / tilesX)
					+ " is missing - a terrain is a full grid";
				return false;
			}
		if (differ > 0 && !force)
		{
			error = g->first + ": " + std::to_string(differ) + " tile(s) have a different material or foliage than tile "
				+ std::to_string(first->x) + "_" + std::to_string(first->z) + "; pass force to give every tile that one's";
			return false;
		}

		const float load = world ? sceneJson["world"].value("loadRadius", 512.f) : std::max(512.f, size * 2.f);
		const float unload = world ? sceneJson["world"].value("unloadRadius", load * 1.25f) : load * 1.25f;
		json terrain;
		terrain["type"] = "Terrain";
		terrain["directory"] = g->first;
		terrain["tilesX"] = tilesX;
		terrain["tilesZ"] = tilesZ;
		terrain["tileSize"] = size;
		terrain["loadRadius"] = load;
		terrain["unloadRadius"] = std::max(load, unload);
		terrain["viewDistance"] = 0.0;
		terrain["overviewSamples"] = 33;
		terrain["tileTemplate"] = tmpl;
		json rootObj;
		rootObj["name"] = fs::path(g->first).filename().string();
		// The object sits at the terrain's centre; the tiles stay where
		// they were.
		rootObj["position"] = { origin.x + 0.5f * tilesX * size, origin.y, origin.z + 0.5f * tilesZ * size };
		rootObj["rotation"] = { 0, 0, 0 };
		rootObj["scale"] = { 1, 1, 1 };
		rootObj["static"] = true;
		rootObj["tags"] = json::array();
		rootObj["components"] = json::array({ terrain });
		rootObj["children"] = json::array();
		newRoots.push_back(rootObj);
		terrains.push_back({ { "name", rootObj["name"] }, { "directory", g->first }, { "tilesX", tilesX }, { "tilesZ", tilesZ },
			{ "tileSize", size }, { "tiles", (uint32_t)ids.size() }, { "differing", differ } });
	}

	// Nothing is written until everything checked out. Cells first.
	for (std::map<std::string, json>::const_iterator c = cellFiles.begin(); c != cellFiles.end(); ++c)
		if (!WriteJson(c->first, c->second)) { error = "could not write " + c->first; return false; }

	// A root left with nothing (the old "Terrain" parent of the tiles) goes.
	if (sceneJson.contains("roots") && sceneJson["roots"].is_array())
	{
		json &roots = sceneJson["roots"];
		for (size_t i = 0; i < roots.size();)
		{
			const json &r = roots[i];
			bool emptied = false;
			for (std::map<std::string, std::vector<size_t> >::const_iterator g = byDir.begin(); g != byDir.end() && !emptied; ++g)
				emptied = r.value("name", std::string()) == fs::path(g->first).filename().string();
			if (emptied && (!r.contains("children") || r["children"].empty()) && (!r.contains("components") || r["components"].empty()))
				roots.erase(roots.begin() + i);
			else i++;
		}
	}
	else sceneJson["roots"] = json::array();
	for (size_t i = 0; i < newRoots.size(); i++) sceneJson["roots"].push_back(newRoots[i]);

	// Cells that held nothing but their tile are gone, and every cell's far
	// version loses its terrain - the Terrain draws its own distance.
	uint32_t cellsRemoved = 0, farRemoved = 0;
	if (world)
	{
		json &w = sceneJson["world"];
		std::error_code ec;
		for (size_t i = 0; i < emptiedCells.size(); i++)
		{
			const std::string stem = std::to_string(emptiedCells[i].first) + "_" + std::to_string(emptiedCells[i].second);
			fs::remove(cellsDir / (stem + ".json"), ec);
			if (w.contains("cells") && w["cells"].is_array())
				for (size_t k = 0; k < w["cells"].size(); k++)
					if (w["cells"][k].is_array() && w["cells"][k].size() >= 2 && w["cells"][k][0] == emptiedCells[i].first
						&& w["cells"][k][1] == emptiedCells[i].second) { w["cells"].erase(w["cells"].begin() + k); break; }
			cellsRemoved++;
		}
		if (w.contains("farCells") && w["farCells"].is_array())
		{
			json &far = w["farCells"];
			for (size_t k = 0; k < far.size();)
			{
				if (!far[k].is_array() || far[k].size() < 2) { k++; continue; }
				const std::string stem = std::to_string(far[k][0].get<int>()) + "_" + std::to_string(far[k][1].get<int>());
				const fs::path farPath = cellsDir / (stem + ".far.json");
				json farJson;
				bool drop = false;
				if (ReadJson(farPath.string(), farJson) && farJson.contains("root") && farJson["root"].contains("children")
					&& farJson["root"]["children"].is_array())
				{
					json &kids = farJson["root"]["children"];
					bool changed = false;
					for (size_t c = 0; c < kids.size();)
						if (IsTileNode(kids[c])) { kids.erase(kids.begin() + c); changed = true; }
						else c++;
					if (changed)
					{
						// Its maps: "<x>_<z>_t<n>_far.png" and "..._farcolor.png".
						for (int t = 0; t < 64; t++)
						{
							const fs::path a = cellsDir / (stem + "_t" + std::to_string(t) + "_far.png");
							if (!fs::exists(a, ec)) break;
							fs::remove(a, ec);
							fs::remove(cellsDir / (stem + "_t" + std::to_string(t) + "_farcolor.png"), ec);
						}
						if (kids.empty()) drop = true;
						else
						{
							// The tiles' materials stay in the pool - an id is an
							// index, so nothing may shift - but must not go on
							// naming the colour maps just deleted.
							std::set<uint32_t> used;
							std::function<void(const json &)> walk = [&](const json &j) {
								if (j.is_object())
									for (json::const_iterator it = j.begin(); it != j.end(); ++it)
									{
										if ((it.key() == "material" || it.key() == "lodMaterial") && it.value().is_number_unsigned())
											used.insert(it.value().get<uint32_t>());
										else walk(it.value());
									}
								else if (j.is_array())
									for (size_t i = 0; i < j.size(); i++) walk(j[i]);
							};
							walk(farJson["root"]);
							if (farJson.contains("materials") && farJson["materials"].is_array())
								for (size_t m = 0; m < farJson["materials"].size(); m++)
									if (!used.count((uint32_t)m))
										farJson["materials"][m] = { { "id", (uint32_t)m }, { "kind", "generic" }, { "options", 16384 }, { "color", { 1, 1, 1, 1 } } };
							WriteJson(farPath.string(), farJson);
						}
					}
				}
				if (drop)
				{
					fs::remove(farPath, ec);
					far.erase(far.begin() + k);
					farRemoved++;
				}
				else k++;
			}
		}
	}
	if (!WriteJson(scenePath, sceneJson)) { error = "could not write " + scenePath; return false; }
	report["terrains"] = terrains;
	report["cellsRemoved"] = cellsRemoved;
	report["farVersionsRemoved"] = farRemoved;
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
	m["wind"] = { 0.16, 1.6, 0.14, 0.85 };       // (sways, and is lit as the ground under it is)
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
	// (far off, a third of the tufts and each one bigger: see FoliageLayerSpec::thinDensity)
	layer["thinFrom"] = 12;
	layer["thinTo"] = 60;
	layer["thinDensity"] = 0.33;
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
	r["asset"] = placeAsset;
	r["spacing"] = placeSpacing;
	r["scaleMin"] = placeScaleMin;
	r["scaleMax"] = placeScaleMax;
	r["randomYaw"] = placeRandomYaw;
	r["alignToSlope"] = placeAlign;
	r["stroking"] = stroking;
	r["hoverValid"] = hoverValid;
	r["hoverEditable"] = HoverEditable();
	r["hoverOnCave"] = HoverOnCave();
	if (hoverValid) r["hover"] = { hover.x, hover.y, hover.z };
	const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
	r["tiles"] = (uint32_t)tiles.size();
	uint32_t unsaved = 0;
	for (size_t i = 0; i < tiles.size(); i++) if (editor.HasUnsaved(tiles[i].owner)) unsaved++;
	r["unsaved"] = unsaved;
	return r;
}
