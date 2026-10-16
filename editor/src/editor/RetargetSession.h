//=============================================================================
// Name        : RetargetSession.h
// Description : What the Animation Editor's Retarget window holds while it is
//               open: the source clips, the source rig they were keyed for,
//               and the bone map onto the document's own rig. The maths is
//               PoseTools::RetargetClip; this is only the loading and the
//               choices around it, shared by the window and the agent command.
//=============================================================================

#ifndef RETARGETSESSION_H
#define RETARGETSESSION_H

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct AnimationEditorDocument;

struct RetargetSession {
	std::string clipFile;      // absolute .p3da
	std::string rigFile;       // absolute .p3dm the clips were keyed for
	std::vector<p3d::Animation> clips;
	std::vector<bool> chosen;
	// target bone name -> source bone name. Seeded by name, editable.
	std::map<std::string, std::string> map;
	float fps = 30.f;
	// Drop the keys the motion passes through anyway after retargeting.
	bool reduce = true;
	std::string error;

	// The source rig, alive only while the session is.
	std::shared_ptr<p3d::Renderable> model;
	std::unique_ptr<p3d::RenderingComponent> rc;
	std::shared_ptr<p3d::SkeletonAnimation> anim;
	p3d::SkeletonAnimationInstance* source = NULL;

	bool Loaded() const { return source != NULL && !clips.empty(); }
	// Loads both files and seeds the map against `target`. False with
	// `error` set when either does not load or the rig has no skeleton.
	bool Load(const std::string& clipAbs, const std::string& rigAbs, p3d::SkeletonAnimationInstance* target);
	// The chosen clips retargeted onto `doc`'s rig and appended to it (names
	// made unique). Not undo-wrapped. Returns the names added.
	std::vector<std::string> ImportInto(AnimationEditorDocument& doc);
};

#endif
