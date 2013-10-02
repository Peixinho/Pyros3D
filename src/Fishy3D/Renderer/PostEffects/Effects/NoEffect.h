//============================================================================
// Name        : NoEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : No Effect - Simply Show Color Buffer in a Quad
//============================================================================

#ifndef NOEFFECT_H
#define	NOEFFECT_H

#include "IEffect.h"

namespace Fishy3D {

    class NoEffect : public IEffect {
        public:
            NoEffect(const unsigned &Tex1);
            virtual ~NoEffect();
        private:

    };

};

#endif	/* NOEFFECT_H */