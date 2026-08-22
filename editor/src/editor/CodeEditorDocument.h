//============================================================================
// Name        : CodeEditorDocument.h
// Description : Lua script documents (dockable peer windows to Scene View)
//============================================================================

#ifndef CODEEDITORDOCUMENT_H
#define CODEEDITORDOCUMENT_H

#include <string>
#include <cstdint>
#include <vector>
#include "TextEditor.h"
#include "LuaCompletion.h"

struct CodeEditorDocument
{
	uint32_t id = 0;
	std::string absolutePath;
	std::string displayName;
	bool dirty = false;
	TextEditor editor;

	// Inline completion — list near caret (F1 / Ctrl+N / typing).
	bool completionOpen = false;
	bool completionOpenAfterType = false; // open after TextEditor consumes this frame's chars
	// Set for exactly one frame when the popup claimed a key that TextEditor
	// would otherwise act on too (Up/Down/PageUp/PageDown/Tab/Enter/Escape).
	// Everything else - printable characters, Backspace, the arrow keys that
	// move the caret, clipboard, undo - is left to TextEditor, which is the
	// only thing that edits the buffer. See HandleCompletionKeys().
	bool completionBlockEditorKeys = false;
	// The popup was opened by the F1 / Ctrl+Space chord rather than by
	// typing, so a stray ' ' from that chord may still be queued.
	bool completionOpenedByChord = false;
	int completionIndex = 0;
	int completionScroll = 0; // first visible row
	int completionOpenedFrame = -999;
	std::string completionPrefix;
	std::string completionReceiver; // e.g. "self" when typing self.foo
	std::vector<LuaCompletion::Item> completionItems;
	const char* completionDebug = "";

	static constexpr int kCompletionVisible = 10;
	// Which CollectCandidates() RefreshCompletionList() calls - set by
	// SetupForLua()/SetupForGlsl(). Everything else in the completion
	// pipeline (open/close triggers, popup, key handling) is language-
	// agnostic (see CodeEditorDocument.cpp), so this one flag is the whole
	// diff between a Lua script window and the Material Editor's Text mode.
	bool completionUseGlsl = false;

	// Bumped every time the user signals "this edit is finished": leaving
	// vim's insert mode, or :w. A host that compiles the buffer (the
	// Material Editor's Text tab) watches this instead of running a
	// keystroke timer, so a shader is never rebuilt from a half-typed line.
	uint32_t commitGeneration = 0;

	// Vim (on by default)
	enum class VimMode { Normal, Insert, Visual, Command };
	bool vimEnabled = true;
	VimMode vimMode = VimMode::Normal;
	std::string vimYank;
	std::string vimCmdLine;
	char vimCmdKind = 0;
	int vimCount = 0;
	char vimOp = 0;
	std::string vimSearch;
	bool vimVisualLine = false;

	void SetupForLua()
	{
		editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
		editor.SetPalette(TextEditor::GetDarkPalette());
		editor.SetShowWhitespaces(false);
		editor.SetTabSize(4);
		completionUseGlsl = false;
	}

	// Material Editor Text mode's counterpart to SetupForLua() - same
	// widget, same vim/completion machinery, just GLSL syntax highlighting
	// and GLSL-flavored completion candidates (see GlslCompletion.h).
	void SetupForGlsl()
	{
		editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
		editor.SetPalette(TextEditor::GetDarkPalette());
		editor.SetShowWhitespaces(false);
		editor.SetTabSize(4);
		completionUseGlsl = true;
	}

	bool LoadFromFile(const std::string& path);
	bool SaveToFile();
	std::string GetDisplayName() const;
	const char* VimModeLabel() const;

	void OpenCompletion();
	void CloseCompletion();
	void HandleEditorInput();
	void AfterEditorRender(); // auto-open after typing; call after editor.Render
	void DrawCompletionPopup();
	void DrawVimStatus();
	void ApplyCompletion(const std::string& item);
	void RefreshCompletionList();
	void EnsureCompletionVisible();
	void InsertUtf8Char(unsigned int c);
	bool CanEditText() const;
	// Whether TextEditor should be handling the keyboard this frame. Kept
	// here rather than duplicated at each host window (Lua scripts and the
	// Material Editor's Text tab both drive the same document the same way).
	bool WantsEditorKeys(bool windowFocused) const;
	bool WantOpenCompletionChord() const;

private:
	// Returns true when the popup consumed the keypress, so nothing further
	// (vim, TextEditor) should also act on it this frame.
	bool HandleCompletionKeys();
	void SyncCompletionToCaret();
	void HandleVimKeys();
	void VimEnterInsert(bool afterCursor);
	void VimYankSelection(bool cut);
	void VimDeleteLine();
	void VimYankLine();
	void VimPaste(bool before);
	void VimSearchNext(bool reverse);
	bool VimConsumeChar(unsigned int c);
};

#endif
