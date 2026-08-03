//============================================================================
// Name        : AudioManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Audio device, listener and master mix
//============================================================================

#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/miniaudio/miniaudio.h>

namespace p3d {

	AudioManager* AudioManager::activeManager = NULL;

	AudioManager::AudioManager() : engine(NULL), initialized(false), masterVolume(1.f),
		hasLastListenerPosition(false)
	{
		engine = new ma_engine();

		// Default config: miniaudio picks the playback device, the sample rate
		// and the channel count from the OS. Overriding any of those here
		// would just be guessing on the user's behalf.
		if (ma_engine_init(NULL, engine) != MA_SUCCESS)
		{
			// A machine with no output device is a completely normal thing to
			// run on (CI, a headless box, a session with audio held
			// exclusively elsewhere). Report it and carry on silent - every
			// entry point in this subsystem checks `initialized`, so nothing
			// downstream has to care.
			echo("WARNING: AudioManager - could not initialize the audio device; audio disabled");
			delete engine;
			engine = NULL;
			return;
		}

		initialized = true;

		// Registered even though construction can fail above, because
		// GetActive() returning non-NULL is what tells Sound/AudioSource that
		// a manager *exists*; they each re-check IsInitialized() themselves.
		activeManager = this;
	}

	void AudioManager::RegisterVoice(ma_sound* voice)
	{
		if (voice == NULL) return;
		liveVoices.push_back(voice);
	}

	void AudioManager::UnregisterVoice(ma_sound* voice)
	{
		for (std::vector<ma_sound*>::iterator i = liveVoices.begin(); i != liveVoices.end(); i++)
		{
			if (*i == voice)
			{
				liveVoices.erase(i);
				return;
			}
		}
	}

	AudioManager::~AudioManager()
	{
		if (initialized)
		{
			// Anything still registered belongs to a Sound/AudioSource that
			// will be destroyed after this manager. ma_engine_uninit() below
			// does not uninitialize caller-owned sounds, so do it here, while
			// the engine is still up and it is legal. Their owners then find
			// GetActive() == NULL and skip a second uninit.
			for (uint32 i = 0; i < liveVoices.size(); i++)
				ma_sound_uninit(liveVoices[i]);
		}
		liveVoices.clear();

		// Cleared before the engine goes down, so any owner destroyed later
		// sees "no manager" and leaves its (already uninitialized, or never
		// initialized) handles alone.
		if (activeManager == this)
			activeManager = NULL;

		if (engine != NULL)
		{
			if (initialized)
				ma_engine_uninit(engine);
			delete engine;
			engine = NULL;
		}

		initialized = false;
	}

	void AudioManager::SetMasterVolume(const f32 volume)
	{
		masterVolume = volume;
		if (!initialized) return;
		ma_engine_set_volume(engine, volume);
	}

	void AudioManager::SetListener(const Vec3 &position, const Vec3 &forward, const Vec3 &up, const f32 dt)
	{
		listenerPosition = position;
		if (!initialized) return;

		ma_engine_listener_set_position(engine, 0, position.x, position.y, position.z);

		// Normalized here rather than trusting the caller: a zero-length or
		// unnormalized forward silently skews every directional calculation
		// downstream, and a GameObject's axes are easy to hand over unscaled.
		Vec3 f = forward;
		if (f.magnitude() > 0.0001f) f = f.normalize();
		else f = Vec3(0.f, 0.f, -1.f);
		ma_engine_listener_set_direction(engine, 0, f.x, f.y, f.z);

		Vec3 u = up;
		if (u.magnitude() > 0.0001f) u = u.normalize();
		else u = Vec3(0.f, 1.f, 0.f);
		ma_engine_listener_set_world_up(engine, 0, u.x, u.y, u.z);

		// Doppler - see the header's comment on `dt`. A non-positive or
		// suspiciously large step is treated as a cut: velocity 0 rather than
		// a spike from dividing by (near) nothing or from a frame that
		// covered a scene reload.
		Vec3 velocity(0.f, 0.f, 0.f);
		if (dt > 0.0001f && dt < 0.25f && hasLastListenerPosition)
			velocity = (position - lastListenerPosition) * (1.f / dt);
		ma_engine_listener_set_velocity(engine, 0, velocity.x, velocity.y, velocity.z);

		lastListenerPosition = position;
		hasLastListenerPosition = true;
	}

	void AudioManager::SetListenerFromGameObject(GameObject* object, const f32 dt)
	{
		if (object == NULL) return;

		const Matrix &world = object->GetWorldTransformation();

		// Engine convention, matching the camera: -Z is forward, +Y is up.
		// Taking these out of the world matrix rather than the local one means
		// a listener parented to something (a head bone, a vehicle) is still
		// oriented correctly.
		Vec3 forward = (world * Vec4(0.f, 0.f, -1.f, 0.f)).xyz();
		Vec3 up = (world * Vec4(0.f, 1.f, 0.f, 0.f)).xyz();

		SetListener(object->GetWorldPosition(), forward, up, dt);
	}

}
