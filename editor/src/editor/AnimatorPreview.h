//=============================================================================
// Name        : AnimatorPreview.h
// Description : The state machine window's own viewport: a rig, the clip
//               files it plays, and an Animator running the document's
//               controller on it - so transitions can be tuned without
//               pressing Play on a scene. The viewport itself is the
//               Animation Editor's (AnimationPreview), not a second one.
//=============================================================================

#ifndef ANIMATORPREVIEW_H
#define ANIMATORPREVIEW_H

#include <Pyros3D/AnimationManager/AnimatorController.h>
#include <memory>
#include <string>
#include <vector>

struct AnimationPreview;
struct AnimationEditorDocument;

struct AnimatorPreview {
	AnimatorPreview();
	~AnimatorPreview();

	std::unique_ptr<AnimationPreview> view;
	// AnimationPreview draws against an animation document (selection,
	// onion skin...); this one is empty and only ever says "nothing selected".
	std::unique_ptr<AnimationEditorDocument> blank;
	p3d::Animator animator;

	std::string meshPath;                 // absolute .p3dm
	std::vector<std::string> clipFiles;   // absolute .p3da, in load order
	bool playing = true;
	float clock = 0.f;
	// The controller text it was last bound with; a change rebinds (keeping
	// the parameter values the user set here).
	std::string boundText;
	std::string error;

	// (Re)builds rig + clips. False with `error` set.
	bool Load();
	bool Ready() const;
	// One frame: rebind if `controller` changed, advance, pose.
	void Step(const p3d::AnimatorController& controller, float dt);
};

#endif
