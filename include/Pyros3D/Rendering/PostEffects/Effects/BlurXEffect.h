//============================================================================
// Name        : BlurXEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef BLURXEFFECT_H
#define	BLURXEFFECT_H

namespace p3d {

	class PYROS3D_API BlurXEffect : public IEffect {
	public:
		BlurXEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~BlurXEffect();
	private:
		Uniform texRes;
	};

};

#endif	/* BLUREFFECT_H */