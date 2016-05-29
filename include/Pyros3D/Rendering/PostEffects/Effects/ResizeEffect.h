//============================================================================
// Name        : ResizeEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Resize Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef RESIZEEFFECT_H
#define	RESIZEEFFECT_H

namespace p3d {

    class PYROS3D_API ResizeEffect : public IEffect {
        public:
            ResizeEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
            virtual ~ResizeEffect();
    };

};

#endif	/* VIGNETTEEFFECT_H */