//============================================================================
// Name        : RTT Debug.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RTT Debug Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef RTTDEBUG_H
#define RTTDEBUG_H

namespace p3d {

    class PYROS3D_API RTTDebug : public IEffect {
        public:
            RTTDebug(const uint32 Tex1,const uint32 Tex2, const uint32 Width, const uint32 Height);
            virtual ~RTTDebug();
        private:
    };

}

#endif	/* RTTDEBUG_H */

