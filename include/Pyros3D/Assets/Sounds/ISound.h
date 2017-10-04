//============================================================================
// Name        : ISound.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
// NOTE        : It has SDL preprocessor directives because emscritpen / android / ios?
//============================================================================

#ifndef ISOUND_H
#define ISOUND_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Ext/Signals/Signal.h>
#include <Pyros3D/Ext/Signals/Delegate.h>
#include <Pyros3D/Other/Export.h>

#if defined(_SDL2) || defined(_SDL)
#include <SDL.h>
#include <SDL_mixer.h>
#else
#include <SFML/Audio.hpp>
#endif

namespace p3d {

	namespace SoundType
	{
		enum {
			Sound = 0,
			Music
		};
	}

	class PYROS3D_API ISound {

	public:

		ISound(const uint32 type);
		virtual ~ISound();

		bool LoadFromFile(const std::string &filename);

		void Play(bool loop = false);
		void Pause();
		void Stop();
		bool isPlaying();
		bool isPaused();
		void SetVolume(const uint32 vol); // 0-100
		const uint32 &GetVolume() const;
		void SetPitch(f32 pitch);
		const f32 &GetPitch();

	protected:

		bool _isPlaying;
		bool _isPaused;
		bool _loop;
		f32 _pitch;
		uint32 _volume;
		uint32 _type;

#if defined(_SDL2) || defined(_SDL)
		Mix_Music *_Music;
		Mix_Chunk *_Sound;
		int32 channel;
#else 
		sf::Music _Music;
		sf::Sound _Sound;
		sf::SoundBuffer _Buffer;
#endif
	};

};

#endif /* ISOUND_H */
