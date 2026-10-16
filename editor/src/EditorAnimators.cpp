//============================================================================
// Name        : EditorAnimators.cpp
// Description : The Editor's side of animation state machines (.p3dac):
//               opening, saving, the document windows, and the agent commands
//               that edit a controller the way the window does.
//============================================================================

#include "Editor.h"
#include "editor/UI/AnimatorEditor.h"
#include <Pyros3D/AnimationManager/AnimatorController.h>
#include <filesystem>

using namespace p3d;

AnimatorDocument* Editor::FindAnimatorDocumentByPath(const std::string& absPath)
{
	for (size_t i = 0; i < animatorDocs.size(); ++i)
		if (animatorDocs[i] && animatorDocs[i]->absolutePath == absPath) return animatorDocs[i];
	return nullptr;
}

bool Editor::OpenAnimatorDocument(const std::string& absPath)
{
	if (absPath.empty()) return false;
	if (AnimatorDocument* existing = FindAnimatorDocumentByPath(absPath))
	{
		pendingSelectAnimatorDocId = existing->id;
		activeAnimatorDoc = existing;
		lastFocusedDocKind = FocusedDocKind::Animator;
		return true;
	}
	AnimatorDocument* doc = new AnimatorDocument();
	doc->id = nextAnimatorDocId++;
	std::string err;
	if (!doc->LoadFromFile(absPath, err))
	{
		echo("ERROR: " + err);
		delete doc;
		return false;
	}
	// Clips offered for its states: a .p3da beside it with the same stem, or
	// the first one any state's clip is found in - a guess the picker corrects.
	const std::filesystem::path stem = std::filesystem::path(absPath).replace_extension(".p3da");
	if (std::filesystem::exists(stem)) doc->LoadClipNames(stem.string());
	doc->undo.onPush = [this, doc]() {
		lastFocusedDocKind = FocusedDocKind::Animator;
		activeAnimatorDoc = doc;
	};
	animatorDocs.push_back(doc);
	pendingSelectAnimatorDocId = doc->id;
	activeAnimatorDoc = doc;
	lastFocusedDocKind = FocusedDocKind::Animator;
	echo("Opened state machine " + project.DisplayPath(absPath));
	return true;
}

bool Editor::SaveAnimatorDocument(AnimatorDocument* doc)
{
	if (!doc) return false;
	std::string err;
	if (!doc->Save(err)) { echo("ERROR: " + err); return false; }
	// Characters using it - in the editor or playing - take it now.
	int updated = 0;
	for (size_t i = 0; i < sceneDocs.size(); i++)
		if (sceneDocs[i]) updated += sceneDocs[i]->ReloadAnimatorsUsing(doc->absolutePath, doc->controller);
	echo("SUCCESS: Saved " + project.DisplayPath(doc->absolutePath)
		+ (updated ? " (" + std::to_string(updated) + " animator" + (updated == 1 ? "" : "s") + " updated)" : std::string()));
	return true;
}

void Editor::DrawAnimatorEditorWindows()
{
	// The Scene View's Animator section asked for its controller's window.
	if (sceneView && !sceneView->pendingOpenAnimator.empty())
	{
		OpenAnimatorDocument(sceneView->pendingOpenAnimator);
		sceneView->pendingOpenAnimator.clear();
	}
	if (animatorDocs.empty()) return;
	if (ImGuiWindow* sv = ImGui::FindWindowByName("Scene View"))
		if (sv->DockId != 0) dockCenterId = sv->DockId;

	// What a state's clip can be picked from.
	std::vector<std::pair<std::string, std::string> > clipFiles;
	if (project.IsOpen())
	{
		std::vector<ProjectAssetEntry> entries;
		project.ListAssets("assets", entries, true);
		for (size_t e = 0; e < entries.size(); e++)
			if (!entries[e].isDirectory && (ProjectManager::IsAnimationExtension(entries[e].relativePath)
				|| ProjectManager::IsCharacter2DExtension(entries[e].relativePath)))
				clipFiles.push_back(std::make_pair(entries[e].relativePath, project.AbsolutePath(entries[e].relativePath)));
	}

	std::vector<uint32> closeIds;
	for (size_t i = 0; i < animatorDocs.size(); ++i)
	{
		AnimatorDocument* doc = animatorDocs[i];
		if (!doc) continue;
		char title[512];
		snprintf(title, sizeof(title), u8" %s###animator_win_%u", doc->displayName.c_str(), doc->id);
		const bool canDock = (dockCenterId != 0);
		const bool forceDock = canDock && (pendingSelectAnimatorDocId == doc->id);
		if (canDock) ImGui::SetNextWindowDockID(dockCenterId, forceDock ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
		if (forceDock) ImGui::SetNextWindowFocus();
		ImGui::SetNextWindowSize(ImVec2(1100, 680), ImGuiCond_FirstUseEver);
		bool open = true;
		ImGuiWindowFlags wflags = ImGuiWindowFlags_None;
		if (doc->dirty) wflags |= ImGuiWindowFlags_UnsavedDocument;
		if (!ImGui::Begin(title, &open, wflags))
		{
			ImGui::End();
			if (!open) closeIds.push_back(doc->id);
			continue;
		}
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			activeAnimatorDoc = doc;
			lastFocusedDocKind = FocusedDocKind::Animator;
		}
		if (canDock && pendingSelectAnimatorDocId == doc->id) pendingSelectAnimatorDocId = 0;

		Animator* live = sceneView ? sceneView->FindLiveAnimator(doc->absolutePath) : NULL;
		AnimatorEditor::FrameRequests req;
		AnimatorEditor::DrawWindow(*doc, clipFiles, live, req);
		ImGui::End();
		if (req.save) SaveAnimatorDocument(doc);
		if (req.close || !open) closeIds.push_back(doc->id);
	}
	for (size_t k = 0; k < closeIds.size(); ++k)
		for (size_t i = 0; i < animatorDocs.size(); ++i)
		{
			if (!animatorDocs[i] || animatorDocs[i]->id != closeIds[k]) continue;
			// Closing saves, as a sprite sheet does: small, and undoable while open.
			if (animatorDocs[i]->dirty) SaveAnimatorDocument(animatorDocs[i]);
			if (activeAnimatorDoc == animatorDocs[i]) activeAnimatorDoc = nullptr;
			delete animatorDocs[i];
			animatorDocs.erase(animatorDocs.begin() + i);
			break;
		}
}

namespace {
	nlohmann::json DescribeController(const AnimatorDocument& doc)
	{
		nlohmann::json r = nlohmann::json::parse(AnimatorControllerToString(doc.controller));
		r["dirty"] = doc.dirty;
		r["clipNames"] = doc.clipNames;
		return r;
	}
}

// new_animator_controller {path} / open_animator {path} / save_animator {path?}
// animator_state {path?} - the controller as JSON, plus the live state if playing.
// animator_edit {path?, action, ...}:
//   add_state {name, clip, pos:[x,y]} / remove_state {name} / set_state {name, rename, clip, speed, loop, pos}
//   set_default {name} / add_transition {from ("*" any), to} -> index / remove_transition {index}
//   set_transition {index, duration, exitTime (-1 none), conditions:[{param, op: > < == != true false, value}]}
//   add_param {name, type: float|bool|trigger, default} / remove_param {name} / set_param {name, rename, default}
//   clips_from {file} - which .p3da (or .p3d2d) the state clip picker lists
bool Editor::HandleAnimatorCommand(const std::string& name, const nlohmann::json& a, nlohmann::json& r)
{
	if (name != "new_animator_controller" && name != "open_animator" && name != "save_animator"
		&& name != "animator_state" && name != "animator_edit") return false;
	if (!project.IsOpen()) throw std::runtime_error("no project open");
	const auto S = [&](const char* k) { return a.is_object() ? a.value(k, std::string()) : std::string(); };

	if (name == "new_animator_controller")
	{
		std::string rel = S("path");
		if (rel.empty()) throw std::runtime_error("give \"path\" (e.g. assets/animations/hero.p3dac)");
		if (!ProjectManager::IsAnimatorExtension(rel)) rel += ".p3dac";
		const std::string abs = project.AbsolutePath(rel);
		std::filesystem::create_directories(std::filesystem::path(abs).parent_path());
		if (!std::filesystem::exists(abs))
		{
			AnimatorController c;
			std::string err;
			if (!SaveAnimatorController(abs, c, &err)) throw std::runtime_error(err);
		}
		if (!OpenAnimatorDocument(abs)) throw std::runtime_error("could not open " + rel);
		r["path"] = rel;
		r["ok"] = true;
		return true;
	}

	AnimatorDocument* doc = activeAnimatorDoc;
	if (!S("path").empty())
	{
		const std::string abs = project.AbsolutePath(S("path"));
		doc = FindAnimatorDocumentByPath(abs);
		if (!doc && !OpenAnimatorDocument(abs)) throw std::runtime_error("could not open " + S("path"));
		doc = FindAnimatorDocumentByPath(abs);
	}
	if (!doc) throw std::runtime_error("no state machine open (pass \"path\")");

	if (name == "open_animator") { r = DescribeController(*doc); return true; }
	if (name == "save_animator")
	{
		if (!SaveAnimatorDocument(doc)) throw std::runtime_error("could not save");
		r["ok"] = true;
		return true;
	}
	if (name == "animator_state")
	{
		r = DescribeController(*doc);
		if (Animator* live = sceneView ? sceneView->FindLiveAnimator(doc->absolutePath) : NULL)
		{
			nlohmann::json l;
			l["state"] = live->GetState();
			l["progress"] = live->GetStateProgress();
			nlohmann::json vals = nlohmann::json::object();
			for (size_t i = 0; i < doc->controller.params.size(); i++)
				vals[doc->controller.params[i].name] = live->GetFloat(doc->controller.params[i].name);
			l["params"] = vals;
			r["live"] = l;
		}
		return true;
	}

	// animator_edit
	const std::string action = S("action");
	AnimatorController& c = doc->controller;
	if (action == "add_state")
	{
		float x = 0.f, y = 0.f;
		if (a.contains("pos") && a["pos"].is_array() && a["pos"].size() >= 2) { x = a["pos"][0].get<float>(); y = a["pos"][1].get<float>(); }
		r["name"] = doc->AddState(S("name").empty() ? std::string("State") : S("name"), S("clip"), x, y);
	}
	else if (action == "remove_state") { if (!doc->RemoveState(S("name"))) throw std::runtime_error("no state '" + S("name") + "'"); }
	else if (action == "set_state")
	{
		const int i = c.FindState(S("name"));
		if (i < 0) throw std::runtime_error("no state '" + S("name") + "'");
		const std::string before = doc->Snapshot();
		AnimatorState& st = c.states[i];
		if (a.contains("clip")) st.clip = S("clip");
		if (a.contains("speed")) st.speed = a.value("speed", 1.f);
		if (a.contains("loop")) st.loop = a.value("loop", true);
		if (a.contains("pos") && a["pos"].is_array() && a["pos"].size() >= 2) st.position = Vec2(a["pos"][0].get<float>(), a["pos"][1].get<float>());
		doc->PushEdit(before, "Edit state");
		if (!S("rename").empty() && !doc->RenameState(S("name"), S("rename"))) throw std::runtime_error("cannot rename to '" + S("rename") + "'");
	}
	else if (action == "set_default")
	{
		if (c.FindState(S("name")) < 0) throw std::runtime_error("no state '" + S("name") + "'");
		const std::string before = doc->Snapshot();
		c.defaultState = S("name");
		doc->PushEdit(before, "Default state");
	}
	else if (action == "add_transition")
	{
		const int t = doc->AddTransition(S("from"), S("to"));
		if (t < 0) throw std::runtime_error("from/to must be states (from may be \"*\")");
		r["index"] = t;
	}
	else if (action == "remove_transition") { if (!doc->RemoveTransition(a.value("index", -1))) throw std::runtime_error("no transition at that index"); }
	else if (action == "set_transition")
	{
		const int i = a.value("index", -1);
		if (i < 0 || i >= (int)c.transitions.size()) throw std::runtime_error("no transition at that index");
		const std::string before = doc->Snapshot();
		AnimatorTransition& t = c.transitions[i];
		if (a.contains("duration")) t.duration = std::max(0.f, a.value("duration", 0.2f));
		if (a.contains("exitTime")) t.exitTime = a.value("exitTime", -1.f);
		if (a.contains("conditions") && a["conditions"].is_array())
		{
			// Through the file reader, so an agent's condition is the file's.
			AnimatorController tmp;
			nlohmann::json one = { { "transitions", nlohmann::json::array({ { { "conditions", a["conditions"] } } }) } };
			if (AnimatorControllerFromString(one.dump(), tmp) && !tmp.transitions.empty())
				t.conditions = tmp.transitions[0].conditions;
		}
		doc->PushEdit(before, "Edit transition");
	}
	else if (action == "add_param")
	{
		const std::string type = S("type");
		const int ty = type == "bool" ? ANIMATOR_BOOL : type == "trigger" ? ANIMATOR_TRIGGER : ANIMATOR_FLOAT;
		const std::string n = doc->AddParam(S("name").empty() ? std::string("param") : S("name"), ty);
		if (a.contains("default"))
		{
			const std::string before = doc->Snapshot();
			c.params[c.FindParam(n)].defaultValue = a["default"].is_boolean() ? (a["default"].get<bool>() ? 1.f : 0.f) : a.value("default", 0.f);
			doc->PushEdit(before, "Parameter default");
		}
		r["name"] = n;
	}
	else if (action == "remove_param") { if (!doc->RemoveParam(S("name"))) throw std::runtime_error("no parameter '" + S("name") + "'"); }
	else if (action == "set_param")
	{
		const int i = c.FindParam(S("name"));
		if (i < 0) throw std::runtime_error("no parameter '" + S("name") + "'");
		if (a.contains("default"))
		{
			const std::string before = doc->Snapshot();
			c.params[i].defaultValue = a["default"].is_boolean() ? (a["default"].get<bool>() ? 1.f : 0.f) : a.value("default", 0.f);
			doc->PushEdit(before, "Parameter default");
		}
		if (!S("rename").empty() && !doc->RenameParam(S("name"), S("rename"))) throw std::runtime_error("cannot rename to '" + S("rename") + "'");
	}
	else if (action == "clips_from")
	{
		if (!doc->LoadClipNames(project.AbsolutePath(S("file")))) throw std::runtime_error("could not read clips from " + S("file"));
	}
	else throw std::runtime_error("animator_edit actions: add_state, remove_state, set_state, set_default, add_transition, "
		"remove_transition, set_transition, add_param, remove_param, set_param, clips_from");
	nlohmann::json d = DescribeController(*doc);
	for (auto it = d.begin(); it != d.end(); ++it) r[it.key()] = it.value();
	return true;
}
