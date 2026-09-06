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

	// Only ever held by pointer here - see EnsureVelocityMap(). Forward
	// declared so every translation unit that draws a post effect does not
	// also pull in a renderer it will never touch.
	class VelocityRenderer;

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

		// Where the LAST effect of the chain draws. Off by default: it goes to
		// the swapchain, which is what a standalone app wants and - on Vulkan -
		// is also what acquires and presents the frame at all (see the long
		// comment in ProcessPostEffects). Turn it on and the whole chain stays
		// offscreen, ending in a texture GetFinalTexture() hands back.
		//
		// That is what an editor viewport needs: it is an ImGui image, not a
		// swapchain, so with the default the chain ran and nothing on screen
		// changed. Only turn this on somewhere that presents a frame of its own -
		// otherwise nothing does, and Vulkan waits forever on a fence no present
		// ever drives.
		void SetRenderLastToTexture(const bool enabled) { renderLastToTexture = enabled; }
		bool GetRenderLastToTexture() const { return renderLastToTexture; }

		// What the chain produced: the last effect's texture when there is a
		// chain and it stayed offscreen, and the captured frame otherwise - so a
		// caller can always just show this without asking whether any effects
		// exist.
		Texture* GetFinalTexture();

		// What the chain treats as "the frame": RTT::Color resolves to it, and
		// so does the first effect's LastRTT. NULL - the default - means this
		// manager's own capture, which is what every caller that wraps its
		// RenderScene() in CaptureFrame()/EndCapture() wants.
		//
		// Deferred is the exception, and the reason this exists:
		// DeferredRenderer's final composite targets framebuffer 0 rather than
		// the capture (see its GetColorTexture() comment), so the capture holds
		// whatever the caller drew afterwards - in the editor, the gizmo/grid
		// overlay. Point this at the renderer's own colour output and the chain
		// processes the scene instead of the overlay drawn over it.
		void SetSceneSourceTexture(Texture* texture) { sceneSource = texture; }

		// The camera the frame was rendered from, for effects that work in
		// view space (PostEffects::ViewFromScene / InverseViewFromScene). Not
		// derivable here - the manager never sees the camera - so whoever
		// rendered the frame has to say. Until this is called those uniforms are
		// left alone, so a chain that pushes the matrix into an effect itself
		// (the Lua SSAO helper does) keeps working unchanged.
		void SetViewMatrix(const Matrix &view);

		void AddEffect(IEffect* Effect);
		void RemoveEffect(IEffect* Effect);

		// Bulk-clear: deletes every currently-added IEffect and empties
		// the chain, without destroying the manager itself (unlike
		// ~PostEffectsManager(), the only place that previously did
		// this). For callers that rebuild the effect chain repeatedly
		// against one long-lived PostEffectsManager instance.
		void RemoveAllEffects();

		const uint32 GetNumberEffects() const;

		// The effect a new one would be appended after, or NULL for an empty
		// chain. A multi-pass built-in needs it: its last pass has to
		// composite over the image as it entered the group, and that is this
		// effect's texture - not RTT::Color, which is always the captured
		// scene and would silently throw away everything earlier in the
		// chain. See PostEffectChain::AppendBuiltIn().
		IEffect* GetLastEffect() const { return effects.empty() ? NULL : effects.back(); }

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

		// Motion blur needs a velocity map, and a velocity map is a render
		// pass over the scene - something the chain has no way to run on its
		// own, the way SetViewMatrix() feeds the one matrix SSAO needs. The
		// manager owns the pass so a chain entry can point at its output, and
		// the caller drives it once a frame; if nobody does, the map holds
		// whatever it last had and the blur simply stops updating rather
		// than reading a dangling texture. Only allocated when a chain
		// actually asks for it - a velocity pass is a full extra draw of the
		// scene, not something every chain should pay for.
		PYROS3D_API Texture* EnsureVelocityMap();
		PYROS3D_API bool HaveVelocityMap() const { return velocityRenderer != NULL; }
		// currentFps scales how far the blur smears: the effect works in
		// "how much of a target frame did this movement take", so a slow
		// frame blurs further. Forwarded to every MotionBlurEffect in the
		// chain, so the caller does not have to hold on to one.
		PYROS3D_API void RenderVelocityPass(const Projection &projection, GameObject* camera,
			SceneGraph* scene, const f32 currentFps);

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
		// See SetRenderLastToTexture().
		bool renderLastToTexture = false;
		// See SetSceneSourceTexture().
		Texture* sceneSource = NULL;
		// See SetViewMatrix().
		Matrix viewMatrix, viewMatrixInverse;
		bool haveViewMatrix = false;
		// The last effect's own texture, valid only when renderLastToTexture.
		Texture *finalTexture = NULL;

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

		// See EnsureVelocityMap(). NULL unless a chain asked for one.
		VelocityRenderer* velocityRenderer = NULL;
	};

};
#endif	/* POSTEFFECTSMANAGER_H */