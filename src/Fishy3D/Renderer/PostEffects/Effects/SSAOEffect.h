//============================================================================
// Name        : SSaoEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Effect
//============================================================================

#include "IEffect.h"

#ifndef SSAOEFFECT_H
#define SSAOEFFECT_H

namespace Fishy3D {

    class SSAOEffect : public IEffect {
    public:
        SSAOEffect(const unsigned &Tex1, const unsigned &Tex2);
        virtual ~SSAOEffect();
    private:

        Texture rnm;
        
    };

};

#endif	/* SSAOEFFECT_H */