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
	};

};

#endif
