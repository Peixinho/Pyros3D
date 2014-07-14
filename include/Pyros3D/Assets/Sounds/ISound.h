//============================================================================
// Name        : ISound.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
//============================================================================

#ifndef ISOUND_H
#define ISOUND_H

#include "../../Core/Math/Math.h"
#include "../../Ext/Signals/Signal.h"
#include "../../Ext/Signals/Delegate.h"
#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

namespace p3d {
    
    namespace SoundType
    {
        enum {
            Sound = 0,
            Music
        };
    }

    class ISound {

        public:

            ISound(const uint32 &type);
            virtual ~ISound();

            bool LoadFromFile(const std::string &filename);

            void Play();
            void Pause();
            void Stop();
            bool isPlaying();
            bool isPaused();
            void Loop(bool loop);

        protected:

            bool _isPlaying;
            bool _isPaused;
            bool _loop;
            uint32 _type;

            sf::Music _Music;
            sf::Sound _Sound;
            sf::SoundBuffer _Buffer;
    };
    
};

#endif /* ISOUND_H */
