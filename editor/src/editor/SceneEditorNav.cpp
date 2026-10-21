#include "SceneEditor.h"
#include <Pyros3D/Utils/Navigation/NavMesh.h>
#include <Pyros3D/Utils/Navigation/NavAgentComponent.h>
#include <cstring>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include "imgui.h"
#include <filesystem>

// The scene's navigation mesh in the editor: baked from the scene's static objects into
// assets/nav/<scene>.p3dnav, named in the scene file ("navigation"), loaded with the scene, and
// drawn over the view on request. Socket commands get_navmesh / bake_navmesh / show_navmesh /
// nav_path / clear_navmesh; the Scene panel's Navigation section goes through them too.

namespace {
	json NavSettingsJson(const NavMeshSettings &s)
	{
		return { { "agentRadius", s.agentRadius }, { "agentHeight", s.agentHeight }, { "agentMaxClimb", s.agentMaxClimb },
			{ "agentMaxSlope", s.agentMaxSlope }, { "cellSize", s.cellSize }, { "cellHeight", s.cellHeight },
			{ "tileSize", s.tileSize }, { "regionMinSize", s.regionMinSize } };
	}
	void NavSettingsFrom(const json &a, NavMeshSettings &s)
	{
		s.agentRadius = a.value("agentRadius", s.agentRadius); s.agentHeight = a.value("agentHeight", s.agentHeight);
		s.agentMaxClimb = a.value("agentMaxClimb", s.agentMaxClimb); s.agentMaxSlope = a.value("agentMaxSlope", s.agentMaxSlope);
		s.cellSize = a.value("cellSize", s.cellSize); s.cellHeight = a.value("cellHeight", s.cellHeight);
		s.tileSize = a.value("tileSize", s.tileSize); s.regionMinSize = a.value("regionMinSize", s.regionMinSize);
	}
	Vec3 V3(const json &j) { return j.is_array() && j.size() >= 3 ? Vec3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>()) : Vec3(); }
	json LinksJson(const std::vector<NavLink> &links)
	{
		json lj = json::array();
		for (const NavLink &l : links)
			lj.push_back({ { "a", json::array({ l.a.x, l.a.y, l.a.z }) }, { "b", json::array({ l.b.x, l.b.y, l.b.z }) }, { "radius", l.radius }, { "bidirectional", l.bidirectional } });
		return lj;
	}
	std::vector<NavLink> LinksFrom(const json &lj)
	{
		std::vector<NavLink> out;
		for (size_t i = 0; lj.is_array() && i < lj.size(); i++)
		{
			NavLink l; l.a = V3(lj[i].value("a", json::array())); l.b = V3(lj[i].value("b", json::array()));
			l.radius = lj[i].value("radius", 0.5f); l.bidirectional = lj[i].value("bidirectional", false);
			out.push_back(l);
		}
		return out;
	}
}

// get_navmesh / bake_navmesh {settings...} / show_navmesh {on} / nav_path {from:[x,y,z], to:[x,y,z]}
// / clear_navmesh. Settings: agentRadius, agentHeight, agentMaxClimb, agentMaxSlope, cellSize,
// cellHeight, tileSize, regionMinSize.
bool SceneEditor::AgentNavigation(const std::string& command, const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	if (!scene) { errOut = "no scene"; return false; }
	NavMesh* nm = scene->GetNavMesh();
	if (command == "get_navmesh")
	{
		out["enabled"] = navOn;
		out["baked"] = navBaked;
		out["settings"] = NavSettingsJson(navSettings);
		out["links"] = LinksJson(navLinks);
		out["loaded"] = nm != NULL;
		out["shown"] = showNavMesh;
		if (nm) { out["polygons"] = nm->GetPolygonCount(); out["tiles"] = nm->GetTileCount(); }
		return true;
	}
	if (command == "show_navmesh")
	{
		showNavMesh = args.value("on", !showNavMesh);
		return AgentNavigation("get_navmesh", json::object(), out, errOut);
	}
	if (command == "nav_path")
	{
		if (!nm) { errOut = "the scene has no navigation mesh (bake_navmesh)"; return false; }
		std::vector<Vec3> pts;
		const bool reached = nm->FindPath(V3(args.value("from", json::array())), V3(args.value("to", json::array())), pts);
		json p = json::array();
		f32 length = 0.f;
		for (size_t i = 0; i < pts.size(); i++)
		{
			p.push_back({ pts[i].x, pts[i].y, pts[i].z });
			if (i) length += pts[i].distance(pts[i - 1]);
		}
		out["reached"] = reached; out["points"] = p; out["length"] = length;
		navOverlayPath = pts;                      // (drawn with the mesh, until the next one)
		return true;
	}
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (command == "set_navigation")
	{
		// The settings and links the next bake uses (nothing is built here).
		NavSettingsFrom(args, navSettings);
		if (args.contains("links")) navLinks = LinksFrom(args["links"]);
		sceneDirty = true;
		return AgentNavigation("get_navmesh", json::object(), out, errOut);
	}
	if (command == "clear_navmesh")
	{
		scene->SetNavMesh(std::shared_ptr<NavMesh>());
		navOverlayOf = NULL; navOverlayTris.clear();
		navOn = false; navBaked.clear(); navOverlayPath.clear();
		sceneDirty = true;
		out["ok"] = true;
		return true;
	}
	if (command == "bake_navmesh")
	{
		if (!project || !project->IsOpen()) { errOut = "open a project first (the mesh is saved into its assets)"; return false; }
		if (scenePath.empty()) { errOut = "save the scene first (the mesh is named after it)"; return false; }
		NavSettingsFrom(args, navSettings);
		if (args.contains("links")) navLinks = LinksFrom(args["links"]);
		std::shared_ptr<NavMesh> made = std::make_shared<NavMesh>();
		for (const NavLink &l : navLinks) made->AddLink(l.a, l.b, l.radius, l.bidirectional);
		// The level: what is marked static and is the scene's own (not the editor's helpers).
		std::vector<std::shared_ptr<GameObject> > &all = scene->GetAllGameObjectList();
		uint32 objects = 0;
		for (size_t i = 0; i < all.size(); i++)
		{
			GameObject* go = all[i].get();
			if (!go || !go->IsStatic() || IsInternalGameObject(go)) continue;
			made->AddObject(go, false);
			objects++;
		}
		if (made->GetTriangleCount() == 0) { errOut = "nothing to bake from: mark the level's objects Static"; return false; }
		if (!made->Build(navSettings)) { errOut = "nothing walkable came out (check the agent size and that the ground is static)"; return false; }
		const std::string stem = std::filesystem::path(scenePath).stem().string();
		const std::string rel = "assets/nav/" + stem + ".p3dnav";
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(project->AbsolutePath(rel)).parent_path(), ec);
		if (!made->Save(project->AbsolutePath(rel))) { errOut = "could not write " + rel; return false; }
		scene->SetNavMesh(made);
		navOverlayOf = NULL;
		navOn = true; navBaked = rel; navOverlayPath.clear();
		sceneDirty = true;
		out["file"] = rel;
		out["objects"] = objects;
		out["triangles"] = made->GetTriangleCount();
		out["polygons"] = made->GetPolygonCount();
		out["tiles"] = made->GetTileCount();
		out["ms"] = made->GetBuildMilliseconds();
		out["settings"] = NavSettingsJson(navSettings);
		out["links"] = navLinks.size();
		echo("Navigation: baked " + std::to_string(made->GetPolygonCount()) + " polygons from " + std::to_string(objects) + " objects into " + rel);
		return true;
	}
	errOut = "unknown navigation command " + command;
	return false;
}

void SceneEditor::DrawNavigationSettings()
{
	ImGui::TextUnformatted("Navigation");
	ImGui::PushID("navigation");
	NavMesh* nm = scene ? scene->GetNavMesh() : NULL;
	if (nm) ImGui::TextDisabled("%u polygons in %u tiles%s", nm->GetPolygonCount(), nm->GetTileCount(), navBaked.empty() ? "" : (" - " + navBaked).c_str());
	else ImGui::TextDisabled("None baked. Mark the level Static, then Bake.");
	auto field = [&](const char* label, float &v, float lo, float hi, const char* fmt, const char* tip) {
		ImGui::SetNextItemWidth(110.f);
		ImGui::DragFloat(label, &v, (hi - lo) / 400.f, lo, hi, fmt);
		if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
	};
	field("Radius##nav", navSettings.agentRadius, 0.05f, 5.f, "%.2f m", "How far from walls the characters keep.");
	ImGui::SameLine();
	field("Height##nav", navSettings.agentHeight, 0.2f, 10.f, "%.2f m", "The headroom they need.");
	field("Step##nav", navSettings.agentMaxClimb, 0.f, 3.f, "%.2f m", "The highest step they walk up.");
	ImGui::SameLine();
	field("Slope##nav", navSettings.agentMaxSlope, 0.f, 85.f, "%.0f deg", "The steepest ground they walk on.");
	field("Cell##nav", navSettings.cellSize, 0.05f, 2.f, "%.2f m", "How finely the level is sampled across: about half the radius.");
	ImGui::SameLine();
	field("Tile##nav", navSettings.tileSize, 4.f, 256.f, "%.0f m", "A big world is built tile by tile, in parallel.");
	// Links: where walking does not join (a drop, a ladder, a jump), baked into the mesh.
	if (ImGui::TreeNode("Links##navlinks"))
	{
		bool edited = false;
		for (size_t i = 0; i < navLinks.size(); i++)
		{
			ImGui::PushID((int)i);
			NavLink &l = navLinks[i];
			ImGui::SetNextItemWidth(180.f); ImGui::DragFloat3("from", &l.a.x, 0.05f); edited |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::SetNextItemWidth(180.f); ImGui::DragFloat3("to", &l.b.x, 0.05f); edited |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("radius", &l.radius, 0.01f, 0.05f, 5.f); edited |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::SameLine(); edited |= ImGui::Checkbox("both ways", &l.bidirectional);
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) { navLinks.erase(navLinks.begin() + i); edited = true; ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (!playMode && ImGui::SmallButton("+ Link"))
		{
			// (in front of the view, a metre long: then dragged to the ledge and the ground below)
			NavLink l; l.a = Vec3(0.f, 0.f, 0.f); l.b = Vec3(0.f, 0.f, 1.f);
			navLinks.push_back(l); edited = true;
		}
		if (edited) sceneDirty = true;
		ImGui::TextDisabled("Bake again for the links to count.");
		ImGui::TreePop();
	}
	if (!playMode && ImGui::Button("Bake##nav")) pendingFoliageOp = { { "cmd", "bake_navmesh" } };
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Builds it from the scene's static objects and saves it with the project\n(assets/nav/<scene>.p3dnav); the scene loads it, and scripts get it with getNavMesh(scene).");
	ImGui::SameLine();
	ImGui::Checkbox("Show##nav", &showNavMesh);
	if (nm && !playMode) { ImGui::SameLine(); if (ImGui::SmallButton("Clear##nav")) pendingFoliageOp = { { "cmd", "clear_navmesh" } }; }
	ImGui::PopID();
}

void SceneEditor::DrawNavMeshOverlay()
{
	NavMesh* nm = scene ? scene->GetNavMesh() : NULL;
	if (!showNavMesh || !debugRenderer) return;
	if (!nm) { navOverlayTris.clear(); navOverlayOf = NULL; }
	// (the mesh's triangles are read once a mesh, not each frame)
	if (nm && navOverlayOf != nm) { nm->GetDebugTriangles(navOverlayTris); navOverlayOf = nm; }
	const Vec3 up(0.f, 0.04f, 0.f);
	const Vec4 edge(0.15f, 0.85f, 1.f, 1.f);
	for (size_t i = 0; i + 2 < navOverlayTris.size(); i += 3)
	{
		const Vec3 a = navOverlayTris[i] + up, b = navOverlayTris[i + 1] + up, c = navOverlayTris[i + 2] + up;
		debugRenderer->drawLine(a, b, edge); debugRenderer->drawLine(b, c, edge); debugRenderer->drawLine(c, a, edge);
	}
	// Links: a line from start to end, the end marked (a drop goes one way).
	const Vec4 link(1.f, 0.9f, 0.2f, 1.f);
	for (const NavLink &l : navLinks)
	{
		const Vec3 a = l.a + up * 2.f, b = l.b + up * 2.f, mid = (a + b) * 0.5f + Vec3(0.f, 0.25f + a.distance(b) * 0.15f, 0.f);
		debugRenderer->drawLine(a, mid, link); debugRenderer->drawLine(mid, b, link);
		debugRenderer->drawLine(b, b + Vec3(0.f, 0.3f, 0.f), link);
		if (l.bidirectional) debugRenderer->drawLine(a, a + Vec3(0.f, 0.3f, 0.f), link);
	}
	const Vec4 path(1.f, 0.55f, 0.1f, 1.f);
	for (size_t i = 1; i < navOverlayPath.size(); i++)
		debugRenderer->drawLine(navOverlayPath[i - 1] + up * 2.f, navOverlayPath[i] + up * 2.f, path);
}

// ---------------------------------------------------------------- the NavAgent component

namespace {
	NavAgentComponent* FindNavAgent(GameObject* go)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::NavAgent) return static_cast<NavAgentComponent*>(comps[i].get());
		return NULL;
	}
	const char* kAgentKeys[] = { "speed", "radius", "acceleration", "stoppingDistance", "faceMovement", "yawOffset", "turnSpeed" };
}

// get_nav_agent / add_nav_agent / set_nav_agent / remove_nav_agent {name|id, speed, radius,
// acceleration, stoppingDistance, faceMovement, yawOffset, turnSpeed}; nav_agent_goto {name|id,
// to: [x,y,z]} in Play.
bool SceneEditor::AgentNavAgent(const std::string& command, const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	SceneObject* target = NULL;
	if (args.contains("id")) target = sceneObjects->GetSceneObject(args.value("id", 0u));
	else if (args.contains("name"))
	{
		const std::string name = args.value("name", std::string());
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end() && !target; ++i)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->second->GetName() == name) target = i->second;
	}
	else target = SelectedSceneObject;
	if (!target || target->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found (name, id, or a selection)"; return false; }
	GameObject* go = (GameObject*)target->GetPTR();
	const uint32 id = target->GetID();
	NavAgentComponent* ag = FindNavAgent(go);
	if (command == "get_nav_agent")
	{
		if (!ag) { out["navAgent"] = nullptr; return true; }
		out["speed"] = ag->GetMaxSpeed(); out["radius"] = ag->GetRadius(); out["acceleration"] = ag->GetAcceleration();
		out["stoppingDistance"] = ag->GetStoppingDistance(); out["faceMovement"] = ag->GetFaceMovement();
		out["yawOffset"] = ag->GetYawOffset(); out["turnSpeed"] = ag->GetTurnSpeed();
		out["running"] = ag->IsRunning(); out["moving"] = ag->IsMoving(); out["reachable"] = ag->IsReachable();
		out["currentSpeed"] = ag->GetSpeed(); out["remaining"] = ag->GetRemainingDistance(); out["onLink"] = ag->IsOnLink();
		const Vec3 p = go->GetWorldPosition();
		out["position"] = { p.x, p.y, p.z };
		return true;
	}
	if (command == "nav_agent_goto")
	{
		if (!ag) { errOut = "it has no NavAgent"; return false; }
		if (!playMode) { errOut = "agents walk in Play"; return false; }
		out["reachable"] = ag->SetDestination(V3(args.value("to", json::array())));
		return AgentNavAgent("get_nav_agent", json{ { "id", id } }, out, errOut);
	}
	if (playMode) { errOut = "stop play mode first"; return false; }
	json settings = json::object();
	for (const char* k : kAgentKeys) if (args.contains(k)) settings[k] = args[k];
	if (command == "add_nav_agent" || command == "set_nav_agent")
	{
		const bool add = command == "add_nav_agent";
		if (add && ag) { errOut = "it already has a NavAgent"; return false; }
		if (!add && !ag) { errOut = "it has no NavAgent (add_nav_agent)"; return false; }
		const bool ok = EditSubtreeJson(id, [settings, add](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			json* cj = NULL;
			for (size_t i = 0; comps.is_array() && i < comps.size(); i++) if (comps[i].value("type", std::string()) == "NavAgent") cj = &comps[i];
			if (add) { json c = { { "type", "NavAgent" } }; for (json::const_iterator k = settings.begin(); k != settings.end(); ++k) c[k.key()] = k.value(); comps.push_back(c); return true; }
			if (!cj) { err = "it has no NavAgent"; return false; }
			for (json::const_iterator k = settings.begin(); k != settings.end(); ++k) (*cj)[k.key()] = k.value();
			return true;
		}, add ? "Add Nav Agent" : "Edit Nav Agent", errOut);
		if (!ok) return false;
		if (SceneObject* now = sceneObjects->GetSceneObject(id)) SelectSceneObject(now);
		return AgentNavAgent("get_nav_agent", json{ { "id", id } }, out, errOut);
	}
	if (command == "remove_nav_agent")
	{
		const bool ok = EditSubtreeJson(id, [](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
				if (comps[i].value("type", std::string()) == "NavAgent") { comps.erase(comps.begin() + i); return true; }
			err = "it has no NavAgent";
			return false;
		}, "Remove Nav Agent", errOut);
		if (!ok) return false;
		out["ok"] = true;
		return true;
	}
	errOut = "unknown nav agent command " + command;
	return false;
}

void SceneEditor::DrawNavAgentProperties(GameObject* go, uint32 goId)
{
	NavAgentComponent* ag = FindNavAgent(go);
	if (!ag) return;
	if (!ImGui::CollapsingHeader("Nav Agent##props_navagent", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("navagent_props");
	if (playMode)
		ImGui::TextDisabled("%s  %.1f m/s  %.1f m to go%s", ag->IsMoving() ? "walking" : "standing", ag->GetSpeed(), ag->GetRemainingDistance(), ag->IsOnLink() ? "  (on a link)" : "");
	else if (!scene || !scene->GetNavMesh())
		ImGui::TextDisabled("The scene has no navigation mesh: Scene > Navigation > Bake.");
	else
		ImGui::TextDisabled("Walks in Play: a script calls setDestination(x, y, z).");
	auto field = [&](const char* label, const char* key, float v, float lo, float hi, const char* fmt, const char* tip) {
		ImGui::SetNextItemWidth(-1.f);
		const bool changed = ImGui::SliderFloat(label, &v, lo, hi, fmt);
		if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
		if (playMode && changed)
		{
			if (!strcmp(key, "speed")) ag->SetMaxSpeed(v); else if (!strcmp(key, "radius")) ag->SetRadius(v);
			else if (!strcmp(key, "acceleration")) ag->SetAcceleration(v); else if (!strcmp(key, "stoppingDistance")) ag->SetStoppingDistance(v);
			else if (!strcmp(key, "turnSpeed")) ag->SetTurnSpeed(v);
		}
		else if (!playMode && ImGui::IsItemDeactivatedAfterEdit()) pendingFoliageOp = { { "cmd", "set_nav_agent" }, { "id", goId }, { key, v } };
	};
	field("##speed", "speed", ag->GetMaxSpeed(), 0.f, 12.f, "speed %.1f m/s", "Its top speed (1.4 a walk, 3.5 a jog, 6 a run).");
	field("##radius", "radius", ag->GetRadius(), 0.05f, 2.f, "radius %.2f m", "How much room it takes among the others.");
	field("##accel", "acceleration", ag->GetAcceleration(), 0.5f, 40.f, "acceleration %.1f", "How quickly it gets up to speed and stops.");
	field("##stop", "stoppingDistance", ag->GetStoppingDistance(), 0.f, 5.f, "stops %.2f m short", "How near the destination counts as there.");
	field("##turn", "turnSpeed", ag->GetTurnSpeed(), 0.f, 30.f, "turns %.1f rad/s", NULL);
	bool face = ag->GetFaceMovement();
	if (ImGui::Checkbox("Face the way it walks", &face))
	{
		if (playMode) ag->SetFaceMovement(face, ag->GetYawOffset());
		else pendingFoliageOp = { { "cmd", "set_nav_agent" }, { "id", goId }, { "faceMovement", face } };
	}
	if (!playMode && ImGui::SmallButton("Remove Nav Agent")) pendingFoliageOp = { { "cmd", "remove_nav_agent" }, { "id", goId } };
	ImGui::PopID();
}
