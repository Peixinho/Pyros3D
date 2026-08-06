//============================================================================
// Name        : SSAOCompositeEffect.h
// Description : Multiplies scene color by SSAO occlusion (last RTT).
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef SSAOCOMPOSITEEFFECT_H
#define	SSAOCOMPOSITEEFFECT_H

namespace p3d {

	class PYROS3D_API SSAOCompositeEffect : public IEffect {
	public:
		SSAOCompositeEffect(const uint32 TexColor, const uint32 TexSSAO, const uint32 Width, const uint32 Height);
		virtual ~SSAOCompositeEffect();
	};

};

#endif
