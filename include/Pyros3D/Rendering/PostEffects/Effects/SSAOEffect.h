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
			Matrix inversedM = m.Inverse();
			uViewMatrixUniform->SetValue(&m);
			uInverseViewMatrixUniform->SetValue(&m);
		}
		void SetRadius(const f32 radius)
		{
			this->radius = radius;
			uRadiusHandle->SetValue(&this->radius);
		}
		void SetStrength(const f32 strength)
		{
			total_strength = strength;
			uStrengthHandle->SetValue(&total_strength);
		}
		void SetTreshOld(const f32 treshold)
		{
			treshOld = treshold;
			uTreshOldHandle->SetValue(&treshOld);
		}
		void SetScale(const f32 scale)
		{
			this->scale = scale;
			uScaleHandle->SetValue(&this->scale);
		}

		f32 total_strength;
		f32 radius;
		uint32 samples;
		f32 scale;
		f32 treshOld;

	private:

		Texture* rnm;
		Uniform *uInverseViewMatrixUniform, *uViewMatrixUniform, *uScaleHandle, *uRadiusHandle, *uStrengthHandle, *uTreshOldHandle;

	};

};

#endif	/* SSAOEFFECT_H */
