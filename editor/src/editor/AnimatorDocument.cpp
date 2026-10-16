//=============================================================================
// Name        : AnimatorDocument.cpp
// Description : See the header.
//=============================================================================

#include "AnimatorDocument.h"
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>
#include <Pyros3D/Assets/Character2D/Character2DAsset.h>
#include <filesystem>
#include <memory>

using namespace p3d;

namespace {
	class AnimatorEditCommand : public IUndoableCommand {
	public:
		AnimatorEditCommand(AnimatorDocument* doc, const std::string& before, const std::string& after, const std::string& what)
			: doc_(doc), before_(before), after_(after), what_(what) {}
		void Undo() override { doc_->Restore(before_); }
		void Redo() override { doc_->Restore(after_); }
		std::string Description() const override { return what_; }
		size_t MemoryCost() const override { return sizeof(*this) + before_.capacity() + after_.capacity(); }
	private:
		AnimatorDocument* doc_;
		std::string before_, after_, what_;
	};
}

bool AnimatorDocument::LoadFromFile(const std::string& path, std::string& errOut)
{
	if (!LoadAnimatorController(path, controller, &errOut)) return false;
	absolutePath = path;
	displayName = std::filesystem::path(path).stem().string();
	dirty = false;
	return true;
}

bool AnimatorDocument::Save(std::string& errOut)
{
	if (absolutePath.empty()) { errOut = "this controller has no file yet"; return false; }
	return SaveAs(absolutePath, errOut);
}

bool AnimatorDocument::SaveAs(const std::string& path, std::string& errOut)
{
	if (!SaveAnimatorController(path, controller, &errOut)) return false;
	absolutePath = path;
	displayName = std::filesystem::path(path).stem().string();
	dirty = false;
	return true;
}

bool AnimatorDocument::LoadClipNames(const std::string& absPath)
{
	clipNames.clear();
	clipSourcePath = absPath;
	if (absPath.size() > 6 && absPath.compare(absPath.size() - 6, 6, ".p3d2d") == 0)
	{
		Character2DAsset a;
		if (!LoadCharacter2D(absPath, a)) return false;
		for (size_t i = 0; i < a.clips.size(); i++) clipNames.push_back(a.clips[i].AnimationName);
		return true;
	}
	AnimationLoader loader;
	if (!loader.Load(absPath)) return false;
	for (size_t i = 0; i < loader.animations.size(); i++) clipNames.push_back(loader.animations[i].AnimationName);
	return true;
}

std::string AnimatorDocument::Snapshot() const { return AnimatorControllerToString(controller); }

void AnimatorDocument::Restore(const std::string& text)
{
	AnimatorController c;
	if (!AnimatorControllerFromString(text, c)) return;
	controller = c;
	dirty = true;
	if (controller.FindState(selectedState) < 0) selectedState.clear();
	if (selectedTransition >= (int)controller.transitions.size()) selectedTransition = -1;
}

void AnimatorDocument::PushEdit(const std::string& before, const std::string& what)
{
	dirty = true;
	if (editOpen) return;   // the open gesture records it
	const std::string after = Snapshot();
	if (after == before) return;
	undo.Push(std::unique_ptr<IUndoableCommand>(new AnimatorEditCommand(this, before, after, what)));
}

void AnimatorDocument::BeginEdit()
{
	if (editOpen) return;
	editOpen = true;
	editBefore = Snapshot();
}

void AnimatorDocument::EndEdit(const std::string& what)
{
	if (!editOpen) return;
	editOpen = false;
	PushEdit(editBefore, what);
}

std::string AnimatorDocument::AddState(const std::string& name, const std::string& clip, float x, float y)
{
	const std::string before = Snapshot();
	AnimatorState s;
	s.name = controller.UniqueStateName(name);
	s.clip = clip;
	s.position = Vec2(x, y);
	controller.states.push_back(s);
	if (controller.defaultState.empty()) controller.defaultState = s.name;
	PushEdit(before, "Add state '" + s.name + "'");
	return s.name;
}

bool AnimatorDocument::RemoveState(const std::string& name)
{
	const int i = controller.FindState(name);
	if (i < 0) return false;
	const std::string before = Snapshot();
	controller.states.erase(controller.states.begin() + i);
	// Transitions into or out of it go with it.
	for (size_t t = controller.transitions.size(); t > 0; t--)
		if (controller.transitions[t - 1].from == name || controller.transitions[t - 1].to == name)
			controller.transitions.erase(controller.transitions.begin() + (t - 1));
	if (controller.defaultState == name)
		controller.defaultState = controller.states.empty() ? std::string() : controller.states[0].name;
	if (selectedState == name) selectedState.clear();
	selectedTransition = -1;
	PushEdit(before, "Delete state '" + name + "'");
	return true;
}

bool AnimatorDocument::RenameState(const std::string& from, const std::string& to)
{
	const int i = controller.FindState(from);
	if (i < 0 || to.empty() || from == to || controller.FindState(to) >= 0) return false;
	const std::string before = Snapshot();
	controller.states[i].name = to;
	for (size_t t = 0; t < controller.transitions.size(); t++)
	{
		if (controller.transitions[t].from == from) controller.transitions[t].from = to;
		if (controller.transitions[t].to == from) controller.transitions[t].to = to;
	}
	if (controller.defaultState == from) controller.defaultState = to;
	if (selectedState == from) selectedState = to;
	PushEdit(before, "Rename state");
	return true;
}

int AnimatorDocument::AddTransition(const std::string& from, const std::string& to)
{
	if (controller.FindState(to) < 0 || (from != "*" && controller.FindState(from) < 0)) return -1;
	const std::string before = Snapshot();
	AnimatorTransition t;
	t.from = from;
	t.to = to;
	// With no condition yet it leaves when the clip is done - the most
	// common first transition, and one that can be seen working at once.
	t.exitTime = 1.f;
	controller.transitions.push_back(t);
	PushEdit(before, "Add transition " + from + " -> " + to);
	return (int)controller.transitions.size() - 1;
}

bool AnimatorDocument::RemoveTransition(int index)
{
	if (index < 0 || index >= (int)controller.transitions.size()) return false;
	const std::string before = Snapshot();
	controller.transitions.erase(controller.transitions.begin() + index);
	selectedTransition = -1;
	PushEdit(before, "Delete transition");
	return true;
}

std::string AnimatorDocument::AddParam(const std::string& name, int type)
{
	const std::string before = Snapshot();
	AnimatorParam p;
	p.name = controller.UniqueParamName(name);
	p.type = type;
	controller.params.push_back(p);
	PushEdit(before, "Add parameter '" + p.name + "'");
	return p.name;
}

bool AnimatorDocument::RemoveParam(const std::string& name)
{
	const int i = controller.FindParam(name);
	if (i < 0) return false;
	const std::string before = Snapshot();
	controller.params.erase(controller.params.begin() + i);
	// Conditions on it can never be met now; drop them rather than leave a
	// transition that silently never fires.
	for (size_t t = 0; t < controller.transitions.size(); t++)
	{
		std::vector<AnimatorCondition>& c = controller.transitions[t].conditions;
		for (size_t k = c.size(); k > 0; k--) if (c[k - 1].param == name) c.erase(c.begin() + (k - 1));
	}
	PushEdit(before, "Delete parameter '" + name + "'");
	return true;
}

bool AnimatorDocument::RenameParam(const std::string& from, const std::string& to)
{
	const int i = controller.FindParam(from);
	if (i < 0 || to.empty() || from == to || controller.FindParam(to) >= 0) return false;
	const std::string before = Snapshot();
	controller.params[i].name = to;
	for (size_t t = 0; t < controller.transitions.size(); t++)
		for (size_t k = 0; k < controller.transitions[t].conditions.size(); k++)
			if (controller.transitions[t].conditions[k].param == from) controller.transitions[t].conditions[k].param = to;
	PushEdit(before, "Rename parameter");
	return true;
}
