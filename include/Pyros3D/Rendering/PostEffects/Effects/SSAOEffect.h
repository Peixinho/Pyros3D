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
        SSAOEffect(const uint32& Tex1);
        virtual ~SSAOEffect();

        f32 total_strength;
        f32 base;
        f32 area;
        f32 falloff;
        f32 radius;
        uint32 samples;
        f32 scale;

    private:

        Texture* rnm;
        
    };

};

#endif	/* SSAOEFFECT_H */