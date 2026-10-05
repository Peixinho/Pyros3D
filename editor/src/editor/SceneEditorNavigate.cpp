//============================================================================
// Name        : SceneEditorNavigate.cpp
// Description : Getting to things in a big scene. A map kilometres across
//               has thousands of objects, most of them in cells that are not
//               even loaded, and the only ways to reach one were to know
//               where it was and fly there, or to scroll the tree for it.
//
//                 F                  the view goes to what is selected
//                 double-click       an object in the tree: the same
//                 the search box     over the tree: every object whose name
//                                    has the text in it - the ones in cells
//                                    that are not loaded too - and a click
//                                    (or Enter, for the first) goes there
//                                    and selects it
//                 x, z               go to a place by its coordinates
//
//               The agent bridge has each: frame_object, find_objects, goto.
//============================================================================

#include "SceneEditor.h"
#include "EditorWorld.h"
#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

using namespace p3d;

namespace {
	std::string Lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	// How big a thing is, for standing back far enough to see all of it: the
	// largest bounding sphere among its own meshes and its children's.
	f32 RadiusOf(GameObject* go, const int depth)
	{
		if (!go || depth > 6) return 0.f;
		f32 r = 0.f;
		const Vec3 s = go->GetScale();
		const f32 scale = std::max(std::fabs(s.x), std::max(std::fabs(s.y), std::fabs(s.z)));
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			RenderingComponent* rc = dynamic_cast<RenderingComponent*>(cs[i].get());
			if (rc && rc->GetRenderable())
				r = std::max(r, (rc->GetRenderable()->GetBoundingSphereRadius() + rc->GetRenderable()->GetBoundingSphereCenter().magnitude()) * scale);
		}
		const Vec3 at = go->GetWorldPosition();
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
		{
			const Vec3 d = kids[i]->GetWorldPosition() - at;
			r = std::max(r, d.magnitude() + RadiusOf(kids[i].get(), depth + 1));
		}
		return r;
	}
}

// The view goes to a point, standing `distance` back along the way it is
// already looking.
void SceneEditor::GoToPoint(const Vec3 &at, const f32 distance)
{
	json a;
	a["position"] = json::array({ at.x, at.y, at.z });
	if (distance > 0.f) a["distance"] = distance;
	std::string err;
	AgentSetViewPivot(a, err);
}

void SceneEditor::FrameGameObject(GameObject* go)
{
	if (!go || !CameraPivot) return;
	// a whole root of a streamed world (a cell, a forest of a thousand trees)
	// is not something to stand back from: go to it, do not try to fit it
	const f32 radius = std::min(RadiusOf(go, 0), 400.f);
	GoToPoint(go->GetWorldPosition(), std::max(4.f, radius * 2.4f + 2.f));
}

bool SceneEditor::FrameSelection()
{
	GameObject* go = GetSelectedOwnerGameObject();
	if (!go) return false;
	FrameGameObject(go);
	return true;
}

// Every object of a streamed world's cells, loaded or not: its name, where it
// is and which cell it is in. Read off the cell files once a scene.
void SceneEditor::BuildNavIndex()
{
	navIndex.clear();
	navIndexFor = scenePath;
	if (!editorWorld || !sceneWorld.enabled) return;
	for (size_t c = 0; c < sceneWorld.cells.size(); c++)
	{
		const int32 x = sceneWorld.cells[c].first, z = sceneWorld.cells[c].second;
		std::ifstream in(editorWorld->Streamer().CellPath(x, z).c_str());
		if (!in.is_open()) continue;
		json cell = json::parse(in, NULL, false);
		if (!cell.is_object() || !cell.contains("root")) continue;
		const json &root = cell["root"];
		Vec3 origin;
		if (root.contains("position") && root["position"].is_array() && root["position"].size() >= 3)
			origin = Vec3(root["position"][0].get<f32>(), root["position"][1].get<f32>(), root["position"][2].get<f32>());
		if (!root.contains("children")) continue;
		for (const auto &o : root["children"])
		{
			NavEntry e;
			e.name = o.value("name", std::string());
			if (e.name.empty()) continue;
			if (o.contains("position") && o["position"].is_array() && o["position"].size() >= 3)
				e.position = origin + Vec3(o["position"][0].get<f32>(), o["position"][1].get<f32>(), o["position"][2].get<f32>());
			else e.position = origin;
			e.cellX = x; e.cellZ = z; e.inCell = true; e.id = 0;
			navIndex.push_back(e);
		}
	}
}

// Objects whose name has `query` in it (any case): the ones in the scene now
// first, then the ones in cells that are not loaded. At most `most`.
void SceneEditor::NavSearch(const std::string &query, std::vector<NavEntry> &out, const size_t most)
{
	out.clear();
	const std::string q = Lower(query);
	if (q.empty()) return;
	std::set<std::string> seen;
	for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end() && out.size() < most; ++i)
	{
		SceneObject* so = (*i).second;
		if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
		if (Lower(so->GetName()).find(q) == std::string::npos) continue;
		GameObject* go = (GameObject*)so->GetPTR();
		if (!go) continue;
		NavEntry e;
		e.name = so->GetName();
		e.position = go->GetWorldPosition();
		e.id = so->GetID();
		e.inCell = false;
		out.push_back(e);
		seen.insert(e.name);
	}
	if (!sceneWorld.enabled || !editorWorld) return;
	if (navIndexFor != scenePath) BuildNavIndex();
	// which cells are in the scene now: their objects were found above
	std::set<std::pair<int32, int32> > loaded;
	const std::set<const GameObject*> roots = editorWorld->LoadedRoots();
	for (std::set<const GameObject*>::const_iterator r = roots.begin(); r != roots.end(); ++r)
	{
		int32 x, z;
		if (editorWorld->Streamer().FindCell(*r, x, z)) loaded.insert(std::make_pair(x, z));
	}
	for (size_t i = 0; i < navIndex.size() && out.size() < most; i++)
	{
		const NavEntry &e = navIndex[i];
		if (loaded.count(std::make_pair(e.cellX, e.cellZ))) continue;
		if (Lower(e.name).find(q) == std::string::npos) continue;
		out.push_back(e);
	}
}

// Go to a search result. One in the scene is selected and framed; one in a
// cell that is not loaded is travelled to - the streamer loads the cell as
// the view arrives - and selected when it turns up.
void SceneEditor::NavGoTo(const NavEntry &e)
{
	if (!e.inCell)
	{
		SceneObject* so = sceneObjects->GetSceneObject(e.id);
		if (so && so->GetType() == SceneObjectTypes::GAMEOBJECT)
		{
			SelectAndFocusSceneObject(so);
			FrameGameObject((GameObject*)so->GetPTR());
			return;
		}
	}
	GoToPoint(e.position, 30.f);
	navPendingName = e.name;
	navPendingAt = e.position;
	navPendingFrames = 600;
}

// The row over the tree: the search box and where-to-go.
void SceneEditor::DrawNavigateBar()
{
	// something travelled to that had not loaded yet: select it once it has
	if (navPendingFrames > 0)
	{
		navPendingFrames--;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); ++i)
		{
			SceneObject* so = (*i).second;
			if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT || so->GetName() != navPendingName) continue;
			GameObject* go = (GameObject*)so->GetPTR();
			if (!go || (go->GetWorldPosition() - navPendingAt).magnitude() > 5.f) continue;
			SelectAndFocusSceneObject(so);
			FrameGameObject(go);
			navPendingFrames = 0;
			break;
		}
	}

	ImGui::PushItemWidth(-1);
	const bool entered = ImGui::InputTextWithHint("##scene_search", "Search objects  (Enter: go to the first)", &navQuery, ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();
	if (!navQuery.empty())
	{
		if (navQuery != navResultsFor || navResultsScene != scenePath)
		{
			NavSearch(navQuery, navResults, 200);
			navResultsFor = navQuery;
			navResultsScene = scenePath;
		}
		if (entered && !navResults.empty()) NavGoTo(navResults[0]);
		ImGui::TextDisabled("%d%s found", (int)navResults.size(), navResults.size() >= 200 ? "+" : "");
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear")) { navQuery.clear(); navResults.clear(); navResultsFor.clear(); }
		const f32 rows = std::min<f32>((f32)navResults.size(), 12.f);
		if (rows > 0.f && ImGui::BeginChild("##scene_search_results", ImVec2(0.f, rows * ImGui::GetTextLineHeightWithSpacing() + 6.f), true))
		{
			for (size_t i = 0; i < navResults.size(); i++)
			{
				const NavEntry &e = navResults[i];
				char label[512];
				if (e.inCell)
					snprintf(label, sizeof(label), "%s   (cell %d, %d - not loaded)##nav%d", e.name.c_str(), e.cellX, e.cellZ, (int)i);
				else
					snprintf(label, sizeof(label), "%s##nav%d", e.name.c_str(), (int)i);
				if (ImGui::Selectable(label)) NavGoTo(e);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%.0f, %.0f, %.0f", e.position.x, e.position.y, e.position.z);
			}
		}
		if (rows > 0.f) ImGui::EndChild();
	}

	// by coordinates: what a server's log and an admin page speak in
	ImGui::PushItemWidth(120.f);
	ImGui::InputFloat2("##goto_xz", navGoto, "%.0f");
	ImGui::PopItemWidth();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("x, z in the world");
	ImGui::SameLine();
	if (ImGui::SmallButton("Go to x, z"))
		GoToPoint(Vec3(navGoto[0], CameraPivot ? CameraPivot->GetPosition().y : 0.f, navGoto[1]), 0.f);
	ImGui::SameLine();
	if (ImGui::SmallButton("Frame (F)")) FrameSelection();
	ImGui::Separator();
}

// ---- the same, for the agent bridge ----------------------------------------
// ---------------------------------------------------------------- hidden in the editor
namespace {
	// What draws, on one object.
	void SetDrawn(GameObject* go, const bool drawn)
	{
		const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			if (!cs[i]) continue;
			const bool draws = dynamic_cast<RenderingComponent*>(cs[i].get()) != NULL
				|| cs[i]->GetComponentType() == ComponentType::UICanvas;
			if (!draws) continue;
			if (drawn) cs[i]->Enable(); else cs[i]->Disable();
		}
	}
	void SetDrawnBelow(GameObject* go, const bool drawn)
	{
		SetDrawn(go, drawn);
		const std::vector<std::shared_ptr<GameObject> >& kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (kids[i]) SetDrawnBelow(kids[i].get(), drawn);
	}
}

void SceneEditor::SetEditorHidden(uint32 goId, bool hidden)
{
	SceneObject* obj = sceneObjects ? sceneObjects->GetSceneObject(goId) : NULL;
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	GameObject* go = (GameObject*)obj->GetPTR();
	if (!go) return;
	if (hidden) editorHidden.insert(goId);
	else
	{
		editorHidden.erase(goId);
		// drawn again - unless something above it is still hidden, which the
		// next EnforceEditorHidden() puts right
		if (!playMode) SetDrawnBelow(go, true);
	}
	if (!playMode) EnforceEditorHidden();
	// (kept with the scene's editor settings, not in the scene: no "unsaved changes")
	if (!scenePath.empty()) SaveEditorSidecar(scenePath);
}

void SceneEditor::ShowAllEditorHidden()
{
	const std::set<uint32> was = editorHidden;
	editorHidden.clear();
	for (std::set<uint32>::const_iterator i = was.begin(); i != was.end(); ++i)
	{
		SceneObject* obj = sceneObjects ? sceneObjects->GetSceneObject(*i) : NULL;
		if (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT && obj->GetPTR() && !playMode)
			SetDrawnBelow((GameObject*)obj->GetPTR(), true);
	}
	if (!scenePath.empty()) SaveEditorSidecar(scenePath);
}

void SceneEditor::EnforceEditorHidden()
{
	if (editorHidden.empty() || !sceneObjects) return;
	for (std::set<uint32>::iterator i = editorHidden.begin(); i != editorHidden.end();)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(*i);
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT || !obj->GetPTR())
		{
			i = editorHidden.erase(i);              // deleted since
			continue;
		}
		SetDrawnBelow((GameObject*)obj->GetPTR(), false);
		++i;
	}
}

void SceneEditor::SuspendEditorHidden()
{
	if (!sceneObjects) return;
	for (std::set<uint32>::const_iterator i = editorHidden.begin(); i != editorHidden.end(); ++i)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(*i);
		if (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT && obj->GetPTR())
			SetDrawnBelow((GameObject*)obj->GetPTR(), true);
	}
}

bool SceneEditor::AgentSetHidden(const std::string& name, bool hidden, std::string& errOut)
{
	if (!sceneObjects) { errOut = "no scene"; return false; }
	for (std::map<uint32, SceneObject*>::const_iterator o = sceneObjects->GetList().begin(); o != sceneObjects->GetList().end(); ++o)
	{
		if (!o->second || o->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
		if (o->second->GetName() != name) continue;
		SetEditorHidden(o->second->GetID(), hidden);
		return true;
	}
	errOut = "no object named " + name;
	return false;
}

nlohmann::json SceneEditor::AgentListHidden() const
{
	nlohmann::json names = nlohmann::json::array();
	for (std::set<uint32>::const_iterator i = editorHidden.begin(); i != editorHidden.end(); ++i)
	{
		SceneObject* obj = sceneObjects ? sceneObjects->GetSceneObject(*i) : NULL;
		if (obj) names.push_back(obj->GetName());
	}
	return names;
}

bool SceneEditor::AgentFrameObject(const std::string &name, std::string &errOut)
{
	std::vector<NavEntry> found;
	NavSearch(name, found, 400);
	const NavEntry* best = NULL;
	for (size_t i = 0; i < found.size(); i++)
	{
		if (found[i].name == name) { best = &found[i]; break; }
		if (!best) best = &found[i];
	}
	if (!best) { errOut = "no object with '" + name + "' in its name"; return false; }
	NavGoTo(*best);
	return true;
}

json SceneEditor::AgentFindObjects(const std::string &query, const size_t most)
{
	std::vector<NavEntry> found;
	NavSearch(query, found, most);
	json out = json::array();
	for (size_t i = 0; i < found.size(); i++)
	{
		json e;
		e["name"] = found[i].name;
		e["position"] = json::array({ found[i].position.x, found[i].position.y, found[i].position.z });
		e["loaded"] = !found[i].inCell;
		if (found[i].inCell) e["cell"] = json::array({ found[i].cellX, found[i].cellZ });
		out.push_back(e);
	}
	return out;
}
