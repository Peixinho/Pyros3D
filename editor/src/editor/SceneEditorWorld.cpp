// A streamed world's cells in the editor: the grid drawn over the ground,
// moving objects between cells (on save, and on request), and placing
// objects into cells that are not loaded - or do not exist yet.

#include "SceneEditor.h"
#include "EditorWorld.h"
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/WorldStreamer.h>

#include <cmath>
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
		cellRoot = SceneSerializer::DeserializeSubtree(ss.str(), scenePath, physics, lua, &temp);
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
