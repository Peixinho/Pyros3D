//============================================================================
// Name        : CloseHook.h
// Description : Optional SDL_QUIT interceptor (return false to cancel close)
//============================================================================

#ifndef PYROS_CLOSEHOOK_H
#define PYROS_CLOSEHOOK_H

namespace PyrosWindowClose {

typedef bool (*HandlerFn)();

inline HandlerFn& Handler()
{
	static HandlerFn fn = 0;
	return fn;
}

inline void SetHandler(HandlerFn fn) { Handler() = fn; }

// true = proceed with Close(); false = cancel (handler will prompt / retry).
inline bool AllowClose()
{
	if (Handler()) return Handler()();
	return true;
}

}

#endif /* PYROS_CLOSEHOOK_H */
