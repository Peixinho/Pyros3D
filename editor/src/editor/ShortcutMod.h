//=============================================================================
// Name        : ShortcutMod.h
// Description : The "command" modifier key, per platform.
//=============================================================================

#ifndef SHORTCUTMOD_H
#define SHORTCUTMOD_H

#include <imgui.h>

namespace p3d {

	// Every shortcut in the editor used to test `io.KeyCtrl` alone. On macOS
	// the key people actually press is Cmd, which ImGui reports as KeySuper -
	// KeyCtrl stays false - so Cmd+Z, Cmd+S, Cmd+Shift+Z and Cmd+D all did
	// nothing at all, on every document type, while the menus advertised
	// "Ctrl+Z" (which a Mac user reads AS Cmd+Z). Undo looked broken rather
	// than unbound, and the only working path was the menu item.
	//
	// Accept EITHER modifier rather than switching on the platform: a Mac
	// user who does press physical Ctrl still gets what they asked for, and
	// nothing in the editor binds Ctrl and Cmd to different actions.
	// CodeEditorDocument already did exactly this - it was the one place in
	// the editor whose shortcuts worked on a Mac.
	inline bool ShortcutMod()
	{
		const ImGuiIO &io = ImGui::GetIO();
		return io.KeyCtrl || io.KeySuper;
	}

	// The label to put in a menu item's shortcut column, so what is advertised
	// is what works.
	inline const char* ShortcutPrefix()
	{
#if defined(__APPLE__)
		return "Cmd+";
#else
		return "Ctrl+";
#endif
	}

	// Convenience for the common "Cmd/Ctrl + <key>, nothing else held" test.
	inline bool ShortcutPressed(const ImGuiKey key, const bool shift = false)
	{
		const ImGuiIO &io = ImGui::GetIO();
		return ShortcutMod() && io.KeyShift == shift && !io.KeyAlt
			&& ImGui::IsKeyPressed(key);
	}

}

#endif
