//============================================================================
// Name        : BloomEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bloom Effect
//============================================================================
#ifndef BLOOMEFFECT_H
#define	BLOOMEFFECT_H

#include "IEffect.h"

namespace p3d {

    class BloomEffect : public IEffect {
        public:
            BloomEffect(const uint32 &Tex1);
            virtual ~BloomEffect();
        private:

    };

}

#endif	/* BLOOMEFFECT_H */