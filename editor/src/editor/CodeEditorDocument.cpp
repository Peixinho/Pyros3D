//============================================================================
// Name        : CodeEditorDocument.cpp
//============================================================================

#include "CodeEditorDocument.h"
#include "LuaCompletion.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <algorithm>

bool CodeEditorDocument::LoadFromFile(const std::string& path)
{
	std::ifstream in(path);
	if (!in) return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	SetupForLua();
	editor.SetText(ss.str());
	absolutePath = path;
	displayName = std::filesystem::path(path).filename().string();
	dirty = false;
	CloseCompletion();
	vimMode = VimMode::Normal;
	vimCount = 0;
	vimOp = 0;
	vimCmdKind = 0;
	vimCmdLine.clear();
	return true;
}

bool CodeEditorDocument::SaveToFile()
{
	if (absolutePath.empty()) return false;
	std::ofstream out(absolutePath);
	if (!out) return false;
	out << editor.GetText();
	dirty = false;
	return true;
}

std::string CodeEditorDocument::GetDisplayName() const
{
	if (!displayName.empty()) return displayName;
	if (!absolutePath.empty())
		return std::filesystem::path(absolutePath).filename().string();
	return "Untitled.lua";
}

const char* CodeEditorDocument::VimModeLabel() const
{
	if (!vimEnabled) return "INSERT";
	switch (vimMode)
	{
	case VimMode::Normal: return "NORMAL";
	case VimMode::Insert: return "INSERT";
	case VimMode::Visual: return vimVisualLine ? "V-LINE" : "VISUAL";
	case VimMode::Command: return vimCmdKind == '/' ? "SEARCH" : "COMMAND";
	}
	return "?";
}

static bool IsIdentChar(unsigned int c)
{
	return (c < 128) && (std::isalnum((unsigned char)c) || c == '_');
}

void CodeEditorDocument::RefreshCompletionList()
{
	std::string buf;
	if (!completionReceiver.empty())
		buf = editor.GetText();
	LuaCompletion::CollectCandidates(completionReceiver, completionPrefix, buf, completionItems);
	if (completionItems.empty())
		completionIndex = 0;
	else if (completionIndex >= (int)completionItems.size())
		completionIndex = (int)completionItems.size() - 1;
	EnsureCompletionVisible();
}

void CodeEditorDocument::EnsureCompletionVisible()
{
	const int n = (int)completionItems.size();
	if (n <= 0)
	{
		completionScroll = 0;
		return;
	}
	const int vis = kCompletionVisible;
	if (completionIndex < completionScroll)
		completionScroll = completionIndex;
	if (completionIndex >= completionScroll + vis)
		completionScroll = completionIndex - vis + 1;
	const int maxScroll = std::max(0, n - vis);
	if (completionScroll < 0) completionScroll = 0;
	if (completionScroll > maxScroll) completionScroll = maxScroll;
}

bool CodeEditorDocument::CanEditText() const
{
	if (!vimEnabled) return true;
	return vimMode == VimMode::Insert;
}

bool CodeEditorDocument::WantOpenCompletionChord() const
{
	ImGuiIO& io = ImGui::GetIO();
	// F1 never fights macOS / Spotlight / typing.
	if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
		return true;

	const bool mod = io.KeyCtrl || io.KeySuper
		|| ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)
		|| ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper);
	if (!mod || io.KeyAlt || io.KeyShift)
		return false;

	// Do NOT bind Period here — conflicts with typing "self." when a mod is sticky.
	return ImGui::IsKeyPressed(ImGuiKey_Space, false)
		|| ImGui::IsKeyPressed(ImGuiKey_N, false);
}

void CodeEditorDocument::OpenCompletion()
{
	if (vimEnabled && vimMode != VimMode::Insert)
		VimEnterInsert(false);
	completionOpen = true;
	completionOpenAfterType = false;
	completionIndex = 0;
	completionScroll = 0;
	completionOpenedFrame = ImGui::GetFrameCount();
	editor.GetCompletionContext(completionReceiver, completionPrefix);
	RefreshCompletionList();
	completionDebug = completionReceiver.empty()
		? (completionItems.empty() ? "open (0)" : "open")
		: "member";
	ImGui::GetIO().InputQueueCharacters.resize(0);
}

void CodeEditorDocument::CloseCompletion()
{
	completionOpen = false;
	completionOpenAfterType = false;
	completionPrefix.clear();
	completionReceiver.clear();
	completionItems.clear();
	completionDebug = "";
}

void CodeEditorDocument::ApplyCompletion(const std::string& item)
{
	if (item.empty()) return;
	const int n = (int)completionPrefix.size();
	if (n > 0)
	{
		editor.MoveLeft(n, true);
		if (editor.HasSelection())
			editor.Delete();
	}
	editor.InsertText(item);
	dirty = true;
	CloseCompletion();
}

void CodeEditorDocument::InsertUtf8Char(unsigned int c)
{
	char buf[8] = {};
	int n = 0;
	if (c < 0x80)
		buf[n++] = (char)c;
	else if (c < 0x800)
	{
		buf[n++] = (char)(0xC0 + (c >> 6));
		buf[n++] = (char)(0x80 + (c & 0x3F));
	}
	else if (c < 0x10000)
	{
		buf[n++] = (char)(0xE0 + (c >> 12));
		buf[n++] = (char)(0x80 + ((c >> 6) & 0x3F));
		buf[n++] = (char)(0x80 + (c & 0x3F));
	}
	else
	{
		buf[n++] = (char)(0xF0 + (c >> 18));
		buf[n++] = (char)(0x80 + ((c >> 12) & 0x3F));
		buf[n++] = (char)(0x80 + ((c >> 6) & 0x3F));
		buf[n++] = (char)(0x80 + (c & 0x3F));
	}
	buf[n] = 0;
	editor.InsertText(buf);
	dirty = true;
}

void CodeEditorDocument::VimEnterInsert(bool afterCursor)
{
	vimMode = VimMode::Insert;
	vimCount = 0;
	vimOp = 0;
	if (afterCursor)
		editor.MoveRight(1, false);
}

void CodeEditorDocument::VimYankSelection(bool cut)
{
	vimYank = editor.GetSelectedText();
	if (cut && editor.HasSelection())
	{
		editor.Delete();
		dirty = true;
	}
}

void CodeEditorDocument::VimDeleteLine()
{
	const int line = editor.GetCursorPosition().mLine;
	const int total = editor.GetTotalLines();
	if (total <= 0) return;
	TextEditor::Coordinates start(line, 0);
	TextEditor::Coordinates end;
	if (line + 1 < total)
		end = TextEditor::Coordinates(line + 1, 0);
	else
	{
		editor.MoveEnd(false);
		end = editor.GetCursorPosition();
		end.mColumn = end.mColumn + 1;
	}
	editor.SetSelection(start, end);
	vimYank = editor.GetSelectedText();
	if (vimYank.empty() || vimYank.back() != '\n')
		vimYank.push_back('\n');
	editor.Delete();
	dirty = true;
	editor.MoveHome(false);
}

void CodeEditorDocument::VimYankLine()
{
	const int line = editor.GetCursorPosition().mLine;
	const int total = editor.GetTotalLines();
	TextEditor::Coordinates start(line, 0);
	TextEditor::Coordinates end = (line + 1 < total)
		? TextEditor::Coordinates(line + 1, 0)
		: TextEditor::Coordinates(line, 99999);
	editor.SetSelection(start, end);
	vimYank = editor.GetSelectedText();
	if (vimYank.empty() || vimYank.back() != '\n')
		vimYank.push_back('\n');
	editor.SetSelection(editor.GetCursorPosition(), editor.GetCursorPosition());
}

void CodeEditorDocument::VimPaste(bool before)
{
	if (vimYank.empty()) return;
	if (!before)
		editor.MoveRight(1, false);
	editor.InsertText(vimYank);
	dirty = true;
}

void CodeEditorDocument::VimSearchNext(bool reverse)
{
	if (vimSearch.empty()) return;
	const std::string text = editor.GetText();
	const TextEditor::Coordinates cur = editor.GetCursorPosition();
	// Approximate flat offset from line/col via scanning lines.
	std::vector<std::string> lines = editor.GetTextLines();
	size_t offset = 0;
	for (int i = 0; i < cur.mLine && i < (int)lines.size(); ++i)
		offset += lines[i].size() + 1;
	offset += (size_t)std::min(cur.mColumn, lines.empty() ? 0 : (int)lines[std::min(cur.mLine, (int)lines.size() - 1)].size());

	size_t found = std::string::npos;
	if (!reverse)
	{
		found = text.find(vimSearch, offset + 1);
		if (found == std::string::npos)
			found = text.find(vimSearch);
	}
	else
	{
		if (offset > 0)
			found = text.rfind(vimSearch, offset - 1);
		if (found == std::string::npos)
			found = text.rfind(vimSearch);
	}
	if (found == std::string::npos) return;

	int line = 0;
	size_t at = 0;
	while (line < (int)lines.size())
	{
		const size_t lineLen = lines[line].size() + 1;
		if (at + lineLen > found)
		{
			editor.SetCursorPosition(TextEditor::Coordinates(line, (int)(found - at)));
			return;
		}
		at += lineLen;
		++line;
	}
}

bool CodeEditorDocument::VimConsumeChar(unsigned int c)
{
	if (c < 32 || c > 126) return false;
	const char ch = (char)c;
	const int reps = vimCount > 0 ? vimCount : 1;

	if (vimMode == VimMode::Command)
	{
		if (ch == '\n') return false;
		vimCmdLine.push_back(ch);
		return true;
	}

	if (vimMode == VimMode::Insert)
		return false;

	// Visual mode
	if (vimMode == VimMode::Visual)
	{
		if (ch == 'h') { editor.MoveLeft(reps, true); return true; }
		if (ch == 'l') { editor.MoveRight(reps, true); return true; }
		if (ch == 'j') { editor.MoveDown(reps, true); return true; }
		if (ch == 'k') { editor.MoveUp(reps, true); return true; }
		if (ch == 'w') { for (int i = 0; i < reps; ++i) editor.MoveRight(1, true, true); return true; }
		if (ch == 'b') { for (int i = 0; i < reps; ++i) editor.MoveLeft(1, true, true); return true; }
		if (ch == '0') { editor.MoveHome(true); return true; }
		if (ch == '$') { editor.MoveEnd(true); return true; }
		if (ch == 'y') { VimYankSelection(false); vimMode = VimMode::Normal; return true; }
		if (ch == 'd' || ch == 'x') { VimYankSelection(true); vimMode = VimMode::Normal; return true; }
		if (ch == 'v' || ch == 'V') { vimMode = VimMode::Normal; editor.SetSelection(editor.GetCursorPosition(), editor.GetCursorPosition()); return true; }
		return true; // swallow
	}

	// Normal mode — numeric prefix
	if (ch >= '1' && ch <= '9' && vimOp == 0)
	{
		vimCount = vimCount * 10 + (ch - '0');
		return true;
	}
	if (ch == '0' && vimCount > 0)
	{
		vimCount = vimCount * 10;
		return true;
	}

	if (vimOp == 'g')
	{
		if (ch == 'g')
		{
			editor.SetCursorPosition(TextEditor::Coordinates(0, 0));
			vimOp = 0;
			vimCount = 0;
			return true;
		}
		vimOp = 0;
		return true;
	}

	if (vimOp == 'd')
	{
		if (ch == 'd')
		{
			for (int i = 0; i < reps; ++i) VimDeleteLine();
			vimOp = 0;
			vimCount = 0;
			return true;
		}
		if (ch == 'w')
		{
			for (int i = 0; i < reps; ++i)
			{
				editor.MoveRight(1, true, true);
				if (editor.HasSelection()) editor.Delete();
			}
			dirty = true;
			vimOp = 0;
			vimCount = 0;
			return true;
		}
		vimOp = 0;
		vimCount = 0;
		return true;
	}

	if (vimOp == 'c')
	{
		if (ch == 'c')
		{
			for (int i = 0; i < reps; ++i)
			{
				const int line = editor.GetCursorPosition().mLine;
				editor.SetSelection(TextEditor::Coordinates(line, 0), TextEditor::Coordinates(line, 99999));
				VimYankSelection(true);
			}
			VimEnterInsert(false);
			vimOp = 0;
			vimCount = 0;
			return true;
		}
		if (ch == 'w')
		{
			for (int i = 0; i < reps; ++i)
			{
				editor.MoveRight(1, true, true);
				if (editor.HasSelection()) editor.Delete();
			}
			dirty = true;
			VimEnterInsert(false);
			vimOp = 0;
			vimCount = 0;
			return true;
		}
		vimOp = 0;
		vimCount = 0;
		return true;
	}

	if (vimOp == 'y')
	{
		if (ch == 'y')
		{
			VimYankLine();
			vimOp = 0;
			vimCount = 0;
			return true;
		}
		vimOp = 0;
		vimCount = 0;
		return true;
	}

	switch (ch)
	{
	case 'h': editor.MoveLeft(reps, false); break;
	case 'l': editor.MoveRight(reps, false); break;
	case 'j': editor.MoveDown(reps, false); break;
	case 'k': editor.MoveUp(reps, false); break;
	case 'w': for (int i = 0; i < reps; ++i) editor.MoveRight(1, false, true); break;
	case 'b': for (int i = 0; i < reps; ++i) editor.MoveLeft(1, false, true); break;
	case 'e': for (int i = 0; i < reps; ++i) editor.MoveRight(1, false, true); break;
	case '0': editor.MoveHome(false); break;
	case '$': editor.MoveEnd(false); break;
	case 'G':
		editor.SetCursorPosition(TextEditor::Coordinates(std::max(0, editor.GetTotalLines() - 1), 0));
		break;
	case 'g': vimOp = 'g'; vimCount = 0; return true;
	case 'i': VimEnterInsert(false); break;
	case 'a': VimEnterInsert(true); break;
	case 'A': editor.MoveEnd(false); VimEnterInsert(false); break;
	case 'I': editor.MoveHome(false); VimEnterInsert(false); break;
	case 'o':
		editor.MoveEnd(false);
		editor.InsertText("\n");
		dirty = true;
		VimEnterInsert(false);
		break;
	case 'O':
		editor.MoveHome(false);
		editor.InsertText("\n");
		editor.MoveUp(1, false);
		dirty = true;
		VimEnterInsert(false);
		break;
	case 'x':
		for (int i = 0; i < reps; ++i) editor.Delete();
		dirty = true;
		break;
	case 'X':
		for (int i = 0; i < reps; ++i)
		{
			editor.MoveLeft(1, true);
			if (editor.HasSelection()) editor.Delete();
		}
		dirty = true;
		break;
	case 'D':
		// Delete from cursor to end of line (inclusive of rest of line).
		editor.MoveEnd(true);
		if (editor.HasSelection())
			VimYankSelection(true);
		break;
	case 'C':
		// Change from cursor to end of line → insert mode.
		editor.MoveEnd(true);
		if (editor.HasSelection())
			VimYankSelection(true);
		VimEnterInsert(false);
		break;
	case 'd': vimOp = 'd'; return true;
	case 'c': vimOp = 'c'; return true;
	case 'y': vimOp = 'y'; return true;
	case 'p': VimPaste(false); break;
	case 'P': VimPaste(true); break;
	case 'u': editor.Undo(reps); break;
	case 'v':
		vimMode = VimMode::Visual;
		vimVisualLine = false;
		break;
	case 'V':
	{
		vimMode = VimMode::Visual;
		vimVisualLine = true;
		const int line = editor.GetCursorPosition().mLine;
		editor.SetSelection(TextEditor::Coordinates(line, 0), TextEditor::Coordinates(line, 99999));
		break;
	}
	case '/':
		vimMode = VimMode::Command;
		vimCmdKind = '/';
		vimCmdLine.clear();
		break;
	case 'n': VimSearchNext(false); break;
	case 'N': VimSearchNext(true); break;
	case ':':
		vimMode = VimMode::Command;
		vimCmdKind = ':';
		vimCmdLine.clear();
		break;
	default:
		break;
	}
	vimCount = 0;
	return true;
}

void CodeEditorDocument::HandleVimKeys()
{
	if (!vimEnabled) return;
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		if (completionOpen)
			CloseCompletion();
		else if (vimMode == VimMode::Command)
		{
			vimMode = VimMode::Normal;
			vimCmdLine.clear();
			vimCmdKind = 0;
		}
		else
		{
			vimMode = VimMode::Normal;
			vimCount = 0;
			vimOp = 0;
			editor.SetSelection(editor.GetCursorPosition(), editor.GetCursorPosition());
		}
		return;
	}

	if (vimMode == VimMode::Command)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			if (vimCmdKind == '/')
			{
				vimSearch = vimCmdLine;
				VimSearchNext(false);
			}
			else if (vimCmdKind == ':')
			{
				if (vimCmdLine == "w" || vimCmdLine == "wq")
					SaveToFile();
				// :q ignored here — window close is host UI
			}
			vimMode = VimMode::Normal;
			vimCmdLine.clear();
			vimCmdKind = 0;
			return;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
		{
			if (!vimCmdLine.empty())
				vimCmdLine.pop_back();
			else
			{
				vimMode = VimMode::Normal;
				vimCmdKind = 0;
			}
			return;
		}
		for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
		{
			ImWchar c = io.InputQueueCharacters[i];
			if (c >= 32 && c < 127)
				vimCmdLine.push_back((char)c);
		}
		io.InputQueueCharacters.resize(0);
		return;
	}

	if (vimMode == VimMode::Insert)
	{
		// Let completion / typing path handle insert; Ctrl+O → normal one-shot not implemented.
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R))
		{
			editor.Redo();
			return;
		}
		return;
	}

	// Normal / Visual: swallow text input into vim commands
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R))
	{
		editor.Redo();
		return;
	}

	for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
	{
		ImWchar c = io.InputQueueCharacters[i];
		if (c != 0)
			VimConsumeChar((unsigned int)c);
	}
	io.InputQueueCharacters.resize(0);
}

void CodeEditorDocument::HandleCompletionKeys()
{
	ImGuiIO& io = ImGui::GetIO();
	if (!completionOpen) return;

	// Strip leftover Space from open-chords; do NOT block typing/filter.
	const int age = ImGui::GetFrameCount() - completionOpenedFrame;
	if (age <= 2 && ImGui::IsKeyDown(ImGuiKey_Space) && !io.InputQueueCharacters.empty())
	{
		ImVector<ImWchar> kept;
		for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
		{
			const ImWchar c = io.InputQueueCharacters[i];
			if (c != ' ')
				kept.push_back(c);
		}
		io.InputQueueCharacters.swap(kept);
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		CloseCompletion();
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
	{
		if (completionIndex > 0) --completionIndex;
		EnsureCompletionVisible();
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
	{
		if (completionIndex + 1 < (int)completionItems.size())
			++completionIndex;
		EnsureCompletionVisible();
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
	{
		completionIndex = std::max(0, completionIndex - kCompletionVisible);
		EnsureCompletionVisible();
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
	{
		completionIndex = std::min((int)completionItems.size() - 1, completionIndex + kCompletionVisible);
		if (completionIndex < 0) completionIndex = 0;
		EnsureCompletionVisible();
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)
		|| ImGui::IsKeyPressed(ImGuiKey_Tab))
	{
		if (!completionItems.empty())
			ApplyCompletion(completionItems[(size_t)completionIndex].text);
		else
			CloseCompletion();
		// Stop TextEditor from inserting '\n' / tab on this same keypress.
		completionBlockEditorKeys = true;
		io.InputQueueCharacters.resize(0);
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
	{
		editor.MoveLeft(1, true);
		if (editor.HasSelection()) editor.Delete();
		dirty = true;
		editor.GetCompletionContext(completionReceiver, completionPrefix);
		completionIndex = 0;
		RefreshCompletionList();
		// Keep open after "self." (empty member prefix); close only with no context.
		if (completionPrefix.empty() && completionReceiver.empty())
			CloseCompletion();
		return;
	}

	if (io.InputQueueCharacters.empty()) return;
	for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
	{
		const ImWchar c = io.InputQueueCharacters[i];
		if (c == 0 || c == '\n' || c == '\r') continue;
		if (c == ' ' && ImGui::IsKeyDown(ImGuiKey_Space))
			continue;
		// Member access: stay open and switch to member list.
		if (c == '.' || c == ':')
		{
			InsertUtf8Char((unsigned int)c);
			editor.GetCompletionContext(completionReceiver, completionPrefix);
			completionIndex = 0;
			RefreshCompletionList();
			completionDebug = "member";
			continue;
		}
		if (!IsIdentChar((unsigned int)c))
		{
			InsertUtf8Char((unsigned int)c);
			CloseCompletion();
			break;
		}
		InsertUtf8Char((unsigned int)c);
		editor.GetCompletionContext(completionReceiver, completionPrefix);
		completionIndex = 0;
		RefreshCompletionList();
	}
	io.InputQueueCharacters.resize(0);
}

void CodeEditorDocument::HandleEditorInput()
{
	ImGuiIO& io = ImGui::GetIO();

	if (WantOpenCompletionChord())
	{
		OpenCompletion();
		io.InputQueueCharacters.resize(0);
		return;
	}

	if (completionOpen)
	{
		HandleCompletionKeys();
		return;
	}

	if (vimEnabled)
		HandleVimKeys();

	// If the user is typing an identifier or member access, open after Render.
	if (!completionOpen && CanEditText() && !io.KeyCtrl && !io.KeySuper && !io.KeyAlt)
	{
		for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
		{
			const ImWchar c = io.InputQueueCharacters[i];
			if (IsIdentChar((unsigned int)c) || c == '.' || c == ':')
			{
				completionOpenAfterType = true;
				break;
			}
		}
	}
}

void CodeEditorDocument::AfterEditorRender()
{
	if (!completionOpenAfterType)
		return;
	completionOpenAfterType = false;
	if (completionOpen)
		return;
	editor.GetCompletionContext(completionReceiver, completionPrefix);
	// Open for "self." (empty prefix but has receiver) or any non-empty prefix.
	if (completionPrefix.empty() && completionReceiver.empty())
		return;
	completionOpen = true;
	completionIndex = 0;
	completionScroll = 0;
	completionOpenedFrame = ImGui::GetFrameCount();
	RefreshCompletionList();
	completionDebug = completionReceiver.empty() ? "typed" : "member";
}

void CodeEditorDocument::DrawVimStatus()
{
	if (!vimEnabled) return;
	ImVec4 col = ImVec4(0.4f, 0.85f, 0.5f, 1.f);
	if (vimMode == VimMode::Insert) col = ImVec4(0.45f, 0.7f, 1.f, 1.f);
	else if (vimMode == VimMode::Visual) col = ImVec4(1.f, 0.75f, 0.35f, 1.f);
	else if (vimMode == VimMode::Command) col = ImVec4(0.9f, 0.5f, 0.9f, 1.f);
	ImGui::TextColored(col, "-- %s --", VimModeLabel());
	if (vimMode == VimMode::Command)
	{
		ImGui::SameLine();
		ImGui::Text("%c%s", vimCmdKind ? vimCmdKind : ':', vimCmdLine.c_str());
	}
	else if (vimCount > 0 || vimOp)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("  %d%c", vimCount, vimOp ? vimOp : ' ');
	}
}

void CodeEditorDocument::DrawCompletionPopup()
{
	if (!completionOpen) return;
	RefreshCompletionList();

	ImVec2 pos(80.f, 80.f);
	if (editor.HasCursorScreenPos())
	{
		pos = editor.GetCursorScreenPos();
		pos.y += editor.GetLineHeight() + 2.f;
	}
	else
	{
		pos = ImGui::GetMousePos();
		pos.y += 16.f;
	}

	const float lineH = ImGui::GetTextLineHeightWithSpacing();
	const int n = (int)completionItems.size();
	const int vis = std::max(1, std::min(n > 0 ? n : 1, kCompletionVisible));
	EnsureCompletionVisible();
	const ImVec2 box(320.f, 8.f + (float)vis * lineH);
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	if (vp)
	{
		if (pos.y + box.y > vp->WorkPos.y + vp->WorkSize.y)
			pos.y = (editor.HasCursorScreenPos() ? editor.GetCursorScreenPos().y : pos.y) - box.y - 2.f;
		if (pos.x + box.x > vp->WorkPos.x + vp->WorkSize.x)
			pos.x = vp->WorkPos.x + vp->WorkSize.x - box.x;
		if (pos.x < vp->WorkPos.x) pos.x = vp->WorkPos.x;
		if (pos.y < vp->WorkPos.y) pos.y = vp->WorkPos.y;
	}

	ImGuiIO& io = ImGui::GetIO();
	const bool hover = io.MousePos.x >= pos.x && io.MousePos.x <= pos.x + box.x
		&& io.MousePos.y >= pos.y && io.MousePos.y <= pos.y + box.y;
	if (hover && io.MouseWheel != 0.f && n > kCompletionVisible)
	{
		completionScroll -= (int)io.MouseWheel;
		const int maxScroll = std::max(0, n - kCompletionVisible);
		if (completionScroll < 0) completionScroll = 0;
		if (completionScroll > maxScroll) completionScroll = maxScroll;
		if (completionIndex < completionScroll)
			completionIndex = completionScroll;
		if (completionIndex >= completionScroll + kCompletionVisible)
			completionIndex = completionScroll + kCompletionVisible - 1;
	}
	if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && n > 0)
	{
		const float localY = io.MousePos.y - pos.y - 4.f;
		int row = (int)(localY / lineH);
		int idx = completionScroll + row;
		if (idx >= 0 && idx < n)
			ApplyCompletion(completionItems[(size_t)idx].text);
	}

	ImDrawList* dl = ImGui::GetForegroundDrawList();
	const ImU32 bg = IM_COL32(28, 30, 36, 250);
	const ImU32 bd = IM_COL32(80, 160, 255, 255);
	const ImU32 tx = IM_COL32(230, 230, 235, 255);
	const ImU32 hi = IM_COL32(50, 90, 160, 255);
	dl->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y), bg, 4.f);
	dl->AddRect(pos, ImVec2(pos.x + box.x, pos.y + box.y), bd, 4.f, 0, 2.0f);

	if (completionItems.empty())
	{
		dl->AddText(ImVec2(pos.x + 8.f, pos.y + 4.f), IM_COL32(160, 160, 160, 255), "(no matches)");
		return;
	}

	const int first = completionScroll;
	const int last = std::min(n, first + kCompletionVisible);
	for (int i = first; i < last; ++i)
	{
		const int row = i - first;
		const float y = pos.y + 4.f + (float)row * lineH;
		if (i == completionIndex)
			dl->AddRectFilled(ImVec2(pos.x + 2.f, y), ImVec2(pos.x + box.x - 2.f, y + lineH), hi, 2.f);

		const LuaCompletion::Item& it = completionItems[(size_t)i];
		const unsigned rgb = LuaCompletion::KindColor(it.kind);
		const ImU32 iconCol = IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 255);
		dl->AddText(ImVec2(pos.x + 8.f, y), iconCol, LuaCompletion::KindIcon(it.kind));
		dl->AddText(ImVec2(pos.x + 28.f, y), tx, it.text.c_str());
	}

	// Scrollbar thumb when list is longer than the window.
	if (n > kCompletionVisible)
	{
		const float trackX = pos.x + box.x - 6.f;
		const float trackTop = pos.y + 4.f;
		const float trackH = box.y - 8.f;
		dl->AddRectFilled(ImVec2(trackX, trackTop), ImVec2(trackX + 3.f, trackTop + trackH),
			IM_COL32(60, 60, 70, 200), 2.f);
		const float thumbH = std::max(8.f, trackH * ((float)kCompletionVisible / (float)n));
		const float thumbT = trackTop + ((float)completionScroll / (float)(n - kCompletionVisible)) * (trackH - thumbH);
		dl->AddRectFilled(ImVec2(trackX, thumbT), ImVec2(trackX + 3.f, thumbT + thumbH),
			IM_COL32(120, 160, 220, 220), 2.f);
	}
}
