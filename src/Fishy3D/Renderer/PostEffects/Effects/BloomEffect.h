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

namespace Fishy3D {

    class BloomEffect : public IEffect {
        public:
            BloomEffect(const unsigned &Tex1);
            virtual ~BloomEffect();
        private:

    };

}

#endif	/* BLOOMEFFECT_H */