//=============================================================================
// Name        : Character2DDocument.h
// Description : A .p3d2d open for editing - a dockable peer window to Scene
//               View, the Material Editor and the .p3da Animation Editor.
//
//               The document IS the file: a Character2DAsset edited in place
//               and written straight back out through SaveCharacter2D. There
//               is no separate authoring format, for the same reason the
//               animation editor has none - what you edit is what the runtime
//               loads.
//
//               A character is built HERE, not in a scene. You create the
//               asset, add its bones, pin its artwork to them, and key its
//               clips, all against the character's own viewport. A scene then
//               only ever places one and picks a clip. That split is the whole
//               design: a character is a thing that exists on its own and is
//               used by scenes, rather than something a particular scene
//               happens to contain.
//
//               Pure data + file I/O + edit operations. Nothing here touches
//               ImGui: the viewport lives in Character2DPreview, the panels in
//               UI/Character2DEditor.
//=============================================================================

#ifndef CHARACTER2DDOCUMENT_H
#define CHARACTER2DDOCUMENT_H

#include "UndoStack.h"
#include "AnimationEditorDocument.h"
#include <Pyros3D/Assets/Character2D/Character2DAsset.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Character2DPreview;

struct Character2DDocument {
	// Wires `anim` to this document. In the CONSTRUCTOR, not in
	// SyncClipsFromAsset, because a character created from scratch never loads
	// an asset - so it used to skip the wiring entirely and behave differently
	// from an opened one: its keyframe edits went to a hidden undo stack
	// (Ctrl+Z did nothing) and it wrote position keys a loaded character does
	// not. Two behaviours for the same action, decided by where the character
	// came from.
	Character2DDocument();

	uint32_t id = 0;
	// Absolute path of the .p3d2d. Empty for a character that has never been
	// saved (File > New 2D Character), which is the only state in which Save
	// has to fall back to Save As.
	std::string absolutePath;
	std::string displayName = "NewCharacter";
	bool dirty = false;

	// The file's contents.
	p3d::Character2DAsset asset;

	// Project root, so a texture dropped in from anywhere can be stored
	// project-relative and resolved back on load. Set by the host on open.
	std::string projectRoot;

	// ---- selection ---------------------------------------------------
	// Both by NAME. Ids and indices are invalidated by the edit immediately
	// before the one that reads them (deleting a bone renumbers every later
	// bone), and every one of these is read across frames.
	std::string selectedBone;
	std::string selectedSprite;

	// Which of the three authoring stages the window is showing. They are
	// stages, not independent panels: there is nothing to pin artwork to
	// until there are bones, and nothing to key until there is artwork to
	// watch move.
	enum class Mode { Bones, Sprites, Animate };
	Mode mode = Mode::Bones;

	// ---- the clip editing surface ------------------------------------
	// The .p3da animation editor's document, pointed at this character's
	// preview rig. The dope sheet, keying, retiming, interpolation and the
	// transport are all its code - a second implementation of a timeline is
	// a second set of timeline bugs.
	//
	// Its `clips` are the working copy; SyncClipsToAsset/SyncClipsFromAsset
	// move between it and `asset`.
	AnimationEditorDocument anim;

	// The character's own viewport. Owned here (same arrangement as
	// AnimationEditorDocument::preview) but only constructed when the
	// document is first drawn, so a document opened purely to be read by an
	// agent command never builds a renderer.
	std::unique_ptr<Character2DPreview> preview;

	UndoStack undo;

	// ---- file ---------------------------------------------------------
	bool LoadFromFile(const std::string& path, std::string& errOut);
	bool Save(std::string& errOut);
	bool SaveAs(const std::string& path, std::string& errOut);

	// ---- the asset <-> preview contract --------------------------------
	// Bumped by any edit to the bones or the artwork. The preview compares it
	// against its own copy and rebuilds the character when they differ, rather
	// than every caller having to remember to ask for a rebuild - which is
	// exactly the kind of thing that gets forgotten in one path and then looks
	// like "the viewport is stale sometimes".
	uint32_t rigRevision = 1;
	void TouchRig() { rigRevision++; dirty = true; }
	// For edits that change only where a part is DRAWN - offset, size, pivot,
	// draw order. Those are inputs to the per-frame Pivot matrix, so the
	// character does not have to be rebuilt: bumping rigRevision for them
	// re-ran ApplyCharacter2D, which re-loads every texture from disk, on
	// every frame of a slider drag.
	uint32_t partsRevision = 1;
	void TouchParts() { partsRevision++; dirty = true; }
	// Bumped when the clips change, so the preview re-hands them to the
	// engine before playing.
	uint32_t clipsRevision = 1;

	// Copies `anim.clips` into the asset. Called after any clip edit; the
	// dope sheet writes to `anim`, and the asset is what gets saved.
	void SyncClipsToAsset();
	// Points `anim` at this document: whose undo stack keyframe edits land on,
	// and what keying a pose stores. Called from the constructor; separate
	// only so the contract has somewhere to be written down.
	void ConfigureAnimDocument();
	// And back, on load.
	void SyncClipsFromAsset();

	// ---- edit operations -----------------------------------------------
	// Each pushes one undo entry and marks the document dirty. They return
	// false with `errOut` set rather than asserting, because the agent bridge
	// drives exactly these and has to report what went wrong.

	// `parent` empty makes a root bone. `pos` is in the PARENT's frame.
	bool AddBone(const std::string& name, const std::string& parent,
		const p3d::Math::Vec2& pos, std::string& errOut);
	// Removes the bone and everything under it. Sprites pinned to any of them
	// are unpinned rather than deleted - the artwork is still wanted, it just
	// has nothing to follow, and deleting it would lose work over what is
	// usually a re-parenting mistake.
	bool RemoveBone(const std::string& name, std::string& errOut);
	bool RenameBone(const std::string& from, const std::string& to, std::string& errOut);
	// Reparents, keeping the bone's WORLD position. Rejects a cycle.
	bool ReparentBone(const std::string& name, const std::string& newParent, std::string& errOut);
	bool SetBoneRest(const std::string& name, const p3d::Math::Vec2& pos, float rotDegrees,
		std::string& errOut);

	bool AddSprite(const std::string& name, const std::string& texture,
		const std::string& bone, std::string& errOut);
	bool RemoveSprite(const std::string& name, std::string& errOut);
	// Draw order. `delta` -1 moves the sprite one step towards the viewer.
	bool ReorderSprite(const std::string& name, int delta, std::string& errOut);

	bool AddClip(const std::string& name, float duration, std::string& errOut);
	bool RemoveClip(const std::string& name, std::string& errOut);
	bool RenameClip(const std::string& from, const std::string& to, std::string& errOut);

	// Snapshot/restore for the undo stack. The whole asset, not a diff: it is
	// a few kilobytes and a diff would need one encoder per edit kind.
	std::string Snapshot() const;
	void Restore(const std::string& snapshot);
	// Pushes `before` (from Snapshot()) as one undo entry against the current
	// state.
	void PushEdit(const std::string& before, const std::string& label);

	// One gesture = one undo entry, for the property widgets.
	//
	// Snapshot() serialises the whole character, so a panel cannot call it
	// every frame just in case an edit happens - which is what a naive
	// "before = Snapshot()" at the top of a draw function does. These capture
	// it on the frame a widget is grabbed and push it on the frame it is
	// released, so the cost is paid twice per gesture instead of 60 times a
	// second.
	void BeginInteractiveEdit();
	void EndInteractiveEdit(const std::string& label);
	bool interactiveEditActive = false;
	std::string interactiveBefore;

	int FindBone(const std::string& name) const { return asset.FindBone(name); }
	// Bone ids in the preview rig are the asset's bone indices - the preview
	// is built from `asset.bones` in order - so this is the whole conversion.
	int SelectedBoneId() const { return selectedBone.empty() ? -1 : asset.FindBone(selectedBone); }
	int FindSprite(const std::string& name) const;
};

#endif /* CHARACTER2DDOCUMENT_H */
