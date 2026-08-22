//============================================================================
// Name        : AnimationEditorDocument.h
// Description : Animation documents (dockable peer windows to Scene View and
//               the Material Editor, same convention as
//               MaterialEditorDocument/CodeEditorDocument).
//
//               The document IS the .p3da file: a list of p3d::Animation
//               clips, edited in place and written straight back out through
//               AnimationLoader::Save. There is no intermediate/authoring
//               format - what you edit is what the runtime loads (see the
//               header comment on AnimationLoader::Save for why that
//               round-trips losslessly).
//
//               Pure data + file I/O + clip/keyframe operations. Nothing here
//               touches ImGui or the renderer: the viewport lives in
//               AnimationPreview, the panels in UI/AnimationEditor.
//============================================================================

#ifndef ANIMATIONEDITORDOCUMENT_H
#define ANIMATIONEDITORDOCUMENT_H

#include "UndoStack.h"
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct AnimationPreview;

// Two keys are "at the same time" within this many seconds. Timeline clicks,
// keying and key dragging all resolve to a time rather than to an index (an
// index would be invalidated by the edit immediately before it), so every
// lookup needs one shared tolerance. 1ms is well below a frame at any
// sensible rate and well above f32 drift over a minute-long clip.
static const float kAnimKeyEpsilon = 0.001f;

// One selected key column: every key at ~this time in this channel,
// regardless of which of position/rotation/scale actually has one there. The
// dope sheet draws a single diamond per column for exactly this reason -
// per-component rows would quadruple the UI for a distinction the engine's
// sampler barely honours (scale is sampled but deliberately not applied; see
// SkeletonAnimationInstance::SampleChannel).
struct AnimKeyRef {
	int channel = -1;
	float time = 0.f;
	bool operator<(const AnimKeyRef& o) const
	{
		if (channel != o.channel) return channel < o.channel;
		return time < o.time - kAnimKeyEpsilon;
	}
};

struct AnimationEditorDocument {
	uint32_t id = 0;
	// Absolute path of the .p3da. Empty for a document that has never been
	// saved (File > New Animation), which is the only state in which Save
	// has to fall back to Save As.
	std::string absolutePath;
	std::string displayName = "NewAnimation";
	bool dirty = false;

	// The file's contents. A .p3da holds a *list* of clips, and clip id is
	// just the index here - which is what Play(id) means at runtime, so
	// reordering this vector rewrites the meaning of every saved Play()
	// call in every scene. Nothing in the editor reorders it for that
	// reason; clips are only appended or removed from the end-user's
	// explicit action.
	std::vector<p3d::Animation> clips;
	int activeClip = -1;

	// Absolute path of the .p3dm previewed with these clips. Not stored in
	// the .p3da (the format has nowhere to put it) - persisted per project
	// in project.json's animationBindings map, so reopening a clip brings
	// its rig back. Empty until a mesh is bound.
	std::string meshPath;

	// ---- playback / timeline state (not saved) -----------------------
	float playhead = 0.f;
	bool playing = false;
	bool looping = true;
	float playSpeed = 1.f;
	// Timeline snapping. Authoring at a fixed rate is what makes keys line
	// up across bones; free-floating times are still representable (the
	// format stores f32 seconds), snapping just stops you creating them by
	// accident.
	bool snapEnabled = true;
	float snapFps = 30.f;
	// Horizontal zoom of the dope sheet, in pixels per second.
	float pixelsPerSecond = 220.f;
	float timelineScroll = 0.f;

	// ---- selection ---------------------------------------------------
	// Bone id into the preview skeleton, or -1. Bone ids are only
	// meaningful against the currently bound mesh, so the name is kept
	// alongside and used to re-resolve the id after a mesh swap.
	int selectedBone = -1;
	std::string selectedBoneName;
	std::set<AnimKeyRef> selectedKeys;
	// Draw a row per bone that has a channel, or per bone in the skeleton
	// (so you can key a bone that isn't animated yet).
	bool showAllBones = false;
	// Key both position and rotation when keying a posed bone. Rotation
	// alone is the common case for a limb; position matters for the root.
	bool keyPosition = true;
	bool keyRotation = true;
	// Write a key as soon as a bone gizmo drag ends, instead of leaving the
	// pose pending until the user presses Key.
	bool autoKey = false;

	// Per-document undo/redo history, same contract as SceneEditor's and
	// MaterialEditorDocument's.
	UndoStack undo;

	// The 3D viewport for this document. Owned here (same arrangement as
	// MaterialEditorDocument::preview) but only constructed when the
	// document is first drawn, so a document opened purely to be read by an
	// agent command never builds a renderer.
	std::unique_ptr<AnimationPreview> preview;

	// ---- clip access -------------------------------------------------
	bool HasActiveClip() const { return activeClip >= 0 && activeClip < (int)clips.size(); }
	p3d::Animation* ActiveClip() { return HasActiveClip() ? &clips[activeClip] : NULL; }
	const p3d::Animation* ActiveClip() const { return HasActiveClip() ? &clips[activeClip] : NULL; }

	// ---- file I/O ----------------------------------------------------
	// Replaces clips/activeClip with the file's contents. Returns false and
	// fills errOut when the file can't be read.
	bool LoadFromFile(const std::string& absPath, std::string& errOut);
	// Writes every clip to absPath (which becomes this document's path) and
	// clears dirty.
	bool SaveToFile(const std::string& absPath, std::string& errOut);

	// ---- clip operations (callers wrap these in undo commands) --------
	// Appends an empty clip and returns its index.
	int AddClip(const std::string& name, float duration);
	bool RemoveClip(int index);
	bool RenameClip(int index, const std::string& name);
	bool SetClipDuration(int index, float duration);
	// Unique-ified against the clips already present ("Clip", "Clip 2", ...).
	std::string MakeUniqueClipName(const std::string& base) const;

	// ---- channel / keyframe operations --------------------------------
	// Index of the channel driving `boneName` in clip `clipIndex`, or -1.
	int FindChannel(int clipIndex, const std::string& boneName) const;
	// Same, creating an empty channel when absent. -1 only if clipIndex is
	// out of range.
	int FindOrCreateChannel(int clipIndex, const std::string& boneName);

	// Every distinct key time in a channel, sorted - the union of the
	// position/rotation/scale time lists, which is what one dope-sheet row
	// shows.
	void CollectKeyTimes(int clipIndex, int channel, std::vector<float>& out) const;
	// Union across every channel - the "summary" row at the top of the
	// dope sheet.
	void CollectAllKeyTimes(int clipIndex, std::vector<float>& out) const;

	// Inserts (or replaces) a key at `time`. Only the components flagged
	// are written, so keying rotation on a bone that already has a position
	// track leaves that track alone. Keeps each component's list sorted by
	// time, which the runtime sampler's forward scan requires.
	void SetKey(int clipIndex, int channel, float time,
		const p3d::Math::Vec3& pos, const p3d::Math::Quaternion& rot, const p3d::Math::Vec3& scale,
		bool doPos, bool doRot, bool doScale);
	// Removes every key within kAnimKeyEpsilon of `time` from all three
	// component lists. Returns true if anything was removed.
	bool DeleteKeysAtTime(int clipIndex, int channel, float time);
	// Retimes a key column. Re-sorts afterwards, and collapses onto an
	// existing column at the destination (last write wins) rather than
	// leaving two keys at the same instant.
	bool MoveKeysAtTime(int clipIndex, int channel, float fromTime, float toTime);
	// True when the channel has any key within epsilon of `time`.
	bool HasKeyAtTime(int clipIndex, int channel, float time) const;

	// Drops channels with no keys at all. Called after key deletion so an
	// emptied bone stops occupying a dope-sheet row (and stops being
	// written to the file as a no-op channel).
	void PruneEmptyChannels(int clipIndex);

	// Snaps to the nearest 1/snapFps when snapping is on, and always clamps
	// into [0, duration] of the active clip.
	float SnapTime(float time) const;

	// ---- undo helpers -------------------------------------------------
	// The whole clip list. Coarse on purpose, exactly like
	// MaterialEditorDocument's GraphSnapshot: clip edits are user-scale
	// actions (a key drag, a pose), not per-frame ones, and a snapshot is
	// immune to the aliasing bugs that fine-grained inverse operations on a
	// shared keyframe list invite. MemoryCost() below reports the real size
	// so UndoStack's byte cap governs how many are retained.
	struct ClipsSnapshot {
		std::vector<p3d::Animation> clips;
		int activeClip = -1;
		size_t ByteSize() const;
	};
	ClipsSnapshot Snapshot() const;
	void Restore(const ClipsSnapshot& snap);

	// Captures the current state, runs `edit`, and pushes an undo command
	// that swaps between before/after. Every mutating UI action goes through
	// this - it is the only reason the clip operations above can stay plain
	// mutators with no inverse logic of their own.
	void PushSnapshotEdit(const std::string& description, const std::function<void()>& edit);

	// Same thing for an edit that spans frames rather than happening inside
	// one call - dragging a key along the timeline, dragging a bone gizmo.
	// Begin captures the "before" at mouse-down; End pushes one command
	// covering the whole drag, so a drag is a single Ctrl+Z rather than one
	// per mouse-move. Begin is idempotent while a drag is already open (a
	// drag that starts twice without ending would otherwise lose its
	// original baseline).
	void BeginInteractiveEdit();
	// No-op (and pushes nothing) if the clips are byte-identical to the
	// captured baseline, so a click that selects a key without moving it
	// doesn't litter the undo stack with empty entries.
	void EndInteractiveEdit(const std::string& description);
	bool interactiveEditActive = false;
	ClipsSnapshot interactiveBefore;

	// Both defined out of line in the .cpp, where AnimationPreview is a
	// complete type: `preview` is a unique_ptr to a forward-declared struct,
	// so an implicitly generated constructor or destructor at any other call
	// site would try to instantiate default_delete<AnimationPreview> against
	// an incomplete type and fail to compile.
	AnimationEditorDocument();
	~AnimationEditorDocument();
};

#endif /* ANIMATIONEDITORDOCUMENT_H */
