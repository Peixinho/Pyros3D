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
		void SetListener(const Vec3 &position, const Vec3 &forward, const Vec3 &up = Vec3(0.f, 1.f, 0.f));

		// Convenience for the overwhelmingly common case: the listener is the
		// camera. Reads the object's world transform, so it must be called
		// after the SceneGraph has updated for the frame.
		void SetListenerFromGameObject(GameObject* object);

		const Vec3 &GetListenerPosition() const { return listenerPosition; }

		// ******************************* Access *****************************

		// NULL until an AudioManager exists, and again after it is destroyed.
		static AudioManager* GetActive() { return activeManager; }
		static bool IsActiveSet() { return activeManager != NULL; }

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

		std::vector<ma_sound*> liveVoices;

		static AudioManager* activeManager;
	};

}

#endif	/* AUDIOMANAGER_H */
