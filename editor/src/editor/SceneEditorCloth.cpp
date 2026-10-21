#include "SceneEditor.h"
#include <Pyros3D/Physics/Cloth/ClothComponent.h>
#include "imgui.h"
#include <cstring>

// The Cloth component in the editor: socket commands (get_cloth / add_cloth / set_cloth /
// remove_cloth) and the inspector section, which goes through the same commands - so an agent
// can do whatever the panel does, and every change the panel makes is one undo step.

namespace {
	ClothComponent* FindClothComp(GameObject* go)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::Cloth) return static_cast<ClothComponent*>(comps[i].get());
		return NULL;
	}
	json* ClothJsonIn(json &tree)
	{
		json &comps = tree["root"]["components"];
		for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
			if (comps[i].value("type", std::string()) == "Cloth") return &comps[i];
		return NULL;
	}
	// The keys set_cloth takes - the scene file's own.
	const char* kClothKeys[] = { "pins", "stretch", "bend", "density", "damping", "thickness", "friction", "drag",
		"substeps", "turbulence", "gravity", "wind", "ground", "groundY", "colliders" };
	json V3(const Vec3 &v) { return json::array({ v.x, v.y, v.z }); }
	json ClothSettings(const ClothComponent* c)
	{
		json j;
		json pins = json::array();
		for (const std::pair<std::string, f32> &p : c->GetPinSides()) pins.push_back({ { "side", p.first }, { "band", p.second } });
		j["pins"] = pins;
		j["stretch"] = c->GetStretch(); j["bend"] = c->GetBend(); j["density"] = c->GetDensity(); j["damping"] = c->GetDamping();
		j["thickness"] = c->GetThickness(); j["friction"] = c->GetFriction(); j["drag"] = c->GetDrag();
		j["substeps"] = c->GetSubsteps(); j["turbulence"] = c->GetTurbulence();
		j["gravity"] = V3(c->GetGravity()); j["wind"] = V3(c->GetWind());
		j["ground"] = c->HasGround(); j["groundY"] = c->GetGroundY();
		static const char* kSpaces[] = { "world", "object", "parent" };
		json cols = json::array();
		for (const ClothComponent::Collider &co : c->GetColliders())
		{
			json cj = { { "radius", co.radius }, { "space", kSpaces[co.space < 3 ? co.space : 0] }, { "a", V3(co.a) } };
			if (co.capsule) cj["b"] = V3(co.b);
			cols.push_back(cj);
		}
		j["colliders"] = cols;
		return j;
	}
}

// get_cloth {name|id} / add_cloth {name|id, ...settings} / set_cloth {name|id, ...settings} /
// remove_cloth {name|id}. Settings are the scene file's keys: pins [{side, band}], stretch, bend,
// density, damping, thickness, friction, drag, substeps, turbulence, gravity [x,y,z],
// wind [x,y,z], ground, groundY, colliders [{a, b?, radius, space}]. In Play, set_cloth changes
// the running cloth (not the scene); otherwise it edits the scene, one undo step.
bool SceneEditor::AgentCloth(const std::string& command, const json& a, json& out, std::string& errOut)
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
	ClothComponent* comp = FindClothComp(go);

	json settings = json::object();
	for (const char* k : kClothKeys) if (args.contains(k)) settings[k] = args[k];

	if (command == "get_cloth")
	{
		if (!comp) { out["cloth"] = nullptr; return true; }
		out = ClothSettings(comp);
		// Running = Play started it; particles = what it built from the mesh (0 until it runs).
		out["running"] = comp->IsRunning();
		out["particles"] = comp->GetParticleCount();
		out["pinned"] = comp->GetPinnedCount();
		out["constraints"] = comp->GetConstraintCount();
		return true;
	}
	if (command == "set_cloth" && playMode)
	{
		// Live, for tuning while it plays: what changes is the running cloth, not the scene.
		if (!comp) { errOut = "it has no Cloth"; return false; }
		auto v3 = [](const json &j, const Vec3 &d) { return j.is_array() && j.size() >= 3 ? Vec3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>()) : d; };
		if (settings.contains("stretch")) comp->SetStretch(settings["stretch"].get<f32>());
		if (settings.contains("bend")) comp->SetBend(settings["bend"].get<f32>());
		if (settings.contains("density")) comp->SetDensity(settings["density"].get<f32>());
		if (settings.contains("damping")) comp->SetDamping(settings["damping"].get<f32>());
		if (settings.contains("thickness")) comp->SetThickness(settings["thickness"].get<f32>());
		if (settings.contains("friction")) comp->SetFriction(settings["friction"].get<f32>());
		if (settings.contains("drag")) comp->SetDrag(settings["drag"].get<f32>());
		if (settings.contains("substeps")) comp->SetSubsteps(settings["substeps"].get<uint32>());
		if (settings.contains("turbulence")) comp->SetTurbulence(settings["turbulence"].get<f32>());
		if (settings.contains("gravity")) comp->SetGravity(v3(settings["gravity"], comp->GetGravity()));
		if (settings.contains("wind")) comp->SetWind(v3(settings["wind"], comp->GetWind()));
		if (settings.contains("ground") || settings.contains("groundY"))
			comp->SetGround(settings.value("ground", comp->HasGround()), settings.value("groundY", comp->GetGroundY()));
		if (settings.contains("pins") || settings.contains("colliders")) { errOut = "pins and colliders change the cloth's make-up: stop play mode first"; return false; }
		return AgentCloth("get_cloth", json{ { "id", id } }, out, errOut);
	}
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (command == "add_cloth" || command == "set_cloth")
	{
		const bool add = command == "add_cloth";
		if (add && comp) { errOut = "it already has a Cloth"; return false; }
		if (!add && !comp) { errOut = "it has no Cloth (add_cloth)"; return false; }
		const bool ok = EditSubtreeJson(id, [settings, add](json &tree, std::string &err) {
			json* cj = ClothJsonIn(tree);
			if (add)
			{
				// (a new one hangs from its top edge: the commonest cloth, and visibly cloth at once)
				json c = { { "type", "Cloth" }, { "pins", json::array({ { { "side", "+y" }, { "band", 0.01 } } }) } };
				for (json::const_iterator k = settings.begin(); k != settings.end(); ++k) c[k.key()] = k.value();
				tree["root"]["components"].push_back(c);
				return true;
			}
			if (!cj) { err = "it has no Cloth"; return false; }
			for (json::const_iterator k = settings.begin(); k != settings.end(); ++k) (*cj)[k.key()] = k.value();
			return true;
		}, add ? "Add Cloth" : "Edit Cloth", errOut);
		if (!ok) return false;
		if (SceneObject* now = sceneObjects->GetSceneObject(id)) SelectSceneObject(now);
		return AgentCloth("get_cloth", json{ { "id", id } }, out, errOut);
	}
	if (command == "remove_cloth")
	{
		const bool ok = EditSubtreeJson(id, [](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
				if (comps[i].value("type", std::string()) == "Cloth") { comps.erase(comps.begin() + i); return true; }
			err = "it has no Cloth";
			return false;
		}, "Remove Cloth", errOut);
		if (!ok) return false;
		out["ok"] = true;
		return true;
	}
	errOut = "unknown cloth command " + command;
	return false;
}

void SceneEditor::DrawClothComponentProperties(GameObject* go, uint32 goId)
{
	ClothComponent* comp = FindClothComp(go);
	if (!comp) return;
	if (!ImGui::CollapsingHeader("Cloth##props_cloth", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("cloth_props");
	const json now = ClothSettings(comp);
	// One key changed: in Play straight onto the running cloth; otherwise as a scene edit once
	// the control is let go (one undo step a drag, not one a frame).
	auto commit = [&](const char* key, const json &value) {
		pendingFoliageOp = { { "cmd", "set_cloth" }, { "id", goId }, { key, value } };
	};
	auto slider = [&](const char* label, const char* key, float v, float lo, float hi, const char* fmt, const char* tip) {
		ImGui::SetNextItemWidth(-1.f);          // (the name is in the slider: a narrow dock still shows it)
		const bool changed = ImGui::SliderFloat(label, &v, lo, hi, fmt);
		if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
		if (playMode ? changed : ImGui::IsItemDeactivatedAfterEdit()) commit(key, v);
		// (while dragging in the editor, the number shown follows the drag)
		if (!playMode && changed) { if (!strcmp(key, "stretch")) comp->SetStretch(v); else if (!strcmp(key, "bend")) comp->SetBend(v);
			else if (!strcmp(key, "density")) comp->SetDensity(v); else if (!strcmp(key, "damping")) comp->SetDamping(v);
			else if (!strcmp(key, "turbulence")) comp->SetTurbulence(v); else if (!strcmp(key, "friction")) comp->SetFriction(v);
			else if (!strcmp(key, "drag")) comp->SetDrag(v); else if (!strcmp(key, "thickness")) comp->SetThickness(v); }
	};
	auto vec3 = [&](const char* label, const char* key, const Vec3 &cur, const char* tip) {
		float v[3] = { cur.x, cur.y, cur.z };
		ImGui::TextUnformatted(label);
		ImGui::SetNextItemWidth(-1.f);
		const bool changed = ImGui::DragFloat3((std::string("##") + key).c_str(), v, 0.05f);
		if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
		if (!playMode && changed) { if (!strcmp(key, "wind")) comp->SetWind(Vec3(v[0], v[1], v[2])); else comp->SetGravity(Vec3(v[0], v[1], v[2])); }
		if (playMode ? changed : ImGui::IsItemDeactivatedAfterEdit()) commit(key, json::array({ v[0], v[1], v[2] }));
	};

	if (playMode)
		ImGui::TextDisabled("%u particles, %u pinned, %u constraints", comp->GetParticleCount(), comp->GetPinnedCount(), comp->GetConstraintCount());
	else
		ImGui::TextDisabled("Simulates in Play. It uses this object's mesh: a Plane\nwith segments, or a model's (not a skinned one).");

	// Pins: the six sides as toggles (several at once: a banner held top and bottom).
	ImGui::TextUnformatted("Pinned sides");
	static const char* kSides[] = { "+y", "-y", "-x", "+x", "+z", "-z" };
	static const char* kSideNames[] = { "Top", "Bottom", "Left", "Right", "Front", "Back" };
	std::vector<std::pair<std::string, f32> > pins = comp->GetPinSides();
	f32 band = pins.empty() ? 0.01f : pins[0].second;
	const float column = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f;   // (two a row)
	for (int s = 0; s < 6; s++)
	{
		bool on = false;
		for (size_t p = 0; p < pins.size(); p++) if (pins[p].first == kSides[s]) on = true;
		if (s % 2) ImGui::SameLine(column);
		if (ImGui::Checkbox(kSideNames[s], &on) && !playMode)
		{
			json pj = json::array();
			for (size_t p = 0; p < pins.size(); p++) if (pins[p].first != kSides[s]) pj.push_back({ { "side", pins[p].first }, { "band", pins[p].second } });
			if (on) pj.push_back({ { "side", kSides[s] }, { "band", band } });
			commit("pins", pj);
		}
	}
	ImGui::TextUnformatted("Material");
	slider("##stretch", "stretch", comp->GetStretch(), 0.f, 1.f, "stretch %.2f", "How much it resists stretching: 1 not at all.");
	slider("##bend", "bend", comp->GetBend(), 0.f, 1.f, "bend %.2f", "How much it resists folding: low for silk, high for canvas.");
	slider("##density", "density", comp->GetDensity(), 0.02f, 3.f, "%.2f kg/m2", "Its weight a square metre: 0.2 a flag, 0.5 a curtain, 1.5 canvas.");
	slider("##damping", "damping", comp->GetDamping(), 0.f, 5.f, "damping %.2f", "How quickly it stops swinging.");
	vec3("Wind", "wind", comp->GetWind(), "Metres a second, in world space.");
	slider("##turbulence", "turbulence", comp->GetTurbulence(), 0.f, 2.f, "gusts %.2f", "How much the wind gusts and swirls.");
	slider("##drag", "drag", comp->GetDrag(), 0.f, 3.f, "air drag %.2f", "How hard the air pushes on it.");
	vec3("Gravity", "gravity", comp->GetGravity(), NULL);
	bool ground = comp->HasGround();
	if (ImGui::Checkbox("Ground", &ground)) { if (playMode) comp->SetGround(ground, comp->GetGroundY()); else commit("ground", ground); }
	ImGui::SameLine();
	float gy = comp->GetGroundY();
	ImGui::SetNextItemWidth(80.f);
	if (ImGui::DragFloat("height##groundy", &gy, 0.05f) && playMode) comp->SetGround(comp->HasGround(), gy);
	if (!playMode && ImGui::IsItemDeactivatedAfterEdit()) commit("groundY", gy);

	// Colliders: spheres and capsules it drapes over.
	if (ImGui::TreeNode("Colliders"))
	{
		json cols = now["colliders"];
		bool edited = false;
		static const char* kSpaceNames[] = { "world", "object", "parent" };
		for (size_t i = 0; i < cols.size(); i++)
		{
			ImGui::PushID((int)i);
			json &cj = cols[i];
			float a[3] = { cj["a"][0].get<float>(), cj["a"][1].get<float>(), cj["a"][2].get<float>() };
			float r = cj.value("radius", 0.1f);
			ImGui::SetNextItemWidth(180.f);
			if (ImGui::DragFloat3(cj.contains("b") ? "from" : "centre", a, 0.02f)) {}
			if (ImGui::IsItemDeactivatedAfterEdit()) { cj["a"] = json::array({ a[0], a[1], a[2] }); edited = true; }
			if (cj.contains("b"))
			{
				float b[3] = { cj["b"][0].get<float>(), cj["b"][1].get<float>(), cj["b"][2].get<float>() };
				ImGui::SetNextItemWidth(180.f);
				ImGui::DragFloat3("to", b, 0.02f);
				if (ImGui::IsItemDeactivatedAfterEdit()) { cj["b"] = json::array({ b[0], b[1], b[2] }); edited = true; }
			}
			ImGui::SetNextItemWidth(80.f);
			ImGui::DragFloat("radius", &r, 0.01f, 0.01f, 20.f);
			if (ImGui::IsItemDeactivatedAfterEdit()) { cj["radius"] = r; edited = true; }
			ImGui::SameLine();
			int sp = cj.value("space", std::string("world")) == "object" ? 1 : (cj.value("space", std::string("world")) == "parent" ? 2 : 0);
			ImGui::SetNextItemWidth(80.f);
			if (ImGui::Combo("##space", &sp, kSpaceNames, 3)) { cj["space"] = kSpaceNames[sp]; edited = true; }
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) { cols.erase(cols.begin() + i); edited = true; ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (ImGui::SmallButton("+ Sphere")) { cols.push_back({ { "a", json::array({ 0, 0, 0 }) }, { "radius", 0.5 }, { "space", "object" } }); edited = true; }
		ImGui::SameLine();
		if (ImGui::SmallButton("+ Capsule")) { cols.push_back({ { "a", json::array({ 0, 0, 0 }) }, { "b", json::array({ 0, 1, 0 }) }, { "radius", 0.2 }, { "space", "object" } }); edited = true; }
		if (edited && !playMode) commit("colliders", cols);
		if (playMode) ImGui::TextDisabled("Colliders are edited out of Play.");
		ImGui::TreePop();
	}
	if (!playMode && ImGui::SmallButton("Remove Cloth")) pendingFoliageOp = { { "cmd", "remove_cloth" }, { "id", goId } };
	ImGui::PopID();
}
