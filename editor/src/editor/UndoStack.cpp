#include "UndoStack.h"

UndoStack::UndoStack(size_t maxDepth, size_t maxMemoryBytes)
	: maxDepth_(maxDepth), maxMemoryBytes_(maxMemoryBytes)
{
}

UndoStack::~UndoStack()
{
}

void UndoStack::Push(std::unique_ptr<IUndoableCommand> cmd)
{
	if (!cmd) return;
	redoStack_.clear();
	undoStack_.push_back(std::move(cmd));
	EnforceLimits();
}

void UndoStack::Undo()
{
	if (undoStack_.empty()) return;
	std::unique_ptr<IUndoableCommand> cmd = std::move(undoStack_.back());
	undoStack_.pop_back();
	cmd->Undo();
	redoStack_.push_back(std::move(cmd));
}

void UndoStack::Redo()
{
	if (redoStack_.empty()) return;
	std::unique_ptr<IUndoableCommand> cmd = std::move(redoStack_.back());
	redoStack_.pop_back();
	cmd->Redo();
	undoStack_.push_back(std::move(cmd));
}

std::string UndoStack::UndoDescription() const
{
	return undoStack_.empty() ? std::string() : undoStack_.back()->Description();
}

std::string UndoStack::RedoDescription() const
{
	return redoStack_.empty() ? std::string() : redoStack_.back()->Description();
}

void UndoStack::Clear()
{
	// Destroy most-recently-pushed first, matching pop order elsewhere -
	// no known ordering hazard today (each command's captured state is
	// self-contained), but cheap to keep consistent.
	while (!redoStack_.empty()) redoStack_.pop_back();
	while (!undoStack_.empty()) undoStack_.pop_back();
}

void UndoStack::EnforceLimits()
{
	while (undoStack_.size() > maxDepth_)
		undoStack_.erase(undoStack_.begin());

	size_t total = 0;
	for (const auto &cmd : undoStack_) total += cmd->MemoryCost();
	for (const auto &cmd : redoStack_) total += cmd->MemoryCost();
	while (total > maxMemoryBytes_ && !undoStack_.empty())
	{
		total -= undoStack_.front()->MemoryCost();
		undoStack_.erase(undoStack_.begin());
	}
}
