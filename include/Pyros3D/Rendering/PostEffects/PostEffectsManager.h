//============================================================================
// Name        : PostEffectsManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>

#ifndef POSTEFFECTSMANAGER_H
#define	POSTEFFECTSMANAGER_H

namespace p3d {

	using namespace Uniforms;

	class PYROS3D_API PostEffectsManager {
		friend class IEffect;

	public:

		PostEffectsManager(const uint32 width, const uint32 height);
		virtual ~PostEffectsManager();

		void Resize(const uint32 width, const uint32 height);

		void CaptureFrame();
		void EndCapture();

		// Process Post Effects
		void ProcessPostEffects(Projection* projection);

		void AddEffect(IEffect* Effect);
		void RemoveEffect(IEffect* Effect);

		// Bulk-clear: deletes every currently-added IEffect and empties
		// the chain, without destroying the manager itself (unlike
		// ~PostEffectsManager(), the only place that previously did
		// this). For callers that rebuild the effect chain repeatedly
		// against one long-lived PostEffectsManager instance.
		void RemoveAllEffects();

		const uint32 GetNumberEffects() const;

		FrameBuffer* GetExternalFrameBuffer();

		Texture* GetColor() { return Color; }
		Texture* GetDepth() { return Depth; }
		Texture* GetLastRTT() { return LastRTT; }

		// For ImGui (or any UNORM-sRGB-interpreted present path): returns a
		// gamma-encoded LDR copy of Color on backends where
		// NeedsManualDisplayGamma() is true (pow 1/2.2). On OpenGL returns
		// Color as-is. Call after EndCapture() / after the frame's debug
		// overlays have been drawn into Color.
		Texture* GetViewportColor();

	private:

		void CreateQuad();
		void EnsureViewportGammaEffect();
		void BlitViewportGamma();

		// Set Quad Geometry
		std::vector<Vec3> vertex;
		std::vector<Vec2> texcoord;

		uint32 Width, Height;

		// List of Effects
		std::vector<IEffect*> effects;

		// MRT
		Texture *Color, *Depth, *LastRTT;

		// Frame Buffers
		FrameBuffer *ExternalFBO, *activeFBO;

		// See IRenderDevice.h - same seam IRenderer uses, since
		// PostEffectsManager's full-screen-quad pass has its own small GL
		// call surface. MaybeOwningDevicePtr (not a plain
		// unique_ptr<IRenderDevice>) so this can borrow the already-active
		// device instead of always constructing its own GLRenderDevice -
		// see IRenderDevice.h's comment on MaybeOwningDeviceDeleter/
		// IsActiveRenderDeviceSet() (a hardcoded `new GLRenderDevice()`
		// here crashed every PostEffectsManager-based Vulkan example the
		// instant it made a real GL call, since no real GL context exists
		// in a Vulkan-only process).
		MaybeOwningDevicePtr device;

		// Full-screen triangle path uses noVertexInput pipelines +
		// DrawArrays; the VAO is only needed so BindVertexArray has a
		// non-zero handle. Creating a fresh one every effect every frame
		// leaked entries in VulkanRenderDevice::vaos without bound.
		DeviceHandle fullscreenVao;

		// Lazy GammaEncodeEffect (pow 1/2.2) used only by GetViewportColor()
		// - not part of the public effects chain.
		IEffect* viewportGammaEffect;
	};

};
#endif	/* POSTEFFECTSMANAGER_H */