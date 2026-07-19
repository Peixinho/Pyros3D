//============================================================================
// Name        : ScreenSpaceReflectionEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen Space Reflection Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Core/Math/Matrix.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <cstdlib>
#include <time.h>
#include <vector>
#include <cstdio>

#ifndef SCREENSPACEREFLECTIONEFFECT_H
#define SCREENSPACEREFLECTIONEFFECT_H

namespace p3d {

	using namespace Uniforms;

	class PYROS3D_API ScreenSpaceReflectionEffect : public IEffect {
	public:
		ScreenSpaceReflectionEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~ScreenSpaceReflectionEffect();

		void SetViewMatrix(Matrix m)
		{
			// TODO: Add view matrix uniform when implementing SSR
		}
		void SetProjectionMatrix(Matrix m)
		{
			// TODO: Add projection matrix uniform when implementing SSR
		}
		void SetMaxSteps(const uint32 maxSteps)
		{
			this->maxSteps = maxSteps;
			UpdateUniforms();
		}
		void SetMaxDistance(const f32 maxDistance)
		{
			this->maxDistance = maxDistance;
			UpdateUniforms();
		}
		void SetThickness(const f32 thickness)
		{
			this->thickness = thickness;
			UpdateUniforms();
		}
		void SetReflectionStrength(const f32 strength)
		{
			this->reflectionStrength = strength;
			UpdateUniforms();
		}
		
		void UpdateUniforms()
		{
			if (uMaxStepsUniform) uMaxStepsUniform->SetValue(&maxSteps);
			if (uMaxDistanceUniform) uMaxDistanceUniform->SetValue(&maxDistance);
			if (uReflectionStrengthUniform) uReflectionStrengthUniform->SetValue(&reflectionStrength);
		}
		
		f32 maxDistance;
		f32 thickness;
		f32 reflectionStrength;
		uint32 maxSteps;
		
		// Getter for inverse view matrix uniform
		Uniform* GetInverseViewMatrixUniform() const { return uInverseViewMatrixUniform; }

	private:
		Uniform* uInverseViewMatrixUniform;
		Uniform* uMaxStepsUniform;
		Uniform* uMaxDistanceUniform;
		Uniform* uReflectionStrengthUniform;

	};

};

#endif	/* SCREENSPACEREFLECTIONEFFECT_H */ 