//============================================================================
// Name        : DepthOfFieldEffect.h
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef DEPTHOFFIELDEFFECT_H
#define	DEPTHOFFIELDEFFECT_H

namespace p3d {

	class PYROS3D_API DepthOfFieldEffect : public IEffect {
	public:
		DepthOfFieldEffect(Texture* lowResBlur, Texture* fullResBlur, const uint32 Width, const uint32 Height);
		virtual ~DepthOfFieldEffect();

		// The four knobs the shader actually reads. They were locals in the
		// constructor, so the defaults were the only values this effect could
		// ever have - fine while the one caller was a Lua helper that wanted
		// them, useless for a chain entry a scene is meant to tune.
		// Distances are in world units, along the view direction.
		void SetFocalPosition(const f32 &v);
		void SetFocalRange(const f32 &v);
		// How hard the low-res (far) and full-res (near) blurs are mixed in.
		void SetRatioLow(const f32 &v);
		void SetRatioHigh(const f32 &v);

	private:
		Uniform *focalPositionHandle, *focalRangeHandle, *ratioLowHandle, *ratioHighHandle;
		f32 fPosition, fRange, rL, rH;
	};

};

#endif
