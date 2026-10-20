//============================================================================
// Name        : AudioManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Audio device, listener and master mix
//============================================================================

#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioBus.h>
#include "AudioReverb.h"
#include <cstdio>
#include <cstdlib>
#include "AudioEar.h"
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/miniaudio/miniaudio.h>
#include "AudioDopplerSafety.h"

namespace p3d {

	AudioManager* AudioManager::activeManager = NULL;

	namespace {
		// The last thing the mix goes through before the device. Twenty
		// sounds at once - a tower coming down - add up to more than the
		// device can say, and what is over is cut off flat: a crackle on
		// every loud moment. This turns the whole mix down for as long as
		// it is too loud (at once, so nothing gets through; back up slowly,
		// so it is not heard pumping) and leaves it alone otherwise.
		f32 limiterGain = 1.f;
		f32 limiterPeak = 0.f;      // the loudest the mix has asked for, for whoever wants to know

		void LimitedDataCallback(ma_device* device, void* out, const void* in, ma_uint32 frames)
		{
			(void)in;
			ma_engine* engine = (ma_engine*)device->pUserData;
			if (engine == NULL) return;
#if defined(MA_EMSCRIPTEN)
			// (what miniaudio's own callback does in a browser, where there
			// is no thread to load sounds on: one piece of that work a call)
			if (engine->pResourceManager != NULL
				&& (engine->pResourceManager->config.flags & MA_RESOURCE_MANAGER_FLAG_NO_THREADING) != 0)
				ma_resource_manager_process_next_job(engine->pResourceManager);
#endif
			ma_engine_read_pcm_frames(engine, out, frames, NULL);
			if (device->playback.format != ma_format_f32) return;
			const ma_uint32 channels = device->playback.channels;
			f32* s = (f32*)out;
			const f32 ceiling = 0.92f;
			// (about a third of a second to come back up, whatever the rate)
			const f32 release = 1.f / (0.3f * (f32)device->sampleRate);
			f32 gain = limiterGain, peak = 0.f;
			for (ma_uint32 f = 0; f < frames; f++)
			{
				f32 loud = 0.f;
				for (ma_uint32 c = 0; c < channels; c++)
				{
					const f32 a = s[f * channels + c] < 0.f ? -s[f * channels + c] : s[f * channels + c];
					if (a > loud) loud = a;
				}
				if (loud > peak) peak = loud;
				if (loud * gain > ceiling) gain = ceiling / loud;
				else gain += (1.f - gain) * release;
				for (ma_uint32 c = 0; c < channels; c++) s[f * channels + c] *= gain;
			}
			limiterGain = gain;
			limiterPeak = peak;
			// PYROS_AUDIO_CAPTURE=<file>: the mix as it leaves for the device, raw float32
			// interleaved (rate and channels said once on stderr) - to measure what a game
			// sounds like without listening to it
			static FILE* capture = NULL;
			static bool asked = false;
			if (!asked)
			{
				asked = true;
				const char* path = getenv("PYROS_AUDIO_CAPTURE");
				if (path != NULL && path[0] != 0 && (capture = fopen(path, "wb")) != NULL)
					fprintf(stderr, "AudioManager: capturing the mix to %s - float32, %u channels, %u Hz\n", path, channels, device->sampleRate);
			}
			if (capture != NULL) { fwrite(s, sizeof(f32), (size_t)frames * channels, capture); fflush(capture); }
		}
	}

	AudioManager::AudioManager() : engine(NULL), initialized(false), masterVolume(1.f),
		hasLastListenerPosition(false)
	{
		engine = new ma_engine();

		// Default config: miniaudio picks the playback device, the sample rate
		// and the channel count from the OS. Overriding any of those here
		// would just be guessing on the user's behalf.
		ma_engine_config config = ma_engine_config_init();
		config.dataCallback = LimitedDataCallback;      // the mix, limited - see above
		if (ma_engine_init(&config, engine) != MA_SUCCESS)
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

	std::shared_ptr<AudioBus> AudioManager::GetWorldBus()
	{
		if (!worldBus && initialized) worldBus = std::make_shared<AudioBus>();
		return worldBus;
	}

	void AudioManager::SetWorldReverb(const f32 room, const f32 damping, const f32 wet, const f32 preDelayMs, const f32 early)
	{
		std::shared_ptr<AudioBus> bus = GetWorldBus();
		if (bus) bus->SetReverb(room, damping, wet, preDelayMs, early);
	}

	void AudioManager::ClearWorldReverb()
	{
		if (worldBus) worldBus->ClearReverb();
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
		// (the world bus, if nobody else holds it, goes while the engine is still up)
		worldBus.reset();
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
			// reverb nodes still in the graph (a bus that outlives this manager) leave it now
			if (initialized)
				detail::ReverbNodesShutdown();
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
		detail::EarListenerSet(position.x, position.y, position.z, f.x, f.y, f.z, u.x, u.y, u.z);

		// Doppler - see the header's comment on `dt`. A non-positive or
		// suspiciously large step is treated as a cut: velocity 0 rather than
		// a spike from dividing by (near) nothing or from a frame that
		// covered a scene reload.
		Vec3 velocity(0.f, 0.f, 0.f);
		if (dt > 0.0001f && dt < 0.25f && hasLastListenerPosition)
			velocity = (position - lastListenerPosition) * (1.f / dt);

		// Clamped regardless of how "reasonable" it looks - see
		// AudioDopplerSafety.h. miniaudio's own Doppler formula divides by a
		// term that reaches zero once a velocity's projection toward/away
		// from a source hits speedOfSound/dopplerFactor, and nothing in the
		// public API stops a caller (or ordinary finite-difference noise)
		// from getting there. An unclamped hit is an Inf/NaN pitch reaching
		// the audio thread - reported as sound going silent, then the
		// process hanging on shutdown waiting for that thread to unwind.
		velocity = detail::ClampDopplerVelocity(velocity, detail::LISTENER_ASSUMED_MAX_DOPPLER_FACTOR);
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
