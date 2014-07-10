//============================================================================
// Name        : Sound.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
//============================================================================

#ifndef SOUND_H
#define SOUND_H

#include <Pyros3D/AssetManager/Assets/Sounds/ISound.h>

namespace p3d {
    
    class Sound : public ISound {

        public:

            Sound();
            virtual ~Sound();

    };
    
};

#endif /* SOUND_H */
