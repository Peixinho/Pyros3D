//=============================================================================
// Name        : AnimatorPreview.cpp
// Description : See the header.
//=============================================================================

#include "AnimatorPreview.h"
#include "AnimationPreview.h"
#include "AnimationEditorDocument.h"
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>

using namespace p3d;

AnimatorPreview::AnimatorPreview() : blank(new AnimationEditorDocument())
{
	// The blend-mode flag keeps AnimationPreview's timeline-only drawing
	// (onion skin) out of it; nothing here is a timeline.
	blank->blendMode = true;
}

AnimatorPreview::~AnimatorPreview()
{
	animator.Unbind();
}

bool AnimatorPreview::Ready() const
{
	return view && view->instance && animator.IsBound();
}

bool AnimatorPreview::Load()
{
	error.clear();
	animator.Unbind();
	boundText.clear();
	if (!view) view.reset(new AnimationPreview());
	view->EnsureInit();
	if (meshPath.empty()) { error = "Pick a rig."; return false; }
	view->ClearMesh();
	if (!view->LoadMesh(meshPath) || !view->instance)
	{
		error = view->loadError.empty() ? std::string("That model has no skeleton.") : view->loadError;
		return false;
	}
	view->showBones = false;
	// The clips, in order (a clip's id is its place in the concatenated list).
	std::vector<Animation> clips;
	for (size_t i = 0; i < clipFiles.size(); i++)
	{
		AnimationLoader loader;
		if (loader.Load(clipFiles[i])) clips.insert(clips.end(), loader.animations.begin(), loader.animations.end());
	}
	if (clips.empty()) { error = "Pick the clip files the states play."; return false; }
	view->animOwner->SetAnimations(clips);
	clock = 0.f;
	return true;
}

void AnimatorPreview::Step(const AnimatorController& controller, float dt)
{
	if (!view || !view->instance || !view->animOwner) return;
	const std::string text = AnimatorControllerToString(controller);
	if (text != boundText)
	{
		// Rebind - and carry over the parameters set here, so editing a
		// transition does not reset the speed the user was testing with.
		std::vector<std::pair<std::string, float> > keep;
		for (size_t i = 0; i < controller.params.size(); i++)
			if (animator.IsBound()) keep.push_back(std::make_pair(controller.params[i].name, animator.GetFloat(controller.params[i].name)));
		view->instance->Stop();
		animator.Bind(controller, view->instance);
		for (size_t i = 0; i < keep.size(); i++)
			if (controller.params[controller.FindParam(keep[i].first)].type != ANIMATOR_TRIGGER) animator.SetFloat(keep[i].first, keep[i].second);
		boundText = text;
	}
	if (!playing) dt = 0.f;
	animator.Update(dt);
	clock += dt;
	view->animOwner->UpdateInstance(view->instance, clock);
	view->instance->DeliverPending();
}
