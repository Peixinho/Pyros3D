//============================================================================
// Name        : AudioEar.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : What two ears make of where a sound is
//============================================================================

#ifndef PYROS3D_AUDIO_EAR_H
#define	PYROS3D_AUDIO_EAR_H

#include <cmath>
#include <cstring>

struct ma_engine;

namespace p3d { namespace detail {

	// A sound placed in the world was only shared out between the left and the
	// right channel by its angle. That says "left" or "right" and little else: in
	// front and behind sound the same, and a shot a little to one side is hard to
	// tell from one a little to the other. What a head adds, and this does to a
	// sound after it has been attenuated for distance:
	//   - it reaches the far ear later (up to two thirds of a millisecond): the
	//     strongest cue there is for which side, and how far round;
	//   - the head is in the way of the far ear: quieter there, and duller - the
	//     high frequencies go first;
	//   - the ears point forward: what is behind is a little duller and quieter in
	//     both - what tells behind from in front.
	// No table of measured ears: a round head and two filters. Plain numbers, so
	// it can be tried without an audio device (tools/tests).
	struct EarModel
	{
		enum { kLine = 256 };
		float line[kLine];
		unsigned at;
		float delay[2], gain[2], coeff[2], low[2];      // [0] left ear, [1] right: as they are now
		bool primed;

		EarModel() { Reset(); }
		void Reset() { std::memset(line, 0, sizeof(line)); at = 0; delay[0] = delay[1] = 0.f; gain[0] = gain[1] = 1.f; coeff[0] = coeff[1] = 1.f; low[0] = low[1] = 0.f; primed = false; }

		// sinA: how far to the right the sound is (-1 left .. 1 right); cosA: how far in
		// front (1) or behind (-1). What each ear is to be given.
		static void Targets(const float sinA, const float cosA, const float sampleRate, float delay[2], float gain[2], float coeff[2])
		{
			const float s = sinA < -1.f ? -1.f : (sinA > 1.f ? 1.f : sinA);
			const float side = std::fabs(s);
			const float behind = cosA < 0.f ? -cosA : 0.f;
			// (a head 17.5 cm across: Woodworth's a/c (angle + sin angle))
			const float itd = 0.000255f * (std::asin(side) + side) * sampleRate;
			const float rearCut = 16000.f - 9500.f * behind;
			const float farCut = 16000.f - 12500.f * side;
			const float cutNear = rearCut, cutFar = farCut < rearCut ? farCut : rearCut;
			const float rearGain = 1.f - 0.22f * behind;
			const int farEar = s > 0.f ? 0 : 1, nearEar = 1 - farEar;
			delay[nearEar] = 0.f; delay[farEar] = itd;
			gain[nearEar] = rearGain; gain[farEar] = rearGain * (1.f - 0.42f * side);
			coeff[nearEar] = 1.f - std::exp(-6.2831853f * cutNear / sampleRate);
			coeff[farEar] = 1.f - std::exp(-6.2831853f * cutFar / sampleRate);
		}

		// in/out: interleaved left, right. The sound as it comes is taken as one
		// (its own left/right share is the angle said again, more weakly).
		void Process(const float* in, float* out, const unsigned frames, const float sinA, const float cosA, const float sampleRate)
		{
			float td[2], tg[2], tc[2];
			Targets(sinA, cosA, sampleRate, td, tg, tc);
			if (!primed) { for (int e = 0; e < 2; e++) { delay[e] = td[e]; gain[e] = tg[e]; coeff[e] = tc[e]; } primed = true; }
			// (moved towards over about four milliseconds: a head that turns, not a switch thrown)
			const float k = 1.f - std::exp(-1.f / (0.004f * sampleRate));
			for (unsigned i = 0; i < frames; i++)
			{
				const float mono = 0.5f * (in[i * 2] + in[i * 2 + 1]);
				line[at & (kLine - 1)] = mono;
				for (int e = 0; e < 2; e++)
				{
					delay[e] += (td[e] - delay[e]) * k;
					gain[e] += (tg[e] - gain[e]) * k;
					coeff[e] += (tc[e] - coeff[e]) * k;
					const float back = delay[e];
					const unsigned whole = (unsigned)back;
					const float part = back - (float)whole;
					const float a = line[(at - whole) & (kLine - 1)];
					const float b = line[(at - whole - 1) & (kLine - 1)];
					const float heard = a + (b - a) * part;
					low[e] += coeff[e] * (heard - low[e]);
					// (1.41: what a sound shared equally between two channels was worth, as one)
					out[i * 2 + e] = low[e] * gain[e] * 1.41f;
				}
				at++;
			}
		}
	};

	// Where the head is: said once a frame (AudioManager::SetListener), read by
	// every sound's ears on the audio thread.
	void EarListenerSet(const float px, const float py, const float pz, const float fx, const float fy, const float fz, const float ux, const float uy, const float uz);

	// The ears of one sound: a node between it and what it plays into.
	void* EarNodeCreate(ma_engine* engine);
	void EarNodeDestroy(void* node);
	// Placed in the world at x, y, z - or not placed at all (played as it is).
	void EarNodeSet(void* node, const bool placed, const float x, const float y, const float z);

} }

#endif	/* PYROS3D_AUDIO_EAR_H */
