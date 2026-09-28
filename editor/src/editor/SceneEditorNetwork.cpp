// Networking in the editor: Play as a host or a client, the `network`
// table scripts see during Play, a NetworkIdentity's inspector, and the
// agent commands for both.

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Network/NetworkIdentity.h>
#include <Pyros3D/Network/NetworkSession.h>
#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosLuaNetwork.h>
#endif

#include <algorithm>

namespace {
	const char* kRoleNames[3] = { "offline", "host", "client" };

	NetworkIdentity* FindIdentity(GameObject* go)
	{
		if (!go) return NULL;
		for (size_t c = 0; c < go->GetComponents().size(); c++)
			if (NetworkIdentity* ni = dynamic_cast<NetworkIdentity*>(go->GetComponents()[c].get())) return ni;
		return NULL;
	}

	// The authored half of an identity - what the inspector edits and undo
	// swaps.
	struct IdentitySettings
	{
		std::string prefab;
		f32 relevance = 0.f, priority = 1.f, hitRadius = 0.4f, hitHeight = 1.8f;
		bool syncTransform = true, predicted = false;
		static IdentitySettings Of(const NetworkIdentity* ni)
		{
			IdentitySettings s;
			s.prefab = ni->prefab; s.relevance = ni->relevance; s.priority = ni->priority;
			s.hitRadius = ni->hitRadius; s.hitHeight = ni->hitHeight;
			s.syncTransform = ni->syncTransform; s.predicted = ni->predicted;
			return s;
		}
		void ApplyTo(NetworkIdentity* ni) const
		{
			ni->prefab = prefab; ni->relevance = relevance; ni->priority = priority;
			ni->hitRadius = hitRadius; ni->hitHeight = hitHeight;
			ni->syncTransform = syncTransform; ni->predicted = predicted;
		}
		bool operator!=(const IdentitySettings &o) const
		{
			return prefab != o.prefab || relevance != o.relevance || priority != o.priority || hitRadius != o.hitRadius
				|| hitHeight != o.hitHeight || syncTransform != o.syncTransform || predicted != o.predicted;
		}
		json Json() const
		{
			return { { "prefab", prefab }, { "relevance", relevance }, { "priority", priority }, { "syncTransform", syncTransform },
				{ "predicted", predicted }, { "hitRadius", hitRadius }, { "hitHeight", hitHeight } };
		}
		void Patch(const json &j)
		{
			prefab = j.value("prefab", prefab);
			relevance = std::max(0.f, j.value("relevance", relevance));
			priority = std::max(0.f, j.value("priority", priority));
			syncTransform = j.value("syncTransform", syncTransform);
			predicted = j.value("predicted", predicted);
			hitRadius = std::max(0.f, j.value("hitRadius", hitRadius));
			hitHeight = std::max(0.f, j.value("hitHeight", hitHeight));
		}
	};
}

bool SceneEditor::EditSubtreeJson(uint32 id, const std::function<bool(json&, std::string&)>& edit,
	const std::string& description, std::string& errOut)
{
	SceneObject* target = sceneObjects->GetSceneObject(id);
	if (!target || target->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "no such object"; return false; }
	const std::string before = SnapshotSubtree(id);
	json tree;
	try { tree = json::parse(before); }
	catch (const std::exception &e) { errOut = e.what(); return false; }
	if (!edit(tree, errOut)) return false;
	const bool wasCamera = IsSceneCamera(id);
	std::unique_ptr<ReplaceGameObjectCommand> cmd(new ReplaceGameObjectCommand(this, target->GetParentID(), before, tree.dump(),
		wasCamera, wasCamera ? sceneCameras[id] : EditorCameraSettings(), target->Helper != nullptr, id, description));
	cmd->Redo();
	sceneUndo.Push(std::move(cmd));
	MarkSceneDirty();
	return true;
}

void SceneEditor::RegisterPlayNetwork()
{
#ifdef LUA_BINDINGS
	if (!sharedLua) return;
	// Scripts see `network` whatever the role, so a game that hosts or joins
	// from its own menu works in Play as it does built. The session is made
	// on first use, for the scene being played.
	SceneEditor* self = this;
	RegisterLuaNetwork(sharedLua, [self]() -> NetworkSession* {
		if (!self->playNetwork && self->playMode)
			self->playNetwork.reset(new NetworkSession(self->scene, self->scenePath, self->physics, self->sharedLua));
		return self->playNetwork.get();
	});
	(*sharedLua)["NETWORK_ROLE"] = kRoleNames[std::max(0, std::min(2, playNetRole))];
#endif
}

void SceneEditor::UpdatePlayNetwork(const f64 time)
{
	if (!playMode) return;
	const f64 dt = playNetLastTime < 0.0 ? 0.0 : std::min(0.25, std::max(0.0, time - playNetLastTime));
	playNetLastTime = time;
	// The Play-as role, once the scene's scripts have had their first frame:
	// a script that hosts or joins itself decides, as on PyrosServer.
	if (!playNetStarted && playNetRole != 0)
	{
		playNetStarted = true;
		if (!playNetwork) playNetwork.reset(new NetworkSession(scene, scenePath, physics, sharedLua));
		if (playNetwork->GetRole() == NetworkSession::Offline)
		{
			const uint16 port = (uint16)std::max(1, std::min(65535, playNetPort));
			const bool ok = playNetRole == 1 ? playNetwork->Host(port) : playNetwork->Connect(playNetAddress, port);
			if (ok) echo(std::string("SUCCESS: Play as ") + (playNetRole == 1 ? "host on port " : "client of " + playNetAddress + ":") + std::to_string(port));
			else echo(std::string("ERROR: could not ") + (playNetRole == 1 ? "host on port " : "connect to " + playNetAddress + ":") + std::to_string(port));
		}
	}
	if (!playNetwork) return;
	// Relevance is measured from whatever the viewport looks through.
	if (GameObject* viewCam = GetViewCameraGO()) playNetwork->SetViewer(viewCam->GetWorldPosition());
	playNetwork->Update(dt);
}

void SceneEditor::StopPlayNetwork()
{
	// Before the play session's objects go: the session holds the
	// replicated ones, and a client's replicas are play-spawned objects.
	if (playNetwork)
	{
		playNetwork->Shutdown();
		playNetwork.reset();
	}
	playNetStarted = false;
	playNetLastTime = -1.0;
}

void SceneEditor::DrawNetworkPlayControls()
{
	ImGui::SameLine();
	if (playMode)
	{
		if (!playNetwork || playNetwork->GetRole() == NetworkSession::Offline) return;
		const bool server = playNetwork->GetRole() == NetworkSession::Server;
		const NetworkSession::Stats &st = playNetwork->GetStats();
		if (server)
			ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.f, 1.f), "HOST %u peers, %u objects", (unsigned)playNetwork->ClientViewers().size(), (unsigned)st.replicated);
		else if (playNetwork->IsReady())
			ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.f, 1.f), "CLIENT #%u, %u objects", (unsigned)playNetwork->LocalPeer(), (unsigned)st.replicated);
		else
			ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "CLIENT connecting...");
		return;
	}
	static const char* labels[3] = { "Offline", "Host", "Client" };
	const std::string button = std::string("Net: ") + labels[std::max(0, std::min(2, playNetRole))] + "###netrole";
	if (ImGui::SmallButton(button.c_str())) ImGui::OpenPopup("play_network");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How Play joins a network session.\nOffline: only if a script hosts or joins.\n"
			"Host: this editor is the server. Client: join a host\n(another editor in Host, a built game, or PyrosServer).");
	if (ImGui::BeginPopup("play_network"))
	{
		for (int i = 0; i < 3; i++)
			if (ImGui::RadioButton(labels[i], playNetRole == i)) playNetRole = i;
		if (playNetRole == 2)
		{
			ImGui::SetNextItemWidth(140.f);
			ImGui::InputText("Address", &playNetAddress);
		}
		if (playNetRole != 0)
		{
			ImGui::SetNextItemWidth(140.f);
			ImGui::InputInt("Port", &playNetPort);
			playNetPort = std::max(1, std::min(65535, playNetPort));
		}
		ImGui::EndPopup();
	}
}

void SceneEditor::DrawNetworkIdentityProperties(GameObject* go, uint32 goId)
{
	NetworkIdentity* ni = FindIdentity(go);
	if (!ni) return;
	if (!ImGui::CollapsingHeader("Network Identity##props_netid", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("netid_props");
	const IdentitySettings before = IdentitySettings::Of(ni);
	static IdentitySettings baseline;
	static bool editing = false;
	auto commit = [this, goId](const IdentitySettings &was, const IdentitySettings &now) {
		if (!(was != now)) return;
		auto apply = [this, goId](const IdentitySettings &s) {
			SceneObject* so = sceneObjects->GetSceneObject(goId);
			if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT) return;
			if (NetworkIdentity* id = FindIdentity((GameObject*)so->GetPTR())) { s.ApplyTo(id); MarkSceneDirty(); }
		};
		sceneUndo.Push(std::make_unique<ApplyClosureCommand>([apply, was]() { apply(was); }, [apply, now]() { apply(now); }, "Edit Network Identity"));
		MarkSceneDirty();
	};
	auto track = [&]() {
		if (ImGui::IsItemActivated()) { baseline = before; editing = true; }
		if (ImGui::IsItemDeactivatedAfterEdit() && editing) { commit(baseline, IdentitySettings::Of(ni)); editing = false; }
	};
	ImGui::InputTextWithHint("Prefab", "empty: bound by its scene path", &ni->prefab); track();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("What a client builds to spawn it. Empty: the object is in the scene\n"
			"on both ends already (a door, a pickup) and is found by its path.");
	ImGui::DragFloat("Relevance", &ni->relevance, 1.f, 0.f, 100000.f, ni->relevance > 0.f ? "%.0f m" : "session default"); track();
	ImGui::DragFloat("Priority", &ni->priority, 0.05f, 0.f, 100.f, "%.2f"); track();
	bool sync = ni->syncTransform;
	if (ImGui::Checkbox("Sync transform", &sync)) { ni->syncTransform = sync; commit(before, IdentitySettings::Of(ni)); }
	bool predicted = ni->predicted;
	if (ImGui::Checkbox("Predicted", &predicted)) { ni->predicted = predicted; commit(before, IdentitySettings::Of(ni)); }
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Moved by the session's simulate function from its owner's input;\n"
			"the owner runs ahead of the server and reconciles.");
	ImGui::DragFloat("Hit radius", &ni->hitRadius, 0.01f, 0.f, 50.f, "%.2f m"); track();
	ImGui::DragFloat("Hit height", &ni->hitHeight, 0.01f, 0.f, 50.f, "%.2f m"); track();
	if (playMode && ni->GetNetId() != 0)
		ImGui::TextDisabled("net id %u, owner peer %u", (unsigned)ni->GetNetId(), (unsigned)ni->GetOwnerPeer());
	if (!playMode && ImGui::SmallButton("Remove Network Identity"))
		pendingFoliageOp = { { "cmd", "remove_network_identity" }, { "id", goId } };
	ImGui::PopID();
}

bool SceneEditor::AgentNetwork(const std::string& command, const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	// {"role":"offline|host|client","address","port"} - how the next Play joins.
	if (command == "set_play_network")
	{
		if (args.contains("role"))
		{
			const std::string role = args.value("role", std::string());
			int r = -1;
			for (int i = 0; i < 3; i++) if (role == kRoleNames[i]) r = i;
			if (r < 0) { errOut = "role must be offline, host or client"; return false; }
			playNetRole = r;
		}
		playNetAddress = args.value("address", playNetAddress);
		playNetPort = std::max(1, std::min(65535, args.value("port", playNetPort)));
		return AgentNetwork("network_state", args, out, errOut);
	}
	if (command == "network_state")
	{
		out["playRole"] = kRoleNames[std::max(0, std::min(2, playNetRole))];
		out["address"] = playNetAddress;
		out["port"] = playNetPort;
		if (playNetwork)
		{
			const NetworkSession::Role r = playNetwork->GetRole();
			out["role"] = r == NetworkSession::Server ? "server" : (r == NetworkSession::Client ? "client" : "offline");
			out["ready"] = playNetwork->IsReady();
			out["localPeer"] = playNetwork->LocalPeer();
			out["peers"] = (uint32)playNetwork->ClientViewers().size();
			out["replicated"] = playNetwork->GetStats().replicated;
			out["serverTick"] = playNetwork->ServerTick();
		}
		else out["role"] = "none";
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

	if (command == "get_network_identity")
	{
		NetworkIdentity* ni = FindIdentity(go);
		if (!ni) { out["identity"] = nullptr; return true; }
		out["identity"] = IdentitySettings::Of(ni).Json();
		out["netId"] = ni->GetNetId();
		out["ownerPeer"] = ni->GetOwnerPeer();
		return true;
	}
	if (command == "add_network_identity" || command == "remove_network_identity")
	{
		if (playMode) { errOut = "stop play mode first"; return false; }
		const bool add = command == "add_network_identity";
		if (add == (FindIdentity(go) != NULL)) { errOut = add ? "it already has one" : "it has none"; return false; }
		IdentitySettings settings;
		settings.Patch(args);
		const bool ok = EditSubtreeJson(id, [add, &settings](json &tree, std::string &err) {
			json &comps = tree["root"]["components"];
			if (!comps.is_array()) comps = json::array();
			if (add)
			{
				json c = settings.Json();
				c["type"] = "NetworkIdentity";
				comps.push_back(c);
				return true;
			}
			for (size_t i = 0; i < comps.size(); i++)
				if (comps[i].value("type", std::string()) == "NetworkIdentity") { comps.erase(comps.begin() + i); return true; }
			err = "it has none";
			return false;
		}, add ? "Add Network Identity" : "Remove Network Identity", errOut);
		if (!ok) return false;
		SceneObject* now = sceneObjects->GetSceneObject(id);
		if (now) SelectSceneObject(now);
		out["ok"] = true;
		return true;
	}
	if (command == "set_network_identity")
	{
		NetworkIdentity* ni = FindIdentity(go);
		if (!ni) { errOut = "it has no Network Identity"; return false; }
		const IdentitySettings was = IdentitySettings::Of(ni);
		IdentitySettings now = was;
		now.Patch(args);
		now.ApplyTo(ni);
		auto apply = [this, id](const IdentitySettings &s) {
			SceneObject* so = sceneObjects->GetSceneObject(id);
			if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT) return;
			if (NetworkIdentity* x = FindIdentity((GameObject*)so->GetPTR())) { s.ApplyTo(x); MarkSceneDirty(); }
		};
		sceneUndo.Push(std::make_unique<ApplyClosureCommand>([apply, was]() { apply(was); }, [apply, now]() { apply(now); }, "Edit Network Identity"));
		MarkSceneDirty();
		out["identity"] = now.Json();
		return true;
	}
	errOut = "unknown network command " + command;
	return false;
}
