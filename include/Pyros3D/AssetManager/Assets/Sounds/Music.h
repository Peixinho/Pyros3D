//============================================================================
// Name        : Music.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
//============================================================================

#ifndef MUSIC_H
#define MUSIC_H

#include "Pyros3D/AssetManager/Assets/Sounds/ISound.h"

namespace p3d {
    
    class Music : public ISound {

        public:

            Music();
            virtual ~Music();

    };
    
};

#endif /* MUSIC_H */
