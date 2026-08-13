//============================================================================
// Name        : FileDropHook.h
// Description : Optional SDL_DROPFILE callback (editor registers a handler)
//============================================================================

#ifndef PYROS_FILEDROPHOOK_H
#define PYROS_FILEDROPHOOK_H

namespace PyrosFileDrop {

typedef void (*HandlerFn)(const char* path);

// Set from the editor (or null). Window contexts call this on SDL_DROPFILE.
inline HandlerFn& Handler()
{
	static HandlerFn fn = 0;
	return fn;
}

inline void SetHandler(HandlerFn fn) { Handler() = fn; }

inline void Notify(const char* path)
{
	if (Handler() && path) Handler()(path);
}

}

#endif /* PYROS_FILEDROPHOOK_H */
