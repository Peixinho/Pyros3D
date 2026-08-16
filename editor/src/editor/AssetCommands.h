#pragma once
#include "UndoStack.h"
#include <string>

class ProjectManager;

// Filesystem asset-operation undo commands - the only commands in this
// codebase where Undo()/Redo() perform real disk I/O (via ProjectManager's
// trash primitives, see ProjectManager::MoveToTrash/MoveFromTrash) rather
// than pure in-memory state changes. Deliberately kept separate from
// SceneCommands/MaterialEditorDocument's commands: this is a fundamentally
// different risk domain (real file loss is possible if these are wrong),
// and none of them touch a live SceneEditor/MaterialEditorDocument at all.

// Reverses ProjectManager::DeleteAsset(). `trashRelativePath` is where
// DeleteAsset() moved the file/folder to; Undo() moves it back to
// `originalRelativePath` (which - for a deleted .p3dm - is the whole
// model package folder's path, not the .p3dm file itself; see DeleteAsset's
// own outMovedFromRelativePath doc comment). Redo() re-trashes it, which
// mints a *new* trash path each time (MoveToTrash always does), so
// trashRelativePath_ is reassigned on every Redo(), not fixed at
// construction.
class DeleteAssetCommand : public IUndoableCommand {
public:
	DeleteAssetCommand(ProjectManager* project, const std::string& originalRelativePath,
		const std::string& trashRelativePath);

	void Undo() override;
	void Redo() override;
	std::string Description() const override;

private:
	ProjectManager* project_;
	std::string originalRelativePath_;
	std::string trashRelativePath_;
};

// Reverses an import that overwrote an existing file or model package
// folder (ProjectManager::ImportModel/ImportAssetFile's outTrashed*
// out-params). `trashOfPrevious` empty means nothing occupied the
// destination before the import (Undo just removes what was imported,
// with nothing to put back). Modeled as a swap, symmetric with
// ReplaceGameObjectCommand: Undo() trashes the current (imported) content
// and restores whatever was trashed before (if anything); Redo() reverses
// that exactly. Both trash-path members get reassigned across repeated
// Undo()/Redo() cycles since MoveToTrash mints a fresh name each call.
class ImportOverwriteCommand : public IUndoableCommand {
public:
	ImportOverwriteCommand(ProjectManager* project, const std::string& importedRelativePath,
		const std::string& trashOfPrevious, const std::string& description);

	void Undo() override;
	void Redo() override;
	std::string Description() const override { return description_; }

private:
	ProjectManager* project_;
	std::string importedRelativePath_;
	std::string trashOfPrevious_; // valid (non-empty) when something is waiting to be restored
	std::string trashOfImported_; // valid (non-empty) when the imported content is currently trashed
	std::string description_;
};

// Reverses ProjectManager::CreateLuaScript()/CreateMaterial() - both only
// ever succeed when nothing existed at the destination before (they fail
// outright on a name collision), so there's nothing to trash-and-restore;
// Undo() just captures the just-created file's bytes and deletes it, Redo()
// writes those exact captured bytes back. Capturing bytes (not "recreate
// from the same template") matters because the user may have hand-edited
// the file between creating it and hitting undo.
class CreateAssetCommand : public IUndoableCommand {
public:
	CreateAssetCommand(ProjectManager* project, const std::string& createdRelativePath, const std::string& description);

	void Undo() override;
	void Redo() override;
	std::string Description() const override { return description_; }
	size_t MemoryCost() const override { return sizeof(*this) + savedBytes_.size(); }

private:
	ProjectManager* project_;
	std::string createdRelativePath_;
	std::string savedBytes_; // populated by the first Undo() call
	std::string description_;
};
