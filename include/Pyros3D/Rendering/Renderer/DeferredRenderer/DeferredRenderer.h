//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#ifndef DEFERREDRENDERER_H
#define DEFERREDRENDERER_H

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <memory>

namespace p3d {

	class PYROS3D_API DeferredRenderer : public IRenderer {

	public:

		DeferredRenderer(const uint32 Width, const uint32 Height, FrameBuffer* fbo);

		~DeferredRenderer();

		// Signature must match IRenderer::RenderScene() exactly (3 args) -
		// a 4th BufferOptions param used to sit here, unused in the body,
		// which meant this never actually overrode the base virtual (name
		// hiding introduced a sibling overload instead). Every call made
		// through an IRenderer* - which is exactly how SceneEditor's
		// viewport holds `Renderer` when usingDeferredRenderer is true -
		// resolved to IRenderer::RenderScene()'s own body instead of this
		// one, so the whole G-buffer/lighting/SSR pipeline below never ran
		// and the viewport showed nothing but debug/gizmo overlays. Direct
		// DeferredRenderer* callers were unaffected (name hiding still let
		// them reach this one via the 4th arg's default), which is why the
		// bug stayed latent until the editor's IRenderer*-polymorphic path
		// exercised it.
		virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene);

		// Kept only for the Lua/Embind "renderSceneOptions" bindings' sake -
		// BufferOptions was already unused by RenderScene()'s body before
		// the signature fix above, so this stays a thin, non-virtual,
		// behaviorally-identical forwarder rather than restoring the
		// param to the virtual itself (which is what silently broke
		// IRenderer*-polymorphic callers in the first place).
		void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions) { RenderScene(projection, Camera, Scene); }

		void SetFBO(FrameBuffer* fbo);

		// By-reference params, matching IRenderer::Resize() exactly - same
		// class of bug as RenderScene() above (see its comment). By-value
		// params here meant this never overrode the base virtual either,
		// so SceneEditor's per-frame `Renderer->Resize(viewW, viewH)` (an
		// IRenderer*-polymorphic call) silently ran IRenderer::Resize()'s
		// own body instead - which updates Width/Height/viewport bookkeeping
		// but never touches lastPassFBO, so colorTexture stayed frozen at
		// whatever size the SceneEditor doc's Init() first constructed
		// DeferredRenderer with, regardless of how the viewport panel was
		// later resized/docked. The mismatch between that stale texture
		// and the ImGui::Image() rect it's stretched into is what looked
		// like "the texture is half the size it should be".
		virtual void Resize(const uint32 &Width, const uint32 &Height);

		// Real, per-scene retuning knob for material-aware SSR's ray
		// march - see lastPass.glsl's uSSRStepDistance/uSSRMaxDistance
		// comment. The two arguments are NOT in the same units:
		//
		// stepDistance is the coarse DDA's stride in SCREEN PIXELS,
		// clamped up to 1.0 (the march is a screen-space DDA, so this is
		// what actually decides reach: a reflection cannot travel further
		// than SSR_COARSE_STEPS * stride pixels). It was silently ignored
		// by the shader before the island SSR demo, so every value below
		// 1.0 that predates that - SSRTest's 0.35, the constructor's 0.22
		// - keeps marching at stride 1 exactly as it always did. Raise it
		// for scenes whose reflections span a lot of screen (open water,
		// long corridors): stride N buys N times the reach for
		// SSR_REFINE_STEPS extra taps, at the cost of coarser first-hit
		// quantization.
		//
		// maxDistance is view-space, and only clips the ray's far end -
		// raising it alone cannot make reflections reach further. Scale
		// it with the scene: room-scale wants ~12, an open landscape
		// wants hundreds.
		void SetSSRDistances(const f32 stepDistance, const f32 maxDistance);

		// Real opt-in gate for material-aware SSR - defaults OFF (see the
		// constructor's comment on why). Call this on an instance whose
		// scene is actually meant to showcase reflections.
		void EnableSSR();
		void DisableSSR();

		// 0 = normal composite, 1 = show G-buffer ssrReflective (mr.b),
		// 2 = paint ray-march hits red, 3 = full hit color (ignore Fresnel).
		// For isolating whether SSR is gated off, missing hits, or just dim.
		void SetSSRDebugMode(const uint32 mode);
		uint32 GetSSRDebugMode() const { return (uint32)ssrDebugMode; }

		// The final composited frame (lastPassFBO's Color_Attachment0).
		// RenderScene()'s own final "Render to Screen" draw always targets
		// framebuffer 0 (see its own comment on why a caller-FBO restore
		// there was previously tried and found harmful - LOAD_OP_CLEAR wiped
		// an in-progress frame), so a caller embedding this renderer's
		// output inside its own framebuffer (e.g. an editor viewport texture,
		// as opposed to DeferredRenderer being the whole application's only
		// output) cannot rely on PostEffectsManager-style capture-around-
		// RenderScene() the way ForwardRenderer supports - it must read this
		// texture directly instead.
		Texture* GetColorTexture() const { return colorTexture; }

		// A real copy of the opaque scene's depth (see forwardDepthTexture's
		// own comment), refreshed once per frame in RenderScene() right
		// after the G-buffer pass ends. Lets a caller depth-test its own
		// overlay content (e.g. an editor viewport's gizmo/grid draws)
		// against the actual scene instead of drawing blind - see
		// GetColorTexture()'s comment for why a caller can't just reuse
		// whatever depth its own capture-wrap left behind for Deferred.
		Texture* GetDepthTexture() const { return forwardDepthTexture; }

	private:
		GenericShaderMaterial* shadowMaterial, *shadowSkinnedMaterial;

		// Looks up one of FBO's G-buffer attachments by its
		// FrameBufferAttachmentFormat (e.g. Depth_Attachment,
		// Color_Attachment2 for normal) - used by deferredLastPass's SSR
		// pass, which needs specific named attachments rather than the
		// whole-attachment-list loop the lighting passes use.
		Texture* GetGBufferAttachment(const uint32 attachmentFormat) const;

	protected:

		// Offscreen Frame Buffer Object
		FrameBuffer* FBO, *lastPassFBO;
		Texture* colorTexture;
		// A real *copy* of FBO's depth attachment, not an alias of it -
		// see IRenderDevice::CopyDepthTexture()'s comment for why:
		// lastPassFBO's lighting materials need to sample FBO's depth as
		// a plain texture (tDepth) while lastPassFBO's own later forward
		// sub-pass (transparent objects, drawn after the lighting
		// composite) needs a real depth *attachment* with matching values
		// - the same texture can't be both at once on Vulkan. Refreshed
		// once per frame in RenderScene(), right after FBO's G-buffer
		// pass ends and before lastPassFBO's lighting pass starts
		// sampling it.
		Texture* forwardDepthTexture;

		// Material-aware SSR's reflection source - a snapshot of last
		// frame's fully composited color (post-lighting, post-transparent-
		// forward-pass), refreshed once per frame right after colorTexture
		// is finalized. Using the *previous* frame (reprojected via
		// IRenderer::PrvProjectionMatrix/PrvViewMatrix) rather than this
		// frame's own colorTexture sidesteps two problems at once: it
		// naturally includes transparent objects (already baked into last
		// frame's snapshot, even though they never touch the G-buffer this
		// frame), and it avoids a same-frame read-while-still-writing
		// hazard against colorTexture that a current-frame approach would
		// need its own separate copy for anyway. Cleared to transparent
		// black at creation so frame 1 has no reflections rather than
		// needing an explicit "first frame" flag.
		Texture* previousFrameColorTexture;
		FrameBuffer* previousFrameFBO;

		// Fallback shadow-comparison textures, bound to uShadowMap
		// whenever a light being drawn this frame *doesn't* cast a real
		// shadow. GL tolerates a declared-but-never-bound sampler just
		// fine (reads black, harmless); Vulkan requires every descriptor
		// a bound pipeline statically references to be valid the moment
		// it draws, even behind a runtime `uHaveShadowmap > 0.0` branch
		// the shader never actually takes - found via
		// VUID-vkCmdDrawIndexed-None-08114 on a real DeferredRendering
		// validation-layer run (100 non-shadow-casting point lights, so
		// uShadowMap's descriptor was never written at all). One 2D
		// (sampler2DShadow, shared by Directional/Spot) and one cube
		// (samplerCubeShadow, Point) - the descriptor's declared image
		// *type* has to match what's bound, a plain 2D fallback can't
		// stand in for samplerCubeShadow. Real shadow maps' contents
		// don't matter here since the shader never samples them behind
		// `uHaveShadowmap <= 0.0` - these are as small as
		// CreateEmptyTexture() allows.
		Texture* dummyShadow2D;
		Texture* dummyShadowCube;
		// True once dummyShadow2D/dummyShadowCube have been through a
		// real (contentless) render-pass begin/end - a freshly created
		// texture's backing image starts life in VK_IMAGE_LAYOUT_UNDEFINED
		// on Vulkan and only leaves it via an actual render pass or
		// upload, neither of which CreateEmptyTexture() alone triggers.
		// Deferred to RenderScene()'s first call (not the constructor)
		// so the Bind()/UnBind() pair below runs inside a real frame,
		// matching every other FBO bind in this class - found via
		// VUID-vkCmdDraw-None-09600 on a real DeferredRendering
		// validation-layer run (both dummies still VK_IMAGE_LAYOUT_UNDEFINED
		// the first time a light needed one).
		bool dummyShadowsWarmedUp;

		// Deferred Materials
		CustomShaderMaterial *deferredLastPass;
		CustomShaderMaterial *deferredMaterialAmbient;
		CustomShaderMaterial *deferredMaterialDirectional;
		CustomShaderMaterial *deferredMaterialPoint;
		CustomShaderMaterial *deferredMaterialSpot;
		RenderingComponent *directionalLight;
		RenderingComponent *pointLight;

		std::shared_ptr<Renderable> sphereHandle;
		std::shared_ptr<Renderable> quadHandle;

		// Uniform Handlers
		Uniform *pointPosHandle, *pointRadiusHandle, *pointColorHandle, *pointShadowHandle, *pointShadowDepthsMVPHandle, *pointShadowPCFTexelHandle, *pointHaveShadowHandle;
		Uniform *dirDirHandle, *dirColorHandle, *dirShadowHandle, *dirShadowPCFTexelHandle, *dirShadowDepthsMVPHandle, *dirShadowFarHandle, *dirHaveShadowHandle;
		Uniform *spotPosHandle, *spotDirHandle, *spotRadiusHandle, *spotOutterHandle, *spotInnerHandle, *spotColorHandle, *spotShadowHandle, *spotShadowDepthsMVPHandle, *spotShadowPCFTexelHandle, *spotHaveShadowHandle;

		// See RenderScene()'s comment where these are set - real fix for
		// point/spot light volumes vanishing when the camera is near/
		// inside their radius (near-plane clipping the sphere proxy away
		// entirely before rasterization).
		Uniform *pointUseFullscreenQuadHandle, *spotUseFullscreenQuadHandle;

		// Per-light volumetric params (density, anisotropy, steps) packed
		// into one vec4 - see ILightComponent::SetVolumetricScattering().
		Uniform *pointVolumetricHandle, *spotVolumetricHandle;

		// Real mip count of previousFrameColorTexture, recomputed on
		// resize - see lastPass.glsl's uMaxReflectionLod/textureLod()
		// comment (roughness-based SSR reflection blur).
		Uniform *lastPassMaxReflectionLodHandle;

		// See SetSSRDistances()'s comment - real, per-scene-settable SSR
		// march distances, mirrored here so SetSSRDistances() can be
		// called any time (not just at construction) and still know what
		// to write back into deferredLastPass's uniforms.
		f32 ssrStepDistance, ssrMaxDistance;
		Uniform *lastPassSSRStepDistanceHandle, *lastPassSSRMaxDistanceHandle;

		// See EnableSSR()/DisableSSR()'s comment - real opt-in gate,
		// defaults OFF.
		f32 ssrEnabled;
		Uniform *lastPassSSREnabledHandle;

		// See SetSSRDebugMode() - isolation modes for SSRTest / demos.
		f32 ssrDebugMode;
		Uniform *lastPassSSRDebugHandle;

		// Real, dedicated previous-frame camera state for SSR reprojection -
		// deliberately NOT the shared IRenderer::PrvViewMatrix/
		// PrvProjectionMatrix (fed generically via Uniforms::DataUsage and
		// also consumed by VelocityRenderer). Those get overwritten inside
		// PreRender() (once unconditionally for the main camera's
		// ViewMatrix, and repeatedly for ProjectionMatrix whenever a light
		// in the scene casts shadows - shadow passes temporarily repurpose
		// the same shared scratch members) before RenderScene() ever runs,
		// so by the time RenderScene() shifted them into Prv*, it was
		// shifting THIS frame's already-current values (or, worse, a
		// leftover shadow-camera projection), not a real one-frame-old
		// value - reprojection collapsed to sampling tPreviousFrameColor at
		// this frame's own hit UV with zero motion compensation, visible as
		// reflection smear/ghosting under camera rotation (worst at
		// grazing-angle floor reflections, where pitch changes produce the
		// largest true reprojection offset - see SSRTest). Updated once at
		// ssrPrvViewMatrix/ssrPrvProjectionMatrix - kept for the previous-
		// frame color blit path and any future temporal reuse. lastPass
		// currently samples same-frame tColor at the ray hit UV (see
		// lastPass.glsl) so pitch smear from broken prv reprojection is gone.
		Matrix ssrPrvViewMatrix, ssrPrvProjectionMatrix;
		Uniform *lastPassPrvViewMatrixHandle, *lastPassPrvProjectionMatrixHandle;
	};

};

#endif /* DEFERREDRENDERER_H */
