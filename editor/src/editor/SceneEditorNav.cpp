#include "SceneEditor.h"
#include <Pyros3D/Utils/Navigation/NavMesh.h>
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
		std::shared_ptr<NavMesh> made = std::make_shared<NavMesh>();
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
	if (!showNavMesh || !nm || !debugRenderer) return;
	// (the mesh's triangles are read once a mesh, not each frame)
	if (navOverlayOf != nm) { nm->GetDebugTriangles(navOverlayTris); navOverlayOf = nm; }
	const Vec3 up(0.f, 0.04f, 0.f);
	const Vec4 edge(0.15f, 0.85f, 1.f, 1.f);
	for (size_t i = 0; i + 2 < navOverlayTris.size(); i += 3)
	{
		const Vec3 a = navOverlayTris[i] + up, b = navOverlayTris[i + 1] + up, c = navOverlayTris[i + 2] + up;
		debugRenderer->drawLine(a, b, edge); debugRenderer->drawLine(b, c, edge); debugRenderer->drawLine(c, a, edge);
	}
	const Vec4 path(1.f, 0.55f, 0.1f, 1.f);
	for (size_t i = 1; i < navOverlayPath.size(); i++)
		debugRenderer->drawLine(navOverlayPath[i - 1] + up * 2.f, navOverlayPath[i] + up * 2.f, path);
}
