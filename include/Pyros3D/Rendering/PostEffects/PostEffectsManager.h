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

	private:

		void CreateQuad();

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
	};

};
#endif	/* POSTEFFECTSMANAGER_H */