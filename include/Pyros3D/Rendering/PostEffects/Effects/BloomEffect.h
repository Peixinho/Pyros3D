//============================================================================
// Name        : BloomEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bloom Effect
//============================================================================
#ifndef BLOOMEFFECT_H
#define	BLOOMEFFECT_H

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

namespace p3d {

    class PYROS3D_API BloomEffect : public IEffect {
        public:
            BloomEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
            virtual ~BloomEffect();
        private:

    };

}

#endif	/* BLOOMEFFECT_H */