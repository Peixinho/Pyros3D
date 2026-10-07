//============================================================================
// Name        : AntiAliasingEffects.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : The full-screen passes behind AntiAliasingStage.
//============================================================================

#ifndef ANTIALIASINGEFFECTS_H
#define ANTIALIASINGEFFECTS_H

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

namespace p3d {

	// Every pass here reads its input as RTT::Color, which AntiAliasingStage
	// points at whatever came before it with SetColorOverride() - the stage
	// runs at two different points of the frame, so "the image" is not a
	// fixed texture.

	// FXAA 3.11, quality preset 12.
	class PYROS3D_API FXAAEffect : public IEffect {
	public:
		FXAAEffect(const uint32 Width, const uint32 Height);
		virtual ~FXAAEffect() {}
	};

	// SMAA 1x, the reference implementation's three passes at its HIGH preset.
	class PYROS3D_API SMAAEdgeEffect : public IEffect {
	public:
		SMAAEdgeEffect(const uint32 Width, const uint32 Height);
		virtual ~SMAAEdgeEffect() {}
	};

	class PYROS3D_API SMAAWeightEffect : public IEffect {
	public:
		SMAAWeightEffect(Texture* edges, Texture* areaTex, Texture* searchTex, const uint32 Width, const uint32 Height);
		virtual ~SMAAWeightEffect() {}
	};

	class PYROS3D_API SMAABlendEffect : public IEffect {
	public:
		SMAABlendEffect(Texture* weights, const uint32 Width, const uint32 Height);
		virtual ~SMAABlendEffect() {}
	};

	// One half of TAA's history ping-pong: blends the jittered current frame
	// with `history` (the other half's output) reprojected along the velocity
	// map. Renders into RGBA16F, since it runs before the scene's chain.
	class PYROS3D_API TAAResolveEffect : public IEffect {
	public:
		TAAResolveEffect(Texture* velocity, Texture* velocityDepth, const uint32 Width, const uint32 Height);
		virtual ~TAAResolveEffect() {}

		// The other half's texture. Separate from the constructor because
		// the two halves each need the other to exist first.
		void SetHistory(Texture* history);
		// The scene's depth (after the history: it is the shader's uTex4).
		void SetSceneDepth(Texture* depth);
		// Write the motion it works out, and nothing else (see the shader's uParams.y).
		void SetMotionOnly(const bool only) { motionOnly = only; }

		// reproject maps this frame's clip space to last frame's, for pixels
		// the velocity pass drew nothing into. historyValid false outputs the
		// current frame untouched - first frame, resize, cut.
		void SetFrameParams(const Matrix &reproject, const bool historyValid);

	private:
		Uniform *reprojectUniform, *paramsUniform;
		bool motionOnly = false;
	};

	// Vulkan only: vkCmdResolveImage cannot resolve depth, so the
	// multisample capture's depth is resolved by a shader instead, sample 0
	// written through gl_FragDepth into `target`. GL and Metal blit it.
	class PYROS3D_API MSDepthResolveEffect : public IEffect {
	public:
		MSDepthResolveEffect(Texture* msDepth, Texture* target, const uint32 Width, const uint32 Height);
		virtual ~MSDepthResolveEffect() {}
		FrameBuffer* GetFrameBuffer() { return fbo; }
	};

}

#endif /* ANTIALIASINGEFFECTS_H */
