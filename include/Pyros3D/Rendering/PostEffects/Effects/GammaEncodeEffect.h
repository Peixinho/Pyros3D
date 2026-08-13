//============================================================================
// Name        : GammaEncodeEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Linear HDR -> display-referred LDR (pow 1/2.2)
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef GAMMAENCODEEFFECT_H
#define	GAMMAENCODEEFFECT_H

namespace p3d {

	// pow(rgb, 1/2.2) blit - same encode TonemapEffect applies after ACES,
	// without the filmic curve. Used by PostEffectsManager::GetViewportColor()
	// so the editor can show linear RGBA16F captures through ImGui on
	// Vulkan/Metal UNORM swapchains without changing the swapchain format
	// (which would wash out ImGui UI colours).
	class PYROS3D_API GammaEncodeEffect : public IEffect {
	public:
		GammaEncodeEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~GammaEncodeEffect();
	};

};

#endif	/* GAMMAENCODEEFFECT_H */
