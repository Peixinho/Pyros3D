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
#include "AudioFilterNode.h"

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
		filterNode(NULL), filterType(AudioFilterType::None), filterCutoff(0.f), filterOrder(2)
	{
		if (!EngineAlive())
		{
			echo("WARNING: AudioSource - no initialized AudioManager, '" + file + "' not loaded");
			return;
		}

		sound = new ma_sound();

		// STREAM decodes on demand and keeps only a small buffer resident;
		// DECODE pays the whole decode once up front and never again. Music
		// wants the former (a decoded track is tens of MB), an effect the
		// latter (no decode work at trigger time).
		ma_uint32 flags = stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;

		// Routes into the given bus's submix, or straight into the master mix
		// if none was given - pGroup below is exactly that choice.
		ma_sound* group = this->bus ? this->bus->GetGroup() : NULL;

		if (ma_sound_init_from_file(AudioManager::GetActive()->GetEngine(), file.c_str(), flags, group, NULL, sound) != MA_SUCCESS)
		{
			echo("WARNING: AudioSource - could not load '" + file + "'");
			delete sound;
			sound = NULL;
			return;
		}

		loaded = true;
		AudioManager::GetActive()->RegisterVoice(sound);

		// Spatialized by default with a linear falloff - the shape most
		// callers attaching a sound to an object in the world expect. Music
		// and ambience turn it off via SetSpatialization(false).
		ma_sound_set_spatialization_enabled(sound, MA_TRUE);
		ma_sound_set_attenuation_model(sound, TranslateAttenuation(AttenuationModel::Linear));
		ma_sound_set_min_distance(sound, 1.f);
		ma_sound_set_max_distance(sound, 100.f);
	}

	AudioSource::~AudioSource()
	{
		// See Sound::~Sound() - uninit only while the engine still exists.
		if (sound != NULL)
		{
			if (EngineAlive())
			{
				// No rerouting needed here (unlike detail::DetachFilterNode(),
				// which SetFilter()/ClearFilter() use while the source stays
				// alive) - `sound` itself is about to be fully uninitialized,
				// which detaches it from the graph regardless.
				detail::DestroyFilterNode(filterNode, filterType);

				// See Sound::~Sound() - unregister before uninitializing.
				AudioManager::GetActive()->UnregisterVoice(sound);
				ma_sound_uninit(sound);
			}
			delete sound;
			sound = NULL;
		}
		loaded = false;
		// `bus` (a shared_ptr) releases its reference here, after `sound` no
		// longer points into it - the entire reason it is a shared_ptr member
		// rather than a raw AudioBus*. See the constructor's comment.
	}

	// ******************************* Playback *******************************

	void AudioSource::Play()
	{
		if (!loaded) return;
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

	void AudioSource::SetFilter(const uint32 type, const f32 cutoffHz, const uint32 order)
	{
		if (type == AudioFilterType::None) { ClearFilter(); return; }

		filterCutoff = cutoffHz;
		filterOrder = order;
		// Not committed to `filterType` until the node actually exists below -
		// a failed AttachFilterNode() must leave GetFilterType() reporting
		// what is really playing (nothing), not what was asked for.
		if (!loaded) { filterType = type; return; }

		// EngineAlive(), not just `loaded`: this needs the live ma_engine
		// itself (node graph, channel/sample-rate query, the endpoint node),
		// not just a still-allocated (possibly force-uninitialized) `sound`
		// handle - see EngineAlive()'s other callers for that distinction.
		if (!EngineAlive()) return;

		ma_node* soundNode = reinterpret_cast<ma_node*>(sound);
		ma_engine* engine = AudioManager::GetActive()->GetEngine();
		ma_node* target = bus ? reinterpret_cast<ma_node*>(bus->GetGroup()) : ma_engine_get_endpoint(engine);

		detail::DetachFilterNode(soundNode, target, filterNode, filterType);

		void* node = detail::AttachFilterNode(engine, soundNode, target, type, cutoffHz, order);
		filterNode = node;
		filterType = (node != NULL) ? type : AudioFilterType::None;
	}

	void AudioSource::ClearFilter()
	{
		if (loaded && EngineAlive() && filterNode != NULL)
		{
			ma_engine* engine = AudioManager::GetActive()->GetEngine();
			ma_node* target = bus ? reinterpret_cast<ma_node*>(bus->GetGroup()) : ma_engine_get_endpoint(engine);
			detail::DetachFilterNode(reinterpret_cast<ma_node*>(sound), target, filterNode, filterType);
		}
		filterType = AudioFilterType::None;
		filterCutoff = 0.f;
		filterOrder = 2;
	}

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
		ma_sound_set_velocity(sound, velocity.x, velocity.y, velocity.z);

		lastPosition = position;
		lastUpdateTime = time;
		hasLastUpdate = true;
	}

}
