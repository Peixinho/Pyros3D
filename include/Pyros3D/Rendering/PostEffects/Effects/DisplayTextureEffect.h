//============================================================================
// Name        : DisplayTextureEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Display Texture Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef DISPLAYTEXTUREEFFECT_H
#define	DISPLAYTEXTUREEFFECT_H

namespace p3d {

	// Same trivial "sample uTex0, write FragColor" shader as ResizeEffect,
	// but wired to an arbitrary caller-owned Texture (IEffect::
	// UseCustomTexture()/RTT::CustomTexture) instead of one of
	// PostEffectsManager's own Color/Depth/LastRTT captures - the real
	// mechanism for "show a texture I already have on screen" via the
	// engine's existing full-screen-quad post-effect machinery, not
	// previously used by any shipped effect.
	class PYROS3D_API DisplayTextureEffect : public IEffect {
	public:
		DisplayTextureEffect(Texture* texture, const uint32 Width, const uint32 Height);
		virtual ~DisplayTextureEffect();
	};

};

#endif	/* DISPLAYTEXTUREEFFECT_H */
