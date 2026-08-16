#include "AssetCommands.h"
#include "ProjectManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------
// DeleteAssetCommand
// ---------------------------------------------------------------------

DeleteAssetCommand::DeleteAssetCommand(ProjectManager* project, const std::string& originalRelativePath,
	const std::string& trashRelativePath)
	: project_(project), originalRelativePath_(originalRelativePath), trashRelativePath_(trashRelativePath)
{
}

void DeleteAssetCommand::Undo()
{
	std::string err;
	project_->MoveFromTrash(trashRelativePath_, project_->AbsolutePath(originalRelativePath_), &err);
	trashRelativePath_.clear();
}

void DeleteAssetCommand::Redo()
{
	std::string err;
	trashRelativePath_ = project_->MoveToTrash(project_->AbsolutePath(originalRelativePath_), &err);
}

std::string DeleteAssetCommand::Description() const
{
	return "Delete '" + originalRelativePath_ + "'";
}

// ---------------------------------------------------------------------
// ImportOverwriteCommand
// ---------------------------------------------------------------------

ImportOverwriteCommand::ImportOverwriteCommand(ProjectManager* project, const std::string& importedRelativePath,
	const std::string& trashOfPrevious, const std::string& description)
	: project_(project), importedRelativePath_(importedRelativePath)
	, trashOfPrevious_(trashOfPrevious), description_(description)
{
}

void ImportOverwriteCommand::Undo()
{
	std::string err;
	const std::string dest = project_->AbsolutePath(importedRelativePath_);
	// Trash whatever the import left at `dest`, then bring back whatever
	// (if anything) occupied it before the import.
	trashOfImported_ = project_->MoveToTrash(dest, &err);
	if (!trashOfPrevious_.empty())
	{
		project_->MoveFromTrash(trashOfPrevious_, dest, &err);
		trashOfPrevious_.clear();
	}
}

void ImportOverwriteCommand::Redo()
{
	std::string err;
	const std::string dest = project_->AbsolutePath(importedRelativePath_);
	// If Undo() restored the previous content, trash it again first (a
	// fresh trash entry, same as any other re-trash) so the imported
	// content can retake the slot.
	std::error_code ec;
	if (fs::exists(dest, ec))
		trashOfPrevious_ = project_->MoveToTrash(dest, &err);
	if (!trashOfImported_.empty())
	{
		project_->MoveFromTrash(trashOfImported_, dest, &err);
		trashOfImported_.clear();
	}
}

// ---------------------------------------------------------------------
// CreateAssetCommand
// ---------------------------------------------------------------------

CreateAssetCommand::CreateAssetCommand(ProjectManager* project, const std::string& createdRelativePath, const std::string& description)
	: project_(project), createdRelativePath_(createdRelativePath), description_(description)
{
}

void CreateAssetCommand::Undo()
{
	const std::string abs = project_->AbsolutePath(createdRelativePath_);
	std::ifstream in(abs, std::ios::binary);
	if (in)
	{
		std::ostringstream ss;
		ss << in.rdbuf();
		savedBytes_ = ss.str();
	}
	std::error_code ec;
	fs::remove(abs, ec);
}

void CreateAssetCommand::Redo()
{
	const std::string abs = project_->AbsolutePath(createdRelativePath_);
	std::ofstream out(abs, std::ios::binary);
	if (out)
		out << savedBytes_;
}
