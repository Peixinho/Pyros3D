//============================================================================
// Name        : AudioReverb.h
// Description : The space a sound is heard in - what comes back off the walls.
//               Two parts, as a real space has them:
//                 - early reflections: a handful of distinct, slightly dulled
//                   copies 7-60 ms behind the sound (the slap off a facade, the
//                   floor, the other side of a street) - what says how big the
//                   space is and how near its walls are;
//                 - the tail: everything after, too dense to hear apart, dying
//                   away (Jezar's Freeverb: eight damped combs into four
//                   allpasses an ear, the right ear's tuned a little apart from
//                   the left's for width).
//               Plain numbers, so it can be tried without an audio device
//               (tools/tests); AudioReverb.cpp puts it in miniaudio's graph.
//============================================================================

#ifndef PYROS3D_AUDIO_REVERB_H
#define	PYROS3D_AUDIO_REVERB_H

#include <cmath>
#include <cstring>
#include <vector>

struct ma_engine;

namespace p3d { namespace detail {

	struct ReverbSettings
	{
		float room = 0.5f;        // 0 small and dead .. 1 a hall (how long the tail rings)
		float damping = 0.5f;     // 0 bright walls (glass, stone) .. 1 soft (curtains, people)
		float wet = 0.25f;        // how loud the space is against the sound itself
		float dry = 1.f;
		float preDelayMs = 12.f;  // how far off the nearest wall is (sound covers 34 cm a millisecond)
		float early = 0.6f;       // how loud the first reflections are against the tail
		float width = 1.f;        // 0 mono .. 1 the two ears apart
	};

	class ReverbModel
	{
	public:
		void Init(const float sampleRate)
		{
			rate = sampleRate;
			const float k = sampleRate / 44100.f;
			// (Freeverb's tunings at 44.1 kHz, scaled to the device's rate)
			static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
			static const int allT[4] = { 556, 441, 341, 225 };
			const int spread = 23;
			for (int ch = 0; ch < 2; ch++)
			{
				for (int i = 0; i < 8; i++) comb[ch][i].Size((int)((combT[i] + ch * spread) * k));
				for (int i = 0; i < 4; i++) all[ch][i].Size((int)((allT[i] + ch * spread) * k));
			}
			pre.assign((size_t)(0.25f * sampleRate) + 1, 0.f);
			preAt = 0;
			erLine.assign((size_t)(0.12f * sampleRate) + 1, 0.f);
			erAt = 0;
			erLow[0] = erLow[1] = 0.f;
			Set(ReverbSettings());
		}

		void Set(const ReverbSettings &s)
		{
			settings = s;
			const float room = Clamp(s.room, 0.f, 1.f), damp = Clamp(s.damping, 0.f, 1.f);
			const float feedback = 0.7f + 0.28f * room;           // (Freeverb's roomsize scale and offset)
			const float damping = 0.4f * damp;
			for (int ch = 0; ch < 2; ch++)
				for (int i = 0; i < 8; i++) { comb[ch][i].feedback = feedback; comb[ch][i].damp = damping; }
			const float width = Clamp(s.width, 0.f, 1.f);
			wet1 = s.wet * (width * 0.5f + 0.5f);
			wet2 = s.wet * ((1.f - width) * 0.5f);
			preDelay = (int)Clamp(s.preDelayMs * 0.001f * rate, 0.f, (float)pre.size() - 1.f);
			// Early reflections: six taps spread over the first ~60 ms, further apart in a bigger
			// space, alternating ears, each quieter than the last; the walls take the top off them.
			const float span = 0.018f + 0.045f * room;
			static const float at[6] = { 0.12f, 0.27f, 0.41f, 0.58f, 0.77f, 1.f };
			static const float gain[6] = { 0.84f, 0.70f, 0.60f, 0.48f, 0.38f, 0.30f };
			for (int i = 0; i < 6; i++)
			{
				erTap[i] = (int)Clamp(at[i] * span * rate, 1.f, (float)erLine.size() - 1.f);
				erGain[i] = gain[i] * s.early;
			}
			erCoeff = 1.f - std::exp(-2.f * 3.14159265f * (9000.f - 6000.f * damp) / rate);
		}

		// Interleaved stereo in, interleaved stereo out (may be the same buffer).
		void Process(const float* in, float* out, const unsigned frames)
		{
			const float fixedGain = 0.015f;
			const size_t preN = pre.size(), erN = erLine.size();
			for (unsigned f = 0; f < frames; f++)
			{
				const float l = in[f * 2], r = in[f * 2 + 1];
				const float mono = (l + r) * 0.5f;
				// the nearest wall's distance first
				pre[preAt] = mono;
				const float delayed = pre[(preAt + preN - (size_t)preDelay) % preN];
				preAt = (preAt + 1) % preN;
				// early reflections, off the delayed sound
				erLine[erAt] = delayed;
				float er[2] = { 0.f, 0.f };
				for (int i = 0; i < 6; i++)
					er[i & 1] += erLine[(erAt + erN - (size_t)erTap[i]) % erN] * erGain[i];
				erAt = (erAt + 1) % erN;
				erLow[0] += (er[0] - erLow[0]) * erCoeff; erLow[1] += (er[1] - erLow[1]) * erCoeff;
				// the tail, fed by the sound and its first reflections
				const float feed = (delayed + (erLow[0] + erLow[1]) * 0.5f) * fixedGain;
				float tail[2] = { 0.f, 0.f };
				for (int ch = 0; ch < 2; ch++)
				{
					float acc = 0.f;
					for (int i = 0; i < 8; i++) acc += comb[ch][i].Run(feed);
					for (int i = 0; i < 4; i++) acc = all[ch][i].Run(acc);
					tail[ch] = acc;
				}
				const float wl = tail[0] * wet1 + tail[1] * wet2 + erLow[0] * settings.wet;
				const float wr = tail[1] * wet1 + tail[0] * wet2 + erLow[1] * settings.wet;
				out[f * 2] = l * settings.dry + wl;
				out[f * 2 + 1] = r * settings.dry + wr;
			}
		}

		const ReverbSettings &Get() const { return settings; }

	private:
		static float Clamp(const float v, const float a, const float b) { return v < a ? a : (v > b ? b : v); }

		struct Comb
		{
			std::vector<float> buf; size_t at = 0; float store = 0.f, feedback = 0.84f, damp = 0.2f;
			void Size(const int n) { buf.assign((size_t)(n > 1 ? n : 1), 0.f); at = 0; store = 0.f; }
			float Run(const float x)
			{
				const float y = buf[at];
				store = y * (1.f - damp) + store * damp;
				buf[at] = x + store * feedback;
				if (++at >= buf.size()) at = 0;
				return y;
			}
		};
		struct Allpass
		{
			std::vector<float> buf; size_t at = 0;
			void Size(const int n) { buf.assign((size_t)(n > 1 ? n : 1), 0.f); at = 0; }
			float Run(const float x)
			{
				const float b = buf[at];
				buf[at] = x + b * 0.5f;
				if (++at >= buf.size()) at = 0;
				return b - x;
			}
		};

		ReverbSettings settings;
		float rate = 48000.f;
		Comb comb[2][8];
		Allpass all[2][4];
		std::vector<float> pre; size_t preAt = 0; int preDelay = 0;
		std::vector<float> erLine; size_t erAt = 0; int erTap[6] = { 1, 1, 1, 1, 1, 1 }; float erGain[6] = { 0, 0, 0, 0, 0, 0 };
		float erLow[2] = { 0.f, 0.f }, erCoeff = 1.f;
		float wet1 = 0.f, wet2 = 0.f;
	};

	// AudioReverb.cpp: the model as a node in miniaudio's graph (stereo in and out).
	void* ReverbNodeCreate(::ma_engine* engine);
	void ReverbNodeDestroy(void* node);
	void ReverbNodeSet(void* node, const ReverbSettings &settings);

} }

#endif	/* PYROS3D_AUDIO_REVERB_H */
