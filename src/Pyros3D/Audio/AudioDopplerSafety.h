//============================================================================
// Name        : AudioDopplerSafety.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Velocity clamp shared by AudioManager and AudioSource -
//               not part of the public API (lives under src/, not
//               include/Pyros3D).
//============================================================================

#ifndef PYROS3D_AUDIO_DOPPLERSAFETY_H
#define	PYROS3D_AUDIO_DOPPLERSAFETY_H

#include <Pyros3D/Core/Math/Math.h>
#include <cmath>

namespace p3d { namespace detail {

	// miniaudio's own Doppler formula (ma_doppler_pitch(), internal to
	// miniaudio.h - not part of its public API, so not named directly here)
	// computes a pitch ratio of
	//
	//     (speedOfSound - dopplerFactor * vls) / (speedOfSound - dopplerFactor * vss)
	//
	// where vls/vss are the listener's/source's velocity projected onto the
	// line between them, EACH CLAMPED internally to speedOfSound/dopplerFactor
	// - which stops the denominator going negative, but not from reaching
	// exactly zero once a projected velocity reaches that clamp. speedOfSound
	// is a fixed 343.3 (MA_DEFAULT_SPEED_OF_SOUND, matching OpenAL) that nothing
	// in the public API lets a caller change. Feed in a velocity whose
	// projection reaches that threshold and the result is a division by zero
	// - an Inf/NaN pitch multiplier handed straight to the resampler.
	//
	// This was found the hard way, in NeonPulse: the ball's ordinary top
	// speed (~190 units/sec) plus normal per-frame finite-difference noise
	// (measured up to ~900 units/sec during real play - see
	// [[audio_effects_doppler_buses_filters]]) is well within reach of 343,
	// and once it's crossed, an Inf/NaN pitch reaching the audio callback
	// reads as the reported symptom: sound going silent, followed by the
	// process hanging on shutdown (ma_engine_uninit() waiting on an audio
	// thread stuck processing a non-finite resample ratio).
	//
	// A per-object velocity CLAMP (not a per-object speed-of-sound override,
	// which the public API has no path to) is the only lever available here,
	// and it has to bound the vector's full MAGNITUDE, not just clamp each
	// axis independently - vls/vss are a projection onto an arbitrary
	// direction, which can equal the full magnitude (moving straight at or
	// away from the listener), so anything less than a magnitude clamp can
	// still let that projection through.
	const f32 SPEED_OF_SOUND = 343.3f;

	// Kept below 1.0 with real margin (not up against the exact
	// division-by-zero boundary) because the clamp itself is being applied
	// to already-noisy, discretely-sampled velocity - a value clamped to
	// 99% of the danger line one frame can still read a hair over it the
	// next from ordinary floating-point/finite-difference jitter.
	const f32 SAFETY_MARGIN = 0.75f;

	// The listener's velocity is shared by every source currently playing,
	// each with its own independently-set Doppler factor - so unlike a
	// source clamping its OWN velocity against its OWN factor (exact, no
	// guesswork), the listener has no single factor to clamp against. This
	// is a documented ceiling instead: any source using a factor above this
	// is not fully protected on the listener-velocity side of the formula
	// (it remains fully protected on its own velocity side regardless,
	// since that clamp always uses the real per-source factor).
	const f32 LISTENER_ASSUMED_MAX_DOPPLER_FACTOR = 4.0f;

	// Scales `velocity` down (preserving direction) so its magnitude cannot
	// drive miniaudio's Doppler formula into the division-by-zero case for
	// the given `dopplerFactor`. A no-op if it's already safely under the
	// limit.
	inline Vec3 ClampDopplerVelocity(const Vec3 &velocity, const f32 dopplerFactor)
	{
		const f32 factor = (dopplerFactor > 0.0001f) ? dopplerFactor : 0.0001f;
		const f32 maxSafeSpeed = (SPEED_OF_SOUND / factor) * SAFETY_MARGIN;

		const f32 speed = velocity.magnitude();
		if (speed <= maxSafeSpeed || speed <= 0.0001f)
			return velocity;

		return velocity * (maxSafeSpeed / speed);
	}

} }

#endif	/* PYROS3D_AUDIO_DOPPLERSAFETY_H */
