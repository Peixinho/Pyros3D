//============================================================================
// Name        : PhysicsStepping.h
// Author      : Duarte Peixinho
// Description : How many fixed physics steps one frame may run to catch up.
//============================================================================

#ifndef PHYSICSSTEPPING_H
#define PHYSICSSTEPPING_H

#include <Pyros3D/Core/Math/Math.h>
#include <cstdlib>

namespace p3d {

	// Both physics worlds step at a fixed 60Hz and run extra steps when a
	// frame took longer. The cap was 8: once a frame's physics took longer
	// than the frame it simulated, each slow frame ran up to 8 steps, which
	// made the next frame slower still - Physics Stress climbed to 150ms+
	// frames with physics the whole of it. At 3 the simulation keeps real
	// time down to ~20fps and below that runs slower instead of spiralling.
	// PYROS_PHYSICS_MAX_STEPS=<1..16> overrides it (for tuning and A/B).
	inline uint32 MaxPhysicsCatchUpSteps()
	{
		static const uint32 steps = []() -> uint32 {
			const char* env = std::getenv("PYROS_PHYSICS_MAX_STEPS");
			const long v = env ? std::strtol(env, NULL, 10) : 0;
			if (v <= 0) return 3;
			return (uint32)(v > 16 ? 16 : v);
		}();
		return steps;
	}

}

#endif /* PHYSICSSTEPPING_H */
