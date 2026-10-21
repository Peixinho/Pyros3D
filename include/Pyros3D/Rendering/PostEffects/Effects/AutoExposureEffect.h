//============================================================================
// Name        : AutoExposureEffect.h
// Description : The eye adjusting: exposure that follows how bright the view is.
//============================================================================
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef AUTOEXPOSUREEFFECT_H
#define	AUTOEXPOSUREEFFECT_H

namespace p3d {

	// The "AutoExposure" post effect is three passes, all on the GPU (nothing read back):
	//  1. ExposureMeterEffect: the frame's log luminance, averaged into a small fixed image
	//     (weighted to the middle of the view, where the eye looks).
	//  2. ExposureAdaptEffect: that averaged to one value and eased towards it from last frame's
	//     - quicker into the light than back into the dark, as an eye - into a 1x1 target it
	//     keeps two of, read one / write the other, so each frame starts from the last.
	//  3. ExposureApplyEffect: the frame scaled by key / that luminance, within limits - before
	//     the tonemap, so a tunnel is not black and the street outside it is not white.

	class PYROS3D_API ExposureMeterEffect : public IEffect {
	public:
		ExposureMeterEffect();
		virtual void Resize(const uint32 width, const uint32 height) {}   // (fixed size)
	};

	class PYROS3D_API ExposureAdaptEffect : public IEffect {
	public:
		ExposureAdaptEffect();
		virtual ~ExposureAdaptEffect();
		virtual void Resize(const uint32 width, const uint32 height) {}
		// Seconds-ish to adjust: brightening (into the light) and darkening.
		void SetSpeed(const f32 up, const f32 down) { speedUp = up; speedDown = down; }
	protected:
		virtual void PreBind();
		virtual void PreDraw();
	private:
		FrameBuffer* otherFbo;
		Texture* otherAttachment;
		f32 speedUp, speedDown;
		f64 lastTime;
	};

	class PYROS3D_API ExposureApplyEffect : public IEffect {
	public:
		ExposureApplyEffect(const uint32 TexColor, const uint32 Width, const uint32 Height);
		ExposureApplyEffect(Texture* color, const uint32 Width, const uint32 Height);
		// The luminance that comes out middle grey (0.18 is a photographer's; higher keeps it brighter).
		void SetKey(const f32 k) { key = k; }
		// EV added on top, and the most it may darken and brighten (in EV, from 1).
		void SetCompensation(const f32 ev) { compensation = ev; }
		void SetLimits(const f32 minEV, const f32 maxEV) { this->minEV = minEV; this->maxEV = maxEV; }
	protected:
		virtual void PreDraw();
	private:
		void Build();
		f32 key, compensation, minEV, maxEV;
	};
};

#endif /* AUTOEXPOSUREEFFECT_H */
