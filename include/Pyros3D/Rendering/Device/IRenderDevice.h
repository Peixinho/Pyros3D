//============================================================================
// Name        : IRenderDevice.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Backend-agnostic seam for the state/bind/draw calls
//               IRenderer (and friends) used to issue directly against
//               OpenGL. GLRenderDevice is the only implementation today;
//               see VULKAN_ROADMAP.md for the motivation and a future
//               VulkanRenderDevice.
//============================================================================

#ifndef IRENDERDEVICE_H
#define IRENDERDEVICE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace p3d {

	namespace Buffer_Bit
	{
		enum {
			None = 0,
			Color = 0x10,
			Depth = 0x20,
			Stencil = 0x40
		};
	}

	namespace StencilOp
	{
		enum {
			Keep = 0,
			Zero,
			Replace,
			Incr,
			Incr_Wrap,
			Decr,
			Decr_Wrap,
			Invert
		};
	}

	namespace StencilFunc
	{
		enum {
			Always = 0,
			Never,
			Less,
			LEqual,
			Greater,
			GEqual,
			Equal,
			Notequal
		};
	}

	namespace BlendFunc
	{
		enum {
			Zero = 0,
			One,
			Src_Color,
			One_Minus_Src_Color,
			Dst_Color,
			One_Minus_Dst_Color,
			Src_Alpha,
			One_Minus_Src_Alpha,
			Dst_Alpha,
			One_Minus_Dst_Alpha,
			Constant_Color,
			One_Minus_Constant_Color,
			Constant_Alpha,
			One_Minus_Constant_Alpha,
			Src_Alpha_Saturate,
			Src1_Color,
			One_Minus_Src1_Color,
			Src1_Alpha,
			One_Minus_Src1_Alpha
		};
	}

	namespace BlendEq
	{
		enum {
			Add = 0,
			Subtract,
			Reverse_Subtract
		};
	}

	namespace DepthTest
	{
		enum {
			Less = 0,
			Never,
			Greater,
			Equal,
			Always,
			LEqual,
			GEqual,
			NotEqual
		};
	}

	// Opaque handle returned by resource-creation calls below. Same
	// underlying type as the GL handles this replaces (uint32) - resource
	// creation itself (Texture/FrameBuffer/GeometryBuffer/Shader) is not
	// part of this seam yet, only the state/bind/draw calls IRenderer used
	// to issue directly. See VULKAN_ROADMAP.md Phase 3.
	typedef uint32 DeviceHandle;

	// Handle to a recorded sequence of draw commands. GL executes every
	// call immediately against whatever's currently bound - it has no real
	// command buffer - so GLRenderDevice's BeginCommandBuffer/EndCommandBuffer
	// are no-ops returning a meaningless constant, and every other GL method
	// that takes one just ignores it. Vulkan needs recording into a real
	// VkCommandBuffer before submission; this is the seam that lets
	// VulkanRenderDevice do that. See VULKAN_ROADMAP.md Phase 5 - as of this
	// commit IRenderer obtains one of these per frame and threads it through
	// the draw-related calls below, but RenderObject()'s cull/blend/depth/
	// wireframe state calls still go through the individual Set* methods
	// further down, not through a Pipeline yet - that consolidation needs a
	// real VulkanRenderDevice to validate PipelineDesc's exact shape against
	// first, not just GL to guess at it with.
	typedef uint32 CommandBufferHandle;

	// Backend-agnostic seam for state changes, resource binding, and draw
	// calls. One instance per backend choice (today: GLRenderDevice only),
	// constructed by IRenderer/DebugRenderer/PostEffectsManager. Every
	// method here is a mechanical 1:1 stand-in for a GL call sequence that
	// used to live directly in those classes - behavior is meant to be
	// byte-for-byte identical to before this seam existed.
	class PYROS3D_API IRenderDevice {

	public:

		virtual ~IRenderDevice() {}

		// Command buffer recording - see the comment on CommandBufferHandle
		// above. Obtained once per frame by IRenderer and threaded through
		// the draw-related calls below (BindVertexArray/DrawElements/
		// DrawElementsInstanced/BindPipeline).
		virtual CommandBufferHandle BeginCommandBuffer() = 0;
		virtual void EndCommandBuffer(const CommandBufferHandle cmd) = 0;

		// True per-frame boundary (as opposed to BeginCommandBuffer()/
		// EndCommandBuffer() above, which per-object call sites in
		// IRenderer treat as cheap/idempotent-within-a-frame, not a real
		// begin/end pair) - called once each by ForwardRenderer::RenderScene()
		// around the whole frame. GL: no-op (GL has no concept of a frame
		// boundary distinct from SDL2Context::Draw()'s SDL_GL_SwapWindow()).
		// Vulkan: BeginFrame() acquires the next swapchain image and begins
		// the real VkCommandBuffer + render pass (what BeginCommandBuffer()
		// then just returns a handle for, cheaply, on every subsequent call
		// within the same frame); EndFrame() ends the render pass, submits,
		// and presents. See VULKAN_ROADMAP.md - this only wraps
		// ForwardRenderer's top-level render pass; a shadow sub-pass
		// (rendered into an offscreen shadow-map FBO, called from
		// PreRender() *before* RenderScene()/BeginFrame() even runs) has no
		// swapchain-framebuffer target at all and isn't covered by this -
		// framebuffer/texture support on this backend remains out of scope
		// (RotatingCube, this backend's only validated target, casts no
		// shadows).
		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		// Whether BeginFrame() has run and EndFrame() has not. Lets a pass
		// that may or may not be the only one in an app open its own frame
		// without opening a second one over somebody else's - see
		// UIRenderer::RenderUI(). Always false on GL, where both are
		// genuine no-ops and a second call would cost nothing anyway.
		virtual bool IsFrameInProgress() const { return false; }

		// Blocks until all previously-submitted GPU work has completed.
		// Vulkan-real (wraps vkDeviceWaitIdle - was already implemented,
		// just never part of this virtual interface, so nothing backend-
		// agnostic could call it); a genuine no-op on GL, which has no
		// equivalent async-in-flight-work class of hazard for this
		// pattern to race against. Needed by DeferredRenderer::Resize()
		// (material-aware SSR's previousFrameColorTexture warm-up-on-
		// resize) - found via a real user-reported hang resizing
		// SSRTest on Vulkan, where a resize could interrupt an
		// in-flight frame just before a fresh offscreen render pass
		// began, without anything guaranteeing the interrupted frame's
		// GPU work had actually finished first.
		virtual void WaitIdle() = 0;

		// Real, needed backend branch - not every GL/Vulkan difference is
		// hideable behind a shared abstraction. DeferredRenderer's
		// point/spot light-volume Sphere primitive (reversed winding vs
		// Cube, see its own SetCullFace() comment) genuinely needs
		// CullFace::FrontFace on GL and CullFace::BackFace on Vulkan -
		// confirmed via repeated, real screenshot comparisons on both
		// backends, not a single global value that happens to work on
		// one and not the other (an earlier attempt at "one value for
		// both" produced results that looked right on one run and wrong
		// on another - genuinely different per-backend behavior, not a
		// flaky test).
		virtual bool IsVulkan() const = 0;

		// True when the swapchain is UNORM but presentation still treats
		// bytes as sRGB (Vulkan + Metal). Linear HDR shown through ImGui
		// needs a manual gamma encode (GetViewportColor); do NOT flip the
		// swapchain to an SRGB format for that - ImGui UI colours are
		// already sRGB-authored and wash out on an SRGB target.
		virtual bool NeedsManualDisplayGamma() const { return false; }

		// Where row 0 of a render target lives. GL puts the framebuffer
		// origin at the BOTTOM-left, so glGetTexImage/glReadPixels hand back
		// the image bottom-up and a caller indexing by screen Y has to flip
		// (height - y). Vulkan and Metal both put it at the TOP-left, and
		// their readback is top-down - already the same direction as a mouse
		// coordinate, so flipping there reads the mirrored row.
		//
		// This is the same split the editor's viewport already encodes by
		// hand in its ImGui::Image UVs; anything that reads a rendered image
		// back on the CPU (PainterPick's colour-id buffer, screenshots,
		// pixel-level tests) needs it too, so it lives here rather than as
		// another _SDL2VULKAN/_SDL2METAL #if at each call site.
		virtual bool RenderTargetOriginIsTopLeft() const { return false; }

		// Clearing - TranslateBufferBit() has no side effects (pure
		// translation, cached by the caller); Clear() issues the actual
		// clear using a previously-translated mask.
		virtual uint32 TranslateBufferBit(const uint32 bufferBits) = 0;
		virtual void Clear(const uint32 nativeBufferBits) = 0;
		virtual void SetClearColor(const Vec4 &color) = 0;
		// Last colour handed to SetClearColor(). Exists so a caller that has
		// to clear to something else for a sub-pass can put back what it
		// found instead of leaving this shared state clobbered - see
		// PostEffectsManager::RenderEffects(). Non-virtual: every override of
		// SetClearColor() routes through SetClearColorTracked() below, so
		// there is nothing per-backend to answer here.
		const Vec4 &GetClearColor() const { return lastClearColor; }

		// Depth
		virtual void SetDepthTest(const bool enabled, const uint32 mode) = 0;
		virtual void SetDepthMask(const bool enabled) = 0;
		// glDepthMask(GL_TRUE) + glClearDepth(1.f) - a no-op on GLES3,
		// matching ClearDepthBuffer()'s existing #if !defined(GLES3) guard.
		virtual void PrepareDepthClear() = 0;

		// Stencil
		virtual void SetStencilTestEnabled(const bool enabled) = 0;
		virtual void SetClearStencilValue() = 0;
		virtual void SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) = 0;
		virtual void SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) = 0;

		// Scissor. Pixels, and the origin is the TOP-LEFT of the current
		// render target on every backend - GL flips internally against the
		// viewport it was last given, because a UI that had to know which
		// backend it was drawing through would get it wrong on two of the
		// three. Disabling restores the full viewport rather than leaving
		// the last rect in place: Vulkan and Metal have no scissor test to
		// switch off, only a rect that is always in effect.
		virtual void SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height) = 0;
		virtual void SetScissorTestEnabled(const bool enabled) = 0;

		// Wireframe (no-op on GLES3, matching Enable/DisableWireFrame()'s
		// existing #if !defined(GLES3) guard)
		virtual void SetWireFrame(const bool enabled) = 0;

		// Color
		virtual void SetColorMask(const bool r, const bool g, const bool b, const bool a) = 0;

		// Depth bias / polygon offset
		virtual void SetPolygonOffsetEnabled(const bool enabled) = 0;
		virtual void SetPolygonOffset(const f32 factor, const f32 units) = 0;

		// Blending
		virtual void SetBlendingEnabled(const bool enabled) = 0;
		virtual void SetBlendFunction(const uint32 sfactor, const uint32 dfactor) = 0;
		virtual void SetBlendEquation(const uint32 mode) = 0;

		// Cull face - cullFace is one of IMaterial.h's CullFace::BackFace/
		// FrontFace/DoubleSided values.
		virtual void SetCullFaceMode(const uint32 cullFace) = 0;
		virtual void DisableCullFace() = 0;

		// Pipeline: bundles a shader program with the fixed-function state
		// Vulkan bakes into VkPipeline at creation time (depth test/write,
		// blending, cull mode) - the same state the Set*/Enable*/Disable*
		// methods on this interface already let a caller set individually.
		// Not yet used by IRenderer (see the comment on CommandBufferHandle
		// above) - exists so a VulkanRenderDevice has somewhere to actually
		// create a VkPipeline once a real caller needs one, without another
		// interface change. GLRenderDevice's CreatePipeline just records the
		// desc in a small internal table; BindPipeline applies it via the
		// exact same gl* calls the individual Set* methods already issue.
			// One vertex attribute within a VertexBufferLayoutDesc, sourced
			// directly from RenderingMesh::Geometry's AttributeArray/
			// VertexAttribute list (Renderables.h). `name` matches the
			// attribute's name in PyrosShader.glsl (e.g. "aPosition") -
			// Vulkan has no runtime "get location by name" the way GL's
			// glGetAttribLocation does, so the location is resolved by
			// name against the vertex shader's reflected stage inputs
			// (SpirvShaderCompiler::ReflectStageInputs(), cached per
			// program in VulkanRenderDevice::LinkProgram()) instead of
			// being passed here directly. GL ignores this whole struct -
			// its own per-attribute glVertexAttribPointer calls (already
			// issued elsewhere in IRenderer::BindMesh(), unchanged) handle
			// this dynamically already, so CreatePipeline()/BindPipeline()
			// on GLRenderDevice never look at vertexLayout.
			struct VertexAttributeDesc {
				std::string name;
				uint32 type;   // Buffer::Attribute::Type value (GeometryBuffer.h)
				uint32 offset; // byte offset within this buffer's stride
				uint32 divisor; // VertexAttribute::VertexDivisor - 0 for per-vertex, >0 for instanced

				VertexAttributeDesc() : type(0), offset(0), divisor(0) {}
			};
			// One entry per AttributeArray/AttributeBuffer the mesh's
			// geometry has - each is a separate vertex buffer binding for
			// Vulkan (VkVertexInputBindingDescription). `stride` is the
			// buffer's per-vertex byte size (AttributeBuffer::attributeSize).
			struct VertexBufferLayoutDesc {
				uint32 stride;
				std::vector<VertexAttributeDesc> attributes;

				VertexBufferLayoutDesc() : stride(0) {}
			};

		struct PipelineDesc {
			uint32 shaderProgram;
			bool depthTest;
			uint32 depthTestMode; // DepthTest::* value, meaningful only if depthTest
			bool depthWrite;
			bool blendingEnabled;
			uint32 blendSrcFactor, blendDstFactor; // BlendFunc::* values, meaningful only if blendingEnabled
			uint32 blendEquation; // BlendEq::* value, meaningful only if blendingEnabled
			uint32 cullFace; // IMaterial.h's CullFace::* value; DoubleSided means "no culling"
			bool wireframe;
			// The mesh's DrawingType::* value. GL and Metal take the
			// primitive type as a draw-call argument and ignore this;
			// Vulkan bakes topology into the pipeline, so it has to be
			// known here. Defaults to Triangles so any caller that does
			// not set it behaves exactly as before.
			uint32 drawingType;
			// Mesh's actual per-buffer vertex attribute layout - see
			// VertexBufferLayoutDesc above. Left empty by any caller that
			// never populates it (none today - IRenderer::BindMesh()
			// always fills this before calling CreatePipeline()); an empty
			// vertexLayout on Vulkan fails pipeline creation loudly
			// (VulkanRenderDevice::CreatePipeline() logs and returns 0)
			// rather than silently binding no vertex input at all.
			std::vector<VertexBufferLayoutDesc> vertexLayout;
			// True when this pipeline is for IRenderer's shared
			// shadowMaterial/shadowSkinnedMaterial (see BindMesh()'s
			// caller) - i.e. it will only ever be used within an
			// offscreen depth-only shadow-map render pass, never the
			// main swapchain one. GL ignores this entirely (state is
			// applied per-draw regardless of any render pass concept).
			// Vulkan bakes a specific VkRenderPass's attachment shape
			// into a pipeline at creation time - using the swapchain's
			// color+depth render pass (this backend's default) for a
			// shadow-casting draw is a real render-pass-compatibility
			// mismatch (VUID-vkCmdDrawIndexed-renderPass-02684 caught
			// this the hard way, the first time a real shadow pass tried
			// to draw with it - see VulkanRenderDevice::shadowPipelineRenderPass's
			// comment for the fix), so this flag tells CreatePipeline()
			// to target a depth-only render pass instead.
			bool isShadowPass;
			// True for a full-screen-triangle post-effect pass
			// (PostEffectsManager) - the vertex shader computes its
			// position purely from gl_VertexIndex (see IEffect.cpp), so
			// there genuinely is no vertex attribute data, unlike the
			// "caller forgot to populate vertexLayout" case
			// vertexLayout.empty() alone is meant to catch. Lets
			// CreatePipeline() build a real zero-binding
			// VkPipelineVertexInputStateCreateInfo instead of failing.
			bool noVertexInput;

			PipelineDesc()
				: shaderProgram(0), depthTest(true), depthTestMode(DepthTest::Less), depthWrite(true),
				  blendingEnabled(false), blendSrcFactor(BlendFunc::One), blendDstFactor(BlendFunc::Zero),
				  blendEquation(BlendEq::Add), cullFace(0), wireframe(false), drawingType(0 /* DrawingType::Triangles */),
				  isShadowPass(false), noVertexInput(false) {}
		};
		virtual DeviceHandle CreatePipeline(const PipelineDesc &desc) = 0;
		virtual void DestroyPipeline(const DeviceHandle pipeline) = 0;
		virtual void BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline) = 0;

		// Real, found-via-reproduction bug fix: a pipeline created while
		// currentBoundFBO==0 (i.e. targeting the swapchain directly, not a
		// Texture-backed FBO) is built against whatever VkRenderPass object
		// happens to be VulkanRenderDevice::renderPass at that moment. On
		// Vulkan, a window resize destroys and recreates that render pass
		// (VulkanRenderDevice::RecreateSwapchain()) - a pipeline cached
		// forever against the old, now-destroyed one and never rebuilt
		// (exactly what IEffect::pipelineHandle/PostEffectsManager did,
		// see ProcessPostEffects()'s comment) reads back as solid garbage
		// on MoltenVK - confirmed via debug logging catching the exact
		// moment: a real resize destroyed renderPass A and created
		// compatible-but-different renderPass B, the cached pipeline (still
		// built against A) kept being bound every frame after, and that's
		// exactly when the screen turned solid red. Default 0 (GL, and any
		// Vulkan pipeline that only ever targets a stable Texture-backed
		// FBO - those already get correctly invalidated on resize via
		// InvalidateFramebuffersForTexture(), a swapchain generation bump
		// would just be redundant there) - callers that cache a pipeline
		// targeting the swapchain directly should store the generation at
		// creation time and rebuild whenever this getter's value changes.
		virtual uint32 GetSwapchainGeneration() const { return 0; }

		// Tells the device a resize actually happened, so it can rebuild
		// its swapchain-sized resources proactively. On Vulkan this used
		// to be purely reactive (rebuild only once a swapchain operation
		// returns VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR) on the
		// assumption that's the only way a resize ever reaches the app -
		// true for a native drag-resize, but MoltenVK does not reliably
		// report either result for a window resized through the
		// Accessibility API (a tiling WM like yabai, not a user dragging
		// an edge) - confirmed via [RESIZE] logging showing zero
		// RecreateSwapchain() calls ever following a real, confirmed
		// resize. The swapchain then just keeps presenting its old-size
		// image forever, which the compositor silently stretches to fill
		// the window's real (now different) bounds - reads as "resize
		// hangs/stretches", indefinitely, with no reactive signal ever
		// arriving to fix it. Default no-op (GL has no swapchain).
		virtual void NotifySurfaceResized(const uint32 width, const uint32 height) { (void)width; (void)height; }

		// Clip distances (no-op on GLES3, matching StartClippingPlanes()/
		// EndClippingPlanes()'s existing #if !defined(GLES3) guard)
		virtual void EnableClipDistance(const uint32 index) = 0;
		virtual void DisableClipDistance(const uint32 index) = 0;

		// Viewport
		virtual void SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height) = 0;

		// Program / vertex state
		virtual void UseProgram(const uint32 program) = 0;
		virtual DeviceHandle CreateVertexArray() = 0;
		virtual void DeleteVertexArray(const DeviceHandle vao) = 0;
		virtual void BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao) = 0;
		virtual void BindArrayBuffer(const uint32 buffer) = 0;
		virtual void BindElementBuffer(const uint32 buffer) = 0;
		// typeCount/nativeType come from the engine's existing
		// Buffer::Attribute::GetTypeCount()/GetType() helpers
		// (GeometryBuffer.h) - nativeType is already a backend-native
		// token by the time it reaches here (a GL_* enum today), passed
		// through unmodified since GeometryBuffer's internals aren't part
		// of this seam yet (see VULKAN_ROADMAP.md Phase 3).
		virtual void SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) = 0;
		// Same as SetVertexAttribute(), but for the common case (DebugRenderer,
		// PostEffectsManager) of a plain float/vecN attribute with no
		// engine-side type token available - the device fills in its own
		// native "float" type.
		virtual void SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) = 0;
		virtual void DisableVertexAttribute(const int32 location) = 0;
		virtual void SetVertexAttributeDivisor(const int32 location, const uint32 divisor) = 0;
		// glGetUniformBlockIndex + glUniformBlockBinding, no-op if the
		// shader doesn't declare a block with this name.
		//
		// bufferHandle names *which* UBO this program should read at
		// bindingPoint. 0 means "whatever CreateUniformBuffer() registered
		// at this binding point last" - correct for the engine's shared,
		// single-instance UBOs (GlobalMatrices, LightsBlock, ... - see
		// IRenderer's 0-23 range), where exactly one buffer ever exists per
		// binding. It is NOT correct for the per-material/per-effect
		// extraUniforms blocks (IMaterial::ExtraUniformsBlock,
		// IEffect::extraUniformsBinding): two live instances of the same
		// material type - two DeferredRenderers, e.g. the editor's Scene
		// View plus the Material Editor's preview - each allocate their own
		// buffer at the same binding, and the global registry only remembers
		// the last one. Those callers pass their own handle so every
		// program keeps reading its own buffer.
		virtual void BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle = 0) = 0;

		// Backend-specific clip-space correction for the projection matrix
		// Matrix::PerspectiveMatrix()/OrthoMatrix() build (see
		// IRenderer::SendGlobalUniforms(), the only call site - applied to
		// uProjectionMatrix right before it's uploaded). Those matrices
		// are built once, shared by every backend, using OpenGL's NDC
		// convention (Z in [-1,1], Y+ up) since that's what this engine
		// targeted before this backend existed - GL's identity translation
		// here is a no-op (matches what every example already renders
		// with), Vulkan's applies the two corrections its different NDC
		// convention needs: Z remapped from [-1,1] to [0,1] (Vulkan's
		// clip volume, unlike GL's, doesn't include negative Z - without
		// this, half of any scene's geometry is clipped away entirely,
		// discovered via this session's first actual pixel-level
		// verification showing 0% of the expected color on screen despite
		// zero errors/crashes at every step before that - "no error" and
		// "renders correctly" are not the same claim), and Y flipped
		// (Vulkan's NDC Y+ points down the framebuffer, GL's points up -
		// without this the image would still be visible, just upside
		// down). See VULKAN_ROADMAP.md.
		//
		// skipYFlip: for rendering one face of a point-light's shadow
		// cubemap only - unlike every other render target (the swapchain,
		// a directional/spot shadow map, a G-buffer), a cubemap face is
		// never addressed by a 2D UV that itself needs to agree with a
		// window-space up/down convention; PCFPOINT() (PyrosShader.glsl)
		// samples it by a raw world-space direction vector instead
		// (`texture(samplerCubeShadow, direction)`), and GL/Vulkan use the
		// *same* standardized direction-to-face-texel formula for that -
		// it doesn't depend on either API's window-space Y convention at
		// all. Applying the Y-flip when rasterizing into a face therefore
		// doesn't correct anything a cubemap consumer needs corrected; it
		// just mismatches what real occluder depth is written into a row
		// of that face against what a given direction vector reads back
		// out later, and that direction-vs-content mismatch is exactly
		// wrong except at pixels the flip happens to leave unchanged - the
		// point-light shadow map fills in with real depth data (confirmed
		// via DebugReadDepthTexture) but produces zero measurable
		// darkening (confirmed via luminance-profile comparison) without
		// this. Z is unaffected either way (only the Y row of the
		// correction matrix changes) - GL ignores this parameter entirely
		// (its translation is already a no-op).
		virtual Matrix TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip = false) = 0;

		// Set for the whole of a point light's six cube-face shadow draws
		// (IRenderer::PreRender()'s POINT block), cleared straight after.
		// Companion to TranslateProjectionMatrix()'s skipYFlip: a backend
		// whose cube-face pass uses the *opposite* clip-space Y sign from
		// its normal pass has every triangle mirrored vertically there,
		// which reverses the winding the rasterizer sees. Front-face
		// culling then keeps exactly the wrong surface - a closed
		// occluder's far side instead of its near one - so the cube map
		// records the depth of the back of every object and no shadow
		// survives the comparison. GL never changes Y and so ignores this
		// entirely, exactly as it ignores skipYFlip; Vulkan and Metal both
		// flip their front-face winding while it is set. Culling is not
		// simply disabled for the shadow pass instead because a caster's
		// own material decides it (IRenderer's EffectiveCullFace()) and a
		// single-sided caster depends on it.
		virtual void SetPointShadowCubeFacePass(const bool enabled) = 0;

		// Matrix::BIAS (Matrix.h) is the classic GL shadow-lookup bias:
		// remaps X, Y, *and* Z from clip-space [-1,1] to UV/depth-compare
		// [0,1] - correct standalone, but a shadow-lookup matrix is
		// always built as `TranslateShadowBiasMatrix() * TranslateProjectionMatrix(...) * view * ...`
		// (see IRenderer::PreRender()'s DirectionalShadowMatrix/
		// SpotShadowMatrix), and TranslateProjectionMatrix() on Vulkan
		// *already* remapped Z to [0,1] (see its own comment) - applying
		// BIAS's own Z remap on top would double-transform it. GL:
		// returns Matrix::BIAS unchanged (its TranslateProjectionMatrix()
		// is an identity no-op, so the full remap is still needed here).
		// Vulkan: returns an X/Y-only variant (Z passes through
		// unchanged). Found the hard way: a directional shadow test
		// showed a real depth-compare descriptor correctly written and
		// no validation errors, yet zero shadowing effect whatsoever -
		// the double Z-transform was pushing every comparison reference
		// depth to a nonsensical value, "renders without error but is
		// wrong" (see the comment on TranslateProjectionMatrix() for why
		// pixel-level verification, not just error-free execution,
		// matters here).
		virtual Matrix TranslateShadowBiasMatrix() = 0;

		// Draw - engineDrawType is one of RenderingComponent.h's
		// DrawingType::Triangles/Lines/... values.
		virtual uint32 TranslateDrawType(const uint32 engineDrawType) = 0;
		virtual void DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) = 0;
		virtual void DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount) = 0;
		virtual void DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount) = 0;

		// Uniform buffers - CreateUniformBuffer mirrors what IRenderer's
		// constructor used to do inline for each shared UBO (gen, bind,
		// allocate, bind to binding point, unbind).
		virtual DeviceHandle CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint) = 0;
		// Partial/sub-range update (glBufferSubData) - use only when the
		// buffer has other live data outside [offset, offset+sizeBytes)
		// that must be preserved (e.g. DirectionalShadowBlock's matrices
		// and uDirectionalShadowFar, written via two separate calls at
		// different offsets). For a full-buffer replace, prefer
		// ReplaceUniformBuffer() below - see its comment for why.
		virtual void UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data) = 0;
		// Re-specifies the buffer's entire backing storage with new data
		// ("orphaning" - glBufferData, not glBufferSubData). Only correct
		// when replacing the buffer's full previous contents (any bytes
		// this call doesn't write are left undefined, since the old
		// storage is discarded wholesale) - safe here for buffers where
		// the trailing "unused" region past sizeBytes, if any, is never
		// read by the shader regardless (e.g. LightsBlock/BoneMatrices,
		// gated by uNumberOfLights/actual bone count), and safe/required
		// for buffers written as a single complete write every time
		// (ObjectMatrixUniforms, MaterialUniforms, etc). Exists because
		// naive UpdateUniformBuffer on a UBO rewritten every object every
		// frame forces the driver to stall the CPU until any in-flight GPU
		// work still reading the buffer's old contents finishes - measured
		// as a real ~10x regression (SimplePhysics: ~100fps -> ~10fps with
		// its ~1000 objects sharing one material) once the loose-uniforms
		// UBO migration made ObjectMatrixUniforms/MaterialUniforms get
		// rewritten on every RenderObject() call. Orphaning lets the
		// driver hand back fresh backing memory instead of blocking.
		virtual void ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data) = 0;
		virtual void DestroyUniformBuffer(const DeviceHandle buffer) = 0;

		// Geometry/vertex/index buffers - bufferType/bufferDraw/mappingType
		// are GeometryBuffer.h's Buffer::Type::Index/Vertex/Attribute,
		// Buffer::Draw::Static/Dynamic/Stream, and
		// Buffer::Mapping::Read/Write/ReadAndWrite values respectively.
		virtual DeviceHandle CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) = 0;
		// Full reallocation (glBufferData) - used when new data is a
		// different size than what's currently allocated.
		virtual void ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) = 0;
		// In-place update (glBufferSubData) - same size as before.
		virtual void UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length) = 0;
		virtual void DestroyBuffer(const DeviceHandle buffer) = 0;
		virtual void *MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType) = 0;
		virtual void UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) = 0;

		// Vertex attribute component type translation - engineType is one
		// of GeometryBuffer.h's Buffer::Attribute::Type::Int/Short/Float/
		// Vec2/Vec3/Vec4/Matrix values (matches SetVertexAttribute's
		// nativeType param above).
		virtual uint32 TranslateAttributeType(const uint32 engineType) = 0;

		// Shader compilation/linking - engineShaderType is one of
		// Shaders.h's ShaderType::VertexShader/FragmentShader/
		// GeometryShader values. BuildShaderSource assembles the
		// backend/profile-specific prefix (a `#version` line for GL) ahead
		// of definitions + the shader body - this is the seam Phase 2's
		// GLSL->SPIR-V cross-compilation will replace/extend.
		virtual std::string BuildShaderSource(const std::string &definitions, const std::string &shaderBody) = 0;
		virtual DeviceHandle CreateShaderStage(const uint32 engineShaderType) = 0;
		// Returns false and fills errorLog on failure (errorLog may be
		// left untouched if the log has fewer than 2 bytes, matching the
		// original "length > 1" check).
		virtual bool CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog) = 0;
		virtual DeviceHandle CreateProgram() = 0;
		virtual void AttachShaderStage(const DeviceHandle program, const DeviceHandle shader) = 0;
		virtual bool LinkProgram(const DeviceHandle program, std::string &errorLog) = 0;
		virtual bool IsProgram(const DeviceHandle id) = 0;
		virtual bool IsShaderStage(const DeviceHandle id) = 0;
		virtual void DetachShaderStage(const DeviceHandle program, const DeviceHandle shader) = 0;
		virtual void DeleteShaderStage(const DeviceHandle shader) = 0;
		virtual void DeleteProgram(const DeviceHandle program) = 0;

		virtual int32 GetUniformLocation(const uint32 program, const std::string &name) = 0;
		virtual int32 GetAttributeLocation(const uint32 program, const std::string &name) = 0;

		// One method per Uniforms::DataType (Uniforms.h) - matches the
		// glUniform*v overload set Shader::SendUniform() used to dispatch
		// to directly.
		virtual void SendUniformInt(const int32 handle, const int32 *data, const uint32 count) = 0;
		virtual void SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) = 0;
		virtual void SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) = 0;

		// Texture translation - engineDataType/engineTextureType are
		// Texture.h's TextureDataType::*/TextureType::* values.
		// TranslateTextureFormat mirrors what Texture::GetInternalFormat()
		// used to compute inline; TranslateTextureTarget mirrors
		// Texture::GetGLModes().
		virtual void TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type) = 0;
		virtual void TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode) = 0;

		// Texture object lifecycle
		// An ImTextureID for `texture`, or NULL if this backend cannot hand
		// one over for that kind of target. Debug UI only (the render-target
		// viewer): ImGui draws whatever it is given as a plain 2D colour
		// sample, so depth, cube and multisampled targets return NULL rather
		// than something that would draw as garbage. Not pure - a backend
		// that has not implemented it simply reports nothing viewable.
		virtual void *GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType) { (void)texture; (void)engineTextureType; return NULL; }

		virtual DeviceHandle CreateTextureObject() = 0;
		virtual void DestroyTextureObject(const DeviceHandle texture) = 0;
		virtual void BindTextureToTarget(const uint32 target, const DeviceHandle texture) = 0;

		// Texture upload - internalFormat/format/type are the native
		// tokens TranslateTextureFormat() produced. willMipmap is only
		// meaningful on Vulkan (see the comment on VulkanRenderDevice's
		// override) - GL ignores it, since GenerateMipmap() there can
		// extend an already-uploaded texture's level chain at any time;
		// Vulkan's VkImage mip level count is fixed at creation, so it
		// has to know up front whether GenerateMipmap() is coming.
		virtual void UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap) = 0;
		virtual void UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) = 0;
		virtual void GenerateMipmap(const uint32 target) = 0;

		// Texture parameters - engineRepeat/engineFilter are Texture.h's
		// TextureRepeat::*/TextureFilter::* values. SetTextureWrapR and
		// SetTextureCompareMode are no-ops on GLES3, matching the
		// original #if !defined(GLES3) guards around those call sites.
		virtual void SetTextureWrapS(const uint32 target, const uint32 engineRepeat) = 0;
		virtual void SetTextureWrapT(const uint32 target, const uint32 engineRepeat) = 0;
		virtual void SetTextureWrapR(const uint32 target, const uint32 engineRepeat) = 0;
		virtual void SetTextureMagFilter(const uint32 target, const uint32 engineFilter) = 0;
		virtual void SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap) = 0;
		virtual void SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) = 0;
		virtual void SetTextureBorderColor(const uint32 target, const Vec4 &color) = 0;
		virtual void SetTextureCompareMode(const uint32 target) = 0;
		virtual void SetPixelUnpackAlignment(const uint32 value) = 0;

		// Texture units
		virtual void ActivateTextureUnit(const uint32 unit) = 0;

		// Readback - returns the byte size the caller should have already
		// sized `outBuffer` to (see GetTextureDataSize below); a no-op on
		// GLES3, matching Texture::GetTextureData()'s original
		// #if !defined(GLES3) guard.
		virtual void ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) = 0;
		// Mirrors the byte-size-per-format switch that used to live
		// directly in Texture::GetTextureData() (including its existing
		// quirks/rounding, e.g. RGB8 sizing as if 2 bytes/pixel not 3 -
		// preserved as-is rather than "fixed" during this mechanical move).
		virtual uint32 GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) = 0;

		// Framebuffers/renderbuffers - engine enum params are
		// FrameBuffer.h's FBOAccess::*/FrameBufferAttachmentFormat::*/
		// RenderBufferDataType::*/FBOBufferBit::*/FBOFilter::* values.
		// Attachment texture targets reuse TranslateTextureTarget()'s
		// `mode` output (same GL_TEXTURE_CUBE_MAP_*/GL_TEXTURE_2D/
		// GL_TEXTURE_2D_MULTISAMPLE translation table as Texture.cpp).
		// Which FBO is currently bound for rendering (0 = the default
		// swapchain/backbuffer target) - lets RenderingMesh::PipelineCache
		// (RenderingComponent.h) key its cached pipelines by render
		// target as well as shader, not just shader. A Vulkan pipeline
		// bakes in a specific render pass's attachment shape at creation
		// time, so the *same* mesh+shader drawn into two differently-shaped
        // targets (e.g. IslandDemo's water reflection FBO - color-only,
		// no depth - versus the main swapchain pass - color+depth) needs
		// two separate pipelines, not one wrongly shared between them
		// (VUID-vkCmdDrawIndexed-renderPass-02684, found via a live
		// regression once color-attachment FBOs started working at all -
		// previously silently masked, since every color attachment
		// request was rejected outright). GL has no such restriction (no
		// render-pass concept) and always returns 0, collapsing this back
		// to the original shader-only keying for that backend - zero
		// behavior change there.
		virtual DeviceHandle GetCurrentRenderTarget() = 0;
		virtual DeviceHandle CreateFramebuffer() = 0;
		virtual void DestroyFramebuffer(const DeviceHandle fbo) = 0;
		// Declares that this FBO's depth attachment already holds meaningful
		// data when it is bound, and must survive the bind rather than being
		// cleared.
		//
		// Needed because the two backends express "clear" at opposite ends of
		// a pass. GL clears imperatively *after* binding, so a caller that
		// only wants colour cleared just calls ClearBufferBit(Color) and the
		// depth it prepared is untouched. Vulkan bakes the choice into the
		// render pass's load op at creation time, and this backend used
		// LOAD_OP_CLEAR for every attachment - so any depth written before
		// the bind was discarded before the first draw.
		//
		// DeferredRenderer is the caller that cares: it copies the finished
		// G-buffer depth into lastPassFBO's depth attachment so the forward/
		// transparent pass can depth-test against the opaque scene (the
		// G-buffer's own depth texture can't be aliased here - it is being
		// sampled as tDepth by the lighting materials at the same time, and
		// on Vulkan a texture cannot be sampled and attached at once). No-op
		// on GL, which never had the problem.
		virtual void SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve) = 0;
		virtual uint32 TranslateFramebufferAccess(const uint32 engineAccess) = 0;
		// finalizePending distinguishes a *real* bind (FrameBuffer::Bind(),
		// or its own "restore the previous FBO" bookkeeping on UnBind() -
		// nothing else will attach anything new before draws start) from
		// FrameBuffer::AddAttach()'s self-contained temporary bind/attach/
		// unbind around a *single* attachment call, one of possibly
		// several in a row building up one multi-attachment FBO before
		// anything ever renders into it. Both look identical as a plain
		// BindFramebuffer(access, fbo!=0) call - this parameter is what
		// tells a backend whether it's safe to finalize a still-accumulating
		// attachment set right now, or whether more AttachFramebufferTexture2D()
		// calls (each also self-contained) are still coming for this same
		// FBO before the next real bind. GL ignores this (same reasoning
		// as AttachFramebufferTexture2D()'s wasAlreadyBound parameter -
		// no render-pass/attachment-shape concept to prematurely finalize).
		virtual void BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending) = 0;
		virtual uint32 TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) = 0;
		// wasAlreadyBound (FrameBuffer::AddAttach()'s own `isBinded` check,
		// threaded straight through) tells a backend whether this call is
		// happening *inside* an already-active Bind()...UnBind() session
		// (true - e.g. a point light's per-frame per-cubemap-face
		// re-attach, called between an explicit Bind() and the next
		// draw, expected to take effect immediately) versus a
		// self-contained setup-time attach outside any session (false -
		// AddAttach() does its own temporary bind/attach/unbind in this
		// case, once per attachment, possibly several calls in a row
		// building up one multi-attachment FBO before anything ever
		// renders into it - e.g. DeferredRenderer's G-buffer: depth +
		// several color attachments, each its own AddAttach() call, all
		// before the first real per-frame Bind()). GL ignores this
		// entirely (no render-pass/attachment-shape concept to defer
		// building). Vulkan needs it to know whether it's safe to defer
		// building the real VkRenderPass/VkFramebuffer until more
		// attachments are known, or must render immediately because nothing
		// else will trigger it this frame - see VulkanRenderDevice's
		// comment on this same method for the full reasoning.
		virtual void AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound) = 0;
		virtual void AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) = 0;
		// No-ops on GLES3, matching the original #if !defined(GLES3) guards.
		virtual void SetDrawBufferNone() = 0;
		virtual void SetReadBufferNone() = 0;
		virtual void SetDrawBufferBack() = 0;
		virtual void SetReadBufferBack() = 0;
		// colorAttachmentIndices are 0-based indices into the
		// Color_Attachment0.. sequence (the device adds the
		// GL_COLOR_ATTACHMENT0 base internally).
		virtual void SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) = 0;
		virtual uint32 CheckFramebufferStatus() = 0;
		// Translates a raw status (as returned by CheckFramebufferStatus())
		// into FrameBuffer.h's FBOStatus::* values.
		virtual uint32 TranslateFramebufferStatus(const uint32 nativeStatus) = 0;

		virtual DeviceHandle CreateRenderbuffer() = 0;
		virtual void DestroyRenderbuffer(const DeviceHandle rbo) = 0;
		virtual void BindRenderbuffer(const DeviceHandle rbo) = 0;
		// engineDataType is RenderBufferDataType::Depth/Depth_Multisample/
		// RGBA/RGBA_Multisample/etc (the *_Multisample suffix doesn't
		// affect the translated format, only which of
		// RenderbufferStorage/RenderbufferStorageMultisample below the
		// caller goes on to use). FBOAttachment::DataType is overwritten
		// with this native token afterward (see FrameBuffer::Resize(),
		// which reuses it directly).
		virtual uint32 TranslateRenderbufferFormat(const uint32 engineDataType) = 0;
		// Both take an already-native format (from
		// TranslateRenderbufferFormat() or a previously-stored
		// FBOAttachment::DataType).
		virtual void RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) = 0;
		virtual void RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) = 0;

		virtual void SetMultisampleEnabled(const bool enabled) = 0;
		virtual void BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) = 0;

		// Copies a depth texture's contents into another same-size depth
		// texture. Exists for DeferredRenderer's benefit: its lighting
		// pass needs to *sample* the G-buffer's depth as a plain texture
		// (tDepth) while its later forward sub-pass (transparent objects
		// drawn after the lighting composite) needs a *real depth
		// attachment* with matching values for correct occlusion -
		// aliasing the exact same texture for both roles works fine on GL
		// (no attachment/sampled-image layout distinction), but Vulkan
		// forbids one image being both a render pass's active depth
		// attachment and a shader-sampled input at the same time - found
		// via VUID-vkCmdDrawIndexed-imageLayout-00344 on a real
		// DeferredRendering validation-layer run. A real copy (not an
		// alias) sidesteps the conflict on the backend that actually has
		// it; both `src`/`dst` are DeviceHandles as returned by
		// CreateTextureObject(). GL implements it via a throwaway
		// blit-FBO pair (correctness over performance, matches this
		// codebase's existing bar for a first implementation of a rarely-
		// called operation - once per frame, not once per object).
		virtual void CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height) = 0;

		// Reports whether CompileShaderStage() auto-wrapped this program's
		// given stage's loose (non-opaque) uniforms into a synthesized UBO
		// - see SpirvShaderCompiler::AutoFixForVulkan() and
		// VulkanRenderDevice's override for the actual mechanism, only
		// meaningful on that backend (CustomShaderMaterial's user-authored
		// shaders are plain, unlabeled GLSL that already works as loose
		// uniforms on GL - nothing to report there, so the default here
		// - inherited as-is by GLRenderDevice - always returns false).
		// `engineShaderType` is ShaderType::VertexShader/FragmentShader
		// (matches CreateShaderStage()'s parameter). Fills out* only when
		// returning true. CustomShaderMaterial's constructor calls this
		// for both stages right after LinkProgram() to auto-populate
		// IMaterial::extraUniforms[] with zero hand-authored wiring - the
		// existing IRenderer::SendExtraUniforms()/CaptureExtraUniform()
		// runtime machinery already works unchanged once that struct is
		// populated, regardless of whether it was filled by hand (every
		// shipped effect/material shader) or reported back from here
		// (any future arbitrary user shader).
		virtual bool GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 &outBinding, std::string &outBlockName, uint32 &outSize, std::map<std::string, uint32> &outOffsets) { return false; }

	protected:
		// Backing store for GetClearColor(). Every backend's
		// SetClearColor() override records the colour here as well as
		// applying it, so the getter needs no per-backend implementation.
		// Initialised to the value the backends themselves start at.
		Vec4 lastClearColor = Vec4(0.f, 0.f, 0.f, 1.f);

		// The last viewport handed to SetViewport(), which every backend
		// needs for scissoring: GL to flip the y, Vulkan and Metal to
		// restore a full-target rect when the scissor is turned off.
		uint32 lastViewport[4] = { 0, 0, 0, 0 };
	};

	// Texture/FrameBuffer/GeometryBuffer/Shader/RenderingComponent (the
	// resource-wrapper classes from Phase 3) are constructed all over the
	// engine - asset loading, mesh construction - with no IRenderer/device
	// reference available at most call sites, so each gets its own shared
	// instance via a file-local `static IRenderDevice& Device()` function
	// rather than an injected pointer. Historically that function just did
	// `static GLRenderDevice instance; return instance;`, silently
	// hardcoding GL regardless of what backend the actual IRenderer in use
	// was constructed with - harmless when GL was the only backend, a real
	// bug for Vulkan (shader compilation, geometry buffer creation etc
	// would keep going through GL even in a "Vulkan" build). These two
	// functions let whichever code constructs the "real" device for this
	// process (see IRenderer's backend-injection constructor) register it
	// here once, so every Device() accessor across the engine picks up the
	// SAME instance instead of each independently defaulting to its own
	// GLRenderDevice. GetActiveRenderDevice() falls back to a lazily
	// constructed GLRenderDevice if nothing has been registered yet,
	// preserving today's behavior for every example that doesn't opt into
	// backend injection.
	PYROS3D_API IRenderDevice& GetActiveRenderDevice();
	PYROS3D_API void SetActiveRenderDevice(IRenderDevice* device);

	// A second, narrower registry for handing *ownership* of a device from
	// whoever constructed it (e.g. SDL2VulkanContext, which needs a real
	// VulkanRenderDevice + swapchain to exist before any IRenderer does)
	// to whichever IRenderer::IRenderer(Width, Height, externalDevice=NULL)
	// call comes next - the every-example pattern of
	// `new ForwardRenderer(Width, Height)` never passes a device
	// explicitly, so without this there'd be no way for that call to know
	// a device already exists without editing every example's source.
	//
	// Deliberately NOT the same slot as SetActiveRenderDevice()'s
	// `activeDevice`: that one is read (never owned) by Shaders.cpp/
	// GeometryBuffer.cpp/RenderingComponent.cpp for the device's *entire*
	// lifetime via GetActiveRenderDevice(), which also falls back to a
	// lazily-constructed *static* GLRenderDevice when nothing is
	// registered - wrapping whatever that function returns in an owning
	// unique_ptr would eventually try to `delete` static storage, which
	// is memory corruption, not just a bug. TakeRenderDeviceOwnership()
	// instead returns NULL unless RegisterRenderDeviceForOwnership() was
	// called, and clears itself once taken - so it can only ever hand a
	// real, heap-allocated device to exactly one caller, once. A second,
	// unrelated IRenderer construction (e.g. a second Context/window, or
	// today's DebugRenderer/PostEffectsManager, which use the separate
	// true-no-arg IRenderer() constructor entirely and are untouched by
	// this) safely falls back to its own `new GLRenderDevice()`, exactly
	// as before this existed - EXCEPT this "safely" was never actually
	// safe for Vulkan: a "second, unrelated IRenderer" (VelocityRenderer,
	// PainterPick, and DeferredRenderer/PostEffectsManager's own internal
	// helper renderers - the IRenderer(Width, Height, externalDevice=NULL)
	// constructor, not the true-no-arg one) falling back to
	// `new GLRenderDevice()` in a Vulkan-only process (no real GL context
	// ever created, so every glad function pointer is NULL) crashes the
	// instant it makes any real GL call - confirmed via live crashes in
	// MotionBlurExample
	// (EXC_BAD_ACCESS in GLRenderDevice::CreateTextureObject, address 0x0).
	// IsActiveRenderDeviceSet() lets IRenderer's constructor distinguish
	// "nothing registered yet, `new GLRenderDevice()` really is correct"
	// (the original GL-only examples, still true) from "a real device is
	// already active, borrow *that* instead of creating a second, broken
	// one" - the fix threads through IRenderer.h's device member gaining
	// non-owning-borrow support (see its comment).
	PYROS3D_API bool IsActiveRenderDeviceSet();
	PYROS3D_API void RegisterRenderDeviceForOwnership(IRenderDevice* device);
	PYROS3D_API IRenderDevice* TakeRenderDeviceOwnership();

	// Shared by IRenderer and PostEffectsManager (and anything else that
	// resolves its own IRenderDevice the same way): a unique_ptr deleter
	// that can be told not to actually delete, so a "second, unrelated"
	// device-owning class can safely *borrow* an already-active device
	// (see IsActiveRenderDeviceSet() above) instead of always owning one -
	// without changing any existing `device->...` call site, since
	// unique_ptr<T, CustomDeleter> keeps the same operator-> / .get()
	// interface as unique_ptr<T>.
	struct MaybeOwningDeviceDeleter
	{
		bool owns = true;
		void operator()(IRenderDevice *d) const
		{
			if (!owns) return;
			// Clear the process-wide active pointer first if it is this
			// device. IRenderer's constructor publishes whatever device it
			// resolved via SetActiveRenderDevice(), and when it owns that
			// device it also frees it here - which used to leave activeDevice
			// dangling rather than NULL. IsActiveRenderDeviceSet() then
			// answered "yes" and every later caller went through a freed
			// vtable.
			//
			// That is not hypothetical ordering: Lua-owned objects are
			// finalized in whatever order sol's GC picks, so a script holding
			// both a renderer and a FrameBuffer routinely destroys the
			// renderer first, and the FrameBuffer's destructor then calls
			// Device().DestroyFramebuffer() on freed memory. Segfault on
			// every clean exit, with a garbage PC.
			if (IsActiveRenderDeviceSet() && &GetActiveRenderDevice() == d)
				SetActiveRenderDevice(NULL);
			delete d;
		}
	};
	typedef std::unique_ptr<IRenderDevice, MaybeOwningDeviceDeleter> MaybeOwningDevicePtr;

};

#endif /* IRENDERDEVICE_H */
