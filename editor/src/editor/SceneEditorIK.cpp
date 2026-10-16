//============================================================================
// Name        : SceneEditorIK.cpp
// Description : The runtime IK component (p3d::IKComponent) in the Scene
//               Editor: its section of Properties, adding and removing it, and
//               the agent commands that reach the same controls.
//
//               It was saved in scenes and reachable from Lua, but nothing in
//               the editor could add one or say what it does - a hand on a
//               prop or a foot on a slope had to be typed into the scene file.
//
//               Every edit goes through the component's own scene JSON
//               (EditSubtreeJson): the serializer is the one description of
//               an IK component, so what the panel writes is exactly what a
//               saved scene holds and what Ctrl+Z restores.
//============================================================================

#include "SceneEditor.h"
#include <Pyros3D/AnimationManager/Components/IKComponent.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

using namespace p3d;

namespace {

IKComponent* FindIK(GameObject* go)
{
	if (!go) return NULL;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == ComponentType::IK) return static_cast<IKComponent*>(comps[i].get());
	return NULL;
}

// The scene-relative model path the object renders, which is where the rig
// sidecar (<model>.rig.json) lives.
std::string ModelPathIn(const json &tree)
{
	if (tree.is_object())
	{
		if (tree.value("kind", std::string()) == "model" && tree.contains("path") && tree["path"].is_string())
			return tree["path"].get<std::string>();
		for (json::const_iterator it = tree.begin(); it != tree.end(); ++it)
		{
			if (it.key() == "children") continue;   // the object's own model, not a child's
			const std::string p = ModelPathIn(it.value());
			if (!p.empty()) return p;
		}
	}
	else if (tree.is_array())
		for (size_t i = 0; i < tree.size(); i++)
		{
			const std::string p = ModelPathIn(tree[i]);
			if (!p.empty()) return p;
		}
	return std::string();
}

json* IKJsonIn(json &tree)
{
	json &comps = tree["root"]["components"];
	if (!comps.is_array()) return NULL;
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i].value("type", std::string()) == "IK") return &comps[i];
	return NULL;
}

} // namespace

bool SceneEditor::AgentIK(const std::string& command, const json& a, json& out, std::string& errOut)
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

	// {"name"} -> {"ik": <component json> | null, "chains": [rig chain names]}
	if (command == "get_ik")
	{
		json tree;
		try { tree = json::parse(SnapshotSubtree(id)); } catch (...) { errOut = "could not read the object"; return false; }
		json* ik = IKJsonIn(tree);
		out["ik"] = ik ? *ik : json();
		json chains = json::array();
		if (IKComponent* c = FindIK(go))
			for (size_t i = 0; i < c->GetRig().IKChains.size(); i++) chains.push_back(c->GetRig().IKChains[i].Name);
		out["chains"] = chains;
		out["rigPath"] = ik ? ik->value("rig", std::string()) : ModelPathIn(tree["root"]);
		return true;
	}
	if (playMode) { errOut = "stop play mode first"; return false; }

	// {"name", "rig"?} - the rig defaults to the model the object renders.
	if (command == "add_ik")
	{
		if (FindIK(go)) { errOut = "it already has an IK component"; return false; }
		const std::string rigArg = args.value("rig", std::string());
		const bool ok = EditSubtreeJson(id, [rigArg](json &tree, std::string &err) {
			const std::string rig = rigArg.empty() ? ModelPathIn(tree["root"]) : rigArg;
			if (rig.empty()) { err = "the object renders no model - IK needs a rigged model (or pass \"rig\")"; return false; }
			json c;
			c["type"] = "IK";
			c["iterations"] = 10;
			c["rig"] = rig;
			c["constraints"] = json::array();
			tree["root"]["components"].push_back(c);
			return true;
		}, "Add IK", errOut);
		if (!ok) return false;
		if (SceneObject* now = sceneObjects->GetSceneObject(id)) SelectSceneObject(now);
		return AgentIK("get_ik", json{ { "id", id } }, out, errOut);
	}
	if (command == "remove_ik")
	{
		const bool ok = EditSubtreeJson(id, [](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
				if (comps[i].value("type", std::string()) == "IK") { comps.erase(comps.begin() + i); return true; }
			err = "it has no IK component";
			return false;
		}, "Remove IK", errOut);
		if (!ok) return false;
		if (SceneObject* now = sceneObjects->GetSceneObject(id)) SelectSceneObject(now);
		out["ok"] = true;
		return true;
	}
	// {"name", "iterations"?, "rig"?, "constraints"?: [{chain, target, pole,
	//  weight, enabled, matchRotation}]} - keys given replace; the rest stay.
	// Or {"name", "index", <one constraint's keys>} to patch one constraint.
	if (command == "set_ik")
	{
		const bool ok = EditSubtreeJson(id, [&args](json &tree, std::string &err) {
			json* ik = IKJsonIn(tree);
			if (!ik) { err = "it has no IK component (add_ik first)"; return false; }
			if (args.contains("iterations")) (*ik)["iterations"] = std::max(1, std::min(100, args.value("iterations", 10)));
			if (args.contains("rig")) (*ik)["rig"] = args.value("rig", std::string());
			if (args.contains("constraints"))
			{
				if (!args["constraints"].is_array()) { err = "constraints must be an array"; return false; }
				(*ik)["constraints"] = args["constraints"];
			}
			if (args.contains("index"))
			{
				json &cons = (*ik)["constraints"];
				const int i = args.value("index", -1);
				if (!cons.is_array() || i < 0 || i >= (int)cons.size()) { err = "no constraint at that index"; return false; }
				static const char* keys[] = { "chain", "target", "pole", "weight", "enabled", "matchRotation" };
				for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++)
					if (args.contains(keys[k]))
					{
						// An empty target/pole name clears it.
						if ((std::string(keys[k]) == "target" || std::string(keys[k]) == "pole")
							&& args[keys[k]].is_string() && args[keys[k]].get<std::string>().empty())
							cons[i].erase(keys[k]);
						else cons[i][keys[k]] = args[keys[k]];
					}
			}
			return true;
		}, "Edit IK", errOut);
		if (!ok) return false;
		return AgentIK("get_ik", json{ { "id", id } }, out, errOut);
	}
	errOut = "unknown IK command " + command;
	return false;
}

void SceneEditor::DrawIKComponentProperties(GameObject* go, uint32 goId)
{
	IKComponent* ik = FindIK(go);
	if (!ik) return;
	if (!ImGui::CollapsingHeader("Inverse Kinematics##props_ik", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("ik_props");

	// The component's own JSON is what is shown and what edits rewrite.
	json tree;
	try { tree = json::parse(SnapshotSubtree(goId)); } catch (...) { ImGui::PopID(); return; }
	json* cur = IKJsonIn(tree);
	if (!cur) { ImGui::PopID(); return; }
	json cons = cur->value("constraints", json::array());

	ImGui::TextDisabled("Rig: %s", cur->value("rig", std::string("(none)")).c_str());
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Chains and joint limits come from this model's rig sidecar (<model>.rig.json),\n"
			"authored in the Animation Editor's IK tab.");
	std::vector<std::string> chains;
	for (size_t i = 0; i < ik->GetRig().IKChains.size(); i++) chains.push_back(ik->GetRig().IKChains[i].Name);
	if (chains.empty())
		ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "The rig has no IK chains yet - add them in the Animation Editor.");

	// GameObjects a constraint can target, by name.
	std::vector<std::string> objects;
	for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); ++i)
		if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->first != goId) objects.push_back(i->second->GetName());
	std::sort(objects.begin(), objects.end());

	const auto queue = [this, goId](const json &patch) {
		json op = patch;
		op["cmd"] = "set_ik";
		op["id"] = goId;
		pendingFoliageOp = op;
	};

	int iterations = cur->value("iterations", 10);
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::InputInt("Iterations", &iterations, 1, 5, ImGuiInputTextFlags_EnterReturnsTrue))
		queue(json{ { "iterations", iterations } });
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("For chains longer than two bones (spines, tails). Two-bone limbs solve exactly.");

	static float weightDrag = -1.f;
	for (size_t i = 0; i < cons.size(); i++)
	{
		ImGui::PushID((int)i);
		ImGui::Separator();
		const json c = cons[i];
		const auto pick = [&](const char* label, const char* key, const std::vector<std::string>& choices, bool allowNone) {
			const std::string now = c.value(key, std::string());
			ImGui::SetNextItemWidth(200.f);
			if (ImGui::BeginCombo(label, now.empty() ? "(none)" : now.c_str()))
			{
				if (allowNone && ImGui::Selectable("(none)", now.empty())) queue(json{ { "index", (int)i }, { key, "" } });
				for (size_t k = 0; k < choices.size(); k++)
					if (ImGui::Selectable(choices[k].c_str(), choices[k] == now)) queue(json{ { "index", (int)i }, { key, choices[k] } });
				ImGui::EndCombo();
			}
		};
		pick("Chain", "chain", chains, false);
		pick("Target", "target", objects, true);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The object the chain's end reaches for (a prop, a foot marker).");
		pick("Pole", "pole", objects, true);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Which way the knee or elbow points. None: the rig's own bend.");

		bool enabled = c.value("enabled", true);
		if (ImGui::Checkbox("Enabled", &enabled)) queue(json{ { "index", (int)i }, { "enabled", enabled } });
		ImGui::SameLine();
		bool match = c.value("matchRotation", false);
		if (ImGui::Checkbox("Match target rotation", &match)) queue(json{ { "index", (int)i }, { "matchRotation", match } });
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Turn the end bone to the target's rotation too - a foot flat on the slope it stands on.");

		// Live while dragging (on the component itself), one undo step on release.
		float w = c.value("weight", 1.f);
		ImGui::SetNextItemWidth(200.f);
		if (ImGui::SliderFloat("Weight", &w, 0.f, 1.f, "%.2f") && i < ik->GetNumberConstraints())
		{
			ik->GetConstraint((uint32)i).Weight = w;
			weightDrag = w;
		}
		if (ImGui::IsItemDeactivatedAfterEdit() && weightDrag >= 0.f)
		{
			queue(json{ { "index", (int)i }, { "weight", weightDrag } });
			weightDrag = -1.f;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 leaves the animation alone, 1 reaches fully. Scripts fade it: setIKConstraintWeight.");

		if (ImGui::SmallButton("Remove constraint"))
		{
			json rest = cons;
			rest.erase(rest.begin() + i);
			queue(json{ { "constraints", rest } });
		}
		ImGui::PopID();
	}
	ImGui::Separator();
	if (ImGui::SmallButton("+ Constraint"))
	{
		json rest = cons;
		json c;
		c["chain"] = chains.empty() ? std::string() : chains[0];
		c["weight"] = 1.f;
		c["enabled"] = true;
		rest.push_back(c);
		queue(json{ { "constraints", rest } });
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Remove IK"))
		pendingFoliageOp = { { "cmd", "remove_ik" }, { "id", goId } };
	ImGui::PopID();
}

// ---- the Animator component (state machine) ----------------------------------

#include <Pyros3D/AnimationManager/Components/AnimatorComponent.h>
#include <filesystem>

namespace {

AnimatorComponent* FindAnimatorComp(GameObject* go)
{
	if (!go) return NULL;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == ComponentType::Animator) return static_cast<AnimatorComponent*>(comps[i].get());
	return NULL;
}

bool SameFile(const std::string& a, const std::string& b)
{
	if (a == b) return true;
	std::error_code ec;
	return !a.empty() && !b.empty() && std::filesystem::equivalent(a, b, ec);
}

json* AnimatorJsonIn(json &tree)
{
	json &comps = tree["root"]["components"];
	if (!comps.is_array()) return NULL;
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i].value("type", std::string()) == "Animator") return &comps[i];
	return NULL;
}

} // namespace

Animator* SceneEditor::FindLiveAnimator(const std::string& controllerAbs)
{
	if (!scene) return NULL;
	// The selected object first - the one being looked at - then any.
	if (SelectedSceneObject && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
		if (AnimatorComponent* a = FindAnimatorComp((GameObject*)SelectedSceneObject->GetPTR()))
			if (a->GetAnimator().IsBound() && SameFile(a->GetControllerPath(), controllerAbs)) return &a->GetAnimator();
	std::vector<std::shared_ptr<GameObject> > &all = scene->GetAllGameObjectList();
	for (size_t i = 0; i < all.size(); i++)
		if (AnimatorComponent* a = FindAnimatorComp(all[i].get()))
			if (a->GetAnimator().IsBound() && SameFile(a->GetControllerPath(), controllerAbs)) return &a->GetAnimator();
	return NULL;
}

int SceneEditor::ReloadAnimatorsUsing(const std::string& controllerAbs, const AnimatorController& controller)
{
	if (!scene) return 0;
	int n = 0;
	std::vector<std::shared_ptr<GameObject> > &all = scene->GetAllGameObjectList();
	for (size_t i = 0; i < all.size(); i++)
		if (AnimatorComponent* a = FindAnimatorComp(all[i].get()))
			if (SameFile(a->GetControllerPath(), controllerAbs)) { a->SetController(controller); n++; }
	return n;
}

// get_animator {name} / add_animator {name, controller} / set_animator {name, controller}
// / remove_animator {name} / animator_param {name, param, value|true|"trigger"} (live, in Play)
bool SceneEditor::AgentAnimator(const std::string& command, const json& a, json& out, std::string& errOut)
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
	AnimatorComponent* comp = FindAnimatorComp(go);

	if (command == "get_animator")
	{
		if (!comp) { out["animator"] = nullptr; return true; }
		out["controller"] = comp->GetControllerPath();
		out["state"] = comp->GetAnimator().GetState();
		out["progress"] = comp->GetAnimator().GetStateProgress();
		json vals = json::object();
		const AnimatorController& c = comp->GetController();
		for (size_t i = 0; i < c.params.size(); i++) vals[c.params[i].name] = comp->GetAnimator().GetFloat(c.params[i].name);
		out["params"] = vals;
		return true;
	}
	if (command == "animator_param")
	{
		if (!comp) { errOut = "it has no Animator"; return false; }
		const std::string p = args.value("param", std::string());
		bool ok = false;
		if (args.contains("value") && args["value"].is_string() && args["value"].get<std::string>() == "trigger") ok = comp->GetAnimator().SetTrigger(p);
		else if (args.contains("value") && args["value"].is_boolean()) ok = comp->GetAnimator().SetBool(p, args["value"].get<bool>());
		else ok = comp->GetAnimator().SetFloat(p, args.value("value", 0.f));
		if (!ok) { errOut = "no parameter '" + p + "' (or the animator is not running)"; return false; }
		return AgentAnimator("get_animator", json{ { "id", id } }, out, errOut);
	}
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (command == "add_animator" || command == "set_animator")
	{
		const std::string ctl = args.value("controller", std::string());
		if (command == "add_animator" && comp) { errOut = "it already has an Animator"; return false; }
		if (command == "set_animator" && !comp) { errOut = "it has no Animator (add_animator)"; return false; }
		const bool add = command == "add_animator";
		const bool ok = EditSubtreeJson(id, [ctl, add](json &tree, std::string &err) {
			json* an = AnimatorJsonIn(tree);
			if (add)
			{
				json c; c["type"] = "Animator";
				if (!ctl.empty()) c["controller"] = ctl;
				tree["root"]["components"].push_back(c);
				return true;
			}
			if (!an) { err = "it has no Animator"; return false; }
			if (ctl.empty()) an->erase("controller"); else (*an)["controller"] = ctl;
			return true;
		}, add ? "Add Animator" : "Set Animator controller", errOut);
		if (!ok) return false;
		if (SceneObject* now = sceneObjects->GetSceneObject(id)) SelectSceneObject(now);
		return AgentAnimator("get_animator", json{ { "id", id } }, out, errOut);
	}
	if (command == "remove_animator")
	{
		const bool ok = EditSubtreeJson(id, [](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
				if (comps[i].value("type", std::string()) == "Animator") { comps.erase(comps.begin() + i); return true; }
			err = "it has no Animator";
			return false;
		}, "Remove Animator", errOut);
		if (!ok) return false;
		out["ok"] = true;
		return true;
	}
	errOut = "unknown animator command " + command;
	return false;
}

void SceneEditor::DrawAnimatorComponentProperties(GameObject* go, uint32 goId)
{
	AnimatorComponent* comp = FindAnimatorComp(go);
	if (!comp) return;
	if (!ImGui::CollapsingHeader("Animator##props_animator", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("animator_props");

	// The controller: any .p3dac in the project.
	std::vector<std::string> files;
	if (project && project->IsOpen())
	{
		std::vector<ProjectAssetEntry> entries;
		project->ListAssets("assets", entries, true);
		for (size_t e = 0; e < entries.size(); e++)
			if (!entries[e].isDirectory && ProjectManager::IsAnimatorExtension(entries[e].relativePath)) files.push_back(entries[e].relativePath);
	}
	std::string shown = comp->GetControllerPath().empty() ? std::string("(none)") : comp->GetControllerPath();
	if (project && !comp->GetControllerPath().empty()) shown = project->DisplayPath(comp->GetControllerPath());
	ImGui::SetNextItemWidth(-1.f);
	if (!playMode && ImGui::BeginCombo("##controller", shown.c_str()))
	{
		for (size_t i = 0; i < files.size(); i++)
			if (ImGui::Selectable(files[i].c_str()))
				pendingFoliageOp = { { "cmd", "set_animator" }, { "id", goId }, { "controller", files[i] } };
		if (files.empty()) ImGui::TextDisabled("No .p3dac in the project - make one in Assets.");
		ImGui::EndCombo();
	}
	else if (playMode) ImGui::TextDisabled("%s", shown.c_str());

	Animator& an = comp->GetAnimator();
	if (an.IsBound())
	{
		ImGui::Text("State: %s", an.GetState().empty() ? "-" : an.GetState().c_str());
		ImGui::ProgressBar(an.GetStateProgress(), ImVec2(-1.f, 4.f), "");
		const AnimatorController& c = comp->GetController();
		for (size_t i = 0; i < c.params.size(); i++)
		{
			ImGui::PushID((int)i);
			const AnimatorParam& p = c.params[i];
			if (p.type == ANIMATOR_FLOAT)
			{
				float v = an.GetFloat(p.name);
				ImGui::SetNextItemWidth(120.f);
				if (ImGui::DragFloat(p.name.c_str(), &v, 0.05f)) an.SetFloat(p.name, v);
			}
			else if (p.type == ANIMATOR_BOOL)
			{
				bool v = an.GetBool(p.name);
				if (ImGui::Checkbox(p.name.c_str(), &v)) an.SetBool(p.name, v);
			}
			else if (ImGui::SmallButton(p.name.c_str())) an.SetTrigger(p.name);
			ImGui::PopID();
		}
		if (!playMode) ImGui::TextDisabled("Parameters drive it live in Play; scripts set them with setAnimatorFloat(...).");
	}
	else if (!comp->HasController()) ImGui::TextDisabled("Pick a controller.");
	else ImGui::TextDisabled("Runs once the object has a skinned model or a 2D character.");
	if (!comp->GetControllerPath().empty() && ImGui::SmallButton("Edit State Machine"))
		pendingOpenAnimator = comp->GetControllerPath();
	if (!playMode)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Remove Animator")) pendingFoliageOp = { { "cmd", "remove_animator" }, { "id", goId } };
	}
	ImGui::PopID();
}
