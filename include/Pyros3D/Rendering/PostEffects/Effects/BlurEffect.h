//============================================================================
// Name        : BlurEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include "IEffect.h"

#ifndef BLUREFFECT_H
#define	BLUREFFECT_H

namespace p3d {

    class BlurEffect : public IEffect {
        public:
            BlurEffect(const uint32 &Tex1);
            virtual ~BlurEffect();
        private:

    };

};

#endif	/* BLUREFFECT_H */