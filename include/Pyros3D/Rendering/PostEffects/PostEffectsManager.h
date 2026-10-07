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
#include <Pyros3D/Rendering/PostEffects/AntiAliasing.h>
#include <Pyros3D/Rendering/PostEffects/Upscaling.h>

#ifndef POSTEFFECTSMANAGER_H
#define	POSTEFFECTSMANAGER_H

namespace p3d {

	// Only ever held by pointer here - see EnsureVelocityMap(). Forward
	// declared so every translation unit that draws a post effect does not
	// also pull in a renderer it will never touch.
	class VelocityRenderer;
	class AntiAliasingStage;

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
		// The size of what the last effect draws into when it goes to the
		// swapchain: the window's, where the chain itself works at a smaller
		// size (a game rendering below the window's resolution). The last
		// pass then scales the frame up as it draws. 0 x 0 - the default -
		// means the chain's own size.
		void SetOutputSize(const uint32 width, const uint32 height) { outputWidth = width; outputHeight = height; }
		// A chain that ends somewhere bigger than it works at (SetOutputSize)
		// gets there through AMD's FSR 1 (FsrEffect: EASU, then RCAS) - or,
		// where that cannot be compiled, a sharpening bicubic
		// (SharpUpscaleEffect) - not a plain stretch. On unless said
		// otherwise; sharpness 0..1.
		// IsUsingFsr(): which of the two it turned out to be.
		void SetSharpUpscale(const bool on, const f32 sharpness = 0.85f);
		bool GetSharpUpscale() const { return sharpUpscale; }
		// Whether the chain ends in an upscaling pass as things stand.
		bool WillUpscale() const { return sharpUpscale && outputWidth != 0 && outputHeight != 0 && (outputWidth != Width || outputHeight != Height); }
		bool IsUsingFsr() const { return fsrState == 1 && preferFsr; }
		// Which of the two brings the frame up to size: AMD's FSR 1 (two passes
		// at the size shown - some milliseconds of an integrated GPU at 4K), or
		// the built-in sharpening resample (one pass, next to nothing).
		void SetUpscalerFsr(const bool fsr) { preferFsr = fsr; }
		// The same, by name of mode (Upscaling.h). What is asked for is kept;
		// what runs is Upscaling::Resolve() of it. Sharpness is SetSharpUpscale's.
		void SetUpscaler(const UpscalerMode mode);
		UpscalerMode GetUpscaler() const { return upscalerRequested; }
		UpscalerMode GetEffectiveUpscaler() const;
		bool GetUpscalerFsr() const { return preferFsr; }
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

		// Sets a parameter of an effect authored as an asset (CustomEffect), by
		// the effect's own name, in every chain that has one - a game changes
		// its fog with the time of day, its tint when the player is hurt. As
		// with the ambient scale there is one answer for the whole process: a
		// script does not know which view's chain it is talking to. Returns how
		// many effects took it.
		static uint32 SetEffectParam(const std::string &effectName, const std::string &paramName, const f32 *values, const uint32 count);

		const uint32 GetNumberEffects() const;

		// Anti-aliasing, separate from the chain - see AntiAliasingStage.h
		// for where each mode runs. Applied at the next CaptureFrame(), so it
		// is safe to call mid-frame (a script toggling it, a settings menu).
		// deferred is which renderer draws the frame: MSAA is not possible
		// under it and falls back (see AntiAliasing::Resolve).
		void SetAntiAliasing(const AntiAliasingMode mode, const bool deferred);
		AntiAliasingMode GetAntiAliasing() const;
		// What actually runs, after the renderer/device fallbacks.
		AntiAliasingMode GetEffectiveAntiAliasing() const;
		// The offset the renderer has to apply to this frame's projection -
		// IRenderer::SetProjectionJitter(). Zero unless TAA is on. Set it
		// for the scene pass only and back to zero after, or every other
		// render that renderer does shakes with it.
		Vec2 GetProjectionJitter();
		// Next TAA frame starts from the current image alone. Call on a scene
		// load or a camera cut - anything where last frame is not this one.
		void ResetTemporalHistory();

		// Whether ProcessPostEffects() produces an image other than the
		// capture: a chain, or an anti-aliasing pass. MSAA alone does not -
		// its result is the capture itself, resolved.
		bool HasPasses();
		// Whether the frame has to go through CaptureFrame()/
		// ProcessPostEffects() at all: HasPasses(), or MSAA, whose samples
		// only exist in the capture target.
		bool NeedsCapture();

		// Keep depth across binds of the capture, on whichever target is
		// current - the multisample one included. Replaces calling
		// SetFramebufferPreserveDepth() on GetExternalFrameBuffer() directly.
		void SetCapturePreserveDepth(const bool preserve);

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
		Texture* EnsureVelocityMap();
		// The velocity pass's depth. 1 where nothing was drawn, which is how
		// motion blur tells the sky from a mesh. NULL until EnsureVelocityMap().
		Texture* GetVelocityDepth();
		bool HaveVelocityMap() const { return velocityRenderer != NULL; }
		// currentFps scales how far the blur smears: the effect works in
		// "how much of a target frame did this movement take", so a slow
		// frame blurs further. Forwarded to every MotionBlurEffect in the
		// chain, so the caller does not have to hold on to one.
		void RenderVelocityPass(const Projection &projection, GameObject* camera,
			SceneGraph* scene, const f32 currentFps);

	private:

		void CreateQuad();
		void EnsureViewportGammaEffect();
		// Builds whatever a SetAntiAliasing() since the last frame asked for.
		void ApplyAntiAliasing();
		bool ChainReadsDepth() const;
		bool ChainUsesVelocity() const;
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
		uint32 outputWidth = 0, outputHeight = 0;
		bool sharpUpscale = true;
		f32 upscaleSharpness = 0.85f;
		IEffect* upscalePass = NULL;
		// FSR 1's two passes (FsrEffect), where they can be made; upscalePass
		// is what is used where they cannot. fsrState: 0 not tried, 1 made, 2 not to be had.
		IEffect* fsrEasu = NULL;
		IEffect* fsrRcas = NULL;
		int fsrState = 0;
		bool preferFsr = true;
		UpscalerMode upscalerRequested = UpscalerMode::FSR1;
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

		// See EnsureVelocityMap(). NULL unless a chain or TAA asked for one.
		VelocityRenderer* velocityRenderer = NULL;

		AntiAliasingStage* aaStage = NULL;
		// Which target CaptureFrame() bound, so EndCapture() unbinds the same
		// one even if the mode changed in between.
		FrameBuffer* boundCapture = NULL;
	};

};
#endif	/* POSTEFFECTSMANAGER_H */