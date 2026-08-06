//============================================================================
// Name        : PyrosEmbind.h
// Description : Emscripten Embind surface mirroring PyrosBindings (Lua).
//               Target: full parity with Lua usertypes/functions. Implemented
//               incrementally — see otherplatforms/emscripten/EMBIND_PARITY.md.
//============================================================================

#ifndef PYROSEMBIND_H
#define PYROSEMBIND_H

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

namespace p3d {
	// Force-link all EMSCRIPTEN_BINDINGS TUs (static lib otherwise drops them).
	void EnsurePyrosEmbindLinked();
}

#endif

#endif /* PYROSEMBIND_H */
