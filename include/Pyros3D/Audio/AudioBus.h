//============================================================================
// Name        : AudioBus.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A named submix - "Music", "SFX", etc - for group volume,
//               pausing and fades
//============================================================================

#ifndef AUDIOBUS_H
#define	AUDIOBUS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <memory>

// Not ma_sound_group - it is a plain `typedef ma_sound ma_sound_group;` in
// miniaudio, not a distinct struct, so forward-declaring "ma_sound_group"
// here would clash the moment a .cpp includes both this header and the real
// miniaudio.h (two incompatible declarations of the same name: an opaque
// struct tag here vs. a typedef alias there). ma_sound* is the identical
// type under a different spelling and forward-declares cleanly, same as
// Sound.h/AudioSource.h already do.
struct ma_sound;

namespace p3d {

	// A submix that any number of Sound/AudioSource instances can be routed
	// into instead of the master mix directly - the usual reasons being a
	// separate volume slider (music vs. effects vs. voice) and the ability to
	// pause/duck a whole category at once (mute SFX during a cutscene without
	// touching every individual source).
	//
	// Always managed through std::shared_ptr, never a bare AudioBus* - that
	// is what makes the ordering safe. A Sound/AudioSource routed through a
	// bus keeps its own shared_ptr to it for as long as it lives, so the bus
	// cannot be destroyed out from under something still playing through it
	// regardless of what order a script (or its GC) happens to drop things
	// in - the exact hazard AudioManager's own voice registry exists to
	// close one level up. Construct with std::make_shared<AudioBus>(...).
	class PYROS3D_API AudioBus {

	public:

		// `parent` nests this bus under another one (e.g. a "Voice" bus that
		// itself lives inside "SFX", so muting SFX also mutes voice chat).
		// NULL routes straight into the engine's master mix - the common
		// case, and what every top-level bus ("Music", "SFX") wants.
		explicit AudioBus(const std::shared_ptr<AudioBus> &parent = nullptr);
		~AudioBus();

		// False if there was no initialized AudioManager at construction, or
		// the underlying group failed to initialize. Every method below is a
		// safe no-op in that state.
		bool IsValid() const { return group != NULL; }

		// 0 = silence, 1 = unity. Multiplies with the master volume and with
		// each routed Sound/AudioSource's own volume, not in place of them.
		void SetVolume(const f32 volume);
		f32 GetVolume() const { return volume; }

		void SetPitch(const f32 pitch);
		f32 GetPitch() const { return pitch; }

		// Pauses/resumes every sound currently routed through this bus at
		// once - a source started after Pause() plays normally until the
		// next Resume() catches it up, same as any other paused mix.
		void Pause();
		void Resume();

		// Ramps this bus's own volume over `milliseconds` - "duck the music
		// bed", not a substitute for fading an individual source.
		void FadeIn(const f32 milliseconds);
		void FadeOut(const f32 milliseconds);

		// For Sound/AudioSource only - the raw miniaudio group, passed as
		// ma_sound_init_from_file()'s routing target.
		// The space the bus's sounds are heard in (AudioReverb.h): early reflections off
		// walls `preDelayMs` away, then a tail that rings for longer the bigger `room` (0..1)
		// is and duller the more `damping` (0 stone and glass .. 1 soft furnishing); `wet` is
		// how loud the space is against the sounds, `early` its first reflections against its
		// tail. A street of facades: room 0.45, damping 0.35, wet 0.22, preDelay 9, early 0.8.
		// Put the world's sounds on a bus with one and music/UI on one without.
		void SetReverb(const f32 room, const f32 damping, const f32 wet, const f32 preDelayMs = 12.f, const f32 early = 0.6f);
		void ClearReverb();
		bool HasReverb() const { return reverbNode != NULL; }
		ma_sound* GetGroup() { return group; }

	private:

		// Keeps the parent bus alive at least as long as this one - the same
		// reasoning as a Sound/AudioSource holding a shared_ptr to us.
		std::shared_ptr<AudioBus> parent;

		ma_sound* group;
		void* reverbNode = NULL;
		f32 volume;
		f32 pitch;
	};

}

#endif	/* AUDIOBUS_H */
