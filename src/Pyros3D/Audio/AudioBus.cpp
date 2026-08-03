//============================================================================
// Name        : AudioBus.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A named submix - "Music", "SFX", etc - for group volume,
//               pausing and fades
//============================================================================

#include <Pyros3D/Audio/AudioBus.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/miniaudio/miniaudio.h>

namespace p3d {

	namespace {
		bool EngineAlive()
		{
			return AudioManager::IsActiveSet() && AudioManager::GetActive()->IsInitialized();
		}
	}

	AudioBus::AudioBus(const std::shared_ptr<AudioBus> &parent)
		: parent(parent), group(NULL), volume(1.f), pitch(1.f)
	{
		if (!EngineAlive())
		{
			echo("WARNING: AudioBus - no initialized AudioManager, bus not created");
			return;
		}

		group = new ma_sound();
		ma_sound* parentGroup = this->parent ? this->parent->GetGroup() : NULL;

		if (ma_sound_group_init(AudioManager::GetActive()->GetEngine(), 0, parentGroup, group) != MA_SUCCESS)
		{
			echo("WARNING: AudioBus - could not initialize");
			delete group;
			group = NULL;
			return;
		}

		// ma_sound_group is a plain typedef of ma_sound (miniaudio: "typedef
		// ma_sound ma_sound_group;" - a group IS a sound-shaped node with no
		// data source), so ma_sound_group_uninit() is exactly ma_sound_uninit()
		// under the hood, and this registry - built for Sound/AudioSource's
		// own voices - covers buses for free. Same guarantee: if this bus is
		// still alive when the engine goes down (a bus is just as reachable
		// from Lua as anything else here), the manager uninitializes it
		// before freeing the engine instead of leaving it stranded.
		AudioManager::GetActive()->RegisterVoice(reinterpret_cast<ma_sound*>(group));
	}

	AudioBus::~AudioBus()
	{
		if (group != NULL)
		{
			if (EngineAlive())
			{
				AudioManager::GetActive()->UnregisterVoice(reinterpret_cast<ma_sound*>(group));
				ma_sound_group_uninit(group);
			}
			delete group;
			group = NULL;
		}
		// `parent` releases its reference here, after `group` (which may have
		// been attached as a child of the parent's own group) no longer
		// needs it.
	}

	void AudioBus::SetVolume(const f32 volume)
	{
		this->volume = volume;
		if (group == NULL) return;
		ma_sound_group_set_volume(group, volume);
	}

	void AudioBus::SetPitch(const f32 pitch)
	{
		this->pitch = pitch;
		if (group == NULL) return;
		ma_sound_group_set_pitch(group, pitch);
	}

	void AudioBus::Pause()
	{
		if (group == NULL) return;
		ma_sound_group_stop(group);
	}

	void AudioBus::Resume()
	{
		if (group == NULL) return;
		ma_sound_group_start(group);
	}

	void AudioBus::FadeIn(const f32 milliseconds)
	{
		if (group == NULL) return;
		ma_sound_group_set_fade_in_milliseconds(group, 0.f, volume, (ma_uint64)milliseconds);
		ma_sound_group_start(group);
	}

	void AudioBus::FadeOut(const f32 milliseconds)
	{
		if (group == NULL) return;
		ma_sound_group_set_fade_in_milliseconds(group, -1.f, 0.f, (ma_uint64)milliseconds);
	}

}
