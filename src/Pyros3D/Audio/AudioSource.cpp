//============================================================================
// Name        : AudioSource.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A positional sound emitter bound to a GameObject
//============================================================================

#include <Pyros3D/Audio/AudioSource.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioBus.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/miniaudio/miniaudio.h>
#include "AudioEffectChain.h"
#include "AudioDopplerSafety.h"

namespace p3d {

	namespace {
		ma_attenuation_model TranslateAttenuation(const uint32 model)
		{
			switch (model)
			{
			case AttenuationModel::None:        return ma_attenuation_model_none;
			case AttenuationModel::Linear:      return ma_attenuation_model_linear;
			case AttenuationModel::Exponential: return ma_attenuation_model_exponential;
			case AttenuationModel::Inverse:
			default:                            return ma_attenuation_model_inverse;
			}
		}

		bool EngineAlive()
		{
			return AudioManager::IsActiveSet() && AudioManager::GetActive()->IsInitialized();
		}
	}

	AudioSource::AudioSource(const std::string &file, const bool stream, const std::shared_ptr<AudioBus> &bus)
		: IComponent(), file(file), loaded(false), sound(NULL), bus(bus),
		looping(false), spatialized(true), volume(1.f), pitch(1.f), pan(0.f),
		streamed(stream), attenuationModel(AttenuationModel::Linear),
		minDistance(1.f), maxDistance(100.f), hasCone(false),
		coneInner(6.283185f), coneOuter(6.283185f), coneOuterGain(1.f),
		directionalAttenuation(1.f), dopplerFactor(1.f),
		lastPosition(Vec3::ZERO), lastUpdateTime(0.0), hasLastUpdate(false),
		chain(new detail::AudioEffectChain())
	{
	}

	bool AudioSource::TryLoadFromFile()
	{
		if (loaded) return true;
		if (file.empty()) return false;
		if (!EngineAlive())
		{
			echo("WARNING: AudioSource - no initialized AudioManager, '" + file + "' not loaded");
			return false;
		}

		sound = new ma_sound();

		// Match Sound's init flags — proven path for asset preview in the editor.
		ma_uint32 flags = streamed
			? (MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION)
			: (MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION);

		ma_sound* group = bus ? bus->GetGroup() : NULL;

		if (ma_sound_init_from_file(AudioManager::GetActive()->GetEngine(), file.c_str(), flags, group, NULL, sound) != MA_SUCCESS)
		{
			echo("WARNING: AudioSource - could not load '" + file + "'");
			delete sound;
			sound = NULL;
			return false;
		}

		loaded = true;
		AudioManager::GetActive()->RegisterVoice(sound);

		ma_sound_set_spatialization_enabled(sound, spatialized ? MA_TRUE : MA_FALSE);
		ma_sound_set_attenuation_model(sound, TranslateAttenuation(attenuationModel));
		ma_sound_set_min_distance(sound, minDistance);
		ma_sound_set_max_distance(sound, maxDistance);
		ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
		ma_sound_set_volume(sound, volume);
		ma_sound_set_pitch(sound, pitch);
		ma_sound_set_pan(sound, pan);
		ma_sound_set_doppler_factor(sound, dopplerFactor);
		ma_sound_set_directional_attenuation_factor(sound, directionalAttenuation);
		if (hasCone)
			ma_sound_set_cone(sound, coneInner, coneOuter, coneOuterGain);
		return true;
	}

	bool AudioSource::EnsureLoaded()
	{
		return TryLoadFromFile();
	}

	AudioSource::~AudioSource()
	{
		// See Sound::~Sound() - uninit only while the engine still exists.
		if (sound != NULL)
		{
			if (EngineAlive())
			{
				// No rerouting needed here (unlike chain->SetFilter()/etc,
				// used while the source stays alive) - `sound` itself is
				// about to be fully uninitialized, which detaches it from
				// the graph regardless.
				chain->Destroy();

				// See Sound::~Sound() - unregister before uninitializing.
				AudioManager::GetActive()->UnregisterVoice(sound);
				ma_sound_uninit(sound);
			}
			delete sound;
			sound = NULL;
		}
		delete chain;
		chain = NULL;
		loaded = false;
		// `bus` (a shared_ptr) releases its reference here, after `sound` no
		// longer points into it - the entire reason it is a shared_ptr member
		// rather than a raw AudioBus*. See the constructor's comment.
	}

	// ******************************* Playback *******************************

	void AudioSource::Play()
	{
		if (!EnsureLoaded())
			return;
		// Push the owner's world pose before starting so the first audible
		// sample is already panned/attenuated correctly - Update() has not
		// necessarily run yet (play-on-awake, editor play mode, etc.).
		if (spatialized)
		{
			GameObject* owner = GetOwner();
			if (owner != NULL)
			{
				const Vec3 position = owner->GetWorldPosition();
				ma_sound_set_position(sound, position.x, position.y, position.z);
				const Matrix &world = owner->GetWorldTransformation();
				Vec3 forward = (world * Vec4(0.f, 0.f, -1.f, 0.f)).xyz();
				if (forward.magnitude() > 0.0001f) forward = forward.normalize();
				ma_sound_set_direction(sound, forward.x, forward.y, forward.z);
			}
		}
		ma_sound_start(sound);
	}

	void AudioSource::Pause()
	{
		// ma_sound_stop() halts without rewinding, so a following Play()
		// resumes - which is exactly "pause". Stop() below adds the rewind.
		if (!loaded) return;
		ma_sound_stop(sound);
	}

	void AudioSource::Stop()
	{
		if (!loaded) return;
		ma_sound_stop(sound);
		ma_sound_seek_to_pcm_frame(sound, 0);
	}

	bool AudioSource::IsPlaying() const
	{
		if (!loaded) return false;
		return ma_sound_is_playing(sound) == MA_TRUE;
	}

	void AudioSource::SetLooping(const bool looping)
	{
		this->looping = looping;
		if (!loaded) return;
		ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
	}

	void AudioSource::SetVolume(const f32 volume)
	{
		this->volume = volume;
		if (!loaded) return;
		ma_sound_set_volume(sound, volume);
	}

	void AudioSource::SetPitch(const f32 pitch)
	{
		this->pitch = pitch;
		if (!loaded) return;
		ma_sound_set_pitch(sound, pitch);
	}

	void AudioSource::FadeIn(const f32 milliseconds)
	{
		if (!loaded) return;
		// Explicit 0 -> target rather than -1 ("from current"): a fade-in is
		// nearly always wanted from silence, including when the source is
		// already part-way through a previous fade.
		ma_sound_set_fade_in_milliseconds(sound, 0.f, volume, (ma_uint64)milliseconds);
		ma_sound_start(sound);
	}

	void AudioSource::FadeOut(const f32 milliseconds)
	{
		if (!loaded) return;
		// -1 means "from wherever the volume currently is", which is what a
		// fade-out wants. Note miniaudio does not stop the sound at the end of
		// the fade - it just sits at zero volume, which keeps a later FadeIn()
		// seamless. Call Stop() as well to actually free the voice's work.
		ma_sound_set_fade_in_milliseconds(sound, -1.f, 0.f, (ma_uint64)milliseconds);
	}

	// ****************************** Positioning *****************************

	void AudioSource::SetSpatialization(const bool enabled)
	{
		spatialized = enabled;
		if (!loaded) return;
		ma_sound_set_spatialization_enabled(sound, enabled ? MA_TRUE : MA_FALSE);
	}

	void AudioSource::SetAttenuation(const uint32 model, const f32 minDistance, const f32 maxDistance)
	{
		attenuationModel = model;
		this->minDistance = minDistance;
		this->maxDistance = maxDistance;
		if (!loaded) return;
		ma_sound_set_attenuation_model(sound, TranslateAttenuation(model));
		ma_sound_set_min_distance(sound, minDistance);
		ma_sound_set_max_distance(sound, maxDistance);
	}

	void AudioSource::SetCone(const f32 innerAngle, const f32 outerAngle, const f32 outerGain)
	{
		hasCone = true;
		coneInner = innerAngle;
		coneOuter = outerAngle;
		coneOuterGain = outerGain;
		if (!loaded) return;
		ma_sound_set_cone(sound, innerAngle, outerAngle, outerGain);
	}

	void AudioSource::ClearCone()
	{
		hasCone = false;
		coneInner = coneOuter = 6.283185f;
		coneOuterGain = 1.f;
		if (!loaded) return;
		// A full-circle inner cone with unity outer gain is miniaudio's
		// "no cone" state - there is no separate disable call.
		ma_sound_set_cone(sound, 6.283185f, 6.283185f, 1.f);
	}

	void AudioSource::SetDirectionalAttenuation(const f32 factor)
	{
		directionalAttenuation = factor;
		if (!loaded) return;
		ma_sound_set_directional_attenuation_factor(sound, factor);
	}

	void AudioSource::SetDopplerFactor(const f32 factor)
	{
		dopplerFactor = factor;
		if (!loaded) return;
		ma_sound_set_doppler_factor(sound, factor);
	}

	void AudioSource::SetPan(const f32 pan)
	{
		this->pan = pan;
		if (!loaded) return;
		ma_sound_set_pan(sound, pan);
	}

	// Shared by SetFilter/SetEQ/SetDelay/their Clear* counterparts below -
	// resolves the two node-graph pointers AudioEffectChain needs, or NULL
	// for `engine` when there is no live engine to create anything against
	// (EngineAlive(), not just `loaded`: creating a node needs the live
	// ma_engine itself - node graph, channel/sample-rate query, the
	// endpoint node - not just a still-allocated, possibly
	// force-uninitialized `sound` handle).
	namespace {
		void ResolveChainTargets(ma_sound* sound, AudioBus* bus, bool loaded, ma_engine* &engine, void* &soundNode, void* &target)
		{
			engine = NULL; soundNode = NULL; target = NULL;
			if (!loaded || !EngineAlive()) return;
			engine = AudioManager::GetActive()->GetEngine();
			soundNode = reinterpret_cast<void*>(sound);
			target = bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine));
		}
	}

	void AudioSource::SetFilter(const uint32 type, const f32 cutoffHz, const uint32 order)
	{
		ma_engine* engine; void* soundNode; void* target;
		ResolveChainTargets(sound, bus.get(), loaded, engine, soundNode, target);
		chain->SetFilter(engine, soundNode, target, type, cutoffHz, order);
	}

	void AudioSource::ClearFilter()
	{
		ma_engine* engine; void* soundNode; void* target;
		ResolveChainTargets(sound, bus.get(), loaded, engine, soundNode, target);
		chain->ClearFilter(soundNode, target);
	}

	uint32 AudioSource::GetFilterType() const { return chain->filterType; }
	f32 AudioSource::GetFilterCutoff() const { return chain->filterCutoff; }
	uint32 AudioSource::GetFilterOrder() const { return chain->filterOrder; }

	void AudioSource::SetEQ(const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q)
	{
		ma_engine* engine; void* soundNode; void* target;
		ResolveChainTargets(sound, bus.get(), loaded, engine, soundNode, target);
		chain->SetEQ(engine, soundNode, target, type, frequencyHz, gainDB, q);
	}

	void AudioSource::ClearEQ()
	{
		ma_engine* engine; void* soundNode; void* target;
		ResolveChainTargets(sound, bus.get(), loaded, engine, soundNode, target);
		chain->ClearEQ(soundNode, target);
	}

	uint32 AudioSource::GetEQType() const { return chain->eqType; }
	f32 AudioSource::GetEQFrequency() const { return chain->eqFrequency; }
	f32 AudioSource::GetEQGain() const { return chain->eqGainDB; }
	f32 AudioSource::GetEQQ() const { return chain->eqQ; }

	void AudioSource::SetDelay(const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry)
	{
		ma_engine* engine; void* soundNode; void* target;
		ResolveChainTargets(sound, bus.get(), loaded, engine, soundNode, target);
		chain->SetDelay(engine, soundNode, target, delaySeconds, decay, wet, dry);
	}

	void AudioSource::ClearDelay()
	{
		ma_engine* engine; void* soundNode; void* target;
		ResolveChainTargets(sound, bus.get(), loaded, engine, soundNode, target);
		chain->ClearDelay(soundNode, target);
	}

	bool AudioSource::HasDelay() const { return chain->hasDelay; }
	f32 AudioSource::GetDelaySeconds() const { return chain->delaySeconds; }
	f32 AudioSource::GetDelayDecay() const { return chain->delayDecay; }
	f32 AudioSource::GetDelayWet() const { return chain->delayWet; }
	f32 AudioSource::GetDelayDry() const { return chain->delayDry; }

	// **************************** Playback state ****************************

	f32 AudioSource::GetLengthSeconds() const
	{
		if (!loaded) return 0.f;
		float length = 0.f;
		ma_sound_get_length_in_seconds(sound, &length);
		return length;
	}

	f32 AudioSource::GetCursorSeconds() const
	{
		if (!loaded) return 0.f;
		float cursor = 0.f;
		ma_sound_get_cursor_in_seconds(sound, &cursor);
		return cursor;
	}

	void AudioSource::SeekSeconds(const f32 seconds)
	{
		if (!loaded) return;
		ma_sound_seek_to_second(sound, seconds);
	}

	bool AudioSource::AtEnd() const
	{
		if (!loaded) return false;
		return ma_sound_at_end(sound) == MA_TRUE;
	}

	// ******************************* Component ******************************

	void AudioSource::Update(const f64 time)
	{
		if (!loaded || !active) return;

		// Nothing to track for a non-spatialized source, and writing a
		// position into one would be misleading - skip the work.
		if (!spatialized) return;

		GameObject* owner = GetOwner();
		if (owner == NULL) return;

		const Vec3 position = owner->GetWorldPosition();
		ma_sound_set_position(sound, position.x, position.y, position.z);

		// Forward axis out of the world matrix, matching the listener's
		// convention in AudioManager::SetListenerFromGameObject(). Only
		// consumed when a cone has been set, but it is one matrix-vector
		// multiply and keeping it unconditional means SetCone() takes effect
		// immediately rather than on the next time the object happens to move.
		const Matrix &world = owner->GetWorldTransformation();
		Vec3 forward = (world * Vec4(0.f, 0.f, -1.f, 0.f)).xyz();
		if (forward.magnitude() > 0.0001f) forward = forward.normalize();
		ma_sound_set_direction(sound, forward.x, forward.y, forward.z);

		// Doppler - see AudioManager::SetListener()'s identical reasoning.
		// `time` is the SceneGraph's absolute simulation time, so dt is
		// derived from consecutive Update() calls rather than needing its own
		// parameter; a non-positive or implausibly large gap (the first
		// frame, or a hitch/scene reload) is treated as "no velocity" rather
		// than a spike.
		Vec3 velocity = Vec3::ZERO;
		const f64 dt = time - lastUpdateTime;
		if (hasLastUpdate && dt > 0.0001 && dt < 0.25)
			velocity = (position - lastPosition) * (f32)(1.0 / dt);

		// Clamped against THIS source's own Doppler factor - see
		// AudioDopplerSafety.h and AudioManager::SetListener()'s identical
		// comment. Precise here (unlike the listener's clamp, which has to
		// assume a ceiling since one listener velocity is shared by every
		// source's own independently-set factor): this source's real factor
		// is exactly what miniaudio's formula multiplies its own velocity
		// by, so this clamp is neither too tight nor too loose.
		velocity = detail::ClampDopplerVelocity(velocity, dopplerFactor);
		ma_sound_set_velocity(sound, velocity.x, velocity.y, velocity.z);

		lastPosition = position;
		lastUpdateTime = time;
		hasLastUpdate = true;
	}

}
