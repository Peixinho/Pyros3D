//============================================================================
// Name        : BlurSSAOEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef BLURSSAOEFFECT_H
#define	BLURSSAOEFFECT_H

namespace p3d {

	class PYROS3D_API BlurSSAOEffect : public IEffect {
	public:
		BlurSSAOEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~BlurSSAOEffect();
	private:
		Uniform texRes;
	};

};

#endif	/* BLURSSAOEFFECT_H */