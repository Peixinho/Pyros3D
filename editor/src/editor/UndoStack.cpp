#include "UndoStack.h"

UndoStack::UndoStack(size_t maxDepth, size_t maxMemoryBytes)
	: maxDepth_(maxDepth), maxMemoryBytes_(maxMemoryBytes)
{
}

UndoStack::~UndoStack()
{
}

namespace {
	class GroupCommand : public IUndoableCommand {
	public:
		GroupCommand(std::vector<std::unique_ptr<IUndoableCommand>> &&cmds, const std::string& description)
			: cmds_(std::move(cmds)), description_(description) {}
		void Undo() override { for (size_t i = cmds_.size(); i-- > 0;) cmds_[i]->Undo(); }
		void Redo() override { for (size_t i = 0; i < cmds_.size(); i++) cmds_[i]->Redo(); }
		std::string Description() const override { return description_; }
		size_t MemoryCost() const override
		{
			size_t n = sizeof(*this);
			for (size_t i = 0; i < cmds_.size(); i++) n += cmds_[i]->MemoryCost();
			return n;
		}
	private:
		std::vector<std::unique_ptr<IUndoableCommand>> cmds_;
		std::string description_;
	};
}

void UndoStack::BeginGroup(const std::string& description)
{
	if (groupDepth_++ == 0) { group_.clear(); groupDescription_ = description; }
}

void UndoStack::EndGroup()
{
	if (groupDepth_ == 0 || --groupDepth_ > 0) return;
	std::vector<std::unique_ptr<IUndoableCommand>> cmds;
	cmds.swap(group_);
	if (cmds.empty()) return;
	if (cmds.size() == 1) Push(std::move(cmds[0]));
	else Push(std::unique_ptr<IUndoableCommand>(new GroupCommand(std::move(cmds), groupDescription_)));
}

void UndoStack::Push(std::unique_ptr<IUndoableCommand> cmd)
{
	if (!cmd) return;
	if (groupDepth_ > 0) { group_.push_back(std::move(cmd)); return; }
	redoStack_.clear();
	undoStack_.push_back(std::move(cmd));
	EnforceLimits();
	if (onPush) onPush();
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

ApplyClosureCommand::ApplyClosureCommand(std::function<void()> undoFn, std::function<void()> redoFn, const std::string& description)
	: undoFn_(std::move(undoFn)), redoFn_(std::move(redoFn)), description_(description)
{
}

void ApplyClosureCommand::Undo() { undoFn_(); }
void ApplyClosureCommand::Redo() { redoFn_(); }
