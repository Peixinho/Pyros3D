//============================================================================
// Name        : SharpUpscaleEffect.h
// Description : A frame rendered smaller than where it is shown, brought up
//               to size: a bicubic resample in place of a plain stretch, and
//               the contrast-adaptive sharpening of FidelityFX CAS / FSR 1's
//               RCAS over it. What a renderer drawing at 60% of the window
//               needs for the result not to read as 60%.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef SHARPUPSCALEEFFECT_H
#define	SHARPUPSCALEEFFECT_H

namespace p3d {

	class PYROS3D_API SharpUpscaleEffect : public IEffect {
	public:
		// sharpness: 0 (a little) to 1 (as much as stays free of halos)
		SharpUpscaleEffect(const uint32 Tex1, const uint32 Width, const uint32 Height, const f32 sharpness = 0.6f);
		virtual ~SharpUpscaleEffect();
	};

};

#endif	/* SHARPUPSCALEEFFECT_H */
