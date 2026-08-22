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

struct AnimationEditorDocument;

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
};

// Draws the whole document window (already inside Begin/End - the host owns
// the window itself so it can drive docking and the tab's unsaved marker).
// `dt` advances playback. `meshes` populates the rig picker.
void DrawWindow(AnimationEditorDocument& doc, const std::vector<AnimationMeshChoice>& meshes,
	float dt, FrameRequests& requests);

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
