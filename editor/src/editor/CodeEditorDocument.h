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
	bool completionBlockEditorKeys = false; // true the frame Enter/Tab accepted an item
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
	bool WantOpenCompletionChord() const;

private:
	void HandleCompletionKeys();
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
