//============================================================================
// Name        : SceneEditorExtrusion.cpp
// Description : Extrusion2D in the editor: the commands, the panel, and the
//               outline drawn and dragged in the view.
//
//   {"cmd":"add_extrusion2d","args":{"object":"Ground","points":[[0,0],[6,2],[12,0]],
//      "closed":false,"smooth":6,"depth":6,"floor":-4,"tile":3,
//      "skin":"assets/textures/dirt.jpg","face":"assets/textures/rock.jpg",
//      "skinColor":[1,1,1,1],"faceColor":[1,1,1,1],"collider":true}}
//   set_extrusion2d takes any of the same; extrusion2d_info reads them back,
//   with the curve ("path") the points make. edit_extrusion2d {"object","on"}
//   is the panel's Edit Outline.
//
// In the view, with Edit Outline on: drag a point; Shift-click on the line
// to add one there (or past an end, to carry it on); Alt-click one to remove
// it. It works in a 2D view and a 3D one alike - the cursor is taken to the
// plane the object is in.
//============================================================================

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Rendering/Components/Extrusion2D/Extrusion2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <filesystem>
#include <algorithm>
#include <cmath>

using namespace p3d;

namespace {

	SceneObject* ObjectNamed(SceneObjects* so, const std::string& name)
	{
		const std::map<uint32, SceneObject*>& list = so->GetList();
		for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); i++)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->second->GetName() == name)
				return i->second;
		return NULL;
	}

	template <class T> T* ComponentOf(GameObject* go, const uint32 type)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> >& comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == type) return static_cast<T*>(comps[i].get());
		return NULL;
	}

	bool PointsOf(const json& j, std::vector<Vec2>& out)
	{
		if (!j.is_array()) return false;
		out.clear();
		for (size_t i = 0; i < j.size(); i++)
		{
			if (!j[i].is_array() || j[i].size() < 2) return false;
			out.push_back(Vec2(j[i][0].get<f32>(), j[i][1].get<f32>()));
		}
		return true;
	}
}

bool SceneEditor::AgentApplyExtrusion2D(Extrusion2D* ex, const json& p, std::string& errOut)
{
	static const char* known[] = { "object", "name", "points", "closed", "smooth", "depth", "floor", "tile", "skin", "face",
		"skinColor", "faceColor", "collider", "skinAcross", NULL };
	for (json::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		bool ok = false;
		for (int k = 0; known[k] && !ok; k++) ok = (it.key() == known[k]);
		if (!ok) { errOut = "unknown key '" + it.key() + "'"; return false; }
	}
	try {
		if (p.contains("points"))
		{
			std::vector<Vec2> pts;
			if (!PointsOf(p["points"], pts) || pts.size() < 2) { errOut = "points must be two or more [x, y]"; return false; }
			ex->SetPoints(pts);
		}
		if (p.contains("closed")) ex->SetClosed(p["closed"].get<bool>());
		if (p.contains("smooth")) ex->SetSmooth((uint32)std::max(0, p["smooth"].get<int>()));
		if (p.contains("depth")) { const f32 d = p["depth"].get<f32>(); if (!(d > 0.f)) { errOut = "depth must be greater than 0"; return false; } ex->SetDepth(d); }
		if (p.contains("floor")) ex->SetFloor(p["floor"].get<f32>());
		if (p.contains("tile")) { const f32 t = p["tile"].get<f32>(); if (!(t > 0.f)) { errOut = "tile must be greater than 0"; return false; } ex->SetTile(t); }
		for (int k = 0; k < 2; k++)
		{
			const char* key = k == 0 ? "skin" : "face";
			if (!p.contains(key)) continue;
			const std::string rel = p[key].get<std::string>();
			std::string abs;
			if (!rel.empty())
			{
				abs = ResolveAssetPath(rel);
				std::error_code ec;
				if (!std::filesystem::exists(abs, ec)) { errOut = std::string(key) + " image not found: " + rel; return false; }
			}
			if (k == 0) ex->SetSkinTexture(abs, rel); else ex->SetFaceTexture(abs, rel);
		}
		if (p.contains("skinColor") || p.contains("faceColor"))
		{
			Vec4 s = ex->GetSkinColor(), f = ex->GetFaceColor();
			if (p.contains("skinColor") && p["skinColor"].is_array() && p["skinColor"].size() >= 3)
				s = Vec4(p["skinColor"][0].get<f32>(), p["skinColor"][1].get<f32>(), p["skinColor"][2].get<f32>(), p["skinColor"].size() > 3 ? p["skinColor"][3].get<f32>() : 1.f);
			if (p.contains("faceColor") && p["faceColor"].is_array() && p["faceColor"].size() >= 3)
				f = Vec4(p["faceColor"][0].get<f32>(), p["faceColor"][1].get<f32>(), p["faceColor"][2].get<f32>(), p["faceColor"].size() > 3 ? p["faceColor"][3].get<f32>() : 1.f);
			ex->SetColors(s, f);
		}
		if (p.contains("collider")) ex->SetCollider(p["collider"].get<bool>());
		if (p.contains("skinAcross")) ex->SetSkinAcross(p["skinAcross"].get<bool>());
	}
	catch (const std::exception& e) { errOut = std::string("bad value: ") + e.what(); return false; }
	ex->Apply();
	return true;
}

bool SceneEditor::OpAddExtrusion2D(uint32 goId, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (ComponentOf<Extrusion2D>(go, ComponentType::Extrusion2D)) { errOut = "this object already has an extrusion"; return false; }
	const std::string before = SnapshotSubtree(goId);
	std::shared_ptr<Extrusion2D> ex = std::make_shared<Extrusion2D>();
	// Something to see and to start from: a low hill.
	std::vector<Vec2> pts;
	pts.push_back(Vec2(-6.f, 0.f)); pts.push_back(Vec2(-2.f, 0.f)); pts.push_back(Vec2(1.f, 1.6f)); pts.push_back(Vec2(4.f, 0.f)); pts.push_back(Vec2(8.f, 0.f));
	ex->SetPoints(pts);
	// A body for it to shape, if the object has none: ground does not move.
	if (!ComponentOf<Physics2D>(go, ComponentType::Physics2D))
	{
		std::shared_ptr<Physics2D> body = std::make_shared<Physics2D>(Body2DType::Static, Shape2DType::Chain, Vec2(1.f, 1.f));
		body->SetFriction(1.f);
		go->Add(std::static_pointer_cast<IComponent>(body));
	}
	go->Add(std::static_pointer_cast<IComponent>(ex));
	ex->Apply();
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Extrusion");
	return true;
}

bool SceneEditor::AgentAddExtrusion2D(const std::string& name, const json& p, std::string& errOut)
{
	SceneObject* obj = ObjectNamed(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	if (!OpAddExtrusion2D(obj->GetID(), errOut)) return false;
	Extrusion2D* ex = ComponentOf<Extrusion2D>((GameObject*)obj->GetPTR(), ComponentType::Extrusion2D);
	if (!ex || !AgentApplyExtrusion2D(ex, p, errOut)) return false;
	return true;
}

bool SceneEditor::AgentSetExtrusion2D(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = ObjectNamed(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	Extrusion2D* ex = ComponentOf<Extrusion2D>((GameObject*)obj->GetPTR(), ComponentType::Extrusion2D);
	if (!ex) { errOut = "'" + name + "' has no Extrusion2D - add_extrusion2d first"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	if (!AgentApplyExtrusion2D(ex, p, errOut)) return false;
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Set Extrusion");
	return true;
}

json SceneEditor::AgentExtrusion2DInfo(const std::string& name, std::string& errOut)
{
	json r;
	SceneObject* obj = ObjectNamed(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return r; }
	Extrusion2D* ex = ComponentOf<Extrusion2D>((GameObject*)obj->GetPTR(), ComponentType::Extrusion2D);
	if (!ex) { errOut = "'" + name + "' has no Extrusion2D"; return r; }
	json pts = json::array(), path = json::array();
	for (size_t i = 0; i < ex->GetPoints().size(); i++) pts.push_back({ ex->GetPoints()[i].x, ex->GetPoints()[i].y });
	const std::vector<Vec2>& curve = ex->GetPath();
	for (size_t i = 0; i < curve.size(); i++) path.push_back({ curve[i].x, curve[i].y });
	r["points"] = pts;
	r["path"] = path;
	r["closed"] = ex->IsClosed();
	r["smooth"] = ex->GetSmooth();
	r["depth"] = ex->GetDepth();
	r["floor"] = ex->GetFloor();
	r["tile"] = ex->GetTile();
	r["skin"] = ex->GetSkinTexturePath();
	r["face"] = ex->GetFaceTexturePath();
	r["collider"] = ex->HasCollider();
	r["skinAcross"] = ex->IsSkinAcross();
	r["editing"] = extrusionEdit;
	Physics2D* body = ComponentOf<Physics2D>((GameObject*)obj->GetPTR(), ComponentType::Physics2D);
	r["colliderPoints"] = body ? (int)body->GetPolygon().size() : 0;
	return r;
}

bool SceneEditor::AgentEditExtrusion2D(const std::string& name, const bool on, std::string& errOut)
{
	SceneObject* obj = ObjectNamed(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	if (!ComponentOf<Extrusion2D>((GameObject*)obj->GetPTR(), ComponentType::Extrusion2D)) { errOut = "'" + name + "' has no Extrusion2D"; return false; }
	if (on) SelectSceneObject(obj);
	extrusionEdit = on;
	extrusionHover = extrusionDrag = -1;
	return true;
}

// ----- the panel ----------------------------------------------------------------------

void SceneEditor::DrawExtrusion2DProperties(uint32 goId, Extrusion2D* ex)
{
	if (!ImGui::CollapsingHeader("Extrusion 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;
	bool editing = extrusionEdit;
	if (ImGui::Checkbox("Edit Outline", &editing)) { extrusionEdit = editing; extrusionHover = extrusionDrag = -1; }
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("In the view: drag a point.\nShift-click on the line to add one there, or past an end to carry it on.\nAlt-click a point to remove it.");
	ImGui::SameLine();
	ImGui::TextDisabled("%d points", (int)ex->GetPoints().size());

	bool closed = ex->IsClosed();
	BeginComponentUndo(goId);
	if (ImGui::Checkbox("Closed##extr", &closed)) { ex->SetClosed(closed); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Closed");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Closed: the points are the whole shape - a block, a boulder.\nOpen: they are the top of it - ground - and it goes down to the floor.");
	int smooth = (int)ex->GetSmooth();
	BeginComponentUndo(goId);
	if (ImGui::SliderInt("Smooth##extr", &smooth, 0, 16)) { ex->SetSmooth((uint32)smooth); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Smoothing");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many pieces each stretch between two points is curved into.\n0 is straight lines.");
	f32 depth = ex->GetDepth();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Depth##extr", &depth, 0.05f, 0.01f, 1000.f)) { ex->SetDepth(depth); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Depth");
	if (!ex->IsClosed())
	{
		f32 fl = ex->GetFloor();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Floor##extr", &fl, 0.05f)) { ex->SetFloor(fl); ex->Apply(); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Extrusion Floor");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The height its faces go down to.");
	}
	f32 tile = ex->GetTile();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Tile##extr", &tile, 0.05f, 0.05f, 1000.f)) { ex->SetTile(tile); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Tile");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many units one repeat of its images covers.");
	for (int k = 0; k < 2; k++)
	{
		std::string path = k == 0 ? ex->GetSkinTexturePath() : ex->GetFaceTexturePath();
		BeginComponentUndo(goId);
		if (ImGui::InputText(k == 0 ? "Skin Image##extr" : "Face Image##extr", &path, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			const std::string abs = path.empty() ? std::string() : ResolveAssetPath(path);
			std::error_code ec;
			if (path.empty() || std::filesystem::exists(abs, ec))
			{
				if (k == 0) ex->SetSkinTexture(abs, path); else ex->SetFaceTexture(abs, path);
				ex->Apply();
				MarkSceneDirty();
			}
			else echo("ERROR: Extrusion - image not found: " + path);
		}
		EndComponentUndo(goId, k == 0 ? "Set Extrusion Skin Image" : "Set Extrusion Face Image");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(k == 0 ? "The image on what is walked on, repeated. A path in the project; Enter to set."
				: "The image on the cut faces, repeated. A path in the project; Enter to set.");
	}
	bool across = ex->IsSkinAcross();
	BeginComponentUndo(goId);
	if (ImGui::Checkbox("Skin image once across##extr", &across)) { ex->SetSkinAcross(across); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Skin Across");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("The skin's image laid once across the whole depth (it still repeats along):\nfor an image with lane lines or a kerb in it.");
	Vec4 sc = ex->GetSkinColor(), fc = ex->GetFaceColor();
	BeginComponentUndo(goId);
	bool changed = ImGui::ColorEdit4("Skin Colour##extr", &sc.x);
	changed |= ImGui::ColorEdit4("Face Colour##extr", &fc.x);
	if (changed) { ex->SetColors(sc, fc); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Colours");
	bool collider = ex->HasCollider();
	BeginComponentUndo(goId);
	if (ImGui::Checkbox("Collider follows the outline##extr", &collider)) { ex->SetCollider(collider); ex->Apply(); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Extrusion Collider");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Keeps this object's Physics 2D body as a chain along the outline:\nwhat is touched is what is seen.");
}

// ----- the outline in the view -------------------------------------------------------------

Extrusion2D* SceneEditor::SelectedExtrusion2D(uint32* goIdOut, GameObject** goOut)
{
	if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT) return NULL;
	GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
	Extrusion2D* ex = ComponentOf<Extrusion2D>(go, ComponentType::Extrusion2D);
	if (ex && goIdOut) *goIdOut = SelectedSceneObject->GetID();
	if (ex && goOut) *goOut = go;
	return ex;
}

// The cursor, in the object's own space, on the plane the object is in. `reach` is about nine pixels there.
bool SceneEditor::ExtrusionCursor(GameObject* go, Vec2& local, f32& reach) const
{
	Vec3 origin, dir;
	if (!viewportMouseValid || !ViewportRay(origin, dir)) return false;
	const Matrix world = go->GetWorldTransformation();
	const Vec3 at = world.GetTranslation();
	if (std::fabs(dir.z) < 1e-6f) return false;
	// (the plane of the face towards the camera: where the outline and its points are drawn,
	// so that what is under the cursor is what is picked)
	Extrusion2D* ex = ComponentOf<Extrusion2D>(go, ComponentType::Extrusion2D);
	const f32 planeZ = at.z + (ex ? ex->GetDepth() * 0.5f : 0.f);
	const f32 t = (planeZ - origin.z) / dir.z;
	const Vec3 hit = origin + dir * t;
	const Vec3 l = world.Inverse() * hit;
	local = Vec2(l.x, l.y);
	GameObject* cam = GetViewCameraGO();
	const f32 away = (isPerspective && cam) ? (hit - cam->GetWorldPosition()).magnitude() : 0.f;
	reach = isPerspective ? std::max(0.02f, away * 0.016f) : ViewportWorldPerPixel() * 9.f;
	return true;
}

void SceneEditor::DrawExtrusion2DOverlay()
{
	if (playMode || !debugRenderer) return;
	GameObject* go = NULL;
	Extrusion2D* ex = SelectedExtrusion2D(NULL, &go);
	if (!ex) { extrusionEdit = false; return; }
	const Matrix world = go->GetWorldTransformation();
	const f32 zf = ex->GetDepth() * 0.5f + 0.02f;      // drawn on the face towards the camera, so the dirt does not hide it
	const std::vector<Vec2>& path = ex->GetPath();
	const Vec4 line(0.25f, 0.9f, 1.f, 1.f);
	for (size_t i = 0; i + 1 < path.size() || (ex->IsClosed() && i < path.size()); i++)
	{
		const Vec2 &a = path[i], &b = path[(i + 1) % path.size()];
		debugRenderer->drawLine(world * Vec3(a.x, a.y, zf), world * Vec3(b.x, b.y, zf), line);
	}
	if (!extrusionEdit) return;
	Vec2 cursor; f32 reach = 0.15f;
	ExtrusionCursor(go, cursor, reach);
	const std::vector<Vec2>& pts = ex->GetPoints();
	for (size_t i = 0; i < pts.size(); i++)
	{
		const bool hot = ((int)i == extrusionHover || (int)i == extrusionDrag);
		const Vec4 col = hot ? Vec4(1.f, 1.f, 1.f, 1.f) : Vec4(1.f, 0.85f, 0.2f, 1.f);
		const f32 r = reach * (hot ? 0.75f : 0.55f);
		const Vec3 c0 = world * Vec3(pts[i].x - r, pts[i].y - r, zf), c1 = world * Vec3(pts[i].x + r, pts[i].y - r, zf);
		const Vec3 c2 = world * Vec3(pts[i].x + r, pts[i].y + r, zf), c3 = world * Vec3(pts[i].x - r, pts[i].y + r, zf);
		debugRenderer->drawLine(c0, c1, col); debugRenderer->drawLine(c1, c2, col);
		debugRenderer->drawLine(c2, c3, col); debugRenderer->drawLine(c3, c0, col);
		if (i + 1 < pts.size() || ex->IsClosed())
		{
			const Vec2 &n = pts[(i + 1) % pts.size()];
			debugRenderer->drawLine(world * Vec3(pts[i].x, pts[i].y, zf), world * Vec3(n.x, n.y, zf), Vec4(1.f, 0.85f, 0.2f, 0.45f));
		}
	}
}

bool SceneEditor::HandleExtrusion2DInput()
{
	if (playMode || !extrusionEdit) return false;
	uint32 goId = 0;
	GameObject* go = NULL;
	Extrusion2D* ex = SelectedExtrusion2D(&goId, &go);
	if (!ex) return false;
	Vec2 cursor; f32 reach = 0.15f;
	// (over the picture itself, by where the cursor is: the same test the collider's handles use)
	const ImVec2 mp = ImGui::GetIO().MousePos;
	const bool inView = (mp.x >= viewportImgMin.x && mp.x < viewportImgMin.x + viewportImgSize.x
		&& mp.y >= viewportImgMin.y && mp.y < viewportImgMin.y + viewportImgSize.y);
	const bool over = inView && ExtrusionCursor(go, cursor, reach);
	const std::vector<Vec2> pts = ex->GetPoints();

	if (extrusionDrag >= 0)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && over)
		{
			// (on the editor's move step, if one is set)
			Vec2 to = cursor;
			if (GetMoveSnap() > 0.f) { const Vec3 s = SnapPosition(Vec3(cursor.x, cursor.y, 0.f)); to = Vec2(s.x, s.y); }
			ex->SetPoint((uint32)extrusionDrag, to);
			ex->Apply();
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			MarkSceneDirty();
			PushReplaceCommand(goId, extrusionBefore, "Move Extrusion Point");
			extrusionDrag = -1;
		}
		return true;
	}

	extrusionHover = -1;
	if (!over) return true;
	f32 best = reach * reach;
	for (size_t i = 0; i < pts.size(); i++)
	{
		const f32 d = (pts[i].x - cursor.x) * (pts[i].x - cursor.x) + (pts[i].y - cursor.y) * (pts[i].y - cursor.y);
		if (d < best) { best = d; extrusionHover = (int)i; }
	}
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return true;

	if (ImGui::GetIO().KeyAlt)
	{
		if (extrusionHover >= 0 && pts.size() > (ex->IsClosed() ? 3u : 2u))
		{
			const std::string before = SnapshotSubtree(goId);
			ex->RemovePoint((uint32)extrusionHover);
			ex->Apply();
			MarkSceneDirty();
			PushReplaceCommand(goId, before, "Remove Extrusion Point");
			extrusionHover = -1;
		}
		return true;
	}
	if (ImGui::GetIO().KeyShift)
	{
		// On the line: a point there. Off an end of an open one: it is carried on to here.
		int at = -1;
		f32 nearest = reach * reach * 6.f;
		const size_t edges = ex->IsClosed() ? pts.size() : pts.size() - 1;
		for (size_t i = 0; i < edges; i++)
		{
			const Vec2 &u = pts[i], &w = pts[(i + 1) % pts.size()];
			const f32 ex_ = w.x - u.x, ey = w.y - u.y, len2 = ex_ * ex_ + ey * ey;
			if (len2 < 1e-10f) continue;
			f32 t = ((cursor.x - u.x) * ex_ + (cursor.y - u.y) * ey) / len2;
			t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			const f32 dx = u.x + ex_ * t - cursor.x, dy = u.y + ey * t - cursor.y;
			if (dx * dx + dy * dy < nearest) { nearest = dx * dx + dy * dy; at = (int)i; }
		}
		const std::string before = SnapshotSubtree(goId);
		if (at >= 0) ex->AddPoint(cursor, at + 1);
		else if (!ex->IsClosed())
		{
			const f32 d0 = (pts.front().x - cursor.x) * (pts.front().x - cursor.x) + (pts.front().y - cursor.y) * (pts.front().y - cursor.y);
			const f32 d1 = (pts.back().x - cursor.x) * (pts.back().x - cursor.x) + (pts.back().y - cursor.y) * (pts.back().y - cursor.y);
			if (d0 < d1) ex->AddPoint(cursor, 0); else ex->AddPoint(cursor, -1);
		}
		else return true;
		ex->Apply();
		MarkSceneDirty();
		PushReplaceCommand(goId, before, "Add Extrusion Point");
		return true;
	}
	if (extrusionHover >= 0)
	{
		extrusionDrag = extrusionHover;
		extrusionBefore = SnapshotSubtree(goId);
	}
	return true;
}

// {"cmd":"drag_extrusion2d","args":{"object":"Ground","index":2,"to":[3,1.5]}} - what dragging a point in the view does,
// and "add":[x,y] / "remove":index for Shift-click and Alt-click.
bool SceneEditor::AgentDragExtrusion2D(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = ObjectNamed(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	Extrusion2D* ex = ComponentOf<Extrusion2D>((GameObject*)obj->GetPTR(), ComponentType::Extrusion2D);
	if (!ex) { errOut = "'" + name + "' has no Extrusion2D"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	try {
		if (p.contains("remove"))
		{
			const int i = p["remove"].get<int>();
			if (i < 0 || (size_t)i >= ex->GetPoints().size() || ex->GetPoints().size() <= (ex->IsClosed() ? 3u : 2u)) { errOut = "no such point, or too few left"; return false; }
			ex->RemovePoint((uint32)i);
		}
		else if (p.contains("add") && p["add"].is_array() && p["add"].size() >= 2)
			ex->AddPoint(Vec2(p["add"][0].get<f32>(), p["add"][1].get<f32>()), p.value("at", -1));
		else if (p.contains("index") && p.contains("to") && p["to"].is_array() && p["to"].size() >= 2)
		{
			const int i = p["index"].get<int>();
			if (i < 0 || (size_t)i >= ex->GetPoints().size()) { errOut = "no such point"; return false; }
			ex->SetPoint((uint32)i, Vec2(p["to"][0].get<f32>(), p["to"][1].get<f32>()));
		}
		else { errOut = "give index + to, add, or remove"; return false; }
	}
	catch (const std::exception& e) { errOut = std::string("bad value: ") + e.what(); return false; }
	ex->Apply();
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Edit Extrusion Outline");
	return true;
}
