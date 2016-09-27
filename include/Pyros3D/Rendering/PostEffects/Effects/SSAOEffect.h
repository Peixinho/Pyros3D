//============================================================================
// Name        : SSaoEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef SSAOEFFECT_H
#define SSAOEFFECT_H

namespace p3d {

	using namespace Uniforms;

	class PYROS3D_API SSAOEffect : public IEffect {
	public:
		SSAOEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~SSAOEffect();

		void SetViewMatrix(Matrix m)
		{
			SetUniformValue("uInverseView", &m);
		}

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