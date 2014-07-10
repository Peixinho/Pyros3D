//============================================================================
// Name        : ISound.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
//============================================================================

#ifndef ISOUND_H
#define ISOUND_H

#include "../../../Core/Math/Math.h"
#include "../../../Ext/Signals/Signal.h"
#include "../../../Ext/Signals/Delegate.h"
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

            // CallBacks
            void OnStart(void (*func) (void));
            template< class X, class Y >
            void OnStart( Y * obj, void (X::*func)() )
            {
                haveOnStartFunction = true;
                OnStartFunction.Connect(obj,func);
            }
            
            void OnUpdate(void (*func) (void));
            template< class X, class Y >
            void OnUpdate( Y * obj, void (X::*func)() )
            {
                haveOnUpdateFunction = true;
                OnUpdateFunction.Connect(obj,func);
            }
            
            void OnEnd(void (*func) (void));
            template< class X, class Y >
            void OnEnd( Y * obj, void (X::*func)() )
            {
                haveOnEndFunction = true;
                OnEndFunction.Connect(obj,func);
            }

        protected:

            // CallBack Functions
            Gallant::Signal0<void> OnStartFunction;
            Gallant::Signal0<void> OnUpdateFunction;
            Gallant::Signal0<void> OnEndFunction;
            bool haveOnStartFunction;
            bool haveOnUpdateFunction;
            bool haveOnEndFunction;

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
