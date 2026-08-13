//============================================================================
// Name        : AudioManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Audio device, listener and master mix
//============================================================================

#ifndef AUDIOMANAGER_H
#define	AUDIOMANAGER_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>

// miniaudio's own types are only needed by the implementation, so the header
// stays opaque - including miniaudio.h here would drag ~95k lines into every
// translation unit that just wants to play a sound.
struct ma_engine;
struct ma_sound;

namespace p3d {

	class GameObject;

	namespace AttenuationModel {
		enum {
			// No distance attenuation at all - the sound plays at full volume
			// wherever the listener is. Use for music and UI.
			None = 0,
			// 1/distance, clamped. The closest match to how sound actually
			// falls off, and the default.
			Inverse,
			// Fades linearly to silence at maxDistance. Predictable, and the
			// easiest to design a level around.
			Linear,
			// Steeper than inverse; useful for making a source feel small.
			Exponential
		};
	}

	// Shared by Sound and AudioSource - both can insert one of these on their
	// output, as the first of three independent, always-in-this-order
	// effect stages (filter -> EQ -> delay - see AudioEQType and
	// SetDelay()). Lives here rather than on either class specifically
	// since both need it and neither is the other's header.
	namespace AudioFilterType {
		enum {
			None = 0,
			// Cuts frequencies above the cutoff - the "muffled", behind-a-wall
			// or underwater effect.
			LowPass,
			// Cuts frequencies below the cutoff - thins a sound out, e.g. an
			// old radio/telephone voice, or removing rumble.
			HighPass,
			// Keeps only a band around the cutoff - both muffled AND thin at
			// once, e.g. a voice coming through a walkie-talkie.
			BandPass
		};
	}

	// The second stage - a parametric EQ, shaping (not just cutting) a
	// region of the spectrum. Distinct from AudioFilterType because its
	// real parameters (frequency + gain + Q) don't fit
	// SetFilter(type, cutoffHz, order)'s shape - order means nothing for a
	// shelf/peak/notch, and Peak/LowShelf/HighShelf need a gain that
	// LowPass/HighPass/BandPass don't.
	namespace AudioEQType {
		enum {
			None = 0,
			// Boosts or cuts (gainDB) a narrow region around `frequency` -
			// "more/less bass thump", "tame a harsh mid frequency".
			Peak,
			// Removes a narrow region around `frequency` entirely - no gain
			// concept, unlike Peak (SetEQ()'s gainDB argument is ignored for
			// this type). For killing one specific tone, e.g. a hum.
			Notch,
			// Boosts or cuts everything BELOW `frequency` - "warmer" (boost)
			// or "thinner, less rumble" (cut).
			LowShelf,
			// Boosts or cuts everything ABOVE `frequency` - "brighter" (boost)
			// or "duller, less hiss" (cut).
			HighShelf
		};
	}

	// Owns the audio device and the listener.
	//
	// Construct exactly one, keep it alive for as long as any sound is
	// playing, and destroy it last. The constructor registers it as the
	// process-wide active manager (GetActive()), which is how Sound and
	// AudioSource reach the device without every call site having to pass one
	// around - the same arrangement as SetActiveRenderDevice(). Destroying it
	// clears that registration, and both Sound and AudioSource check
	// GetActive() before touching the device, so audio objects outliving the
	// manager go quiet instead of crashing.
	class PYROS3D_API AudioManager {

	public:

		AudioManager();
		~AudioManager();

		// False if the audio device could not be opened (no output device, or
		// one held exclusively by something else). Everything else on this
		// class, and on Sound/AudioSource, is a safe no-op in that state -
		// audio failing must never take a game down with it.
		bool IsInitialized() const { return initialized; }

		// 0 = silence, 1 = unity. Values above 1 are allowed and will clip.
		void SetMasterVolume(const f32 volume);
		f32 GetMasterVolume() const { return masterVolume; }

		// ****************************** Listener ****************************

		// Where the ears are. `forward` is the direction being faced and `up`
		// the head's up axis; both are normalized internally.
		//
		// `dt` is the real-time step since the last call, and is what drives
		// Doppler: velocity is derived here by finite-differencing position
		// against the previous call, rather than asking the caller for a
		// velocity directly - a GameObject has no velocity of its own, and a
		// derived one is one less thing for a script to get wrong. Leave it
		// at the default 0 to move the listener without any Doppler effect
		// (a teleport/cut, or a one-off placement before the game loop starts).
		// Values above ~0.25s are treated as a cut too, not a fast pan - a
		// hitch or a scene (re)load must not manufacture a velocity spike.
		void SetListener(const Vec3 &position, const Vec3 &forward, const Vec3 &up = Vec3(0.f, 1.f, 0.f), const f32 dt = 0.f);

		// Convenience for the overwhelmingly common case: the listener is the
		// camera. Reads the object's world transform, so it must be called
		// after the SceneGraph has updated for the frame. See SetListener()
		// for what `dt` does.
		void SetListenerFromGameObject(GameObject* object, const f32 dt = 0.f);

		const Vec3 &GetListenerPosition() const { return listenerPosition; }

		// ******************************* Access *****************************

		// NULL until an AudioManager exists, and again after it is destroyed.
		static AudioManager* GetActive() { return activeManager; }
		static bool IsActiveSet() { return activeManager != NULL; }
		// Which manager Sound/AudioSource construction binds to. The editor
		// switches this when the active scene tab changes so new sources and
		// the listener always share one ma_engine.
		static void MakeActive(AudioManager* manager) { activeManager = manager; }

		// For Sound/AudioSource only - the raw miniaudio engine.
		ma_engine* GetEngine() { return engine; }

		// Also for Sound/AudioSource only. Every live voice registers itself
		// here so this class can guarantee the one ordering miniaudio cares
		// about: ma_engine_uninit() does not touch caller-owned ma_sounds, so
		// any still alive when the engine goes must be uninitialized first or
		// their internal resources are stranded. Relying on the owners to be
		// destroyed first does not work - they are routinely Lua-owned, and
		// sol finalizes in whatever order its GC picks.
		void RegisterVoice(ma_sound* voice);
		void UnregisterVoice(ma_sound* voice);

	private:

		ma_engine* engine;
		bool initialized;
		f32 masterVolume;
		Vec3 listenerPosition;

		// For Doppler - see SetListener()'s comment.
		Vec3 lastListenerPosition;
		bool hasLastListenerPosition;

		std::vector<ma_sound*> liveVoices;

		static AudioManager* activeManager;
	};

}

#endif	/* AUDIOMANAGER_H */
