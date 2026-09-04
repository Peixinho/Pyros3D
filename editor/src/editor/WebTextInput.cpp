//=============================================================================
// Name        : WebTextInput.cpp
// Description : On-screen keyboard support for the browser build.
//
//               A canvas has nothing to focus, so a phone never offers its
//               keyboard: iOS and Android only raise one for a focused,
//               editable DOM element. SDL cannot help here - ImGui's SDL
//               backend only calls SDL_StartTextInput() on __ANDROID__, and
//               emscripten's SDL2 port does not turn that into a DOM element
//               anyway.
//
//               So the page owns a hidden <input> (see editor-shell.js) and
//               this file is the two-way contract with it:
//
//                 C++ -> JS   Module.pyrosWantsTextInput, republished every
//                             frame from ImGui, so the page knows whether to
//                             raise the keyboard on the next touch. It cannot
//                             ask at touch time: focus() is only honoured
//                             INSIDE a user gesture, which is over by the time
//                             a frame has run.
//
//                 JS -> C++   the functions below. Not SDL events: an iOS soft
//                             keyboard does not emit usable keydown at all (it
//                             reports keyCode 229, or nothing), it emits
//                             `input` events carrying the text. Those go
//                             straight into ImGui instead.
//=============================================================================

#if defined(EMSCRIPTEN)

#include "imgui.h"
#include <emscripten.h>
#include <string>

extern "C" {

// Text the soft keyboard produced, as UTF-8. Typing, autocorrect and dictation
// all arrive this way.
EMSCRIPTEN_KEEPALIVE void PyrosWebInputText(const char* utf8)
{
	if (!utf8 || !*utf8) return;
	if (ImGui::GetCurrentContext() == NULL) return;
	ImGui::GetIO().AddInputCharactersUTF8(utf8);
}

// The keys a soft keyboard sends as key events rather than as text: backspace,
// enter, and the arrows if the device has them. `key` is an ImGuiKey.
EMSCRIPTEN_KEEPALIVE void PyrosWebInputKey(int key, int down)
{
	if (ImGui::GetCurrentContext() == NULL) return;
	ImGui::GetIO().AddKeyEvent((ImGuiKey)key, down != 0);
}

// The ImGuiKey values the page needs, read once at startup. Hard-coding them
// in JavaScript would silently break the day ImGui renumbers its enum.
EMSCRIPTEN_KEEPALIVE int PyrosWebKeyBackspace() { return (int)ImGuiKey_Backspace; }
EMSCRIPTEN_KEEPALIVE int PyrosWebKeyEnter()     { return (int)ImGuiKey_Enter; }
EMSCRIPTEN_KEEPALIVE int PyrosWebKeyTab()       { return (int)ImGuiKey_Tab; }
EMSCRIPTEN_KEEPALIVE int PyrosWebKeyLeft()      { return (int)ImGuiKey_LeftArrow; }
EMSCRIPTEN_KEEPALIVE int PyrosWebKeyRight()     { return (int)ImGuiKey_RightArrow; }

}

void PyrosWebPublishTextInputState()
{
	if (ImGui::GetCurrentContext() == NULL) return;
	const bool wants = ImGui::GetIO().WantTextInput;
	// Cheap enough to set every frame, but only crossing into JS when it
	// changes: this runs inside the render loop.
	static int last = -1;
	const int now = wants ? 1 : 0;
	if (now == last) return;
	last = now;
	EM_ASM({ Module.pyrosWantsTextInput = !!$0; }, now);
}

#else

// Nothing to do off the web; the call site is unconditional so it needs a body.
void PyrosWebPublishTextInputState() {}

#endif
