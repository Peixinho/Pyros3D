//============================================================================
// Name        : LoadProgress.h
// Author      : Duarte Peixinho
// Description : How far a load has got, for whoever is showing it (the
//               player's loading screen). The loaders report; a handler -
//               when one is set - hears it.
//============================================================================

#ifndef LOADPROGRESS_H
#define LOADPROGRESS_H

#include <Pyros3D/Other/Export.h>
#include <functional>
#include <string>

namespace p3d {

	// A load is a few phases - reading the scene, solving its light, ... - each given its share
	// of the whole by whoever runs the load (Phase), and inside each the code doing the work says
	// how far through it is (Report, 0..1 of that phase). Report costs nothing when nobody listens,
	// so a loader can report as often as it likes; the handler is the one to decide how often to draw.
	//
	// Main thread only: the loaders report from where they run, and the handler draws.
	namespace LoadProgress {

		// fraction: of the whole load, 0..1, never going back; stage: the phase's name
		typedef std::function<void(const float fraction, const std::string &stage)> Handler;

		PYROS3D_API void SetHandler(const Handler &handler);
		PYROS3D_API void ClearHandler();
		PYROS3D_API bool HasHandler();

		// From here on, Report(0..1) maps onto [from, to] of the whole load.
		PYROS3D_API void Phase(const float from, const float to, const std::string &stage);
		PYROS3D_API void Report(const float withinPhase);
		// Where the whole load is, 0..1.
		PYROS3D_API float Current();
	}

}

#endif /* LOADPROGRESS_H */
