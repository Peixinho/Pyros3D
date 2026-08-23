//============================================================================
// Name        : Easing.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shared easing curves
//============================================================================

#ifndef EASING_H
#define	EASING_H

#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	// How a value leaves one end of a span on its way to the other. Used by
	// animation keyframes (per key, see AnimationLoader.h) and by particle
	// size/colour ramps (per system, over a particle's normalised age).
	//
	// Every mode is expressed as a remap of the normalised parameter
	// t in [0,1] rather than as a different way of combining the two values.
	// That matters for rotations: the remapped t still feeds Slerp, so eased
	// rotation stays a proper shortest-arc interpolation instead of degrading
	// into a component-wise blend the way a per-channel Bezier would. It is
	// also what lets the same curve serve particles, where the two ends are
	// colours rather than orientations.
	//
	// Lives here rather than beside the animation keyframe structs because
	// two unrelated subsystems and the editor's curve preview all evaluate
	// it. One definition is the point: a preview drawn from a second copy of
	// these formulas would eventually disagree with what actually plays.
	enum InterpolationMode {
		INTERP_LINEAR = 0,   // constant velocity across the span (the v0 behaviour)
		INTERP_STEP = 1,     // hold this key's value until the next key
		INTERP_EASE_IN = 2,  // start slow
		INTERP_EASE_OUT = 3, // end slow
		INTERP_EASE_BOTH = 4,// slow at both ends
		INTERP_BEZIER = 5    // Hermite on t using OutTangent / the next key's InTangent
	};

	// Number of modes, for iterating a combo box without hard-coding 6.
	static const uint32 kInterpolationModeCount = 6;

	// Display names, indexed by InterpolationMode. Defined here so the
	// animation key popup and the particle inspector cannot drift apart.
	PYROS3D_API const char* InterpolationModeName(const uchar mode);

	// Remaps t according to `mode`. The tangents are only read for
	// INTERP_BEZIER and both default to 1, which reproduces LINEAR exactly -
	// that is why 1/1 is the pair a keyframe is born with.
	//
	// Header-inline deliberately: this sits in the animation sampler's inner
	// loop, which runs per channel per frame for every playing clip.
	inline f32 Ease(const f32 t, const uchar mode,
		const f32 outTangent = 1.f, const f32 nextInTangent = 1.f)
	{
		switch (mode)
		{
		case INTERP_STEP:
			return 0.f;                       // hold until the next key
		case INTERP_EASE_IN:
			return t * t;
		case INTERP_EASE_OUT:
			return t * (2.f - t);
		case INTERP_EASE_BOTH:
			return t * t * (3.f - 2.f * t);   // smoothstep
		case INTERP_BEZIER:
		{
			// Cubic Hermite on the parameter itself, from (0,0) to (1,1)
			// with the two authored slopes. Both tangents at 1 reduces to
			// h00*0 + h10*1 + h01*1 + h11*1 == t, i.e. exactly LINEAR.
			const f32 t2 = t * t;
			const f32 t3 = t2 * t;
			const f32 h10 = t3 - 2.f * t2 + t;
			const f32 h01 = -2.f * t3 + 3.f * t2;
			const f32 h11 = t3 - t2;
			return h10 * outTangent + h01 + h11 * nextInTangent;
		}
		case INTERP_LINEAR:
		default:
			return t;
		}
	}

}

#endif	/* EASING_H */
