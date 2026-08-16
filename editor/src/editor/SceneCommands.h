#pragma once
#include "UndoStack.h"
#include "EditorDebugDraw.h"
#include <Pyros3D/Core/Math/Math.h>
#include <functional>
#include <memory>
#include <string>

using namespace p3d;

class SceneEditor;
namespace p3d { class IMaterial; }

// Structural scene undo commands - pushed by SceneEditor's Op* chokepoint
// methods (SceneEditOps.cpp), one per discrete user-visible edit. Every
// command here only holds plain data (ids, POD, JSON text) and calls back
// into SceneEditor's own low-level Raw*/Apply* primitives to do the actual
// work - no command owns a live engine object, so there is nothing special
// to free when one is discarded (UndoStack::Clear() just destroys them).

// Reverses "a GameObject was created" (used directly for Add, and for the
// add-half of Duplicate - a duplicate is, from undo's point of view,
// indistinguishable from an add of a fully-formed object). `snapshot` is a
// SceneSerializer::SerializeSubtree() JSON string captured right after the
// object reached its final state (all initial components attached).
class AddGameObjectCommand : public IUndoableCommand {
public:
	AddGameObjectCommand(SceneEditor* editor, uint32 parentId, const std::string& snapshot,
		bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper, const std::string& name, uint32 initialLiveId);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;
	size_t MemoryCost() const override { return sizeof(*this) + snapshot_.capacity(); }

private:
	SceneEditor* editor_;
	uint32 parentId_;
	std::string snapshot_;
	uint32 liveId_; // id of the currently-live instance, 0 if undone away
	bool wasCamera_;
	EditorCameraSettings camSettings_;
	bool hadHelper_;
	std::string name_;
};

// Reverses "a GameObject subtree was deleted". Mirror image of
// AddGameObjectCommand - Undo() reinserts from the captured snapshot,
// Redo() deletes the reinserted copy again.
class DeleteGameObjectCommand : public IUndoableCommand {
public:
	DeleteGameObjectCommand(SceneEditor* editor, uint32 parentId, const std::string& snapshot,
		bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper, const std::string& name);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;
	size_t MemoryCost() const override { return sizeof(*this) + snapshot_.capacity(); }

private:
	SceneEditor* editor_;
	uint32 parentId_;
	std::string snapshot_;
	uint32 liveId_; // 0 until Undo() reinserts it
	bool wasCamera_;
	EditorCameraSettings camSettings_;
	bool hadHelper_;
	std::string name_;
};

// Reverses "a GameObject was reparented". No snapshot needed - a pure
// structural move loses nothing.
class ReparentGameObjectCommand : public IUndoableCommand {
public:
	ReparentGameObjectCommand(SceneEditor* editor, uint32 childId, uint32 oldParentId,
		uint32 newParentId, const std::string& name);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;

private:
	SceneEditor* editor_;
	uint32 childId_, oldParentId_, newParentId_;
	std::string name_;
};

// Reverses "a GameObject was renamed" (routes through SceneObjects::SetName
// both ways, so the usual dedup-suffix logic still applies consistently).
class RenameGameObjectCommand : public IUndoableCommand {
public:
	RenameGameObjectCommand(SceneEditor* editor, uint32 objId, const std::string& oldName, const std::string& newName);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;

private:
	SceneEditor* editor_;
	uint32 objId_;
	std::string oldName_, newName_;
};

// Reverses one discrete "set transform" edit (currently only pushed by
// AgentSetTransform - the live gizmo/DragFloat3 drag path gets its own
// commit-boundary-driven command in a later phase).
class SetTransformCommand : public IUndoableCommand {
public:
	SetTransformCommand(SceneEditor* editor, uint32 objId,
		const Vec3& oldPos, const Vec3& oldRot, const Vec3& oldScale,
		const Vec3& newPos, const Vec3& newRot, const Vec3& newScale,
		const std::string& name);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;

private:
	SceneEditor* editor_;
	uint32 objId_;
	Vec3 oldPos_, oldRot_, oldScale_, newPos_, newRot_, newScale_;
	std::string name_;
};

// Reverses "a GameObject's component set changed in place" - covers both
// AttachComponent and DetachComponent, which turn out to be the exact same
// shape from undo's point of view: capture the owner's full subtree before
// and after the edit, and undo/redo just swap which snapshot is live -
// delete the current instance and reinsert the other snapshot, reusing the
// same Raw* primitives AddGameObjectCommand/DeleteGameObjectCommand use.
// Much simpler and more robust than trying to construct/steal individual
// IComponent instances generically across every component kind (mesh
// primitives, lights, physics shapes, audio, scripts).
class ReplaceGameObjectCommand : public IUndoableCommand {
public:
	ReplaceGameObjectCommand(SceneEditor* editor, uint32 parentId,
		const std::string& beforeSnapshot, const std::string& afterSnapshot,
		bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper,
		uint32 initialLiveId, const std::string& description);

	void Undo() override; // delete current instance, reinsert `before`
	void Redo() override; // delete current instance, reinsert `after`
	std::string Description() const override { return description_; }
	size_t MemoryCost() const override { return sizeof(*this) + beforeSnapshot_.capacity() + afterSnapshot_.capacity(); }

private:
	SceneEditor* editor_;
	uint32 parentId_;
	std::string beforeSnapshot_, afterSnapshot_;
	uint32 liveId_;
	bool wasCamera_;
	EditorCameraSettings camSettings_;
	bool hadHelper_;
	std::string description_;
};

// Generic reversible edit for narrow, one-off value changes that don't
// warrant their own command class (camera FOV/Near/Far, per-field light
// color/direction/radius/cone edits, ...): the call site provides two
// closures that each know how to apply "their" value (typically capturing
// the owning id and the specific before/after value) via whatever typed
// setter/resync the field actually needs. Kept separate from the
// dedicated command classes above because those cover structural edits
// where a fixed set of fields is always known ahead of time; this one is
// for call sites where writing a whole class per field would just be
// boilerplate around a single setter call.
class ApplyClosureCommand : public IUndoableCommand {
public:
	ApplyClosureCommand(std::function<void()> undoFn, std::function<void()> redoFn, const std::string& description);

	void Undo() override;
	void Redo() override;
	std::string Description() const override { return description_; }

private:
	std::function<void()> undoFn_, redoFn_;
	std::string description_;
};

// Reverses "a submesh's material was reassigned" - just swaps the
// RenderingMesh::Material shared_ptr back and forth, no serialization
// needed since IMaterial is already refcounted (both the old and new
// material stay alive for the lifetime of this command either way).
class AssignMaterialCommand : public IUndoableCommand {
public:
	AssignMaterialCommand(SceneEditor* editor, uint32 goId, int submeshIndex,
		std::shared_ptr<p3d::IMaterial> oldMaterial, std::shared_ptr<p3d::IMaterial> newMaterial, const std::string& name);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;

private:
	SceneEditor* editor_;
	uint32 goId_;
	int submeshIndex_;
	std::shared_ptr<p3d::IMaterial> oldMaterial_, newMaterial_;
	std::string name_;
};
