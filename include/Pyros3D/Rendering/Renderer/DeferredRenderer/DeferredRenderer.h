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

namespace p3d {

	class PYROS3D_API DeferredRenderer : public IRenderer {

	public:

		DeferredRenderer(const uint32 Width, const uint32 Height, FrameBuffer* fbo);

		~DeferredRenderer();

		virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions = Buffer_Bit::Color | Buffer_Bit::Depth);

		void SetFBO(FrameBuffer* fbo);

		virtual void Resize(const uint32 Width, const uint32 Height);

		// Real, per-scene retuning knob for material-aware SSR's ray
		// march - see lastPass.glsl's uSSRStepDistance/uSSRMaxDistance
		// comment. Both are view-space units; defaults (0.35/12.0, set
		// in the constructor) are proven-correct for a human/room-scale
		// scene. A tabletop diorama built at ~0.01-1 unit spacing wants
		// both much smaller; a city block built at ~100-1000 unit
		// spacing wants both much larger - call this once after
		// construction (or whenever the scene's own scale is known) if
		// SSR reflections are landing short (step too large, skipping
		// past nearby geometry) or absurdly far away/never hitting
		// anything (step too small to cover any real distance in
		// SSR_COARSE_STEPS steps).
		void SetSSRDistances(const f32 stepDistance, const f32 maxDistance);

		// Real opt-in gate for material-aware SSR - defaults OFF (see the
		// constructor's comment on why). Call this on an instance whose
		// scene is actually meant to showcase reflections.
		void EnableSSR();
		void DisableSSR();

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

		Renderable* sphereHandle;
		Renderable* quadHandle;

		// Uniform Handlers
		Uniform *pointPosHandle, *pointRadiusHandle, *pointColorHandle, *pointShadowHandle, *pointShadowDepthsMVPHandle, *pointShadowPCFTexelHandle, *pointHaveShadowHandle;
		Uniform *dirDirHandle, *dirColorHandle, *dirShadowHandle, *dirShadowPCFTexelHandle, *dirShadowDepthsMVPHandle, *dirShadowFarHandle, *dirHaveShadowHandle;
		Uniform *spotPosHandle, *spotDirHandle, *spotRadiusHandle, *spotOutterHandle, *spotInnerHandle, *spotColorHandle, *spotShadowHandle, *spotShadowDepthsMVPHandle, *spotShadowPCFTexelHandle, *spotHaveShadowHandle;

		// See RenderScene()'s comment where these are set - real fix for
		// point/spot light volumes vanishing when the camera is near/
		// inside their radius (near-plane clipping the sphere proxy away
		// entirely before rasterization).
		Uniform *pointUseFullscreenQuadHandle, *spotUseFullscreenQuadHandle;

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
		// the end of RenderScene() directly from this call's own Camera/
		// projection arguments, immune to whatever PreRender() or any
		// shadow sub-pass did to the shared scratch matrices.
		Matrix ssrPrvViewMatrix, ssrPrvProjectionMatrix;
		Uniform *lastPassPrvViewMatrixHandle, *lastPassPrvProjectionMatrixHandle;
	};

};

#endif /* DEFERREDRENDERER_H */
