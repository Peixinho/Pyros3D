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

namespace p3d {

    class SSAOEffect : public IEffect {
    public:
        SSAOEffect(const uint32 &Tex1, const uint32 &Tex2);
        virtual ~SSAOEffect();
    private:

        Texture rnm;
        
    };

};

#endif	/* SSAOEFFECT_H */