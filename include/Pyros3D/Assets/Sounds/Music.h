//============================================================================
// Name        : Music.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sound Interface
//============================================================================

#ifndef MUSIC_H
#define MUSIC_H

#include <Pyros3D/Assets/Sounds/ISound.h>

namespace p3d {
    
    class PYROS3D_API Music : public ISound {

        public:

            Music();
            virtual ~Music();

    };
    
};

#endif /* MUSIC_H */
