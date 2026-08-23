#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

// One reversible edit. Commands own everything they need to invert
// themselves (captured ids, before/after values, JSON snapshots, ...) - the
// UndoStack itself is state-free beyond the two stacks of commands. A
// command's constructor is expected to have already performed the edit (or
// to be constructed right after the caller performed it) - there is no
// separate Do() step, since every command in this codebase is created at
// the exact moment its edit happens, not queued up in advance.
class IUndoableCommand {
public:
	virtual ~IUndoableCommand() {}
	virtual void Undo() = 0;
	// Re-applies the edit after an Undo(). Usually reruns the same logic
	// Undo() reverses, but kept as a distinct method (rather than reusing
	// whatever the constructor did) for commands where the original edit
	// isn't safely repeatable (e.g. reinserting a captured JSON snapshot
	// rather than re-running nondeterministic construction code).
	virtual void Redo() = 0;
	// One line shown in the Edit menu, e.g. "Delete GameObject 'Cube'".
	virtual std::string Description() const = 0;
	// Approximate heap cost, for UndoStack's memory cap. Commands holding a
	// JSON snapshot should report its real size; cheap value-diff commands
	// can rely on the sizeof(*this) default.
	virtual size_t MemoryCost() const { return sizeof(*this); }
};

// A per-document undo/redo history (one per open SceneEditor / per open
// MaterialEditorDocument - see the undo/redo plan). Not thread-safe; all
// editor mutation already happens on the main thread.
class UndoStack {
public:
	explicit UndoStack(size_t maxDepth = 200, size_t maxMemoryBytes = 64u * 1024 * 1024);
	~UndoStack();

	// Takes ownership and pushes onto the undo stack, clearing the redo
	// stack (a new edit invalidates whatever "future" redo pointed at).
	// Does NOT call any method on cmd - the edit is assumed already applied
	// by the time the command is constructed. May evict the oldest entries
	// (bottom of the undo stack) if maxDepth/maxMemoryBytes would otherwise
	// be exceeded; eviction just destroys the command; commands whose
	// captured state owns engine resources must release them from their
	// own destructor.
	void Push(std::unique_ptr<IUndoableCommand> cmd);

	// Called at the end of every Push() that actually stored a command.
	// The host uses this to point Ctrl+Z at whichever document was last
	// EDITED, which is not the same as the one whose window last had focus:
	// an ImGui drag-drop target is never focused by the drop, so dragging a
	// model from the Assets browser into the viewport left focus on Assets
	// and sent the undo to whatever document happened to be focused before
	// it - an open Animation Editor, typically, which then silently undid an
	// unrelated animation edit while the model stayed in the scene.
	std::function<void()> onPush;

	bool CanUndo() const { return !undoStack_.empty(); }
	bool CanRedo() const { return !redoStack_.empty(); }
	void Undo(); // no-op if CanUndo() is false
	void Redo(); // no-op if CanRedo() is false

	std::string UndoDescription() const; // "" if CanUndo() is false
	std::string RedoDescription() const; // "" if CanRedo() is false

	// Drops both stacks, destroying every command (without calling
	// Undo()/Redo() on any of them) - used on New Scene / Load Scene /
	// entering Play Mode. Any resource cleanup a discarded command needs
	// belongs in its destructor, not here.
	void Clear();

	size_t UndoCount() const { return undoStack_.size(); }
	size_t RedoCount() const { return redoStack_.size(); }

private:
	std::vector<std::unique_ptr<IUndoableCommand>> undoStack_;
	std::vector<std::unique_ptr<IUndoableCommand>> redoStack_;
	size_t maxDepth_;
	size_t maxMemoryBytes_;

	void EnforceLimits();
};

// Generic reversible edit for narrow, one-off value changes that don't
// warrant their own command class (camera FOV/Near/Far, per-field light
// color/direction/radius/cone edits, generic-material scalar sliders, ...):
// the call site provides two closures that each know how to apply "their"
// value (typically capturing an owning id/pointer and the specific
// before/after value) via whatever typed setter/resync the field actually
// needs. Deliberately document-agnostic (no scene or material coupling) so
// both SceneEditor and MaterialEditorDocument call sites can share it,
// rather than writing near-identical command classes per document type.
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
