//============================================================================
// Name        : TonemapEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Tonemap Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef TONEMAPEFFECT_H
#define	TONEMAPEFFECT_H

namespace p3d {

	// ACES filmic tonemap (Narkowicz 2015 fit) + gamma 2.2 encode, fused
	// into one full-screen pass - meant to be the LAST effect in a
	// PostEffectsManager chain, reading from an HDR (RGBA16F) capture
	// buffer and writing the LDR result the swapchain expects. Vulkan/
	// Metal swapchains stay UNORM (not SRGB) so ImGui UI colours - which
	// are already authored as sRGB - are not double-encoded on present;
	// gamma-encode must therefore happen here for scene content (and via
	// PostEffectsManager::GetViewportColor() when the editor shows the
	// HDR capture through ImGui::Image).
	// Exposure is a fixed 1.0, not a runtime uniform - see
	// TonemapEffect.cpp's constructor comment for why: a real, reproduced
	// Vulkan-only bug where a single-scalar fragment-stage UBO on a
	// binding no other IEffect had ever used before corrupted this same
	// draw's *sampler* read (unrelated descriptor set), independent of
	// whether the shader referenced the uniform or not. Narrowed to the
	// extraUniforms/BindUniformBlockIfPresent path specifically (real,
	// reproducible, not chased to a final root cause - flagged as a
	// follow-up, not silently worked around).
	class PYROS3D_API TonemapEffect : public IEffect {
	public:
		TonemapEffect(const uint32 Tex1, const uint32 Width, const uint32 Height);
		virtual ~TonemapEffect();

	};

};

#endif	/* TONEMAPEFFECT_H */
