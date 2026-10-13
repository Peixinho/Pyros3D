//============================================================================
// Name        : SceneEditorShapes2D.cpp
// Description : The editor's side of two 2D components that are shapes and
//               nothing more: SpriteMask2D (what sprites are shown inside
//               or outside of) and Line2D (a line with width, or a trail).
//               Their add operations, Properties sections, the outline a
//               mask is given in the Scene View, and the commands that do
//               the same from outside.
//============================================================================

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteBatch2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteMask2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Line2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Text2D.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <map>

using namespace p3d;

namespace {

	template <typename T>
	T* FindComponent(GameObject* go, const uint32 type)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == type)
				return static_cast<T*>(comps[i].get());
		return NULL;
	}

	SceneObject* FindObjectByName(SceneObjects* so, const std::string& name)
	{
		const std::map<uint32, SceneObject*>& list = so->GetList();
		for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); i++)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->second->GetName() == name)
				return i->second;
		return NULL;
	}

	std::string Lower(std::string s)
	{
		for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
		return s;
	}

	const char* const kMaskShapes[] = { "rect", "circle", "polygon", "image" };

	// [[x, y], ...] or a flat [x, y, x, y, ...].
	bool PointsFrom(const json& a, std::vector<Vec2>& out)
	{
		out.clear();
		if (!a.is_array()) return false;
		if (!a.empty() && a[0].is_array())
		{
			for (size_t i = 0; i < a.size(); i++)
			{
				if (!a[i].is_array() || a[i].size() < 2) return false;
				out.push_back(Vec2(a[i][0].get<f32>(), a[i][1].get<f32>()));
			}
		}
		else
			for (size_t i = 0; i + 1 < a.size(); i += 2) out.push_back(Vec2(a[i].get<f32>(), a[i + 1].get<f32>()));
		return true;
	}
	bool ColorFrom(const json& a, Vec4& out)
	{
		if (!a.is_array() || a.size() < 3) return false;
		out = Vec4(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>(), a.size() >= 4 ? a[3].get<f32>() : 1.f);
		return true;
	}
}

// --- masks -----------------------------------------------------------------

bool SceneEditor::OpAddSpriteMask2D(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindComponent<SpriteMask2D>(go, ComponentType::SpriteMask2D)) { errOut = "this object already has a sprite mask"; return false; }
	const std::string before = SnapshotSubtree(goId);
	std::shared_ptr<SpriteMask2D> mk = std::make_shared<SpriteMask2D>();
	// On a sprite it starts as that sprite's own rectangle, which is the
	// shape most often wanted: "show what is behind this panel".
	if (Sprite2D* sp = FindComponent<Sprite2D>(go, ComponentType::Sprite2D))
	{
		const Vec4 r = sp->GetLocalRect();
		if (r.z - r.x > 1e-4f && r.w - r.y > 1e-4f)
		{
			mk->SetSize(Vec2(r.z - r.x, r.w - r.y));
			mk->SetOffset(Vec2((r.x + r.z) * 0.5f, (r.y + r.w) * 0.5f));
		}
	}
	go->Add(std::static_pointer_cast<IComponent>(mk));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Sprite Mask");
	return true;
}

void SceneEditor::DrawSpriteMask2DProperties(uint32 goId, SpriteMask2D* mk)
{
	ImGui::Text("Sprite Mask");
	bool en = mk->IsEnabled();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Enabled##mask", &en)) { mk->SetEnabled(en); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Sprite Mask"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Off, the sprites that use it are drawn whole.");

	int shape = mk->GetShape();
	const char* shapes[] = { "Rectangle", "Circle", "Polygon", "Image" };
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Shape##mask", &shape, shapes, 4))
		{
			if (shape == SpriteMaskShape2D::Polygon && mk->GetPolygon().size() < 3)
			{
				const f32 hx = std::max(0.05f, mk->GetSize().x * 0.5f), hy = std::max(0.05f, mk->GetSize().y * 0.5f);
				std::vector<Vec2> pts;
				pts.push_back(Vec2(-hx, -hy)); pts.push_back(Vec2(hx, -hy)); pts.push_back(Vec2(hx, hy)); pts.push_back(Vec2(-hx, hy));
				mk->SetPolygon(pts);
			}
			mk->SetShape(shape);
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Set Mask Shape");
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Sprites are cut to the mask's shape before they are drawn. Image\ntakes the shape from a picture's opaque part - any silhouette -\nlaid over Size; its edge is hard, a texel is in or out.\nWhich sprites: those whose Mask is set to Inside or Outside, and\nthat are under this object in the tree - or name it.");

	if (mk->GetShape() == SpriteMaskShape2D::Image)
	{
		static std::string imgText;
		static SpriteMask2D* imgTextFor = NULL;
		if (imgTextFor != mk || (!ImGui::IsAnyItemActive() && imgText != mk->GetImagePath()))
		{ imgText = mk->GetImagePath(); imgTextFor = mk; }
		ImGui::SetNextItemWidth(200);
		std::string want;
		bool commit = ImGui::InputTextWithHint("Image##mask", "an image with alpha", &imgText, ImGuiInputTextFlags_EnterReturnsTrue)
			|| ImGui::IsItemDeactivatedAfterEdit();
		if (commit) want = imgText;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
			{
				const char* rel = (const char*)payload->Data;
				if (rel && ProjectManager::IsTextureExtension(rel)) { want = rel; commit = true; }
			}
			ImGui::EndDragDropTarget();
		}
		if (commit && want != mk->GetImagePath())
		{
			const std::string b = SnapshotSubtree(goId);
			json a; a["image"] = want;
			std::string ierr;
			if (AgentApplySpriteMask(mk, a, ierr)) { MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Mask Image"); }
			else echo("ERROR: " + ierr);
			imgText = mk->GetImagePath();
		}
		if (mk->GetImagePath().empty()) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "No image yet: the mask cuts nothing.");
		else ImGui::TextDisabled("%u pieces", mk->ImagePieceCount());
	}

	if (mk->GetShape() == SpriteMaskShape2D::Polygon)
	{
		std::vector<Vec2> pts = mk->GetPolygon();
		ImGui::TextDisabled("%d points (local space, convex)", (int)pts.size());
		bool changed = false, structural = false;
		for (size_t k = 0; k < pts.size(); k++)
		{
			ImGui::PushID((int)(3000 + k));
			ImGui::SetNextItemWidth(150);
			BeginComponentUndo(goId);
			if (ImGui::DragFloat2("##mpt", (float*)&pts[k], 0.01f)) changed = true;
			EndComponentUndo(goId, "Move Mask Point");
			ImGui::SameLine();
			if (pts.size() > 3 && ImGui::SmallButton("x")) { pts.erase(pts.begin() + (std::ptrdiff_t)k); structural = true; ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (pts.size() < 16 && ImGui::SmallButton("Add Point##mask"))
		{
			const Vec2 a = pts.back(), b = pts.front();
			Vec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
			const Vec2 out(b.y - a.y, -(b.x - a.x));
			const f32 len = sqrtf(out.x * out.x + out.y * out.y);
			if (len > 1e-5f) mid = Vec2(mid.x + out.x / len * 0.1f, mid.y + out.y / len * 0.1f);
			pts.push_back(mid);
			structural = true;
		}
		if (changed || structural)
		{
			const std::string b = structural ? SnapshotSubtree(goId) : std::string();
			if (mk->SetPolygon(pts)) { MarkSceneDirty(); if (structural) PushReplaceCommand(goId, b, "Edit Mask Polygon"); }
			else ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "That would not be convex - not applied.");
		}
	}
	else
	{
		Vec2 sz = mk->GetSize();
		BeginComponentUndo(goId);
		if (mk->GetShape() == SpriteMaskShape2D::Circle)
		{
			if (ImGui::DragFloat("Diameter", &sz.x, 0.01f, 0.f, 10000.f)) { mk->SetSize(Vec2(sz.x, sz.x)); MarkSceneDirty(); }
		}
		else if (ImGui::DragFloat2("Size##mask", (float*)&sz, 0.01f, 0.f, 10000.f)) { mk->SetSize(sz); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Mask Size");
	}
	Vec2 off = mk->GetOffset();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat2("Offset##mask", (float*)&off, 0.01f)) { mk->SetOffset(off); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Mask Offset");
	if (mk->GetShape() != SpriteMaskShape2D::Image)
	{
		f32 soft = mk->GetFeather();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Soft Edge", &soft, 0.01f, 0.f, 1000.f, soft > 0.f ? "%.2f" : "hard")) { mk->SetFeather(soft); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Mask Soft Edge");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("How far in from the outline what the mask shows fades out.\n0 is a hard edge.");
	}
}

// The outline of every mask on the selected object or above it, and of the
// mask a selected sprite is cut by: a mask draws nothing, so without this it
// is a shape nobody can see.
void SceneEditor::DrawShapes2DOverlay()
{
	if (!debugRenderer || playMode || SelectedSceneObject == NULL
		|| SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
	std::vector<SpriteMask2D*> show;
	if (SpriteMask2D* own = FindComponent<SpriteMask2D>(go, ComponentType::SpriteMask2D)) show.push_back(own);
	if (Sprite2D* sp = FindComponent<Sprite2D>(go, ComponentType::Sprite2D))
		if (sp->GetMaskInteraction() != Sprite2D::MaskNone)
			if (SpriteMask2D* used = SpriteMask2D::Find(go, sp->GetMaskName()))
				if (std::find(show.begin(), show.end(), used) == show.end()) show.push_back(used);
	for (size_t k = 0; k < show.size(); k++)
	{
		std::vector<Vec2> o;
		show[k]->WorldOutline(o);
		const Vec4 col = show[k]->IsEnabled() ? Vec4(1.f, 0.45f, 0.9f, 1.f) : Vec4(1.f, 0.45f, 0.9f, 0.35f);
		for (size_t i = 0; i < o.size(); i++)
			debugRenderer->drawLine(Vec3(o[i].x, o[i].y, 0.f), Vec3(o[(i + 1) % o.size()].x, o[(i + 1) % o.size()].y, 0.f), col);
	}
}

bool SceneEditor::AgentApplySpriteMask(SpriteMask2D* mk, const json& p, std::string& errOut)
{
	static const char* known[] = { "object", "name", "shape", "size", "diameter", "offset", "polygon", "enabled",
		"image", "alphaThreshold", "feather", NULL };
	for (json::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		bool ok = false;
		for (int k = 0; known[k] && !ok; k++) ok = (it.key() == known[k]);
		if (!ok) { errOut = "unknown key '" + it.key() + "'"; return false; }
	}
	try {
		if (p.contains("size"))
		{
			if (!p["size"].is_array() || p["size"].size() < 2) { errOut = "size must be [width, height]"; return false; }
			mk->SetSize(Vec2(p["size"][0].get<f32>(), p["size"][1].get<f32>()));
		}
		if (p.contains("diameter")) { const f32 d = p["diameter"].get<f32>(); mk->SetSize(Vec2(d, d)); }
		if (p.contains("feather")) mk->SetFeather(p["feather"].get<f32>());
		if (p.contains("offset"))
		{
			if (!p["offset"].is_array() || p["offset"].size() < 2) { errOut = "offset must be [x, y]"; return false; }
			mk->SetOffset(Vec2(p["offset"][0].get<f32>(), p["offset"][1].get<f32>()));
		}
		if (p.contains("polygon"))
		{
			std::vector<Vec2> pts;
			if (!PointsFrom(p["polygon"], pts) || !mk->SetPolygon(pts))
			{ errOut = "polygon must be 3 to 16 points making a convex outline"; return false; }
			if (!p.contains("shape")) mk->SetShape(SpriteMaskShape2D::Polygon);
		}
		if (p.contains("image"))
		{
			if (!p["image"].is_string()) { errOut = "image is an image path"; return false; }
			const std::string img = p["image"].get<std::string>();
			const std::string abs = img.empty() ? std::string() : ResolveAssetPath(img);
			if (!img.empty() && (abs.empty() || !std::filesystem::exists(abs))) { errOut = "image '" + img + "' not found"; return false; }
			if (!mk->SetImage(abs, abs.empty() ? std::string() : DisplayPath(abs), p.value("alphaThreshold", mk->GetAlphaThreshold())) && !img.empty())
			{ errOut = "image '" + img + "' could not be read"; return false; }
			if (!p.contains("shape") && !img.empty()) mk->SetShape(SpriteMaskShape2D::Image);
		}
		if (p.contains("shape"))
		{
			const std::string sh = Lower(p["shape"].get<std::string>());
			int found = -1;
			for (int i = 0; i < 4; i++) if (sh == kMaskShapes[i]) found = i;
			if (found < 0) { errOut = "shape must be rect, circle, polygon or image"; return false; }
			if (found == SpriteMaskShape2D::Polygon && mk->GetPolygon().size() < 3) { errOut = "a polygon mask needs `polygon`: its points"; return false; }
			mk->SetShape(found);
		}
		if (p.contains("enabled")) mk->SetEnabled(p["enabled"].get<bool>());
	}
	catch (const std::exception& e) { errOut = std::string("bad value: ") + e.what(); return false; }
	return true;
}

json SceneEditor::AgentSpriteMaskInfo(const std::string& name, std::string& errOut)
{
	json r;
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return r; }
	GameObject* go = (GameObject*)obj->GetPTR();
	SpriteMask2D* mk = FindComponent<SpriteMask2D>(go, ComponentType::SpriteMask2D);
	if (!mk) { errOut = "'" + name + "' has no SpriteMask2D"; return r; }
	r["shape"] = kMaskShapes[mk->GetShape() <= 3 ? mk->GetShape() : 0];
	if (!mk->GetImagePath().empty()) { r["image"] = mk->GetImagePath(); r["pieces"] = mk->ImagePieceCount(); }
	r["size"] = { mk->GetSize().x, mk->GetSize().y };
	r["offset"] = { mk->GetOffset().x, mk->GetOffset().y };
	r["feather"] = mk->GetFeather();
	r["enabled"] = mk->IsEnabled();
	json pts = json::array();
	for (size_t i = 0; i < mk->GetPolygon().size(); i++) pts.push_back({ mk->GetPolygon()[i].x, mk->GetPolygon()[i].y });
	r["polygon"] = pts;
	std::vector<Vec2> o;
	mk->WorldOutline(o);
	json wo = json::array();
	for (size_t i = 0; i < o.size() && i < 16; i++) wo.push_back({ o[i].x, o[i].y });
	r["worldOutline"] = wo;
	// Which sprites it is cutting, so "why is nothing masked" has an answer.
	json users = json::array();
	const std::map<uint32, SceneObject*>& list = sceneObjects->GetList();
	for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		if (!i->second || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
		GameObject* o2 = (GameObject*)i->second->GetPTR();
		Sprite2D* sp = FindComponent<Sprite2D>(o2, ComponentType::Sprite2D);
		if (sp && sp->GetMaskInteraction() != Sprite2D::MaskNone && SpriteMask2D::Find(o2, sp->GetMaskName()) == mk)
			users.push_back(i->second->GetName());
	}
	r["sprites"] = users;
	return r;
}

bool SceneEditor::AgentAddSpriteMask(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindComponent<SpriteMask2D>(go, ComponentType::SpriteMask2D)) { errOut = "this object already has a sprite mask"; return false; }
	std::shared_ptr<SpriteMask2D> mk = std::make_shared<SpriteMask2D>();
	if (!AgentApplySpriteMask(mk.get(), p.is_object() ? p : json::object(), errOut)) return false;
	go->Add(std::static_pointer_cast<IComponent>(mk));
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Add Sprite Mask");
	return true;
}

bool SceneEditor::AgentSetSpriteMask(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	SpriteMask2D* mk = FindComponent<SpriteMask2D>((GameObject*)obj->GetPTR(), ComponentType::SpriteMask2D);
	if (!mk) { errOut = "'" + name + "' has no SpriteMask2D - add_sprite_mask first"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	const uint32 rev = mk->GetRevision();
	if (!AgentApplySpriteMask(mk, p, errOut)) return false;
	if (mk->GetRevision() != rev) { MarkSceneDirty(); PushReplaceCommand(obj->GetID(), before, "Set Sprite Mask"); }
	return true;
}

// --- lines -----------------------------------------------------------------

bool SceneEditor::OpAddLine2D(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindComponent<Line2D>(go, ComponentType::Line2D)) { errOut = "this object already has a line"; return false; }
	if (FindComponent<Sprite2D>(go, ComponentType::Sprite2D) || FindComponent<SpriteBatch2D>(go, ComponentType::SpriteBatch2D))
	{
		// Each draws through the object's one RenderingComponent.
		errOut = "a line and a sprite cannot share an object - put the line on a child";
		return false;
	}
	const std::string before = SnapshotSubtree(goId);
	std::shared_ptr<Line2D> ln = std::make_shared<Line2D>();
	// Something to see and to edit, rather than a component that draws
	// nothing until its points are typed in.
	std::vector<Vec2> pts;
	pts.push_back(Vec2(-1.f, 0.f)); pts.push_back(Vec2(1.f, 0.f));
	ln->SetPoints(pts);
	go->Add(std::static_pointer_cast<IComponent>(ln));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Line");
	return true;
}

void SceneEditor::DrawLine2DProperties(uint32 goId, Line2D* ln)
{
	ImGui::Text("Line 2D");
	bool trail = ln->IsTrail();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Trail", &trail)) { ln->SetTrail(trail); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Trail"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Off, the line is the points below. On, it is the path this object\nhas just taken: laid down behind it as it moves and fading out.\nA trail lays nothing in the editor - press Play.");

	f32 w[2] = { ln->GetStartWidth(), ln->GetEndWidth() };
	BeginComponentUndo(goId);
	if (ImGui::DragFloat2(trail ? "Width head / tail" : "Width start / end", w, 0.005f, 0.f, 1000.f)) { ln->SetWidth(w[0], w[1]); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Line Width");
	Vec4 c0 = ln->GetStartColor(), c1 = ln->GetEndColor();
	BeginComponentUndo(goId);
	if (ImGui::ColorEdit4(trail ? "Head colour" : "Start colour", (float*)&c0)) { ln->SetColors(c0, c1); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Line Colour");
	BeginComponentUndo(goId);
	if (ImGui::ColorEdit4(trail ? "Tail colour" : "End colour", (float*)&c1)) { ln->SetColors(c0, c1); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Line Colour");
	bool add = ln->IsAdditive();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Additive", &add)) { ln->SetAdditive(add); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Line Blend"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Added to what is behind it rather than laid over it: a laser,\na spark, anything that is light.");
	int order = ln->GetSortOrder();
	BeginComponentUndo(goId);
	if (ImGui::DragInt("Sort Order##line", &order, 0.1f)) { ln->SetSortOrder(order); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Line Sort Order");

	{
		// Typed, or an image dropped on it from Assets.
		static std::string texText;
		static Line2D* texTextFor = NULL;
		if (texTextFor != ln || (!ImGui::IsAnyItemActive() && texText != ln->GetTexturePath()))
		{ texText = ln->GetTexturePath(); texTextFor = ln; }
		ImGui::SetNextItemWidth(200);
		std::string want;
		bool commit = ImGui::InputTextWithHint("Image##line", "none (plain colour)", &texText, ImGuiInputTextFlags_EnterReturnsTrue)
			|| ImGui::IsItemDeactivatedAfterEdit();
		if (commit) want = texText;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
			{
				const char* rel = (const char*)payload->Data;
				if (rel && ProjectManager::IsTextureExtension(rel)) { want = rel; commit = true; }
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("An image run along the line: left to right in the image is start to\nend along the line, and its height is the line's width. The colours\nabove tint it. Empty for a plain colour.");
		if (commit && want != ln->GetTexturePath())
		{
			const std::string b = SnapshotSubtree(goId);
			json a; a["texture"] = want;
			std::string terr;
			if (AgentApplyLine2D(ln, a, terr)) { MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Line Image"); }
			else echo("ERROR: " + terr);
			texText = ln->GetTexturePath();
		}
		if (!ln->GetTexturePath().empty())
		{
			f32 rep = ln->GetTextureRepeat();
			BeginComponentUndo(goId);
			if (ImGui::DragFloat("Repeat every", &rep, 0.01f, 0.f, 1000.f, rep > 0.f ? "%.2f" : "stretched once")) { ln->SetTextureRepeat(rep); MarkSceneDirty(); }
			EndComponentUndo(goId, "Set Line Image Repeat");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("How much of the line's length one copy of the image covers, in\nunits: a chain link, a dash. 0 stretches one copy over the whole line.");
			bool nearest = ln->IsTextureNearest();
			const std::string b = SnapshotSubtree(goId);
			if (ImGui::Checkbox("Pixel art (nearest)", &nearest)) { ln->SetTextureNearest(nearest); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Line Filter"); }
		}
	}

	if (trail)
	{
		f32 life = ln->GetTrailLifetime(), dist = ln->GetTrailMinDistance();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Lifetime (s)", &life, 0.01f, 0.01f, 60.f)) { ln->SetTrailLifetime(life); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Trail Lifetime");
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Point every", &dist, 0.005f, 0.001f, 100.f)) { ln->SetTrailMinDistance(dist); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Trail Spacing");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("How far the object travels between points. Smaller is a smoother\ncurve and more quads.");
		return;
	}

	bool closed = ln->IsClosed();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Closed", &closed)) { ln->SetClosed(closed); MarkSceneDirty(); PushReplaceCommand(goId, b, "Close Line"); }
	}
	std::vector<Vec2> pts = ln->GetPoints();
	ImGui::TextDisabled("%d points (local space)", (int)pts.size());
	bool changed = false, structural = false;
	const size_t shown = pts.size() > 16 ? 16 : pts.size();
	for (size_t k = 0; k < shown; k++)
	{
		ImGui::PushID((int)(4000 + k));
		ImGui::SetNextItemWidth(150);
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("##lpt", (float*)&pts[k], 0.01f)) changed = true;
		EndComponentUndo(goId, "Move Line Point");
		ImGui::SameLine();
		if (pts.size() > 2 && ImGui::SmallButton("x")) { pts.erase(pts.begin() + (std::ptrdiff_t)k); structural = true; ImGui::PopID(); break; }
		ImGui::PopID();
	}
	if (pts.size() > shown) ImGui::TextDisabled("... and %d more", (int)(pts.size() - shown));
	if (ImGui::SmallButton("Add Point##line"))
	{
		if (pts.size() >= 2)
		{
			const Vec2 a = pts[pts.size() - 2], b = pts.back();
			pts.push_back(Vec2(b.x + (b.x - a.x), b.y + (b.y - a.y)));
		}
		else pts.push_back(pts.empty() ? Vec2(0.f, 0.f) : Vec2(pts.back().x + 1.f, pts.back().y));
		structural = true;
	}
	if (changed || structural)
	{
		const std::string b = structural ? SnapshotSubtree(goId) : std::string();
		ln->SetPoints(pts);
		MarkSceneDirty();
		if (structural) PushReplaceCommand(goId, b, "Edit Line Points");
	}
}

bool SceneEditor::AgentApplyLine2D(Line2D* ln, const json& p, std::string& errOut)
{
	static const char* known[] = { "object", "name", "points", "closed", "width", "color", "colorStart", "colorEnd",
		"additive", "sortOrder", "trail", "lifetime", "minDistance", "emitting", "clearTrail",
		"texture", "textureRepeat", "textureNearest", NULL };
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
			if (!PointsFrom(p["points"], pts)) { errOut = "points must be [[x, y], ...]"; return false; }
			ln->SetPoints(pts);
		}
		if (p.contains("closed")) ln->SetClosed(p["closed"].get<bool>());
		if (p.contains("width"))
		{
			// One number for an even line, two for one that tapers.
			if (p["width"].is_number()) { const f32 w = p["width"].get<f32>(); ln->SetWidth(w, w); }
			else if (p["width"].is_array() && p["width"].size() >= 2) ln->SetWidth(p["width"][0].get<f32>(), p["width"][1].get<f32>());
			else { errOut = "width must be a number or [start, end]"; return false; }
		}
		Vec4 c0 = ln->GetStartColor(), c1 = ln->GetEndColor();
		bool colours = false;
		if (p.contains("color")) { if (!ColorFrom(p["color"], c0)) { errOut = "color must be [r, g, b, a]"; return false; } c1 = c0; colours = true; }
		if (p.contains("colorStart")) { if (!ColorFrom(p["colorStart"], c0)) { errOut = "colorStart must be [r, g, b, a]"; return false; } colours = true; }
		if (p.contains("colorEnd")) { if (!ColorFrom(p["colorEnd"], c1)) { errOut = "colorEnd must be [r, g, b, a]"; return false; } colours = true; }
		if (colours) ln->SetColors(c0, c1);
		if (p.contains("additive")) ln->SetAdditive(p["additive"].get<bool>());
		if (p.contains("sortOrder")) ln->SetSortOrder(p["sortOrder"].get<int>());
		if (p.contains("texture"))
		{
			if (!p["texture"].is_string()) { errOut = "texture is an image path, or \"\" for none"; return false; }
			const std::string t = p["texture"].get<std::string>();
			if (t.empty()) ln->SetTexture(std::string(), std::string());
			else
			{
				const std::string abs = ResolveAssetPath(t);
				if (abs.empty() || !std::filesystem::exists(abs)) { errOut = "texture '" + t + "' not found"; return false; }
				ln->SetTexture(abs, DisplayPath(abs));
			}
		}
		if (p.contains("textureRepeat")) ln->SetTextureRepeat(p["textureRepeat"].get<f32>());
		if (p.contains("textureNearest")) ln->SetTextureNearest(p["textureNearest"].get<bool>());
		if (p.contains("lifetime")) ln->SetTrailLifetime(p["lifetime"].get<f32>());
		if (p.contains("minDistance")) ln->SetTrailMinDistance(p["minDistance"].get<f32>());
		if (p.contains("trail")) ln->SetTrail(p["trail"].get<bool>());
		if (p.contains("emitting")) ln->SetEmitting(p["emitting"].get<bool>());
		if (p.value("clearTrail", false)) ln->ClearTrail();
	}
	catch (const std::exception& e) { errOut = std::string("bad value: ") + e.what(); return false; }
	return true;
}

json SceneEditor::AgentLine2DInfo(const std::string& name, std::string& errOut)
{
	json r;
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return r; }
	Line2D* ln = FindComponent<Line2D>((GameObject*)obj->GetPTR(), ComponentType::Line2D);
	if (!ln) { errOut = "'" + name + "' has no Line2D"; return r; }
	json pts = json::array();
	for (size_t i = 0; i < ln->GetPoints().size(); i++) pts.push_back({ ln->GetPoints()[i].x, ln->GetPoints()[i].y });
	r["points"] = pts;
	r["closed"] = ln->IsClosed();
	r["width"] = { ln->GetStartWidth(), ln->GetEndWidth() };
	r["colorStart"] = { ln->GetStartColor().x, ln->GetStartColor().y, ln->GetStartColor().z, ln->GetStartColor().w };
	r["colorEnd"] = { ln->GetEndColor().x, ln->GetEndColor().y, ln->GetEndColor().z, ln->GetEndColor().w };
	r["additive"] = ln->IsAdditive();
	r["sortOrder"] = ln->GetSortOrder();
	r["texture"] = ln->GetTexturePath();
	r["textureRepeat"] = ln->GetTextureRepeat();
	r["textureNearest"] = ln->IsTextureNearest();
	r["trail"] = ln->IsTrail();
	r["lifetime"] = ln->GetTrailLifetime();
	r["minDistance"] = ln->GetTrailMinDistance();
	r["emitting"] = ln->IsEmitting();
	// How many points a running trail has laid down: zero in the editor,
	// and zero while its object stands still.
	r["trailPoints"] = ln->TrailPointCount();
	return r;
}

bool SceneEditor::AgentAddLine2D(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindComponent<Line2D>(go, ComponentType::Line2D)) { errOut = "this object already has a line"; return false; }
	if (FindComponent<Sprite2D>(go, ComponentType::Sprite2D) || FindComponent<SpriteBatch2D>(go, ComponentType::SpriteBatch2D))
	{ errOut = "a line and a sprite cannot share an object - put the line on a child"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	std::shared_ptr<Line2D> ln = std::make_shared<Line2D>();
	if (!AgentApplyLine2D(ln.get(), p.is_object() ? p : json::object(), errOut)) return false;
	go->Add(std::static_pointer_cast<IComponent>(ln));
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Add Line");
	return true;
}

bool SceneEditor::AgentSetLine2D(const std::string& name, const json& p, std::string& errOut)
{
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	Line2D* ln = FindComponent<Line2D>((GameObject*)obj->GetPTR(), ComponentType::Line2D);
	if (!ln) { errOut = "'" + name + "' has no Line2D - add_line2d first"; return false; }
	// Playing, it pokes the running line and records nothing - the way to
	// stop a trail emitting or clear one from outside.
	const std::string before = playMode ? std::string() : SnapshotSubtree(obj->GetID());
	if (!AgentApplyLine2D(ln, p, errOut)) return false;
	if (!playMode) { MarkSceneDirty(); PushReplaceCommand(obj->GetID(), before, "Set Line"); }
	return true;
}


// --- Text2D: words in the world ------------------------------------------------
// {"cmd":"add_text2d","args":{"object":"Sign","font":"assets/fonts/x.ttf","text":"GO","size":0.8,
//   "color":[1,1,1,1],"anchor":[0.5,0.5]}} and set_text2d with any of text / size / color / anchor.
static bool ApplyText2D(Text2D* t, const json& p, std::string& errOut)
{
	if (p.contains("text")) { if (!p["text"].is_string()) { errOut = "text must be a string"; return false; } t->SetText(p["text"].get<std::string>()); }
	if (p.contains("size")) { const f32 s = p["size"].get<f32>(); if (!(s > 0.f)) { errOut = "size must be greater than 0"; return false; } t->SetSize(s); }
	if (p.contains("color") && p["color"].is_array() && p["color"].size() >= 3)
		t->SetColor(Vec4(p["color"][0].get<f32>(), p["color"][1].get<f32>(), p["color"][2].get<f32>(), p["color"].size() > 3 ? p["color"][3].get<f32>() : 1.f));
	if (p.contains("anchor") && p["anchor"].is_array() && p["anchor"].size() >= 2)
		t->SetAnchor(Vec2(p["anchor"][0].get<f32>(), p["anchor"][1].get<f32>()));
	return true;
}

bool SceneEditor::AgentAddText2D(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindComponent<Text2D>(go, ComponentType::Text2D)) { errOut = "this object already has a text"; return false; }
	if (FindComponent<Sprite2D>(go, ComponentType::Sprite2D) || FindComponent<Line2D>(go, ComponentType::Line2D))
	{ errOut = "a text cannot share an object with a sprite or a line - put it on a child"; return false; }
	const std::string fontRel = p.value("font", std::string());
	if (fontRel.empty()) { errOut = "'font' is required (a .ttf in the project)"; return false; }
	const std::string fontAbs = ResolveAssetPath(fontRel);
	std::error_code ec;
	if (!std::filesystem::exists(fontAbs, ec)) { errOut = "font not found: " + fontRel; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	std::shared_ptr<Font> font = std::make_shared<Font>(fontAbs, p.value("fontSize", 64.f), p.value("sdf", true));
	std::shared_ptr<Text2D> t = std::make_shared<Text2D>(font, p.value("text", std::string("Text")), p.value("size", 1.f));
	t->SetFontPath(fontRel);
	if (!ApplyText2D(t.get(), p, errOut)) return false;
	go->Add(std::static_pointer_cast<IComponent>(t));
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Add Text");
	return true;
}

bool SceneEditor::AgentSetText2D(const std::string& name, const json& p, std::string& errOut)
{
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	Text2D* t = FindComponent<Text2D>((GameObject*)obj->GetPTR(), ComponentType::Text2D);
	if (!t) { errOut = "'" + name + "' has no Text2D - add_text2d first"; return false; }
	const std::string before = playMode ? std::string() : SnapshotSubtree(obj->GetID());
	if (!ApplyText2D(t, p, errOut)) return false;
	if (!playMode) { MarkSceneDirty(); PushReplaceCommand(obj->GetID(), before, "Set Text"); }
	return true;
}

json SceneEditor::AgentText2DInfo(const std::string& name, std::string& errOut)
{
	json r;
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return r; }
	Text2D* t = FindComponent<Text2D>((GameObject*)obj->GetPTR(), ComponentType::Text2D);
	if (!t) { errOut = "'" + name + "' has no Text2D"; return r; }
	r["text"] = t->GetText();
	r["size"] = t->GetSize();
	r["font"] = t->GetFontPath();
	r["color"] = { t->GetColor().x, t->GetColor().y, t->GetColor().z, t->GetColor().w };
	r["anchor"] = { t->GetAnchor().x, t->GetAnchor().y };
	const Vec2 block = t->GetBlockSize();
	r["blockSize"] = { block.x, block.y };
	return r;
}

void SceneEditor::DrawText2DProperties(uint32 goId, Text2D* t)
{
	if (!ImGui::CollapsingHeader("Text 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::TextDisabled("%s", t->GetFontPath().c_str());
	std::string text = t->GetText();
	BeginComponentUndo(goId);
	if (ImGui::InputTextMultiline("Text##text2d", &text, ImVec2(0.f, ImGui::GetTextLineHeight() * 3.f))) { t->SetText(text); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Text");
	f32 size = t->GetSize();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Size##text2d", &size, 0.01f, 0.01f, 1000.f)) { t->SetSize(std::max(0.01f, size)); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Text Size");
	Vec4 c = t->GetColor();
	BeginComponentUndo(goId);
	if (ImGui::ColorEdit4("Color##text2d", &c.x)) { t->SetColor(c); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Text Color");
	Vec2 a = t->GetAnchor();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat2("Anchor##text2d", &a.x, 0.01f, 0.f, 1.f)) { t->SetAnchor(a); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Text Anchor");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Where in the block of text the object's origin is:\n(0.5, 0.5) its middle, (0, 0) its bottom left.");
}
