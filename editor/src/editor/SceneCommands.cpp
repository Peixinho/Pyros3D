#include "SceneCommands.h"
#include "SceneEditor.h"

// ---------------------------------------------------------------------
// AddGameObjectCommand
// ---------------------------------------------------------------------

AddGameObjectCommand::AddGameObjectCommand(SceneEditor* editor, uint32 parentId, const std::string& snapshot,
	bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper, const std::string& name, uint32 initialLiveId)
	: editor_(editor), parentId_(parentId), snapshot_(snapshot), liveId_(initialLiveId)
	, wasCamera_(wasCamera), camSettings_(camSettings), hadHelper_(hadHelper), name_(name)
{
}

void AddGameObjectCommand::Undo()
{
	if (liveId_ != 0)
		editor_->RawDeleteSubtree(liveId_);
	liveId_ = 0;
}

void AddGameObjectCommand::Redo()
{
	SceneObject* obj = editor_->RawInsertSubtree(snapshot_, parentId_, wasCamera_, camSettings_, hadHelper_);
	liveId_ = obj ? obj->GetID() : 0;
	editor_->SelectAndFocusSceneObject(obj);
}

std::string AddGameObjectCommand::Description() const
{
	return "Add '" + name_ + "'";
}

// ---------------------------------------------------------------------
// DeleteGameObjectCommand
// ---------------------------------------------------------------------

DeleteGameObjectCommand::DeleteGameObjectCommand(SceneEditor* editor, uint32 parentId, const std::string& snapshot,
	bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper, const std::string& name)
	: editor_(editor), parentId_(parentId), snapshot_(snapshot), liveId_(0)
	, wasCamera_(wasCamera), camSettings_(camSettings), hadHelper_(hadHelper), name_(name)
{
}

void DeleteGameObjectCommand::Undo()
{
	SceneObject* obj = editor_->RawInsertSubtree(snapshot_, parentId_, wasCamera_, camSettings_, hadHelper_);
	liveId_ = obj ? obj->GetID() : 0;
	editor_->SelectAndFocusSceneObject(obj);
}

void DeleteGameObjectCommand::Redo()
{
	if (liveId_ != 0)
		editor_->RawDeleteSubtree(liveId_);
	liveId_ = 0;
}

std::string DeleteGameObjectCommand::Description() const
{
	return "Delete '" + name_ + "'";
}

// ---------------------------------------------------------------------
// ReparentGameObjectCommand
// ---------------------------------------------------------------------

ReparentGameObjectCommand::ReparentGameObjectCommand(SceneEditor* editor, uint32 childId, uint32 oldParentId,
	uint32 newParentId, const std::string& name)
	: editor_(editor), childId_(childId), oldParentId_(oldParentId), newParentId_(newParentId), name_(name)
{
}

void ReparentGameObjectCommand::Undo()
{
	editor_->GetSceneObjects()->ReparentGameObject(childId_, oldParentId_);
	editor_->MarkSceneDirty();
}

void ReparentGameObjectCommand::Redo()
{
	editor_->GetSceneObjects()->ReparentGameObject(childId_, newParentId_);
	editor_->MarkSceneDirty();
}

std::string ReparentGameObjectCommand::Description() const
{
	return "Reparent '" + name_ + "'";
}

// ---------------------------------------------------------------------
// RenameGameObjectCommand
// ---------------------------------------------------------------------

RenameGameObjectCommand::RenameGameObjectCommand(SceneEditor* editor, uint32 objId, const std::string& oldName, const std::string& newName)
	: editor_(editor), objId_(objId), oldName_(oldName), newName_(newName)
{
}

void RenameGameObjectCommand::Undo()
{
	editor_->GetSceneObjects()->SetName(objId_, oldName_);
	editor_->MarkSceneDirty();
}

void RenameGameObjectCommand::Redo()
{
	editor_->GetSceneObjects()->SetName(objId_, newName_);
	editor_->MarkSceneDirty();
}

std::string RenameGameObjectCommand::Description() const
{
	return "Rename '" + oldName_ + "' to '" + newName_ + "'";
}

// ---------------------------------------------------------------------
// SetTransformCommand
// ---------------------------------------------------------------------

SetTransformCommand::SetTransformCommand(SceneEditor* editor, uint32 objId,
	const Vec3& oldPos, const Vec3& oldRot, const Vec3& oldScale,
	const Vec3& newPos, const Vec3& newRot, const Vec3& newScale,
	const std::string& name)
	: editor_(editor), objId_(objId)
	, oldPos_(oldPos), oldRot_(oldRot), oldScale_(oldScale)
	, newPos_(newPos), newRot_(newRot), newScale_(newScale)
	, name_(name)
{
}

void SetTransformCommand::Undo()
{
	editor_->ApplyTransform(objId_, oldPos_, oldRot_, oldScale_);
}

void SetTransformCommand::Redo()
{
	editor_->ApplyTransform(objId_, newPos_, newRot_, newScale_);
}

std::string SetTransformCommand::Description() const
{
	return "Set Transform '" + name_ + "'";
}

// ---------------------------------------------------------------------
// ReplaceGameObjectCommand
// ---------------------------------------------------------------------

ReplaceGameObjectCommand::ReplaceGameObjectCommand(SceneEditor* editor, uint32 parentId,
	const std::string& beforeSnapshot, const std::string& afterSnapshot,
	bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper,
	uint32 initialLiveId, const std::string& description)
	: editor_(editor), parentId_(parentId)
	, beforeSnapshot_(beforeSnapshot), afterSnapshot_(afterSnapshot), liveId_(initialLiveId)
	, wasCamera_(wasCamera), camSettings_(camSettings), hadHelper_(hadHelper), description_(description)
{
}

void ReplaceGameObjectCommand::Undo()
{
	if (liveId_ != 0)
		editor_->RawDeleteSubtree(liveId_);
	SceneObject* obj = editor_->RawInsertSubtree(beforeSnapshot_, parentId_, wasCamera_, camSettings_, hadHelper_);
	liveId_ = obj ? obj->GetID() : 0;
	editor_->SelectAndFocusSceneObject(obj);
}

void ReplaceGameObjectCommand::Redo()
{
	if (liveId_ != 0)
		editor_->RawDeleteSubtree(liveId_);
	SceneObject* obj = editor_->RawInsertSubtree(afterSnapshot_, parentId_, wasCamera_, camSettings_, hadHelper_);
	liveId_ = obj ? obj->GetID() : 0;
	editor_->SelectAndFocusSceneObject(obj);
}

// ---------------------------------------------------------------------
// ApplyClosureCommand
// ---------------------------------------------------------------------

ApplyClosureCommand::ApplyClosureCommand(std::function<void()> undoFn, std::function<void()> redoFn, const std::string& description)
	: undoFn_(std::move(undoFn)), redoFn_(std::move(redoFn)), description_(description)
{
}

void ApplyClosureCommand::Undo() { undoFn_(); }
void ApplyClosureCommand::Redo() { redoFn_(); }

// ---------------------------------------------------------------------
// AssignMaterialCommand
// ---------------------------------------------------------------------

AssignMaterialCommand::AssignMaterialCommand(SceneEditor* editor, uint32 goId, int submeshIndex,
	std::shared_ptr<p3d::IMaterial> oldMaterial, std::shared_ptr<p3d::IMaterial> newMaterial, const std::string& name)
	: editor_(editor), goId_(goId), submeshIndex_(submeshIndex)
	, oldMaterial_(oldMaterial), newMaterial_(newMaterial), name_(name)
{
}

void AssignMaterialCommand::Undo()
{
	editor_->RawAssignMaterial(goId_, submeshIndex_, oldMaterial_);
}

void AssignMaterialCommand::Redo()
{
	editor_->RawAssignMaterial(goId_, submeshIndex_, newMaterial_);
}

std::string AssignMaterialCommand::Description() const
{
	return "Assign Material on '" + name_ + "'";
}
