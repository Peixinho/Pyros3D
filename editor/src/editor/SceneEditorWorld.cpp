// A streamed world's cells in the editor: the grid drawn over the ground,
// moving objects between cells (on save, and on request), and placing
// objects into cells that are not loaded - or do not exist yet.

#include "SceneEditor.h"
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include "EditorWorld.h"
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <algorithm>
#include <map>

#include <cmath>
#include <set>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
	// Object `go` may change cell only as a direct child of a cell root or
	// of the scene - anything deeper moves with whatever it hangs from.
	bool IsCellMovable(EditorWorld* world, GameObject* go)
	{
		if (!go || world->IsCellRoot(go)) return false;
		return !go->HaveParent() || world->IsCellRoot(go->GetParent());
	}
}

bool SceneEditor::MoveObjectToCell(uint32 id, const int32 x, const int32 z, std::string& errOut)
{
	if (!editorWorld) { errOut = "the scene is not a streamed world"; return false; }
	if (playMode) { errOut = "stop play mode first"; return false; }
	SceneObject* so = sceneObjects->GetSceneObject(id);
	if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)so->GetPTR();
	if (!IsCellMovable(editorWorld.get(), go)) { errOut = "only a cell's (or the scene's) direct children change cell"; return false; }

	const Vec3 world = go->GetWorldPosition();
	const std::shared_ptr<GameObject> root = editorWorld->CellRoot(x, z);
	if (root)
	{
		// Loaded: adopted by its root, kept where it is. One undo entry
		// holding both halves - the parent and the position relative to it.
		const uint32 rootId = sceneObjects->GetSceneObjectID(root.get());
		if (!rootId) { errOut = "the cell's root is not registered"; return false; }
		const uint32 oldParent = so->GetParentID();
		const Vec3 oldLocal = go->GetPosition();
		const Vec3 newLocal = world - root->GetWorldPosition();
		if (oldParent == rootId) return true;
		auto place = [this, id](const uint32 parent, const Vec3 &local) {
			SceneObject* o = sceneObjects->GetSceneObject(id);
			if (!o || o->GetType() != SceneObjectTypes::GAMEOBJECT) return;
			((GameObject*)o->GetPTR())->SetPosition(local);
			sceneObjects->ReparentGameObject(id, parent);
			if (o == SelectedSceneObject) SyncTransformFromGameObject(o);
			MarkSceneDirty();
		};
		place(rootId, newLocal);
		sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
			[place, oldParent, oldLocal]() { place(oldParent, oldLocal); },
			[place, rootId, newLocal]() { place(rootId, newLocal); }, "Move to Cell"));
		return true;
	}

	if (editorWorld->IsLoading(x, z)) { errOut = "that cell is loading - try again in a moment"; return false; }
	// Not loaded, or no such cell yet: into its file. Built off-scene from
	// the file and the object's own saved form, then written back, so the
	// serializer does the material bookkeeping.
	namespace fs = std::filesystem;
	const std::string path = editorWorld->Streamer().CellPath(x, z);
#ifdef LUA_BINDINGS
	sol::state* lua = sharedLua;
#else
	sol::state* lua = NULL;
#endif
	LoadedSceneAssets temp;
	std::shared_ptr<GameObject> cellRoot;
	std::error_code ec;
	if (fs::exists(path, ec))
	{
		std::ifstream in(path.c_str(), std::ios::binary);
		std::stringstream ss;
		ss << in.rdbuf();
		// (a cell's file names its prefab instances: put in, as on any load)
		std::string cellText = ss.str();
		prefab::ExpandSubtreeText(cellText, scenePath);
		cellRoot = SceneSerializer::DeserializeSubtree(cellText, scenePath, physics, lua, &temp);
		if (!cellRoot) { errOut = "could not read " + path; return false; }
	}
	else
	{
		cellRoot = std::make_shared<GameObject>();
		cellRoot->SetName("Cell_" + std::to_string(x) + "_" + std::to_string(z));
		cellRoot->SetPosition(editorWorld->CellOrigin(x, z));
	}
	std::shared_ptr<GameObject> copy = SceneSerializer::DeserializeSubtree(SnapshotSubtree(id), scenePath, physics, lua, &temp);
	if (!copy) { errOut = "could not copy the object"; return false; }
	copy->SetPosition(world - cellRoot->GetPosition());
	cellRoot->Add(copy);
	if (!EditorWorld::WriteCell(cellRoot.get(), path, scenePath, lua, errOut)) return false;
	if (!editorWorld->HasCell(x, z)) editorWorld->AddCell(x, z);
	sceneWorld.cells = editorWorld->Cells();
	echo("SUCCESS: '" + so->GetName() + "' moved into cell " + std::to_string(x) + "_" + std::to_string(z)
		+ " (not loaded - it is in that cell's file now)");
	RawDeleteSubtree(id);
	MarkSceneDirty();
	return true;
}

int SceneEditor::ReassignCellMembers(std::string& errOut)
{
	if (!editorWorld) return 0;
	struct Move { uint32 id; int32 x, z; };
	std::vector<Move> moves;
	const std::set<const GameObject*> roots = editorWorld->LoadedRoots();
	for (std::set<const GameObject*>::const_iterator r = roots.begin(); r != roots.end(); ++r)
	{
		int32 cx, cz;
		if (!editorWorld->Streamer().FindCell(*r, cx, cz)) continue;
		const std::vector<std::shared_ptr<GameObject> > &children = (*r)->GetChildren();
		for (size_t i = 0; i < children.size(); i++)
		{
			GameObject* c = children[i].get();
			if (!c || c->IsTransient()) continue;
			int32 x, z;
			editorWorld->CellOf(c->GetWorldPosition(), x, z);
			if (x == cx && z == cz) continue;
			const uint32 id = sceneObjects->GetSceneObjectID(c);
			if (id) moves.push_back({ id, x, z });
		}
	}
	int moved = 0;
	for (size_t i = 0; i < moves.size(); i++)
	{
		std::string err;
		if (MoveObjectToCell(moves[i].id, moves[i].x, moves[i].z, err)) moved++;
		else { errOut = err; return -1; }
	}
	return moved;
}

void SceneEditor::DrawCellGrid(DebugRenderer* debug, GameObject* viewCam)
{
	if (!editorWorld || !debug || !viewCam || sceneIsTwoD) return;
	const f32 size = editorWorld->CellSize();
	if (!(size > 0.f)) return;
	const SceneMeta::World &w = editorWorld->Streamer().GetWorld();
	const Vec3 cam = viewCam->GetWorldPosition();
	const f32 range = std::max(w.unloadRadius, size) * 1.5f;
	int32 x0, z0, x1, z1;
	WorldStreamer::CellOf(cam - Vec3(range, 0.f, range), size, x0, z0);
	WorldStreamer::CellOf(cam + Vec3(range, 0.f, range), size, x1, z1);
	// Very small cells seen from far away would be a wash of lines.
	if ((x1 - x0) > 96 || (z1 - z0) > 96) return;
	const f32 y = 0.05f;

	// The grid itself, faint - where cells would be.
	const Vec4 faint(1.f, 1.f, 1.f, 0.12f);
	for (int32 x = x0; x <= x1 + 1; x++)
		debug->drawLine(Vec3(x * size, y, z0 * size), Vec3(x * size, y, (z1 + 1) * size), faint);
	for (int32 z = z0; z <= z1 + 1; z++)
		debug->drawLine(Vec3(x0 * size, y, z * size), Vec3((x1 + 1) * size, y, z * size), faint);

	// Cells that exist, outlined just inside their square: green loaded,
	// orange with unsaved edits, grey on disk only.
	const std::set<const GameObject*> &dirty = editorWorld->DirtyRootsCached();
	const std::vector<std::pair<int32_t, int32_t> > &cells = editorWorld->Cells();
	const f32 inset = size * 0.02f;
	for (size_t i = 0; i < cells.size(); i++)
	{
		const int32 x = cells[i].first, z = cells[i].second;
		if (x < x0 || x > x1 || z < z0 || z > z1) continue;
		const std::shared_ptr<GameObject> root = editorWorld->CellRoot(x, z);
		const Vec4 col = !root ? Vec4(0.6f, 0.6f, 0.65f, 0.6f)
			: (dirty.count(root.get()) ? Vec4(1.f, 0.6f, 0.2f, 1.f) : Vec4(0.35f, 0.9f, 0.45f, 1.f));
		const f32 ax = x * size + inset, az = z * size + inset, bx = (x + 1) * size - inset, bz = (z + 1) * size - inset;
		debug->drawLine(Vec3(ax, y, az), Vec3(bx, y, az), col);
		debug->drawLine(Vec3(bx, y, az), Vec3(bx, y, bz), col);
		debug->drawLine(Vec3(bx, y, bz), Vec3(ax, y, bz), col);
		debug->drawLine(Vec3(ax, y, bz), Vec3(ax, y, az), col);
	}

	// What streams in around the camera.
	const Vec4 ring(0.4f, 0.7f, 1.f, 0.8f);
	const int n = 96;
	for (int i = 0; i < n; i++)
	{
		const f32 a0 = (f32)i / n * 6.2831853f, a1 = (f32)(i + 1) / n * 6.2831853f;
		debug->drawLine(Vec3(cam.x + std::cos(a0) * w.loadRadius, y, cam.z + std::sin(a0) * w.loadRadius),
			Vec3(cam.x + std::cos(a1) * w.loadRadius, y, cam.z + std::sin(a1) * w.loadRadius), ring);
	}
}

bool SceneEditor::AgentMoveToCell(const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	if (!editorWorld) { errOut = "the scene is not a streamed world"; return false; }
	SceneObject* target = NULL;
	if (args.contains("id")) target = sceneObjects->GetSceneObject(args.value("id", 0u));
	else
	{
		const std::string name = args.value("name", std::string());
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end() && !target; ++i)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->second->GetName() == name) target = i->second;
	}
	if (!target || target->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	int32 x, z;
	if (args.contains("cell") && args["cell"].is_array() && args["cell"].size() >= 2)
	{ x = args["cell"][0].get<int32>(); z = args["cell"][1].get<int32>(); }
	else editorWorld->CellOf(((GameObject*)target->GetPTR())->GetWorldPosition(), x, z);
	if (!MoveObjectToCell(target->GetID(), x, z, errOut)) return false;
	out["cell"] = { x, z };
	out["loaded"] = editorWorld->CellRoot(x, z) != nullptr;
	return true;
}

void SceneEditor::DrawWorldCellProperties(GameObject* go, uint32 goId)
{
	if (!editorWorld || !go || editorWorld->IsCellRoot(go)) return;
	int32 x, z;
	editorWorld->CellOf(go->GetWorldPosition(), x, z);
	if (go->HaveParent() && editorWorld->IsCellRoot(go->GetParent()))
	{
		int32 ox, oz;
		editorWorld->Streamer().FindCell(go->GetParent(), ox, oz);
		if (ox == x && oz == z) ImGui::TextDisabled("World cell %d_%d", (int)x, (int)z);
		else ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "In cell %d_%d, over %d_%d - moves there on save", (int)ox, (int)oz, (int)x, (int)z);
		return;
	}
	if (go->HaveParent()) return;
	// A scene root: loaded whatever the camera does.
	ImGui::TextDisabled("Always loaded (in the scene file)");
	ImGui::SameLine();
	const std::string label = "Move into Cell " + std::to_string(x) + "_" + std::to_string(z);
	if (ImGui::SmallButton(label.c_str()))
		pendingFoliageOp = { { "cmd", "move_to_cell" }, { "id", goId } };
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Streams it with the cell under it instead of keeping it\nloaded everywhere. A cell that is not loaded gets it in its\nfile, and it leaves the scene until that cell streams in.");
}

namespace {
	struct FarTile
	{
		Vec3 offset;			// from the cell root
		const Heightfield* hf = NULL;
		IMaterial* material = NULL;
	};

	// Terrain tiles under `node`, with their offset from the cell root
	// (tiles are never rotated or scaled, so positions just add up).
	void CollectFarTiles(GameObject* node, const Vec3 &offset, std::vector<FarTile> &out)
	{
		for (size_t c = 0; c < node->GetComponents().size(); c++)
			if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(node->GetComponents()[c].get()))
				if (const Heightfield* hf = dynamic_cast<const Heightfield*>(rc->GetRenderable()))
					if (hf->GetData())
					{
						FarTile t;
						t.offset = offset;
						t.hf = hf;
						if (!rc->GetMeshes().empty() && rc->GetMeshes()[0]) t.material = rc->GetMeshes()[0]->Material.get();
						out.push_back(t);
					}
		for (size_t i = 0; i < node->GetChildren().size(); i++)
		{
			GameObject* child = node->GetChildren()[i].get();
			if (child && !child->IsTransient()) CollectFarTiles(child, offset + child->GetPosition(), out);
		}
	}

	// Images a bake reads, loaded once per bake.
	struct ImageCache
	{
		std::map<std::string, std::shared_ptr<PaintableImage> > images;
		std::map<std::string, Vec3> averages;
		const PaintableImage* Get(const std::string &path)
		{
			std::map<std::string, std::shared_ptr<PaintableImage> >::iterator it = images.find(path);
			if (it != images.end()) return it->second.get();
			std::shared_ptr<PaintableImage> img = std::make_shared<PaintableImage>();
			if (path.empty() || !img->Load(path, 4)) img.reset();
			images[path] = img;
			return img.get();
		}
		// A ground texture's average colour: all a far tile can show of it.
		Vec3 Average(const std::string &path)
		{
			std::map<std::string, Vec3>::iterator it = averages.find(path);
			if (it != averages.end()) return it->second;
			Vec3 sum(0.5f, 0.5f, 0.5f);
			if (const PaintableImage* img = Get(path))
			{
				double r = 0, g = 0, b = 0;
				const size_t n = (size_t)img->width * img->height;
				for (size_t i = 0; i < n; i++) { r += img->pixels[i * 4]; g += img->pixels[i * 4 + 1]; b += img->pixels[i * 4 + 2]; }
				if (n) sum = Vec3((f32)(r / n / 255.0), (f32)(g / n / 255.0), (f32)(b / n / 255.0));
			}
			averages[path] = sum;
			return sum;
		}
	};
}

bool SceneEditor::BakeFarCell(const int32 x, const int32 z, GameObject* root, const int resolution, std::string& errOut,
	const bool allowRender)
{
	if (!editorWorld || !root || !project || !project->IsOpen()) { errOut = "no streamed world or project"; return false; }
	std::vector<FarTile> tiles;
	CollectFarTiles(root, Vec3(), tiles);

	// Large objects: the cell's direct children (a building of several
	// parts is one) at least farObjectMinSize across, terrain aside.
	std::vector<GameObject*> objects;
	for (size_t i = 0; i < root->GetChildren().size(); i++)
	{
		GameObject* go = root->GetChildren()[i].get();
		if (!go || go->IsTransient()) continue;
		std::vector<FarTile> own;
		CollectFarTiles(go, Vec3(), own);
		if (!own.empty()) continue;
		f32 extent = 0.f;
		std::function<void(GameObject*)> measure = [&](GameObject* g) {
			for (size_t c = 0; c < g->GetComponents().size(); c++)
				if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(g->GetComponents()[c].get()))
					if (rc->GetRenderable())
					{
						const Vec3 sz = rc->GetRenderable()->GetBoundingMaxValue() - rc->GetRenderable()->GetBoundingMinValue();
						extent = std::max(extent, std::max(sz.x, std::max(sz.y, sz.z)));
					}
			for (size_t k = 0; k < g->GetChildren().size(); k++) if (g->GetChildren()[k] && !g->GetChildren()[k]->IsTransient()) measure(g->GetChildren()[k].get());
		};
		measure(go);
		const Vec3 sc = go->GetScale();
		if (extent * std::max(sc.x, std::max(sc.y, sc.z)) >= farObjectMinSize) objects.push_back(go);
	}
	if (tiles.empty() && objects.empty()) return false;

	namespace fs = std::filesystem;
	const fs::path projectRoot(project->GetProjectPath());
	const std::string farJson = editorWorld->Streamer().FarPath(x, z);
	const fs::path dir = fs::path(farJson).parent_path();
	auto rel = [&projectRoot](const fs::path &p) { return p.lexically_normal().lexically_relative(projectRoot.lexically_normal()).generic_string(); };
	const int32 n = std::max(3, std::min(257, resolution));
	const int32 cn = (n - 1) * 2;			// colour texels a side
	ImageCache localCache;
	ImageCache* cache = &localCache;

	json materials = json::array(), children = json::array();
	for (size_t t = 0; t < tiles.size(); t++)
	{
		const FarTile &tile = tiles[t];
		const HeightfieldData &d = *tile.hf->GetData();
		const Heightfield::Source &src = tile.hf->source;
		const f32 scale = src.heightScale != 0.f ? src.heightScale : 1.f;
		const std::string stem = std::to_string(x) + "_" + std::to_string(z) + "_t" + std::to_string(t);

		// Heights, resampled, in the tile's own 16-bit mapping.
		std::vector<uint16> px((size_t)n * n);
		for (int32 r = 0; r < n; r++)
			for (int32 c = 0; c < n; c++)
			{
				const f32 h = d.HeightAt((f32)c / (n - 1) * d.size, (f32)r / (n - 1) * d.size);
				px[(size_t)r * n + c] = (uint16)std::lround(std::min(std::max((h - src.heightOffset) / scale, 0.f), 1.f) * 65535.f);
			}
		const fs::path heightPath = dir / (stem + "_far.png");
		if (!PaintableImage::WritePNG16(heightPath.string(), n, n, &px[0])) { errOut = "could not write " + heightPath.string(); return false; }

		// Colour, from what the tile's material shows: a splat map over
		// four ground textures (each reduced to its average), or a plain
		// colour map.
		const PaintableImage* splat = NULL;
		Vec3 layerColour[4] = { Vec3(0.3f, 0.45f, 0.2f), Vec3(0.4f, 0.32f, 0.22f), Vec3(0.45f, 0.45f, 0.43f), Vec3(0.75f, 0.7f, 0.5f) };
		const PaintableImage* colourMap = NULL;
		if (CustomShaderMaterial* cm = dynamic_cast<CustomShaderMaterial*>(tile.material))
		{
			const std::vector<std::string> &names = cm->GetSamplerNames();
			for (size_t i = 0; i < names.size() && i < cm->textures.size(); i++)
			{
				if (!cm->textures[i]) continue;
				const std::string file = cm->textures[i]->GetFilename();
				if (names[i] == "splatMap") splat = cache->Get(file);
				else if (names[i].size() == 6 && names[i].compare(0, 5, "layer") == 0 && names[i][5] >= '0' && names[i][5] <= '3')
					layerColour[names[i][5] - '0'] = cache->Average(file);
			}
		}
		else if (GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(tile.material))
			if (gm->GetColorMap()) colourMap = cache->Get(gm->GetColorMap()->GetFilename());
		std::vector<uchar> col((size_t)cn * cn * 4, 255);
		for (int32 r = 0; r < cn; r++)
			for (int32 c = 0; c < cn; c++)
			{
				const f32 u = (c + 0.5f) / cn, v = (r + 0.5f) / cn;
				Vec3 rgb(0.35f, 0.45f, 0.25f);
				if (splat)
				{
					f32 w[4], sum = 0.f;
					for (int k = 0; k < 4; k++) { w[k] = splat->Sample(u, v, (uint32)k); sum += w[k]; }
					if (sum > 1e-4f)
					{
						rgb = Vec3();
						for (int k = 0; k < 4; k++) rgb = rgb + layerColour[k] * (w[k] / sum);
					}
				}
				else if (colourMap)
					rgb = Vec3(colourMap->Sample(u, v, 0), colourMap->Sample(u, v, 1), colourMap->Sample(u, v, 2));
				uchar* p = &col[((size_t)r * cn + c) * 4];
				p[0] = (uchar)std::lround(std::min(std::max(rgb.x, 0.f), 1.f) * 255.f);
				p[1] = (uchar)std::lround(std::min(std::max(rgb.y, 0.f), 1.f) * 255.f);
				p[2] = (uchar)std::lround(std::min(std::max(rgb.z, 0.f), 1.f) * 255.f);
			}
		const fs::path colourPath = dir / (stem + "_farcolor.png");
		if (!PaintableImage::WritePNG(colourPath.string(), cn, cn, 4, &col[0])) { errOut = "could not write " + colourPath.string(); return false; }
		// A far version being replaced may still hold the old picture; the
		// new one must not be handed that one back from the cache.
		Texture::ForgetShared(colourPath.string());

		json m;
		m["id"] = (uint32)t;
		m["kind"] = "generic";
		m["options"] = ShaderUsage::Texture | ShaderUsage::Diffuse;
		m["color"] = { 1, 1, 1, 1 };
		m["colorMap"] = rel(colourPath);
		m["clampMaps"] = true;
		m["roughness"] = 0.95;
		m["castingShadows"] = false;
		materials.push_back(m);

		json tileJson;
		tileJson["name"] = "FarTile_" + std::to_string(t);
		tileJson["position"] = { tile.offset.x, tile.offset.y, tile.offset.z };
		tileJson["rotation"] = { 0, 0, 0 };
		tileJson["scale"] = { 1, 1, 1 };
		tileJson["children"] = json::array();
		json rc;
		rc["type"] = "RenderingComponent";
		rc["material"] = (uint32)t;
		rc["castingShadows"] = false;
		rc["cullTest"] = true;
		// A deeper skirt than the tile's: the far tile meets full tiles
		// sampled 4-8x finer, and the skirt is what hides the step.
		rc["renderable"] = { { "kind", "heightfield" }, { "heightmap", rel(heightPath) }, { "size", d.size },
			{ "heightScale", src.heightScale }, { "heightOffset", src.heightOffset }, { "skirt", std::max(src.skirt, d.size / 16.f) },
			{ "lods", json::array({ { { "step", 1 }, { "distance", 0 } } }) } };
		tileJson["components"] = json::array({ rc });
		children.push_back(tileJson);
	}

	// The objects, as cards. Each distinct object - its saved form with its
	// own position, yaw, scale and name taken out - is baked once and kept
	// by content, so a hundred copies of a house are one picture.
#ifdef LUA_BINDINGS
	sol::state* lua = sharedLua;
#else
	sol::state* lua = NULL;
#endif
	std::map<std::string, uint32> impostorMaterial;
	const fs::path impostorDir = projectRoot / "assets" / "terrain" / "impostors";
	bool deferred = false;
	for (size_t o = 0; o < objects.size(); o++)
	{
		GameObject* go = objects[o];
		json form;
		try { form = json::parse(SceneSerializer::SerializeSubtree(go, scenePath, lua)); }
		catch (const std::exception &) { continue; }
		if (!form.contains("root")) continue;
		json &r = form["root"];
		const json rot = r.value("rotation", json::array({ 0, 0, 0 }));
		r["position"] = { 0, 0, 0 };
		r["rotation"] = { rot.size() > 0 ? rot[0] : json(0), 0, rot.size() > 2 ? rot[2] : json(0) };
		r["scale"] = { 1, 1, 1 };
		r["name"] = "";
		// A building - a model of some size, read from a file - stands in the
		// distance as itself with a share of its triangles (SimplifiedModel),
		// in its own material: a card of it, two pictures crossed, is seen for
		// what it is from across a valley. Small things stay cards.
		{
			RenderingComponent* body = NULL;
			for (size_t c = 0; c < go->GetComponents().size() && !body; c++) body = dynamic_cast<RenderingComponent*>(go->GetComponents()[c].get());
			Model* model = body ? dynamic_cast<Model*>(body->GetRenderable()) : NULL;
			uint32 tris = 0;
			if (model) for (size_t g = 0; g < model->Geometries.size(); g++) tris += (uint32)(model->Geometries[g]->GetIndexData().size() / 3);
			const json* comp = NULL;
			if (model && r.contains("components"))
				for (const json &c : r["components"])
					if (c.value("type", std::string()) == "RenderingComponent" && c.contains("renderable") && c["renderable"].value("kind", std::string()) == "model") { comp = &c; break; }
			const bool skinned = model && !model->Geometries.empty() && model->Geometries[0]->materialProperties.haveBones;
			if (model && comp && !skinned && tris >= 1500 && model->GetBoundingMinValue().distance(model->GetBoundingMaxValue()) >= 8.f
				&& comp->contains("material") && (*comp)["material"].is_number_unsigned() && form.contains("materials")
				&& (*comp)["material"].get<size_t>() < form["materials"].size())
			{
				json m = form["materials"][(*comp)["material"].get<size_t>()];
				m.erase("id");
				m["castingShadows"] = false;
				const std::string key = "simplified|" + m.dump();
				std::map<std::string, uint32>::iterator mat = impostorMaterial.find(key);
				if (mat == impostorMaterial.end())
				{
					const uint32 id = (uint32)materials.size();
					m["id"] = id;
					materials.push_back(m);
					mat = impostorMaterial.insert(std::make_pair(key, id)).first;
				}
				json far;
				far["name"] = "FarObject_" + std::to_string(o);
				far["position"] = { go->GetPosition().x, go->GetPosition().y, go->GetPosition().z };
				far["rotation"] = { go->GetRotation().x, go->GetRotation().y, go->GetRotation().z };
				far["scale"] = { go->GetScale().x, go->GetScale().y, go->GetScale().z };
				far["children"] = json::array();
				json rc;
				rc["type"] = "RenderingComponent";
				rc["material"] = mat->second;
				rc["castingShadows"] = false;
				rc["cullTest"] = true;
				rc["renderable"] = { { "kind", "simplified" }, { "path", (*comp)["renderable"].value("path", std::string()) }, { "ratio", 0.3 } };
				far["components"] = json::array({ rc });
				children.push_back(far);
				continue;
			}
		}
		const std::string text = form.dump();
		char hex[32];
		std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)std::hash<std::string>()(text));
		const fs::path texPath = impostorDir / (std::string("obj_") + hex + ".png");
		const fs::path metaPath = impostorDir / (std::string("obj_") + hex + ".json");
		std::error_code ec;
		if (!fs::exists(texPath, ec) || !fs::exists(metaPath, ec))
		{
			if (!allowRender) { deferred = true; continue; }
			LoadedSceneAssets temp;
			std::shared_ptr<GameObject> subject = SceneSerializer::DeserializeSubtree(text, scenePath, NULL, NULL, &temp);
			std::vector<unsigned char> rgba;
			uint32 w = 0, h = 0;
			f32 l, rr, b, t;
			if (!subject || !RenderImpostorRGBA8(std::string(), rgba, w, h, l, rr, b, t, subject)) continue;
			fs::create_directories(impostorDir, ec);
			if (!PaintableImage::WritePNG(texPath.string(), (int32)w, (int32)h, 4, rgba.data())) { errOut = "could not write " + texPath.string(); return false; }
			Texture::ForgetShared(texPath.string());
			std::ofstream meta(metaPath.string().c_str());
			meta << json({ { "left", l }, { "right", rr }, { "bottom", b }, { "top", t } }).dump();
		}
		json bounds;
		try { std::ifstream in(metaPath.string().c_str()); in >> bounds; }
		catch (const std::exception &) { continue; }

		std::map<std::string, uint32>::iterator mat = impostorMaterial.find(hex);
		if (mat == impostorMaterial.end())
		{
			const uint32 id = (uint32)materials.size();
			json m;
			m["id"] = id;
			m["kind"] = "generic";
			m["options"] = ShaderUsage::Texture | ShaderUsage::Diffuse | ShaderUsage::AlphaTest;
			m["color"] = { 1, 1, 1, 1 };
			m["colorMap"] = rel(texPath);
			m["clampMaps"] = true;
			m["alphaCutoff"] = 0.5;
			m["cullFace"] = 2;
			m["roughness"] = 0.9;
			m["castingShadows"] = false;
			materials.push_back(m);
			mat = impostorMaterial.insert(std::make_pair(std::string(hex), id)).first;
		}
		json card;
		card["name"] = "FarObject_" + std::to_string(o);
		card["position"] = { go->GetPosition().x, go->GetPosition().y, go->GetPosition().z };
		card["rotation"] = { 0, go->GetRotation().y, 0 };
		card["scale"] = { go->GetScale().x, go->GetScale().y, go->GetScale().z };
		card["children"] = json::array();
		json rc;
		rc["type"] = "RenderingComponent";
		rc["material"] = mat->second;
		rc["castingShadows"] = false;
		rc["cullTest"] = true;
		rc["renderable"] = { { "kind", "primitive" }, { "shape", "Card" }, { "left", bounds.value("left", -1.f) }, { "right", bounds.value("right", 1.f) },
			{ "bottom", bounds.value("bottom", 0.f) }, { "top", bounds.value("top", 1.f) }, { "crossed", true } };
		card["components"] = json::array({ rc });
		children.push_back(card);
	}
	// Some pictures still to render: this cell comes round again between
	// frames, where an offscreen pass is safe (see ProcessPendingModelThumbnails).
	if (deferred) farRebakeQueue.insert(std::make_pair(x, z));

	json tree;
	tree["root"] = { { "name", "Far_" + std::to_string(x) + "_" + std::to_string(z) },
		{ "position", { root->GetPosition().x, root->GetPosition().y, root->GetPosition().z } },
		{ "rotation", { 0, 0, 0 } }, { "scale", { 1, 1, 1 } }, { "components", json::array() }, { "children", children } };
	tree["materials"] = materials;
	std::ofstream out(farJson.c_str(), std::ios::binary | std::ios::trunc);
	out << tree.dump();
	if (!out) { errOut = "could not write " + farJson; return false; }
	out.close();

	editorWorld->AddFarCell(x, z);
	const std::pair<int32, int32> key(x, z);
	if (std::find(sceneWorld.farCells.begin(), sceneWorld.farCells.end(), key) == sceneWorld.farCells.end())
	{
		sceneWorld.farCells.push_back(key);
		MarkSceneDirty();
	}
	return true;
}

int SceneEditor::BakeFarCellAt(const int32 x, const int32 z, const int resolution, const bool allowRender, std::string& errOut)
{
	if (!editorWorld) { errOut = "the scene is not a streamed world"; return -1; }
	std::string err;
	if (std::shared_ptr<GameObject> root = editorWorld->CellRoot(x, z))
	{
		if (BakeFarCell(x, z, root.get(), resolution, err, allowRender)) return 1;
		if (!err.empty()) { errOut = err; return -1; }
		return 0;
	}
	if (editorWorld->IsLoading(x, z)) return 0;
	// Not loaded: read off-scene just long enough to bake.
	const std::string path = editorWorld->Streamer().CellPath(x, z);
	std::ifstream in(path.c_str(), std::ios::binary);
	if (!in) return 0;
	std::stringstream ss;
	ss << in.rdbuf();
	LoadedSceneAssets temp;
	std::string cellText = ss.str();
	prefab::ExpandSubtreeText(cellText, scenePath);
	std::shared_ptr<GameObject> root = SceneSerializer::DeserializeSubtree(cellText, scenePath, NULL, NULL, &temp);
	if (!root) return 0;
	if (BakeFarCell(x, z, root.get(), resolution, err, allowRender)) return 1;
	if (!err.empty()) { errOut = err; return -1; }
	return 0;
}

int SceneEditor::BakeFarCells(const bool all, const int resolution, std::string& errOut, const bool allowRender)
{
	if (!editorWorld) { errOut = "the scene is not a streamed world"; return -1; }
	int baked = 0;
	const std::vector<std::pair<int32_t, int32_t> > cells = editorWorld->Cells();
	for (size_t i = 0; i < cells.size(); i++)
	{
		if (!all && !editorWorld->CellRoot(cells[i].first, cells[i].second)) continue;
		const int r = BakeFarCellAt(cells[i].first, cells[i].second, resolution, allowRender, errOut);
		if (r < 0) return -1;
		baked += r;
	}
	return baked;
}
