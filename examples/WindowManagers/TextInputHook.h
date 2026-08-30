//============================================================================
// Name        : TextInputHook.h
// Description : Optional SDL_TEXTINPUT callback (a UI with a text field
//               registers a handler)
//============================================================================

#ifndef PYROS_TEXTINPUTHOOK_H
#define PYROS_TEXTINPUTHOOK_H

namespace PyrosTextInput {

typedef void (*HandlerFn)(const char* utf8);

// Set by whoever owns a UICanvas with something typeable on it, or null.
// Window contexts call this on SDL_TEXTINPUT.
//
// A character, not a key: which key produces which character is the
// platform's business (layouts, dead keys, IMEs), and a UI that decoded
// keycodes itself would work on one keyboard and no others.
inline HandlerFn& Handler()
{
	static HandlerFn fn = 0;
	return fn;
}

inline void SetHandler(HandlerFn fn) { Handler() = fn; }

inline void Notify(const char* utf8)
{
	if (Handler() && utf8) Handler()(utf8);
}

// Deliberately no start/stop of the platform's text input: SDL2 has it
// active from the start on desktop, and ImGui drives it for its own fields.
// A second owner turning it off would break every text field in the editor
// to save nothing.

}

#endif /* PYROS_TEXTINPUTHOOK_H */
