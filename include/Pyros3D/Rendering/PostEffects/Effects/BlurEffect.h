//============================================================================
// Name        : BlurEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef BLUREFFECT_H
#define	BLUREFFECT_H

namespace p3d {

    class PYROS3D_API BlurEffect : public IEffect {
        public:
            BlurEffect(const uint32 &Tex1);
            virtual ~BlurEffect();
        private:

    };

};

#endif	/* BLUREFFECT_H */