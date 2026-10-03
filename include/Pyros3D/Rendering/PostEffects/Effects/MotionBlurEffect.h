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
		MotionBlurEffect(const uint32 Tex1, Texture* VelocityMap, Texture* VelocityDepth, const uint32 Width, const uint32 Height);
		virtual ~MotionBlurEffect();
		void SetCurrentFPS(const f32 &currentfps);
		void SetTargetFPS(const f32 &targetfps);
		// prevVP * inverse(currentVP), from VelocityRenderer. Identity
		// until two frames exist. Sky pixels have nothing written in the
		// velocity map, so they reproject with this instead.
		void SetCameraReproject(const Matrix &reproject);

	private:
		void UploadScale();
		Uniform *velHandle;
		Uniform *reprojectHandle;
		f32 cfps, tfps, strength;
		Matrix reproject;
	};

};

#endif	/* MOTIONBLUREFFECT_H */
