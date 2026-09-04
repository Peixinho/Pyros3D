//=============================================================================
// Name        : AnimationEditor.h
// Description : ImGui panels for an AnimationEditorDocument - toolbar, bone
//               tree, viewport host and dope-sheet timeline. Free functions
//               rather than a class, matching MaterialEditor: the state all
//               lives on the document and its AnimationPreview.
//
//               The keyframe-writing helpers are public because the agent /
//               MCP bridge drives exactly the same operations the buttons do
//               (Editor::HandleAgentCommand), and a second implementation of
//               "key this bone" would be a second set of bugs.
//=============================================================================

#ifndef ANIMATIONEDITOR_UI_H
#define ANIMATIONEDITOR_UI_H

#include <string>
#include <utility>
#include <vector>

// Included rather than forward-declared: the IK helpers below take
// AnimationEditorDocument::IKHandle, a nested type that needs the full
// definition. No cycle - the document header does not include this one.
#include "../AnimationEditorDocument.h"

// One selectable mesh in the rig picker: display label + absolute path.
typedef std::pair<std::string, std::string> AnimationMeshChoice;

namespace AnimationEditor {

// What the window is asking the host to do after this frame. The host owns
// file dialogs, document lifetime and the project, so the panel only ever
// requests.
struct FrameRequests {
	bool save = false;
	bool saveAs = false;
	bool close = false;
	// Set when the user picked a different rig; the host persists the
	// binding into project.json and calls BindMesh.
	bool meshChanged = false;
	std::string newMeshPath;
	// Sticky "copied" acknowledgement for the Blend tab's Copy Lua button.
	bool copiedLua = false;
	// Set when the blend configuration changed, so the host can persist it
	// into project.json alongside the rig binding.
	bool blendChanged = false;
};

// Draws the whole document window (already inside Begin/End - the host owns
// the window itself so it can drive docking and the tab's unsaved marker).
// `dt` advances playback. `meshes` populates the rig picker.
void DrawWindow(AnimationEditorDocument& doc, const std::vector<AnimationMeshChoice>& meshes,
	float dt, FrameRequests& requests);

// ---- pieces the 2D animation window reuses ------------------------------
// The Animation 2D window is this same editor pointed at a rig that lives on
// a scene object rather than in a .p3da file. It draws its own clip bar (2D
// clips are stored on the GameObject, not in a file, so there is nothing to
// save or bind) and then calls straight into these.

// The dope sheet: ruler, summary row, one track per bone, key selection,
// drag-to-retime, and the right-click interpolation menu.
void DrawTimelinePanel(AnimationEditorDocument& doc);
// Transport bar: step/play/loop/speed, snapping, auto-key, key/delete.
void DrawTransportBar(AnimationEditorDocument& doc, float dt);
// Bone list plus numeric pose fields for the selected bone. `emptyHint` is
// the line shown when no rig is bound; the default one talks about .p3dm
// files, which means nothing in a scene.
void DrawSkeletonPanel(AnimationEditorDocument& doc, const char* emptyHint = NULL);
// Poses the bound rig from the active clip at the playhead and re-applies any
// uncommitted pose overrides. The timeline-mode half of
// AnimationPreview::SyncPose, which calls this rather than repeating it.
void ApplyTimelinePose(AnimationEditorDocument& doc);

// ---- operations shared with the agent bridge ---------------------------

// Ensures the document has a preview object and that the preview has the
// document's meshPath loaded. Cheap to call every frame.
void EnsurePreview(AnimationEditorDocument& doc);

// Writes one bone's *current posed* local transform into the active clip at
// `time`, creating the bone's channel if needed. Honours doc.keyPosition /
// doc.keyRotation. Returns false when there is no rig, no active clip, or
// the bone id is unknown. Not undo-wrapped - callers decide the grouping
// (the UI keys several bones under one command).
bool KeyBoneAtTime(AnimationEditorDocument& doc, int boneId, float time);

// Keys every bone the user has posed but not yet committed
// (AnimationPreview::poseOverrides) and clears them. Wrapped in one undo
// command. Returns how many bones were keyed.
int KeyPendingPose(AnimationEditorDocument& doc);

// Keys every bone in the skeleton at `time` - the "whole pose" key an
// animator wants when blocking out. Wrapped in one undo command.
int KeyWholeSkeleton(AnimationEditorDocument& doc, float time);

// Deletes every currently selected key column. Wrapped in one undo command.
// Returns how many columns were removed.
int DeleteSelectedKeys(AnimationEditorDocument& doc);

// ---- inverse kinematics ---------------------------------------------------
// Shared by the IK panel and the agent socket, so a scripted solve and a
// clicked one go through exactly the same code - the same reason the runtime
// and the editor share IKSolver rather than each having their own.

// Solves `h` against whatever pose is currently on the rig. False if the
// chain's bones do not resolve against the bound mesh.
bool ApplyIK(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h);
// Keys every bone of the chain at `time`. Assumes ApplyIK has just run.
int KeyIKChain(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h, float time);
// Re-samples the clip and solves once per frame across the range, keying each
// result. Pushes one undo entry for the whole bake.
int BakeIKOverRange(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h,
	float startTime, float endTime);

// Sets the playhead, clearing any uncommitted pose (see poseOverrides) so
// scrubbing always shows what the clip actually contains.
//
// keepPendingPose overrides that for the one caller that means "move to this
// time in order to key what is posed right now" (the key_animation_pose
// agent command): dropping the pose there would silently key nothing, since
// the pose being committed is exactly what the move would discard.
void SetPlayhead(AnimationEditorDocument& doc, float time, bool keepPendingPose = false);

} // namespace AnimationEditor

#endif /* ANIMATIONEDITOR_UI_H */
