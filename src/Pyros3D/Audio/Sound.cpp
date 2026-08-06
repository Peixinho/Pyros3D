//============================================================================
// Name        : Sound.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A loaded sound effect with a pool of playback voices
//============================================================================

#include <Pyros3D/Audio/Sound.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioBus.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/miniaudio/miniaudio.h>
#include "AudioEffectChain.h"

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

	Sound::Sound(const std::string &file, const uint32 voices, const std::shared_ptr<AudioBus> &bus)
		: file(file), loaded(false), bus(bus), nextVoice(0),
		attenuationModel(AttenuationModel::Linear), minDistance(1.f), maxDistance(100.f)
	{
		AudioManager* audio = AudioManager::GetActive();
		if (audio == NULL || !audio->IsInitialized())
		{
			echo("WARNING: Sound - no initialized AudioManager, '" + file + "' not loaded");
			return;
		}

		const uint32 count = (voices == 0) ? 1 : voices;
		ma_sound* group = this->bus ? this->bus->GetGroup() : NULL;

		for (uint32 i = 0; i < count; i++)
		{
			ma_sound* voice = new ma_sound();

			// DECODE: the whole file is decoded into memory once, up front.
			// Right for effects - it makes every later Play() free of decoding
			// work, and the same decoded data is shared between these voices
			// by miniaudio's resource manager (they are all the same path).
			// NO_SPATIALIZATION is the default state; PlayAt() turns it on per
			// trigger, so one pool serves both 2D and positioned playback.
			ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

			if (ma_sound_init_from_file(audio->GetEngine(), file.c_str(), flags, group, NULL, voice) != MA_SUCCESS)
			{
				delete voice;
				// Only complain once, and tear down any voices that did load -
				// a half-initialized pool would be worse than none.
				echo("WARNING: Sound - could not load '" + file + "'");
				for (uint32 k = 0; k < this->voices.size(); k++)
				{
					ma_sound_uninit(this->voices[k]);
					delete this->voices[k];
				}
				this->voices.clear();
				return;
			}

			ma_sound_set_min_distance(voice, minDistance);
			ma_sound_set_max_distance(voice, maxDistance);
			ma_sound_set_attenuation_model(voice, TranslateAttenuation(attenuationModel));

			audio->RegisterVoice(voice);
			this->voices.push_back(voice);
			chains.push_back(new detail::AudioEffectChain());
		}

		loaded = true;
	}

	Sound::~Sound()
	{
		// Only if the manager is still alive: ~AudioManager() calls
		// ma_engine_uninit(), which already tore down every node, and
		// ma_sound_uninit() on a voice whose engine is gone walks freed
		// memory. Mirrors the IsActiveRenderDeviceSet() guard on the GPU
		// resource destructors, and for exactly the same reason - Lua-owned
		// objects are finalized in an order nothing here controls.
		AudioManager* audio = AudioManager::GetActive();
		const bool engineAlive = audio != NULL && audio->IsInitialized();

		for (uint32 i = 0; i < voices.size(); i++)
		{
			if (engineAlive)
			{
				// No rerouting needed - see AudioSource::~AudioSource()'s
				// identical reasoning.
				if (i < chains.size()) chains[i]->Destroy();

				// Unregister first: the manager's own teardown must not find
				// a voice this destructor is about to uninitialize.
				audio->UnregisterVoice(voices[i]);
				ma_sound_uninit(voices[i]);
			}
			delete voices[i];
			if (i < chains.size()) delete chains[i];
		}
		voices.clear();
		chains.clear();
		// `bus` releases its reference here, after every voice that routed
		// through it is gone.
	}

	ma_sound* Sound::AcquireVoice()
	{
		if (voices.empty()) return NULL;

		// Prefer a voice that has finished.
		for (uint32 i = 0; i < voices.size(); i++)
		{
			const uint32 index = (nextVoice + i) % voices.size();
			if (!ma_sound_is_playing(voices[index]))
			{
				nextVoice = (index + 1) % voices.size();
				return voices[index];
			}
		}

		// All busy - steal the round-robin one, which is the least recently
		// started. See the class comment on why stealing beats dropping.
		ma_sound* voice = voices[nextVoice];
		nextVoice = (nextVoice + 1) % voices.size();
		ma_sound_stop(voice);
		return voice;
	}

	void Sound::Play(const f32 volume, const f32 pitch, const f32 pan)
	{
		if (!loaded) return;

		ma_sound* voice = AcquireVoice();
		if (voice == NULL) return;

		ma_sound_set_spatialization_enabled(voice, MA_FALSE);
		ma_sound_set_volume(voice, volume);
		ma_sound_set_pitch(voice, pitch);
		ma_sound_set_pan(voice, pan);
		// Rewind: a stolen voice is mid-playback, and even a finished one
		// stays parked at its end frame.
		ma_sound_seek_to_pcm_frame(voice, 0);
		ma_sound_start(voice);
	}

	void Sound::PlayAt(const Vec3 &position, const f32 volume, const f32 pitch)
	{
		if (!loaded) return;

		ma_sound* voice = AcquireVoice();
		if (voice == NULL) return;

		ma_sound_set_spatialization_enabled(voice, MA_TRUE);
		ma_sound_set_position(voice, position.x, position.y, position.z);
		ma_sound_set_volume(voice, volume);
		ma_sound_set_pitch(voice, pitch);
		ma_sound_seek_to_pcm_frame(voice, 0);
		ma_sound_start(voice);
	}

	void Sound::Stop()
	{
		if (!loaded) return;
		for (uint32 i = 0; i < voices.size(); i++)
			ma_sound_stop(voices[i]);
	}

	uint32 Sound::GetPlayingCount() const
	{
		if (!loaded) return 0;
		uint32 count = 0;
		for (uint32 i = 0; i < voices.size(); i++)
			if (ma_sound_is_playing(voices[i])) count++;
		return count;
	}

	void Sound::SetAttenuation(const uint32 model, const f32 minDistance, const f32 maxDistance)
	{
		attenuationModel = model;
		this->minDistance = minDistance;
		this->maxDistance = maxDistance;

		if (!loaded) return;
		for (uint32 i = 0; i < voices.size(); i++)
		{
			ma_sound_set_attenuation_model(voices[i], TranslateAttenuation(model));
			ma_sound_set_min_distance(voices[i], minDistance);
			ma_sound_set_max_distance(voices[i], maxDistance);
		}
	}

	// Shared by SetFilter/SetEQ/SetDelay below - see AudioSource.cpp's
	// identical helper for why `engine` is NULL when there is nothing live
	// to create a node against.
	namespace {
		void ResolveChainTargets(bool loaded, AudioBus* bus, ma_engine* &engine)
		{
			engine = NULL;
			if (!loaded || !EngineAlive()) return;
			engine = AudioManager::GetActive()->GetEngine();
		}
	}

	void Sound::SetFilter(const uint32 type, const f32 cutoffHz, const uint32 order)
	{
		ma_engine* engine;
		ResolveChainTargets(loaded, bus.get(), engine);
		void* target = (engine != NULL) ? (bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine))) : NULL;
		for (uint32 i = 0; i < chains.size(); i++)
			chains[i]->SetFilter(engine, reinterpret_cast<void*>(voices[i]), target, type, cutoffHz, order);
	}

	void Sound::ClearFilter()
	{
		ma_engine* engine;
		ResolveChainTargets(loaded, bus.get(), engine);
		void* target = (engine != NULL) ? (bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine))) : NULL;
		for (uint32 i = 0; i < chains.size(); i++)
			chains[i]->ClearFilter(reinterpret_cast<void*>(voices[i]), target);
	}

	// Every voice's chain is kept configured identically by the loops above,
	// so any one of them (chains[0], if the pool isn't empty) represents the
	// whole pool's state.
	uint32 Sound::GetFilterType() const { return chains.empty() ? AudioFilterType::None : chains[0]->filterType; }
	f32 Sound::GetFilterCutoff() const { return chains.empty() ? 0.f : chains[0]->filterCutoff; }
	uint32 Sound::GetFilterOrder() const { return chains.empty() ? 2u : chains[0]->filterOrder; }

	void Sound::SetEQ(const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q)
	{
		ma_engine* engine;
		ResolveChainTargets(loaded, bus.get(), engine);
		void* target = (engine != NULL) ? (bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine))) : NULL;
		for (uint32 i = 0; i < chains.size(); i++)
			chains[i]->SetEQ(engine, reinterpret_cast<void*>(voices[i]), target, type, frequencyHz, gainDB, q);
	}

	void Sound::ClearEQ()
	{
		ma_engine* engine;
		ResolveChainTargets(loaded, bus.get(), engine);
		void* target = (engine != NULL) ? (bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine))) : NULL;
		for (uint32 i = 0; i < chains.size(); i++)
			chains[i]->ClearEQ(reinterpret_cast<void*>(voices[i]), target);
	}

	uint32 Sound::GetEQType() const { return chains.empty() ? AudioEQType::None : chains[0]->eqType; }
	f32 Sound::GetEQFrequency() const { return chains.empty() ? 0.f : chains[0]->eqFrequency; }
	f32 Sound::GetEQGain() const { return chains.empty() ? 0.f : chains[0]->eqGainDB; }
	f32 Sound::GetEQQ() const { return chains.empty() ? 1.f : chains[0]->eqQ; }

	void Sound::SetDelay(const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry)
	{
		ma_engine* engine;
		ResolveChainTargets(loaded, bus.get(), engine);
		void* target = (engine != NULL) ? (bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine))) : NULL;
		for (uint32 i = 0; i < chains.size(); i++)
			chains[i]->SetDelay(engine, reinterpret_cast<void*>(voices[i]), target, delaySeconds, decay, wet, dry);
	}

	void Sound::ClearDelay()
	{
		ma_engine* engine;
		ResolveChainTargets(loaded, bus.get(), engine);
		void* target = (engine != NULL) ? (bus ? reinterpret_cast<void*>(bus->GetGroup()) : reinterpret_cast<void*>(ma_engine_get_endpoint(engine))) : NULL;
		for (uint32 i = 0; i < chains.size(); i++)
			chains[i]->ClearDelay(reinterpret_cast<void*>(voices[i]), target);
	}

	bool Sound::HasDelay() const { return !chains.empty() && chains[0]->hasDelay; }
	f32 Sound::GetDelaySeconds() const { return chains.empty() ? 0.f : chains[0]->delaySeconds; }
	f32 Sound::GetDelayDecay() const { return chains.empty() ? 0.f : chains[0]->delayDecay; }
	f32 Sound::GetDelayWet() const { return chains.empty() ? 1.f : chains[0]->delayWet; }
	f32 Sound::GetDelayDry() const { return chains.empty() ? 1.f : chains[0]->delayDry; }

}
