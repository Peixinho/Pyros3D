//============================================================================
// Name        : Sound.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A loaded sound effect with a pool of playback voices
//============================================================================

#ifndef SOUND_H
#define	SOUND_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>
#include <memory>

struct ma_sound;

namespace p3d {

	class AudioBus;
	// Forward-declared only - see AudioSource.h's identical comment; the
	// full class (and miniaudio.h) stays out of this public header.
	namespace detail { class AudioEffectChain; }

	// One sound file plus a fixed pool of voices that can play it.
	//
	// This is the fire-and-forget half of the audio API, for effects that are
	// triggered and then forgotten: a footstep, an impact, a pickup. Each
	// Play() grabs a voice that has finished and restarts it, so the same
	// effect can overlap with itself up to `voices` times - hit a wall of
	// bricks and you hear a wall of bricks, not one clipped blip.
	//
	// The pool is why this is a class and not a free function: a voice is a
	// live miniaudio node, and creating one per trigger would allocate,
	// decode-check and free on the audio hot path. Voices are created once
	// here and reused forever. When all of them are busy the oldest is stolen,
	// which is the standard behaviour for effect playback - dropping the new
	// sound would be more noticeable than cutting the oldest one short.
	//
	// For sound that has to persist, follow the listener, loop, or be attached
	// to a moving object, use AudioSource instead.
	class PYROS3D_API Sound {

	public:

		// `voices` is the maximum number of simultaneous plays of this one
		// file. 1 makes it self-cancelling (each Play() interrupts the last),
		// which is right for something like a UI click.
		//
		// `bus` routes every voice in this pool through an AudioBus (a submix
		// - e.g. "SFX") instead of straight into the master mix. Held as a
		// shared_ptr for the pool's whole lifetime, so the bus cannot be
		// destroyed out from under it - see AudioBus's own comment.
		Sound(const std::string &file, const uint32 voices = 4, const std::shared_ptr<AudioBus> &bus = nullptr);
		~Sound();

		// False if the file was missing/undecodable, or if there is no
		// AudioManager. Every method below is a safe no-op in that state.
		bool IsLoaded() const { return loaded; }
		const std::string &GetFile() const { return file; }

		// Plays without spatialization - same in both ears regardless of where
		// the listener is. For UI, music stings and anything diegetically
		// "everywhere". `pan` is manual stereo balance, -1 (left) to 1
		// (right) - meaningless (and ignored) on a PlayAt() voice, whose pan
		// comes from its 3D position instead.
		void Play(const f32 volume = 1.f, const f32 pitch = 1.f, const f32 pan = 0.f);

		// Plays positioned in the world, attenuated and panned relative to the
		// listener. The position is fixed at trigger time; the voice does not
		// follow anything afterwards (that is what AudioSource is for).
		void PlayAt(const Vec3 &position, const f32 volume = 1.f, const f32 pitch = 1.f);

		// Silences every voice of this sound.
		void Stop();

		// How many voices are currently sounding - mostly useful for tuning
		// the pool size.
		uint32 GetPlayingCount() const;

		// Distance model for the positioned voices. Defaults to a linear
		// falloff between min and max distance, which is the easiest to reason
		// about when placing sounds in a scene by hand.
		void SetAttenuation(const uint32 model, const f32 minDistance, const f32 maxDistance);

		// Three independent effect stages applied to the WHOLE pool at once -
		// every voice is kept configured identically (they are all "the same
		// sound" conceptually; an individually configured voice is what
		// AudioSource is for), always chained in this fixed order (filter ->
		// EQ -> delay) regardless of the order they're set in, so e.g. a
		// "muffled AND echoey" effect is SetFilter(LowPass, ...) plus
		// SetDelay(...) together, not a choice between them.

		// See AudioFilterType::* - LowPass/HighPass/BandPass. `order` is the
		// filter's steepness (2 = 12dB/octave, a reasonable default).
		void SetFilter(const uint32 type, const f32 cutoffHz, const uint32 order = 2);
		void ClearFilter();
		uint32 GetFilterType() const;
		f32 GetFilterCutoff() const;
		uint32 GetFilterOrder() const;

		// See AudioEQType::* - Peak/Notch/LowShelf/HighShelf. `gainDB` is
		// ignored for Notch; `q` shapes how narrow/broad the affected region
		// is - 1.0 is a reasonable default for all four types.
		void SetEQ(const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q = 1.0f);
		void ClearEQ();
		uint32 GetEQType() const;
		f32 GetEQFrequency() const;
		f32 GetEQGain() const;
		f32 GetEQQ() const;

		// Delay/echo - see AudioSource.h's identical comment on `decay`/
		// `wet`/`dry`.
		void SetDelay(const f32 delaySeconds, const f32 decay, const f32 wet = 1.0f, const f32 dry = 1.0f);
		void ClearDelay();
		bool HasDelay() const;
		f32 GetDelaySeconds() const;
		f32 GetDelayDecay() const;
		f32 GetDelayWet() const;
		f32 GetDelayDry() const;

	private:

		// Returns a voice that is free, or steals the one used longest ago.
		ma_sound* AcquireVoice();

		std::string file;
		bool loaded;

		std::shared_ptr<AudioBus> bus;

		std::vector<ma_sound*> voices;
		// Round-robin cursor - also the steal order, so the voice taken when
		// everything is busy is the least recently started one.
		uint32 nextVoice;

		uint32 attenuationModel;
		f32 minDistance, maxDistance;

		// One AudioEffectChain per voice, parallel to `voices` - see
		// AudioEffectChain.h. Every SetFilter()/SetEQ()/SetDelay() call loops
		// over all of them applying the same settings; the getters just read
		// chains[0] back (they are always kept in sync with each other, so
		// any one voice's state represents the whole pool's).
		std::vector<detail::AudioEffectChain*> chains;
	};

}

#endif	/* SOUND_H */
