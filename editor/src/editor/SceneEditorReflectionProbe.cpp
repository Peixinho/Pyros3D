#include "SceneEditor.h"
#include <Pyros3D/Rendering/GI/ReflectionProbes.h>
#include "imgui.h"

// The Reflection Probe component in the editor: socket commands (get_reflection_probe /
// add_reflection_probe / set_reflection_probe / remove_reflection_probe / capture_reflection_probes)
// and the inspector section, which goes through the same commands - so an agent can do whatever
// the panel does, and every change the panel makes is one undo step.

namespace {
	ReflectionProbe* FindProbe(GameObject* go)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::ReflectionProbe) return static_cast<ReflectionProbe*>(comps[i].get());
		return NULL;
	}
	json* ProbeJsonIn(json &tree)
	{
		json &comps = tree["root"]["components"];
		for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
			if (comps[i].value("type", std::string()) == "ReflectionProbe") return &comps[i];
		return NULL;
	}
	const char* kProbeKeys[] = { "extents", "blend" };
	json ProbeSettings(const ReflectionProbe* p)
	{
		const Vec3 e = p->GetExtents();
		return json{ { "extents", json::array({ e.x, e.y, e.z }) }, { "blend", p->GetBlend() } };
	}
}

// get_reflection_probe {name|id} / add_reflection_probe {name|id, extents?, blend?} /
// set_reflection_probe {name|id, extents?, blend?} / remove_reflection_probe {name|id} /
// capture_reflection_probes {} (every probe of the scene seen again). extents is half the box's
// size on each axis, metres, world-aligned around the object; blend is how far inside the box's
// faces it fades out. An edit sees the probe again; so does moving its object (outside Play).
bool SceneEditor::AgentReflectionProbe(const std::string& command, const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	if (command == "capture_reflection_probes")
	{
		ReflectionProbes::CaptureAll(scene);
		out["probes"] = ReflectionProbes::Count(scene);
		out["pending"] = viewRenderer ? viewRenderer->ProbesPending(scene) : ReflectionProbes::Count(scene);
		return true;
	}
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
	ReflectionProbe* probe = FindProbe(go);

	json settings = json::object();
	for (const char* k : kProbeKeys) if (args.contains(k)) settings[k] = args[k];
	if (settings.contains("extents") && (!settings["extents"].is_array() || settings["extents"].size() != 3)) { errOut = "extents is [x, y, z]"; return false; }

	if (command == "get_reflection_probe")
	{
		if (!probe) { out["reflectionProbe"] = nullptr; return true; }
		out = ProbeSettings(probe);
		out["captured"] = viewRenderer && viewRenderer->IsProbeCaptured(probe);
		return true;
	}
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (command == "add_reflection_probe" || command == "set_reflection_probe")
	{
		const bool add = command == "add_reflection_probe";
		if (add && probe) { errOut = "it already has a Reflection Probe"; return false; }
		if (!add && !probe) { errOut = "it has no Reflection Probe (add_reflection_probe)"; return false; }
		const bool ok = EditSubtreeJson(id, [settings, add](json &tree, std::string &err) {
			json* pj = ProbeJsonIn(tree);
			if (add)
			{
				json c = { { "type", "ReflectionProbe" }, { "extents", json::array({ 5.0, 3.0, 5.0 }) }, { "blend", 1.0 } };
				for (json::const_iterator k = settings.begin(); k != settings.end(); ++k) c[k.key()] = k.value();
				tree["root"]["components"].push_back(c);
				return true;
			}
			if (!pj) { err = "it has no Reflection Probe"; return false; }
			for (json::const_iterator k = settings.begin(); k != settings.end(); ++k) (*pj)[k.key()] = k.value();
			return true;
		}, add ? "Add Reflection Probe" : "Edit Reflection Probe", errOut);
		if (!ok) return false;
		if (SceneObject* now = sceneObjects->GetSceneObject(id)) SelectSceneObject(now);
		return AgentReflectionProbe("get_reflection_probe", json{ { "id", id } }, out, errOut);
	}
	if (command == "remove_reflection_probe")
	{
		const bool ok = EditSubtreeJson(id, [](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			for (size_t i = 0; comps.is_array() && i < comps.size(); i++)
				if (comps[i].value("type", std::string()) == "ReflectionProbe") { comps.erase(comps.begin() + i); return true; }
			err = "it has no Reflection Probe";
			return false;
		}, "Remove Reflection Probe", errOut);
		if (!ok) return false;
		out["ok"] = true;
		return true;
	}
	errOut = "unknown reflection probe command " + command;
	return false;
}

void SceneEditor::DrawReflectionProbeProperties(GameObject* go, uint32 goId)
{
	ReflectionProbe* probe = FindProbe(go);
	if (!probe) return;
	if (!ImGui::CollapsingHeader("Reflection Probe##props_rprobe", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("rprobe_props");
	Vec3 e = probe->GetExtents();
	float ext[3] = { e.x, e.y, e.z };
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::DragFloat3("##extents", ext, 0.05f, 0.05f, 500.f, "%.2f m"))
		probe->SetExtents(Vec3(ext[0], ext[1], ext[2]));      // (shown as it is dragged)
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Half the box's size on each axis, world-aligned around the object: the room\nthe probe stands for. Reflections are looked up where the reflected ray leaves it.");
	if (ImGui::IsItemDeactivatedAfterEdit())
		pendingFoliageOp = { { "cmd", "set_reflection_probe" }, { "id", goId }, { "extents", json::array({ ext[0], ext[1], ext[2] }) } };
	float blend = probe->GetBlend();
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::SliderFloat("##blend", &blend, 0.f, 5.f, "fade %.2f m")) probe->SetBlend(blend);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far inside the box's faces it fades out, to the next probe or the sky.");
	if (ImGui::IsItemDeactivatedAfterEdit())
		pendingFoliageOp = { { "cmd", "set_reflection_probe" }, { "id", goId }, { "blend", blend } };
	ImGui::TextDisabled("%s", viewRenderer && viewRenderer->IsProbeCaptured(probe) ? "captured" : "waiting to be captured");
	ImGui::SameLine();
	if (ImGui::SmallButton("Capture again")) probe->Capture();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("See the room again - after lights or objects in it changed.\n(capture_reflection_probes sees every probe again.)");
	ImGui::PopID();
}
