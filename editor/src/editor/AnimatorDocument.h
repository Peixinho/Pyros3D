//=============================================================================
// Name        : AnimatorDocument.h
// Description : A .p3dac (animation state machine) open for editing - a
//               dockable peer of the other document windows. The document IS
//               the file: an AnimatorController edited in place, undone as
//               serialized snapshots (the TileSet/SpriteSheet arrangement), so
//               an undo can never restore what the format cannot hold.
//=============================================================================

#ifndef ANIMATORDOCUMENT_H
#define ANIMATORDOCUMENT_H

#include "UndoStack.h"
#include <Pyros3D/AnimationManager/AnimatorController.h>
#include <cstdint>
#include <string>
#include <vector>

struct AnimatorDocument {
	uint32_t id = 0;
	std::string absolutePath;
	std::string displayName = "Animator";
	bool dirty = false;
	p3d::AnimatorController controller;
	UndoStack undo;

	// ---- view (not saved) ---------------------------------------------
	// Selection: a state by name, or a transition by index (-1 none).
	std::string selectedState;
	int selectedTransition = -1;
	// Canvas pan and zoom.
	float panX = 0.f, panY = 0.f, zoom = 1.f;
	// Fit the graph to the view on the next draw (set on open).
	bool frameRequested = true;
	// "Any State"'s node, where its transitions are drawn from.
	float anyX = -260.f, anyY = 0.f;
	// Making a transition: the state it starts at, while the next click picks
	// where it goes ("*" for Any State).
	std::string linkingFrom;
	// Clip names offered for a state - read from the .p3da the user picks to
	// preview with (the controller only stores names).
	std::string clipSourcePath;
	std::vector<std::string> clipNames;

	bool LoadFromFile(const std::string& path, std::string& errOut);
	bool Save(std::string& errOut);
	bool SaveAs(const std::string& path, std::string& errOut);
	// Reads the clip names out of a .p3da (or a .p3d2d's clips) into clipNames.
	bool LoadClipNames(const std::string& absPath);

	// One undo entry per edit: snapshot before, edit, push.
	std::string Snapshot() const;
	void Restore(const std::string& text);
	void PushEdit(const std::string& before, const std::string& what);
	// One entry for a gesture that spans frames (a node drag, a slider).
	void BeginEdit();
	void EndEdit(const std::string& what);
	bool editOpen = false;
	std::string editBefore;

	// ---- layers -------------------------------------------------------------
	// Which machine the graph shows and edits: 0 the base, k the k-th layer.
	int activeLayer = 0;
	p3d::AnimatorLayer& Machine(int index)
	{
		return (index <= 0 || index > (int)controller.layers.size()) ? (p3d::AnimatorLayer&)controller : controller.layers[index - 1];
	}
	p3d::AnimatorLayer& L() { return Machine(activeLayer); }
	// The layer's engine name ("" for the base), for a running Animator.
	std::string LayerName() const { return activeLayer <= 0 || activeLayer > (int)controller.layers.size() ? std::string() : controller.layers[activeLayer - 1].name; }
	std::string AddLayer(const std::string& name, const std::string& rootBone);
	bool RemoveLayer(int index);

	// ---- edits (each pushes its own undo entry) ---------------------------
	std::string AddState(const std::string& name, const std::string& clip, float x, float y);
	bool RemoveState(const std::string& name);
	bool RenameState(const std::string& from, const std::string& to);
	int AddTransition(const std::string& from, const std::string& to);
	bool RemoveTransition(int index);
	std::string AddParam(const std::string& name, int type);
	bool RemoveParam(const std::string& name);
	bool RenameParam(const std::string& from, const std::string& to);
};

#endif
