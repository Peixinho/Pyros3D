//============================================================================
// Name        : BlurEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef BLURYEFFECT_H
#define	BLURYEFFECT_H

namespace p3d {

	class PYROS3D_API BlurYEffect : public IEffect {
	public:
		BlurYEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~BlurYEffect();
	private:
		Uniform texRes;
	};

};

#endif	/* BLUREFFECT_H */