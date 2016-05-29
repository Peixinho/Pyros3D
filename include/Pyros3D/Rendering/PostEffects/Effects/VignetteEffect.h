//============================================================================
// Name        : VignetteEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vignette Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef VIGNETTEEFFECT_H
#define	VIGNETTEEFFECT_H

namespace p3d {

    class PYROS3D_API VignetteEffect : public IEffect {
        public:
            VignetteEffect(const uint32 Tex1, const uint32 Width, const uint32 Height, const f32 radius = 0.5f, const f32 softness = 0.2f);
            virtual ~VignetteEffect();

			void SetRadius(const f32 radius);
			void SetSoftness(const f32 softness);

        private:

			Uniform 
					screenDimensions,
					radiusUniform,
					softnessUniform;

    };

};

#endif	/* VIGNETTEEFFECT_H */