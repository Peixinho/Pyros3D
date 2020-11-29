//============================================================================
// Name        : MotionBlurEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : MotionBlur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef MOTIONBLUREFFECT_H
#define	MOTIONBLUREFFECT_H

namespace p3d {

	class PYROS3D_API MotionBlurEffect : public IEffect {
	public:
		MotionBlurEffect(const uint32 Tex1, Texture* VelocityMap, const uint32 Width, const uint32 Height);
		virtual ~MotionBlurEffect();
	private:
		Uniform *texResHandle, velocityScale;
	};

};

#endif	/* MOTIONBLUREFFECT_H */
