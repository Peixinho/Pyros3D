//=============================================================================
// Name        : AnimatorDocument.cpp
// Description : See the header.
//=============================================================================

#include "AnimatorDocument.h"
#include "AnimatorPreview.h"
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

AnimatorDocument::AnimatorDocument() {}
AnimatorDocument::~AnimatorDocument() {}

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
	if (activeLayer > (int)controller.layers.size()) activeLayer = 0;
	if (L().FindState(selectedState) < 0) selectedState.clear();
	if (selectedTransition >= (int)L().transitions.size()) selectedTransition = -1;
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
	s.name = L().UniqueStateName(name);
	s.clip = clip;
	s.position = Vec2(x, y);
	L().states.push_back(s);
	if (L().defaultState.empty()) L().defaultState = s.name;
	PushEdit(before, "Add state '" + s.name + "'");
	return s.name;
}

bool AnimatorDocument::RemoveState(const std::string& name)
{
	const int i = L().FindState(name);
	if (i < 0) return false;
	const std::string before = Snapshot();
	L().states.erase(L().states.begin() + i);
	// Transitions into or out of it go with it.
	for (size_t t = L().transitions.size(); t > 0; t--)
		if (L().transitions[t - 1].from == name || L().transitions[t - 1].to == name)
			L().transitions.erase(L().transitions.begin() + (t - 1));
	if (L().defaultState == name)
		L().defaultState = L().states.empty() ? std::string() : L().states[0].name;
	if (selectedState == name) selectedState.clear();
	selectedTransition = -1;
	PushEdit(before, "Delete state '" + name + "'");
	return true;
}

bool AnimatorDocument::RenameState(const std::string& from, const std::string& to)
{
	const int i = L().FindState(from);
	if (i < 0 || to.empty() || from == to || L().FindState(to) >= 0) return false;
	const std::string before = Snapshot();
	L().states[i].name = to;
	for (size_t t = 0; t < L().transitions.size(); t++)
	{
		if (L().transitions[t].from == from) L().transitions[t].from = to;
		if (L().transitions[t].to == from) L().transitions[t].to = to;
	}
	if (L().defaultState == from) L().defaultState = to;
	if (selectedState == from) selectedState = to;
	PushEdit(before, "Rename state");
	return true;
}

int AnimatorDocument::AddTransition(const std::string& from, const std::string& to)
{
	if (L().FindState(to) < 0 || (from != "*" && L().FindState(from) < 0)) return -1;
	const std::string before = Snapshot();
	AnimatorTransition t;
	t.from = from;
	t.to = to;
	// With no condition yet it leaves when the clip is done - the most
	// common first transition, and one that can be seen working at once.
	t.exitTime = 1.f;
	L().transitions.push_back(t);
	PushEdit(before, "Add transition " + from + " -> " + to);
	return (int)L().transitions.size() - 1;
}

bool AnimatorDocument::RemoveTransition(int index)
{
	if (index < 0 || index >= (int)L().transitions.size()) return false;
	const std::string before = Snapshot();
	L().transitions.erase(L().transitions.begin() + index);
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
	// transition that silently never fires - in every layer.
	for (int l = 0; l <= (int)controller.layers.size(); l++)
	{
		AnimatorLayer& m = Machine(l);
		for (size_t t = 0; t < m.transitions.size(); t++)
		{
			std::vector<AnimatorCondition>& c = m.transitions[t].conditions;
			for (size_t k = c.size(); k > 0; k--) if (c[k - 1].param == name) c.erase(c.begin() + (k - 1));
		}
		for (size_t st = 0; st < m.states.size(); st++)
			if (m.states[st].blendParam == name) m.states[st].blendParam.clear();
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
	for (int l = 0; l <= (int)controller.layers.size(); l++)
	{
		AnimatorLayer& m = Machine(l);
		for (size_t t = 0; t < m.transitions.size(); t++)
			for (size_t k = 0; k < m.transitions[t].conditions.size(); k++)
				if (m.transitions[t].conditions[k].param == from) m.transitions[t].conditions[k].param = to;
		for (size_t st = 0; st < m.states.size(); st++)
			if (m.states[st].blendParam == from) m.states[st].blendParam = to;
	}
	PushEdit(before, "Rename parameter");
	return true;
}

std::string AnimatorDocument::AddLayer(const std::string& name, const std::string& rootBone)
{
	const std::string before = Snapshot();
	AnimatorLayer l;
	l.name = controller.UniqueLayerName(name.empty() ? std::string("Layer") : name);
	l.rootBone = rootBone;
	// Starts with one empty state: the layer lets the base show until it
	// is told to do something.
	AnimatorState none;
	none.name = "Empty";
	l.states.push_back(none);
	l.defaultState = none.name;
	controller.layers.push_back(l);
	activeLayer = (int)controller.layers.size();
	selectedState.clear();
	selectedTransition = -1;
	PushEdit(before, "Add layer '" + l.name + "'");
	return l.name;
}

bool AnimatorDocument::RemoveLayer(int index)
{
	if (index < 1 || index > (int)controller.layers.size()) return false;
	const std::string before = Snapshot();
	controller.layers.erase(controller.layers.begin() + (index - 1));
	activeLayer = 0;
	selectedState.clear();
	selectedTransition = -1;
	PushEdit(before, "Delete layer");
	return true;
}
