//============================================================================
// Name        : PyrosEmbindAudio.cpp
// Description : Embind AudioManager / AudioBus / Sound / AudioSource.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioBus.h>
#include <Pyros3D/Audio/Sound.h>
#include <Pyros3D/Audio/AudioSource.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Math/Math.h>

#include <memory>
#include <string>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;

namespace {

	std::shared_ptr<AudioBus> MakeBus() { return std::make_shared<AudioBus>(); }
	std::shared_ptr<AudioBus> MakeBusParent(std::shared_ptr<AudioBus> parent) { return std::make_shared<AudioBus>(parent); }

	std::shared_ptr<AudioSource> MakeAudioSource(const std::string &file) { return std::make_shared<AudioSource>(file); }
	std::shared_ptr<AudioSource> MakeAudioSourceStream(const std::string &file, bool stream) { return std::make_shared<AudioSource>(file, stream); }
	std::shared_ptr<AudioSource> MakeAudioSourceBus(const std::string &file, bool stream, std::shared_ptr<AudioBus> bus)
	{
		return std::make_shared<AudioSource>(file, stream, bus);
	}

	void AudioManager_SetListener2(AudioManager &a, const Vec3 &pos, const Vec3 &fwd) { a.SetListener(pos, fwd); }
	void AudioManager_SetListener3(AudioManager &a, const Vec3 &pos, const Vec3 &fwd, const Vec3 &up) { a.SetListener(pos, fwd, up); }
	void AudioManager_SetListener4(AudioManager &a, const Vec3 &pos, const Vec3 &fwd, const Vec3 &up, f32 dt) { a.SetListener(pos, fwd, up, dt); }
	void AudioManager_SetListenerGO(AudioManager &a, GameObject *go) { a.SetListenerFromGameObject(go); }
	void AudioManager_SetListenerGODt(AudioManager &a, GameObject *go, f32 dt) { a.SetListenerFromGameObject(go, dt); }

	void Sound_Play0(Sound &s) { s.Play(); }
	void Sound_Play1(Sound &s, f32 volume) { s.Play(volume); }
	void Sound_Play2(Sound &s, f32 volume, f32 pitch) { s.Play(volume, pitch); }
	void Sound_Play3(Sound &s, f32 volume, f32 pitch, f32 pan) { s.Play(volume, pitch, pan); }
	void Sound_PlayAt1(Sound &s, const Vec3 &pos) { s.PlayAt(pos); }
	void Sound_PlayAt2(Sound &s, const Vec3 &pos, f32 volume) { s.PlayAt(pos, volume); }
	void Sound_PlayAt3(Sound &s, const Vec3 &pos, f32 volume, f32 pitch) { s.PlayAt(pos, volume, pitch); }
	void Sound_SetFilter2(Sound &s, uint32 type, f32 cutoff) { s.SetFilter(type, cutoff); }
	void Sound_SetFilter3(Sound &s, uint32 type, f32 cutoff, uint32 order) { s.SetFilter(type, cutoff, order); }
	void Sound_SetEQ3(Sound &s, uint32 type, f32 freq, f32 gain) { s.SetEQ(type, freq, gain); }
	void Sound_SetEQ4(Sound &s, uint32 type, f32 freq, f32 gain, f32 q) { s.SetEQ(type, freq, gain, q); }
	void Sound_SetDelay2(Sound &s, f32 delay, f32 decay) { s.SetDelay(delay, decay); }
	void Sound_SetDelay3(Sound &s, f32 delay, f32 decay, f32 wet) { s.SetDelay(delay, decay, wet); }
	void Sound_SetDelay4(Sound &s, f32 delay, f32 decay, f32 wet, f32 dry) { s.SetDelay(delay, decay, wet, dry); }

	void AudioSource_SetFilter2(AudioSource &a, uint32 type, f32 cutoff) { a.SetFilter(type, cutoff); }
	void AudioSource_SetFilter3(AudioSource &a, uint32 type, f32 cutoff, uint32 order) { a.SetFilter(type, cutoff, order); }
	void AudioSource_SetEQ3(AudioSource &a, uint32 type, f32 freq, f32 gain) { a.SetEQ(type, freq, gain); }
	void AudioSource_SetEQ4(AudioSource &a, uint32 type, f32 freq, f32 gain, f32 q) { a.SetEQ(type, freq, gain, q); }
	void AudioSource_SetDelay2(AudioSource &a, f32 delay, f32 decay) { a.SetDelay(delay, decay); }
	void AudioSource_SetDelay3(AudioSource &a, f32 delay, f32 decay, f32 wet) { a.SetDelay(delay, decay, wet); }
	void AudioSource_SetDelay4(AudioSource &a, f32 delay, f32 decay, f32 wet, f32 dry) { a.SetDelay(delay, decay, wet, dry); }

} // namespace

namespace p3d {
	void PyrosEmbindAudioForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_audio)
{
	class_<AudioManager>("AudioManager")
		.constructor<>()
		.function("isInitialized", &AudioManager::IsInitialized)
		.function("setMasterVolume", &AudioManager::SetMasterVolume)
		.function("getMasterVolume", &AudioManager::GetMasterVolume)
		.function("setListener", &AudioManager_SetListener2)
		.function("setListenerUp", &AudioManager_SetListener3)
		.function("setListenerDt", &AudioManager_SetListener4)
		.function("setListenerFromGameObject", &AudioManager_SetListenerGO, allow_raw_pointers())
		.function("setListenerFromGameObjectDt", &AudioManager_SetListenerGODt, allow_raw_pointers())
		.function("getListenerPosition", &AudioManager::GetListenerPosition);

	class_<AudioBus>("AudioBus")
		.smart_ptr<std::shared_ptr<AudioBus>>("AudioBusPtr")
		.constructor(&MakeBus)
		.constructor(&MakeBusParent)
		.function("isValid", &AudioBus::IsValid)
		.function("setVolume", &AudioBus::SetVolume)
		.function("getVolume", &AudioBus::GetVolume)
		.function("setPitch", &AudioBus::SetPitch)
		.function("getPitch", &AudioBus::GetPitch)
		.function("pause", &AudioBus::Pause)
		.function("resume", &AudioBus::Resume)
		.function("fadeIn", &AudioBus::FadeIn)
		.function("fadeOut", &AudioBus::FadeOut);

	class_<Sound>("Sound")
		.constructor<std::string>()
		.constructor<std::string, uint32>()
		.constructor<std::string, uint32, std::shared_ptr<AudioBus>>()
		.function("isLoaded", &Sound::IsLoaded)
		.function("getFile", &Sound::GetFile)
		.function("play", &Sound_Play0)
		.function("playVolume", &Sound_Play1)
		.function("playVolumePitch", &Sound_Play2)
		.function("playFull", &Sound_Play3)
		.function("playAt", &Sound_PlayAt1)
		.function("playAtVolume", &Sound_PlayAt2)
		.function("playAtVolumePitch", &Sound_PlayAt3)
		.function("stop", &Sound::Stop)
		.function("getPlayingCount", &Sound::GetPlayingCount)
		.function("setAttenuation", &Sound::SetAttenuation)
		.function("setFilter", &Sound_SetFilter2)
		.function("setFilterOrder", &Sound_SetFilter3)
		.function("clearFilter", &Sound::ClearFilter)
		.function("getFilterType", &Sound::GetFilterType)
		.function("getFilterCutoff", &Sound::GetFilterCutoff)
		.function("getFilterOrder", &Sound::GetFilterOrder)
		.function("setEQ", &Sound_SetEQ3)
		.function("setEQQ", &Sound_SetEQ4)
		.function("clearEQ", &Sound::ClearEQ)
		.function("getEQType", &Sound::GetEQType)
		.function("getEQFrequency", &Sound::GetEQFrequency)
		.function("getEQGain", &Sound::GetEQGain)
		.function("getEQQ", &Sound::GetEQQ)
		.function("setDelay", &Sound_SetDelay2)
		.function("setDelayWet", &Sound_SetDelay3)
		.function("setDelayFull", &Sound_SetDelay4)
		.function("clearDelay", &Sound::ClearDelay)
		.function("hasDelay", &Sound::HasDelay)
		.function("getDelaySeconds", &Sound::GetDelaySeconds)
		.function("getDelayDecay", &Sound::GetDelayDecay)
		.function("getDelayWet", &Sound::GetDelayWet)
		.function("getDelayDry", &Sound::GetDelayDry);

	class_<AudioSource, base<IComponent>>("AudioSource")
		.smart_ptr<std::shared_ptr<AudioSource>>("AudioSourcePtr")
		.constructor(&MakeAudioSource)
		.constructor(&MakeAudioSourceStream)
		.constructor(&MakeAudioSourceBus)
		.function("isLoaded", &AudioSource::IsLoaded)
		.function("getFile", &AudioSource::GetFile)
		.function("play", &AudioSource::Play)
		.function("pause", &AudioSource::Pause)
		.function("stop", &AudioSource::Stop)
		.function("isPlaying", &AudioSource::IsPlaying)
		.function("setLooping", &AudioSource::SetLooping)
		.function("isLooping", &AudioSource::IsLooping)
		.function("setVolume", &AudioSource::SetVolume)
		.function("getVolume", &AudioSource::GetVolume)
		.function("setPitch", &AudioSource::SetPitch)
		.function("getPitch", &AudioSource::GetPitch)
		.function("setPan", &AudioSource::SetPan)
		.function("getPan", &AudioSource::GetPan)
		.function("fadeIn", &AudioSource::FadeIn)
		.function("fadeOut", &AudioSource::FadeOut)
		.function("setSpatialization", &AudioSource::SetSpatialization)
		.function("isSpatialized", &AudioSource::IsSpatialized)
		.function("setAttenuation", &AudioSource::SetAttenuation)
		.function("setCone", &AudioSource::SetCone)
		.function("clearCone", &AudioSource::ClearCone)
		.function("setDirectionalAttenuation", &AudioSource::SetDirectionalAttenuation)
		.function("setDopplerFactor", &AudioSource::SetDopplerFactor)
		.function("resetVelocityTracking", &AudioSource::ResetVelocityTracking)
		.function("setFilter", &AudioSource_SetFilter2)
		.function("setFilterOrder", &AudioSource_SetFilter3)
		.function("clearFilter", &AudioSource::ClearFilter)
		.function("getFilterType", &AudioSource::GetFilterType)
		.function("getFilterCutoff", &AudioSource::GetFilterCutoff)
		.function("getFilterOrder", &AudioSource::GetFilterOrder)
		.function("setEQ", &AudioSource_SetEQ3)
		.function("setEQQ", &AudioSource_SetEQ4)
		.function("clearEQ", &AudioSource::ClearEQ)
		.function("getEQType", &AudioSource::GetEQType)
		.function("getEQFrequency", &AudioSource::GetEQFrequency)
		.function("getEQGain", &AudioSource::GetEQGain)
		.function("getEQQ", &AudioSource::GetEQQ)
		.function("setDelay", &AudioSource_SetDelay2)
		.function("setDelayWet", &AudioSource_SetDelay3)
		.function("setDelayFull", &AudioSource_SetDelay4)
		.function("clearDelay", &AudioSource::ClearDelay)
		.function("hasDelay", &AudioSource::HasDelay)
		.function("getDelaySeconds", &AudioSource::GetDelaySeconds)
		.function("getDelayDecay", &AudioSource::GetDelayDecay)
		.function("getDelayWet", &AudioSource::GetDelayWet)
		.function("getDelayDry", &AudioSource::GetDelayDry)
		.function("getLengthSeconds", &AudioSource::GetLengthSeconds)
		.function("getCursorSeconds", &AudioSource::GetCursorSeconds)
		.function("seekSeconds", &AudioSource::SeekSeconds)
		.function("atEnd", &AudioSource::AtEnd);
}

#endif /* EMSCRIPTEN */
