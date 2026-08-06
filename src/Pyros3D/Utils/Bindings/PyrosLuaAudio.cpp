//============================================================================
// Name        : PyrosLuaAudio.cpp
// Description : AudioManager / AudioBus / Sound / AudioSource.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaAudio(sol::state* lua)
	{
		{
			// ******************************* Audio *******************************

			lua->new_enum("AttenuationModel",
				"None", AttenuationModel::None,
				"Inverse", AttenuationModel::Inverse,
				"Linear", AttenuationModel::Linear,
				"Exponential", AttenuationModel::Exponential
			);

			lua->new_enum("AudioFilterType",
				"None", AudioFilterType::None,
				"LowPass", AudioFilterType::LowPass,
				"HighPass", AudioFilterType::HighPass,
				"BandPass", AudioFilterType::BandPass
			);

			lua->new_enum("AudioEQType",
				"None", AudioEQType::None,
				"Peak", AudioEQType::Peak,
				"Notch", AudioEQType::Notch,
				"LowShelf", AudioEQType::LowShelf,
				"HighShelf", AudioEQType::HighShelf
			);

			// AudioManager - construct exactly one and keep it alive; see the
			// class comment for the active-manager registration this relies on.
			sol::constructors<sol::types<>> audioCon;
			lua->new_usertype<AudioManager>("AudioManager",
				audioCon,
				"isInitialized", &AudioManager::IsInitialized,
				"setMasterVolume", &AudioManager::SetMasterVolume,
				"getMasterVolume", &AudioManager::GetMasterVolume,
				// `dt` overloads spelled out: sol binds a function's full
				// arity, so SetListener()/SetListenerFromGameObject()'s C++
				// default (dt=0, "no Doppler this call") is not optional from
				// Lua - same reason Sound's play()/playAt() below do this.
				// The real per-frame call is the 4-argument one; the shorter
				// ones are for a one-off placement/teleport with no velocity.
				"setListener", sol::overload(
					[](AudioManager &a, const Vec3 &position, const Vec3 &forward) { a.SetListener(position, forward); },
					[](AudioManager &a, const Vec3 &position, const Vec3 &forward, const Vec3 &up) { a.SetListener(position, forward, up); },
					[](AudioManager &a, const Vec3 &position, const Vec3 &forward, const Vec3 &up, const f32 dt) { a.SetListener(position, forward, up, dt); }
				),
				// The common case - point the listener at the camera object.
				// Call it after scene:update(), which is what refreshes the
				// world transform this reads, with dt as this frame's real
				// time step so Doppler has something to compute from.
				"setListenerFromGameObject", sol::overload(
					[](AudioManager &a, GameObject* object) { a.SetListenerFromGameObject(object); },
					[](AudioManager &a, GameObject* object, const f32 dt) { a.SetListenerFromGameObject(object, dt); }
				),
				"getListenerPosition", &AudioManager::GetListenerPosition
				);

			// AudioBus - a named submix ("Music", "SFX"). Always shared_ptr-
			// managed (sol::factories, not sol::constructors) - see the class
			// comment for why: a Sound/AudioSource routed through a bus keeps
			// its own shared_ptr to it, so the bus can't be destroyed out
			// from under something still playing through it. `AudioBus.new()`
			// takes an optional parent bus (also a shared_ptr<AudioBus>) to
			// nest submixes.
			lua->new_usertype<AudioBus>("AudioBus",
				sol::factories(
					[]() { return std::make_shared<AudioBus>(); },
					[](std::shared_ptr<AudioBus> parent) { return std::make_shared<AudioBus>(parent); }
				),
				"isValid", &AudioBus::IsValid,
				"setVolume", &AudioBus::SetVolume,
				"getVolume", &AudioBus::GetVolume,
				"setPitch", &AudioBus::SetPitch,
				"getPitch", &AudioBus::GetPitch,
				"pause", &AudioBus::Pause,
				"resume", &AudioBus::Resume,
				"fadeIn", &AudioBus::FadeIn,
				"fadeOut", &AudioBus::FadeOut
				);

			// Sound - pooled one-shot effects.
			sol::constructors<
				sol::types<std::string>,
				sol::types<std::string, uint32>,
				sol::types<std::string, uint32, std::shared_ptr<AudioBus>>
			> soundCon;
			lua->new_usertype<Sound>("Sound",
				soundCon,
				"isLoaded", &Sound::IsLoaded,
				"getFile", &Sound::GetFile,
				// Defaults spelled out as overloads: sol binds a function's
				// full arity, so C++ default arguments are not optional from
				// Lua (the same reason DeferredRenderer_RenderScene above
				// needs its 3-argument wrapper).
				"play", sol::overload(
					[](Sound &s) { s.Play(); },
					[](Sound &s, const f32 volume) { s.Play(volume); },
					[](Sound &s, const f32 volume, const f32 pitch) { s.Play(volume, pitch); },
					[](Sound &s, const f32 volume, const f32 pitch, const f32 pan) { s.Play(volume, pitch, pan); }
				),
				"playAt", sol::overload(
					[](Sound &s, const Vec3 &position) { s.PlayAt(position); },
					[](Sound &s, const Vec3 &position, const f32 volume) { s.PlayAt(position, volume); },
					[](Sound &s, const Vec3 &position, const f32 volume, const f32 pitch) { s.PlayAt(position, volume, pitch); }
				),
				"stop", &Sound::Stop,
				"getPlayingCount", &Sound::GetPlayingCount,
				"setAttenuation", &Sound::SetAttenuation,
				"setFilter", sol::overload(
					[](Sound &s, const uint32 type, const f32 cutoffHz) { s.SetFilter(type, cutoffHz); },
					[](Sound &s, const uint32 type, const f32 cutoffHz, const uint32 order) { s.SetFilter(type, cutoffHz, order); }
				),
				"clearFilter", &Sound::ClearFilter,
				"getFilterType", &Sound::GetFilterType,
				"getFilterCutoff", &Sound::GetFilterCutoff,
				"getFilterOrder", &Sound::GetFilterOrder,
				"setEQ", sol::overload(
					[](Sound &s, const uint32 type, const f32 frequencyHz, const f32 gainDB) { s.SetEQ(type, frequencyHz, gainDB); },
					[](Sound &s, const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q) { s.SetEQ(type, frequencyHz, gainDB, q); }
				),
				"clearEQ", &Sound::ClearEQ,
				"getEQType", &Sound::GetEQType,
				"getEQFrequency", &Sound::GetEQFrequency,
				"getEQGain", &Sound::GetEQGain,
				"getEQQ", &Sound::GetEQQ,
				"setDelay", sol::overload(
					[](Sound &s, const f32 delaySeconds, const f32 decay) { s.SetDelay(delaySeconds, decay); },
					[](Sound &s, const f32 delaySeconds, const f32 decay, const f32 wet) { s.SetDelay(delaySeconds, decay, wet); },
					[](Sound &s, const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry) { s.SetDelay(delaySeconds, decay, wet, dry); }
				),
				"clearDelay", &Sound::ClearDelay,
				"hasDelay", &Sound::HasDelay,
				"getDelaySeconds", &Sound::GetDelaySeconds,
				"getDelayDecay", &Sound::GetDelayDecay,
				"getDelayWet", &Sound::GetDelayWet,
				"getDelayDry", &Sound::GetDelayDry
				);

			// AudioSource - a positional emitter component. shared_ptr via
			// sol::factories (same pattern as AudioBus / Stage 1 components).
			lua->new_usertype<AudioSource>("AudioSource",
				sol::factories(
					[](const std::string &file) { return std::make_shared<AudioSource>(file); },
					[](const std::string &file, bool stream) { return std::make_shared<AudioSource>(file, stream); },
					[](const std::string &file, bool stream, std::shared_ptr<AudioBus> bus) { return std::make_shared<AudioSource>(file, stream, bus); }
				),
				"isLoaded", &AudioSource::IsLoaded,
				"getFile", &AudioSource::GetFile,
				"play", &AudioSource::Play,
				"pause", &AudioSource::Pause,
				"stop", &AudioSource::Stop,
				"isPlaying", &AudioSource::IsPlaying,
				"setLooping", &AudioSource::SetLooping,
				"isLooping", &AudioSource::IsLooping,
				"setVolume", &AudioSource::SetVolume,
				"getVolume", &AudioSource::GetVolume,
				"setPitch", &AudioSource::SetPitch,
				"getPitch", &AudioSource::GetPitch,
				"setPan", &AudioSource::SetPan,
				"getPan", &AudioSource::GetPan,
				"fadeIn", &AudioSource::FadeIn,
				"fadeOut", &AudioSource::FadeOut,
				"setSpatialization", &AudioSource::SetSpatialization,
				"isSpatialized", &AudioSource::IsSpatialized,
				"setAttenuation", &AudioSource::SetAttenuation,
				"setCone", &AudioSource::SetCone,
				"clearCone", &AudioSource::ClearCone,
				"setDirectionalAttenuation", &AudioSource::SetDirectionalAttenuation,
				"setDopplerFactor", &AudioSource::SetDopplerFactor,
				"resetVelocityTracking", &AudioSource::ResetVelocityTracking,
				"setFilter", sol::overload(
					[](AudioSource &a, const uint32 type, const f32 cutoffHz) { a.SetFilter(type, cutoffHz); },
					[](AudioSource &a, const uint32 type, const f32 cutoffHz, const uint32 order) { a.SetFilter(type, cutoffHz, order); }
				),
				"clearFilter", &AudioSource::ClearFilter,
				"getFilterType", &AudioSource::GetFilterType,
				"getFilterCutoff", &AudioSource::GetFilterCutoff,
				"getFilterOrder", &AudioSource::GetFilterOrder,
				"setEQ", sol::overload(
					[](AudioSource &a, const uint32 type, const f32 frequencyHz, const f32 gainDB) { a.SetEQ(type, frequencyHz, gainDB); },
					[](AudioSource &a, const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q) { a.SetEQ(type, frequencyHz, gainDB, q); }
				),
				"clearEQ", &AudioSource::ClearEQ,
				"getEQType", &AudioSource::GetEQType,
				"getEQFrequency", &AudioSource::GetEQFrequency,
				"getEQGain", &AudioSource::GetEQGain,
				"getEQQ", &AudioSource::GetEQQ,
				"setDelay", sol::overload(
					[](AudioSource &a, const f32 delaySeconds, const f32 decay) { a.SetDelay(delaySeconds, decay); },
					[](AudioSource &a, const f32 delaySeconds, const f32 decay, const f32 wet) { a.SetDelay(delaySeconds, decay, wet); },
					[](AudioSource &a, const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry) { a.SetDelay(delaySeconds, decay, wet, dry); }
				),
				"clearDelay", &AudioSource::ClearDelay,
				"hasDelay", &AudioSource::HasDelay,
				"getDelaySeconds", &AudioSource::GetDelaySeconds,
				"getDelayDecay", &AudioSource::GetDelayDecay,
				"getDelayWet", &AudioSource::GetDelayWet,
				"getDelayDry", &AudioSource::GetDelayDry,
				"getLengthSeconds", &AudioSource::GetLengthSeconds,
				"getCursorSeconds", &AudioSource::GetCursorSeconds,
				"seekSeconds", &AudioSource::SeekSeconds,
				"atEnd", &AudioSource::AtEnd,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

	}

} // namespace p3d

#endif
