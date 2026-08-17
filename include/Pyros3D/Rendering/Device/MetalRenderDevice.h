//============================================================================
// Name        : MetalRenderDevice.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Native Metal IRenderDevice implementation - candidate
//               replacement for MoltenVK-translated Vulkan on Apple
//               platforms, scoped out of the Vulkan/MoltenVK segfault
//               investigation (see git history around
//               VulkanRenderDevice.cpp's BindFramebuffer() fix). Mirrors
//               VulkanRenderDevice.h's shape (same IRenderDevice contract,
//               same public/private split) but maps most of its Vulkan-
//               specific bookkeeping onto simpler Metal concepts - see
//               inline notes below for what collapses or disappears
//               entirely versus what still needs real design work.
//               Real so far (see MetalRenderDevice.mm): device/queue/layer
//               setup (BindToLayer) and ClearAndPresent() - this backend's
//               first-light milestone, mirroring VulkanRenderDevice's own
//               Step B/D bring-up order (see its ClearAndPresent()
//               comment). Every other IRenderDevice virtual has a stub
//               body only, not yet exercised by anything - the real
//               per-frame path (BeginFrame() + CreatePipeline()/
//               CreateBuffer()/DrawElements() + EndFrame()) is the next
//               milestone, not this one.
//
//               Objective-C++ note: this header is included from both plain
//               C++ (engine code that only ever sees an IRenderDevice*) and
//               the .mm implementation that actually touches Metal/Cocoa
//               types. Metal object types (id<MTLDevice> etc) are ARC-
//               managed Objective-C, not plain C handles like VkInstance/
//               VkBuffer - can't appear by-value in a header parsed by a
//               plain .cpp translation unit. Every Metal-typed member below
//               is therefore declared as `void*` here and `id<MTLXxx>` only
//               inside MetalRenderDevice.mm (same reasoning as
//               VulkanRenderDevice.h hiding volk's function-pointer loading
//               behind functions defined out-of-line in its own .cpp - see
//               WaitIdle()'s comment there - just forced here by the
//               language, not just link-time symbol scoping). Only built
//               when CMake finds Metal.framework + QuartzCore.framework
//               (Apple platforms only) - same opt-in pattern as
//               BUILD_VULKAN_BACKEND.
//============================================================================

#ifndef METALRENDERDEVICE_H
#define METALRENDERDEVICE_H

#ifdef METAL_BACKEND

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace p3d {

	class PYROS3D_API MetalRenderDevice : public IRenderDevice {

	public:

		// No instance-extension-list equivalent (Metal has no
		// instance/physical-device-selection step exposed the way Vulkan
		// does) - device selection is just MTLCreateSystemDefaultDevice(),
		// done inside the constructor. Surface/layer creation still needs
		// a real window, same reasoning as VulkanRenderDevice's
		// InitializeSwapchain() split - see BindToLayer() below.
		MetalRenderDevice();
		virtual ~MetalRenderDevice();

		// Second-phase init - binds this device to a CAMetalLayer the
		// windowing layer already created and sized (e.g. a new
		// SDL2MetalContext via SDL_Metal_CreateView(), mirroring
		// SDL2VulkanContext::CreateSurface()). Layer is `void*`
		// (CAMetalLayer*) for the same ARC/ObjC-in-a-C++-header reason as
		// the class comment above. Returns false on failure (no supported
		// GPU, layer already bound elsewhere).
		bool BindToLayer(void* metalLayer, const uint32 width, const uint32 height);

		// CAMetalLayer defaults to an sRGB colorspace while using a UNORM
		// pixel format: written bytes are treated as already-encoded, so
		// linear scene values (and ImGui::Image of an HDR capture) look
		// much darker than the same content on OpenGL. Call this with
		// true from the editor after BindToLayer so presentation treats
		// values as linear (matching GL). DemoLauncher must NOT enable
		// this - TonemapEffect already writes display-encoded LDR that
		// expects the default sRGB interpretation.
		// Pair with converting ImGui style colours sRGB→linear (see
		// Editor.cpp) so UI chrome stays the same brightness.
		void SetPresentAsLinear(const bool enabled);
		bool IsPresentLinear() const { return presentAsLinear; }

		void WaitIdle();
		bool IsVulkan() const { return false; }
		bool NeedsManualDisplayGamma() const { return !presentAsLinear; }

		// Minimal "hello window" frame loop - acquires the next drawable,
		// clears it to clearColor, and presents. Not part of IRenderDevice,
		// same reasoning as VulkanRenderDevice::ClearAndPresent() (see its
		// comment): exists so BindToLayer() has something concrete to
		// verify against before the real per-frame path (BeginFrame() +
		// CreatePipeline()/CreateBuffer()/DrawElements() + EndFrame(),
		// driven by IRenderer once those are real) exists. This is this
		// backend's actual first-light milestone - see MetalHelloWindow.
		bool ClearAndPresent(const Vec4 &clearColor);

		// ImGui-on-Metal, mirroring VulkanRenderDevice::InitImGuiVulkanBackend()/
		// NewImGuiVulkanFrame()/ShutdownImGuiVulkanBackend() - example code
		// (BaseExample.cpp/DemoLauncher.cpp) calls these three instead of
		// linking ImGui_ImplMetal_* itself, same reasoning as Vulkan's
		// comment (imgui_impl_metal.mm is compiled into this library, see
		// the root CMakeLists.txt), plus one Metal-specific wrinkle:
		// ImGui_ImplMetal_NewFrame() needs a render pass descriptor to read
		// pixel formats from, but the real swapchain one isn't built until
		// BeginFrame() - which hasn't run yet when DemoLauncher's
		// PrepareImGuiFrame() calls NewImGuiMetalFrame(), earlier in the
		// same frame. Defined in MetalImGuiBackend.mm, using a persistent
		// 1x1 dummy color texture + the real (also persistent) depthTexture
		// member so the format ImGui's pipeline bakes in always matches
		// what EndFrame() will actually draw into later this same frame.
		bool InitImGuiMetalBackend();
		void NewImGuiMetalFrame();
		void ShutdownImGuiMetalBackend();

		virtual CommandBufferHandle BeginCommandBuffer();
		virtual void EndCommandBuffer(const CommandBufferHandle cmd);
		virtual void BeginFrame();
		virtual void EndFrame();

		virtual uint32 TranslateBufferBit(const uint32 bufferBits);
		virtual void Clear(const uint32 nativeBufferBits);
		virtual void SetClearColor(const Vec4 &color);

		// Depth/stencil/blend/cull below: same split Vulkan already forces
		// on IRenderDevice - most of this is *pipeline* state on Metal too
		// (baked into MTLRenderPipelineState + a separate immutable
		// MTLDepthStencilState at CreatePipeline() time, not a live
		// gl*-style toggle), so these Set* methods mostly just stage
		// values into the in-progress PipelineDesc-equivalent the next
		// CreatePipeline() call reads, exactly like VulkanRenderDevice
		// already does. Scissor/viewport/depth-bias are the exception -
		// real per-encoder dynamic state on Metal too (setScissorRect:/
		// setViewport:/setDepthBias:slope:clamp:), same as Vulkan's
		// dynamic state array.
		virtual void SetDepthTest(const bool enabled, const uint32 mode);
		virtual void SetDepthMask(const bool enabled);
		virtual void PrepareDepthClear();

		virtual void SetStencilTestEnabled(const bool enabled);
		virtual void SetClearStencilValue();
		virtual void SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask);
		virtual void SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass);

		virtual void SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height);
		virtual void SetScissorTestEnabled(const bool enabled);

		// No Metal wireframe fill mode on its own triangle pipeline the
		// way GL_LINE polygon mode is - MTLTriangleFillMode::Lines exists
		// per-encoder (setTriangleFillMode:), so unlike most of this
		// group it's *not* pipeline-baked. Simpler than Vulkan
		// (VK_POLYGON_MODE_LINE requires a whole separate VkPipeline).
		virtual void SetWireFrame(const bool enabled);

		virtual void SetColorMask(const bool r, const bool g, const bool b, const bool a);

		virtual void SetPolygonOffsetEnabled(const bool enabled);
		virtual void SetPolygonOffset(const f32 factor, const f32 units);

		virtual void SetBlendingEnabled(const bool enabled);
		virtual void SetBlendFunction(const uint32 sfactor, const uint32 dfactor);
		virtual void SetBlendEquation(const uint32 mode);

		virtual void SetCullFaceMode(const uint32 cullFace);
		virtual void DisableCullFace();

		// CreatePipeline(): PipelineDesc::vertexLayout maps to
		// MTLVertexDescriptor (one MTLVertexBufferLayoutDescriptor per
		// VertexBufferLayoutDesc, one MTLVertexAttributeDescriptor per
		// VertexAttributeDesc - attribute *index*, not name lookup, same
		// as Vulkan already resolves via SpirvShaderCompiler::
		// ReflectStageInputs() rather than GL's glGetAttribLocation).
		// isShadowPass/noVertexInput: no render-pass-compatibility concept
		// to satisfy on Metal (a MTLRenderPipelineState just declares
		// pixel formats, checked against whatever MTLRenderPassDescriptor
		// is active when the encoder is created - format equality, not
		// object identity) - these two flags likely collapse to "which
		// pixelFormat(s)/depth format to declare" and nothing else. No
		// shadowPipelineRenderPass-equivalent template object needed.
		virtual DeviceHandle CreatePipeline(const PipelineDesc &desc);
		virtual void DestroyPipeline(const DeviceHandle pipeline);
		virtual void BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline);
		// No swapchain-recreation-vs-pipeline-cache-invalidation hazard to
		// report here (see IRenderDevice::GetSwapchainGeneration()'s
		// comment) - a Metal pipeline doesn't reference a render-pass
		// object at all, only pixel formats, so a drawable resize can't
		// invalidate one. Default (0, inherited) should already be
		// correct; kept virtual only in case CAMetalLayer format changes
		// turn out to need the same escape hatch.

		// Unlike a Vulkan swapchain, CAMetalLayer has no reactive OUT_OF_DATE/
		// SUBOPTIMAL-style signal at all - `drawableSize` is a plain
		// property nothing updates on its own just because the owning
		// NSView's frame changed. This is the only path that ever touches
		// it (see SDL2MetalContext::OnResize()'s comment) - a real,
		// required override, not the default no-op GL/most-Vulkan-callers
		// get.
		virtual void NotifySurfaceResized(const uint32 width, const uint32 height);

		virtual void EnableClipDistance(const uint32 index);
		virtual void DisableClipDistance(const uint32 index);

		virtual void SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height);

		// UseProgram/CreateVertexArray/BindVertexArray/BindArrayBuffer/
		// BindElementBuffer/SetVertexAttribute*: same fake-VAO shim
		// Vulkan already built (VaoRecord below is a straight copy - it
		// was never a Vulkan concept either, just "which buffers to bind
		// at draw time", see its comment in VulkanRenderDevice.h).
		// UseProgram is a bigger no-op here than even on Vulkan: a Metal
		// "program" is really two independent MTLFunctions (vertex/
		// fragment) already captured by the MTLRenderPipelineState at
		// BindPipeline() time, so there's no separate active-program
		// concept to set at all.
		virtual void UseProgram(const uint32 program);
		virtual DeviceHandle CreateVertexArray();
		virtual void DeleteVertexArray(const DeviceHandle vao);
		virtual void BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao);
		virtual void BindArrayBuffer(const uint32 buffer);
		virtual void BindElementBuffer(const uint32 buffer);
		virtual void SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset);
		virtual void SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset);
		virtual void DisableVertexAttribute(const int32 location);
		virtual void SetVertexAttributeDivisor(const int32 location, const uint32 divisor);
		// No descriptor-set/uniform-block-binding step to mirror - a
		// uniform buffer is just bound at a plain integer index
		// (setVertexBuffer:offset:atIndex:/setFragmentBuffer:...) exactly
		// like any vertex buffer, no separate VkDescriptorSetLayout/
		// VkDescriptorSet machinery to build first. Likely a no-op or a
		// small bookkeeping step (remember which index a name maps to,
		// from SPIRV-Cross's reflection output) rather than a real bind.
		virtual void BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle = 0);

		// Same NDC-correction seam as Vulkan (Z remap [-1,1]->[0,1], Y
		// flip) - Metal's clip space matches Vulkan's here (both left
		// MoltenVK's own NDC translation transparent to the app in
		// practice; MoltenVK itself just implements this exact remap
		// today). Likely close to a copy of VulkanRenderDevice's
		// implementation, not a new derivation.
		virtual Matrix TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip = false);
		virtual Matrix TranslateShadowBiasMatrix();

		virtual uint32 TranslateDrawType(const uint32 engineDrawType);
		virtual void DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count);
		virtual void DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount);
		virtual void DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount);

		// Buffers/uniform buffers: plain MTLBuffer throughout, no VMA
		// equivalent needed - Metal's own resource-options model
		// (storageModeShared/Managed/Private) already picks the right
		// memory type per platform, and on Apple Silicon's unified
		// memory a `storageModeShared` buffer's `.contents` pointer is
		// directly writable with no separate staging+transfer round trip
		// (compare VulkanRenderDevice's whole pendingStagingBuffers/
		// transferCommandBuffer batching machinery, added specifically to
		// amortize that round trip - likely unnecessary here). MapBuffer/
		// UnmapBuffer probably just return/no-op on `.contents` rather
		// than doing real map/unmap work.
		virtual DeviceHandle CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint);
		virtual void UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data);
		virtual void ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data);
		virtual void DestroyUniformBuffer(const DeviceHandle buffer);

		virtual DeviceHandle CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length);
		virtual void ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length);
		virtual void UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length);
		virtual void DestroyBuffer(const DeviceHandle buffer);
		virtual void *MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType);
		virtual void UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType);

		virtual uint32 TranslateAttributeType(const uint32 engineType);

		// Shader path: GLSL -> SPIR-V (already have this, via
		// SpirvShaderCompiler/shaderc for the Vulkan backend) -> MSL
		// source (new: SPIRV-Cross) -> MTLLibrary/MTLFunction via
		// newLibraryWithSource:options:completionHandler: (or
		// newLibraryWithData: if precompiling to .metallib offline
		// later - not needed for a first correct version). The loose-
		// uniform auto-UBO-wrap step (GetAutoUniformBlockLayout(),
		// AutoFixForVulkan()) is reusable as-is - Metal has exactly the
		// same "no glUniform equivalent, everything through buffers"
		// constraint Vulkan already forced.
		virtual std::string BuildShaderSource(const std::string &definitions, const std::string &shaderBody);
		virtual DeviceHandle CreateShaderStage(const uint32 engineShaderType);
		virtual bool CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog);
		virtual DeviceHandle CreateProgram();
		virtual void AttachShaderStage(const DeviceHandle program, const DeviceHandle shader);
		virtual bool LinkProgram(const DeviceHandle program, std::string &errorLog);
		virtual bool IsProgram(const DeviceHandle id);
		virtual bool IsShaderStage(const DeviceHandle id);
		virtual void DetachShaderStage(const DeviceHandle program, const DeviceHandle shader);
		virtual void DeleteShaderStage(const DeviceHandle shader);
		virtual void DeleteProgram(const DeviceHandle program);

		virtual int32 GetUniformLocation(const uint32 program, const std::string &name);
		virtual int32 GetAttributeLocation(const uint32 program, const std::string &name);
		virtual bool GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 &outBinding, std::string &outBlockName, uint32 &outSize, std::map<std::string, uint32> &outOffsets);

		virtual void SendUniformInt(const int32 handle, const int32 *data, const uint32 count);
		virtual void SendUniformFloat(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec2(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec3(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec4(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count);

		// engineDataType/engineTextureType -> MTLPixelFormat/MTLTextureType,
		// a table swap like Vulkan's VkFormat mapping, not new design.
		virtual void TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type);
		virtual void TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode);

		virtual void *GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType);
		virtual DeviceHandle CreateTextureObject();
		virtual void DestroyTextureObject(const DeviceHandle texture);
		virtual void BindTextureToTarget(const uint32 target, const DeviceHandle texture);

		// UploadTexture2D's willMipmap param (Vulkan-only today - GL
		// ignores it) becomes relevant here too: MTLTexture's mipmap
		// level count is also fixed at creation
		// (MTLTextureDescriptor::mipmapLevelCount), same constraint as
		// VkImage. GenerateMipmap has a direct built-in
		// (MTLBlitCommandEncoder::generateMipmapsForTexture:) - no
		// manual per-level blit loop to hand-write, unlike Vulkan's
		// GenerateMipmap() (see VulkanRenderDevice.cpp's manual blit
		// chain).
		virtual void UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap);
		virtual void UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height);
		virtual void GenerateMipmap(const uint32 target);

		// Sampler parameters: unlike GL (per-texture state) and like
		// Vulkan (separate VkSampler object), Metal groups all of this
		// into one immutable MTLSamplerState built from a
		// MTLSamplerDescriptor - these Set* calls almost certainly stage
		// into a small dirty-tracked descriptor per texture record and
		// (re)build the MTLSamplerState lazily, mirroring whatever
		// RebuildSamplerIfDirty()-equivalent the Vulkan backend already
		// has (same shape of problem, already solved once).
		virtual void SetTextureWrapS(const uint32 target, const uint32 engineRepeat);
		virtual void SetTextureWrapT(const uint32 target, const uint32 engineRepeat);
		virtual void SetTextureWrapR(const uint32 target, const uint32 engineRepeat);
		virtual void SetTextureMagFilter(const uint32 target, const uint32 engineFilter);
		virtual void SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap);
		virtual void SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel);
		virtual void SetTextureBorderColor(const uint32 target, const Vec4 &color);
		virtual void SetTextureCompareMode(const uint32 target);
		virtual void SetPixelUnpackAlignment(const uint32 value);

		// No real "active texture unit" concept on Metal (setFragmentTexture:
		// atIndex:/setFragmentSamplerState:atIndex: always take an explicit
		// index) - likely just bookkeeping so a later ActivateTextureUnit()+
		// BindTextureToTarget() pair (the GL-shaped calling convention every
		// existing call site still uses) resolves to the right index.
		virtual void ActivateTextureUnit(const uint32 unit);

		virtual void ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer);
		virtual uint32 GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height);

		// Framebuffers: this whole group is where Metal is structurally
		// simpler than Vulkan, not just differently-shaped - see the
		// FBORecord-equivalent note in the private section below.
		// GetCurrentRenderTarget()'s pipeline-cache-keying purpose still
		// applies (a MTLRenderPipelineState declares specific pixel
		// formats, so two differently-shaped targets still need two
		// pipelines) even though there's no render-pass *object* to key
		// against.
		virtual DeviceHandle GetCurrentRenderTarget();
		virtual DeviceHandle CreateFramebuffer();
		virtual void DestroyFramebuffer(const DeviceHandle fbo);
		virtual void SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve);
		virtual uint32 TranslateFramebufferAccess(const uint32 engineAccess);
		virtual void BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending);
		virtual uint32 TranslateFramebufferAttachment(const uint32 engineAttachmentFormat);
		virtual void AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound);
		virtual void AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer);
		virtual void SetDrawBufferNone();
		virtual void SetReadBufferNone();
		virtual void SetDrawBufferBack();
		virtual void SetReadBufferBack();
		virtual void SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices);
		virtual uint32 CheckFramebufferStatus();
		virtual uint32 TranslateFramebufferStatus(const uint32 nativeStatus);

		virtual DeviceHandle CreateRenderbuffer();
		virtual void DestroyRenderbuffer(const DeviceHandle rbo);
		virtual void BindRenderbuffer(const DeviceHandle rbo);
		virtual uint32 TranslateRenderbufferFormat(const uint32 engineDataType);
		virtual void RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height);
		virtual void RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height);

		virtual void SetMultisampleEnabled(const bool enabled);
		// Direct MTLBlitCommandEncoder copy (or a fullscreen-triangle draw
		// if a format/filter conversion is needed, same fallback GL's
		// blit-FBO-pair implementation already uses) - no manual
		// VkImageMemoryBarrier layout-transition dance either way, Metal's
		// default automatic hazard tracking covers read-after-write
		// between encoders without the caller doing anything.
		virtual void BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter);
		virtual void CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height);

	private:

		// void* (id<MTLDevice>/id<MTLCommandQueue> in the .mm) - see the
		// class comment on why Metal's ARC object types can't appear
		// typed in a header parsed by plain C++ translation units.
		void* device;
		void* commandQueue;
		void* metalLayer;      // CAMetalLayer* - set by BindToLayer()
		uint32 drawableWidth, drawableHeight;
		// MTLPixelFormat of the CAMetalLayer / swapchain pipelines - kept
		// in sync with BindToLayer() so CreatePipeline()'s no-FBO path
		// declares the same format the drawable actually has.
		uint32 swapchainPixelFormat;
		// See SetPresentAsLinear() - default false (CAMetalLayer sRGB).
		bool presentAsLinear;
		// One shared depth buffer sized to match the drawable - mirrors
		// VulkanRenderDevice's single swapchain-wide depthImage (see its
		// comment: only one frame's worth of swapchain rendering is ever
		// in flight against it). (Re)built by BindToLayer()/
		// NotifySurfaceResized() whenever drawableWidth/Height change.
		void* depthTexture; // id<MTLTexture>, nullable until BindToLayer() succeeds

		// Set by InitImGuiMetalBackend(), cleared by
		// ShutdownImGuiMetalBackend() - called from EndFrame() right before
		// ending the swapchain encoder, mirroring VulkanRenderDevice::
		// UIRenderHook's identical "last chance to record into the
		// still-open render pass" role. Takes the raw command
		// buffer/encoder (not id<MTLXxx> - this header is also parsed by
		// plain C++) since MetalImGuiBackend.mm, not this class's own .mm
		// file, is the one place that needs to know about ImGui at all.
		std::function<void(void* commandBuffer, void* encoder)> UIRenderHook;
		bool imguiMetalBackendActive;
		// 1x1 BGRA8Unorm/RenderTarget texture, real for the whole time
		// imguiMetalBackendActive is true - see NewImGuiMetalFrame()'s
		// comment on why ImGui needs *some* real texture's pixel format
		// before the real swapchain drawable exists this frame. Format
		// must match swapchainPixelFormat.
		void* imguiDummyColorTexture;

		// Frames-in-flight throttling: Apple's own documented pattern
		// (dispatch_semaphore_t, signalled from each MTLCommandBuffer's
		// addCompletedHandler:) replaces VulkanRenderDevice's
		// frameFences[MAX_FRAMES_IN_FLIGHT] + per-fence
		// vkWaitForFences/vkResetFences bookkeeping - one semaphore,
		// wait before encoding frame N, signal in frame N's completion
		// handler, no manual signaled/unsignaled state machine to get
		// wrong (compare today's offscreenFenceInFlight/
		// offscreenSubmitPending pair, which is exactly the kind of
		// hand-rolled state this sidesteps).
		void* frameBoundarySemaphore; // dispatch_semaphore_t
		static const uint32 MAX_FRAMES_IN_FLIGHT = 2;

		// Current frame's command buffer + active render command encoder,
		// if any is open - the offscreenCommandBuffer/frameCommandBuffer
		// split VulkanRenderDevice needs (two separate VkCommandBuffers,
		// since a shadow pass must submit+wait *before* the swapchain
		// frame's own command buffer exists) likely isn't needed at all:
		// nothing stops recording shadow-map passes and the swapchain
		// pass into encoders on the *same* MTLCommandBuffer, submitted
		// together once at EndFrame() - Metal's automatic hazard tracking
		// handles the shadow-map-write-then-sample ordering within one
		// command buffer for free. If that turns out not to hold once
		// real multi-light scenes are tested, the offscreen/frame split
		// can be reintroduced later - starting from "one command buffer"
		// and adding a second only if something demands it, not the
		// other way around.
		void* currentCommandBuffer;   // id<MTLCommandBuffer>
		void* currentRenderEncoder;   // id<MTLRenderCommandEncoder>
		void* currentDrawable;        // id<CAMetalDrawable>, held until present
		// Most recently submitted command buffer, kept only so WaitIdle()
		// has something to call waitUntilCompleted on - simpler than
		// draining/restoring frameBoundarySemaphore by MAX_FRAMES_IN_FLIGHT
		// to get the same "block until GPU is idle" guarantee.
		void* lastSubmittedCommandBuffer; // id<MTLCommandBuffer>, nullable

		// Same fake-VAO shim as VulkanRenderDevice::VaoRecord - copied,
		// not redesigned (see BindVertexArray()'s comment above).
		struct VaoRecord
		{
			std::vector<DeviceHandle> vertexBuffers;
			DeviceHandle indexBuffer;
			VaoRecord() : indexBuffer(0) {}
		};
		std::map<DeviceHandle, VaoRecord> vaos;
		DeviceHandle nextVaoHandle;
		DeviceHandle currentVao;
		DeviceHandle currentPipeline;

		// GLSL -> SPIR-V -> MSL, reusing the exact same GLSL->SPIR-V step
		// (SpirvShaderCompiler/shaderc) the Vulkan backend already has -
		// only the last step (SPIR-V -> MSL source via SPIRV-Cross, then
		// MSL source -> MTLLibrary/MTLFunction) is Metal-specific. spirv
		// is kept per stage (not just the compiled MTLFunction) because
		// LinkProgram() reflects both stages together the same way
		// VulkanRenderDevice::LinkProgram() does.
		struct ShaderStageRecord
		{
			uint32 engineShaderType;
			std::vector<uint32> spirv;
			void* function; // id<MTLFunction>
			// Engine UBO binding -> actual MSL [[buffer(N)]] index this
			// stage's compiled MTLFunction reads from, populated only for
			// bindings CompileShaderStage() had to remap (see its comment) -
			// absent means "MSL buffer index equals the engine binding",
			// still true for every PyrosShader.glsl material (0-23).
			std::map<uint32, uint32> highBindingRemap;
			// Engine sampler binding -> base MSL [[sampler(N)]] /
			// [[texture(N)]] index. Both tables are compacted in
			// declaration order, advancing by the resource's array size:
			// `array<texturecube, 4> [[texture(10)]]` reserves slots
			// 10..13, so a following `array<depth2d, 4>` cannot also start
			// at engine binding 11 (that was the GenericMaterial
			// Diffuse+Point+Spot shadow failure - newLibraryWithSource
			// rejected overlapping texture/sampler ranges). Samplers are
			// additionally capped at 16 entries per stage
			// (uMetallicRoughnessmap's engine binding 16 already exceeds
			// that without compaction). Per-stage, not per-program.
			std::map<uint32, uint32> samplerIndexRemap;
			std::map<uint32, uint32> textureIndexRemap;
			// See GetAutoUniformBlockLayout()'s comment - populated only
			// when CompileShaderStage() had to run AutoFixForVulkan()
			// (loose, non-layout-qualified uniforms - CustomShaderMaterial
			// shaders like particleSystem.glsl) on this stage's source.
			bool autoUboHasBlock;
			uint32 autoUboBinding;
			std::string autoUboBlockName;
			uint32 autoUboSize;
			std::map<std::string, uint32> autoUboOffsets;
			ShaderStageRecord() : engineShaderType(0), function(NULL), autoUboHasBlock(false), autoUboBinding(0), autoUboSize(0) {}
		};
		std::map<DeviceHandle, ShaderStageRecord> shaderStages;
		DeviceHandle nextShaderStageHandle;
		// See VulkanRenderDevice::nextAutoUboBinding's identical comment -
		// starts well above kFirstVertexBufferIndex so every synthesized
		// AutoFix UBO always takes the highBindingRemap path. Shares
		// kFirstAutoUboBinding with IsPerObjectDynamicBinding() below,
		// not a coincidence - that function's own >= kFirstAutoUboBinding
		// fallback already anticipated this counter existing.
		uint32 nextAutoUboBinding;

		// No VkDescriptorSetLayout/VkPipelineLayout equivalent to build here
		// (see BindUniformBlockIfPresent()'s comment) - LinkProgram() only
		// needs to reflect attribute locations (for CreatePipeline()'s
		// MTLVertexDescriptor) and which binding indices exist / which
		// stage(s) use each (for DrawElements()'s setVertexBuffer/
		// setFragmentBuffer calls, done fresh per draw instead of once into
		// a cached descriptor set - see the comment there for why that's
		// simpler here, not just differently-shaped).
		struct ProgramRecord
		{
			DeviceHandle vertexShader, fragmentShader;
			std::map<std::string, uint32> attributeLocations;
			// bit0 = used by vertex stage, bit1 = used by fragment stage.
			std::map<uint32, uint32> bindingStageMask;
			// Sampler name -> reflected binding index, and which stage(s)
			// use it - GetUniformLocation()/SendUniformInt()'s mechanism,
			// see their comments. Separate from bindingStageMask/UBOs above:
			// samplers and buffers are different MSL argument tables
			// (setFragmentTexture: vs setFragmentBuffer:), so a texture and
			// a UBO can validly share the same *numeric* binding without
			// colliding (matches PyrosShader.glsl's own numbering, which
			// interleaves SAMPLER_BINDING and UBO_BINDING values freely -
			// see that file's #define block).
			std::map<std::string, uint32> samplerBindings;
			std::map<uint32, uint32> samplerStageMask;
			// Merged from both stages' ShaderStageRecord::highBindingRemap -
			// see its comment. BindProgramUniformBuffers() consults this
			// instead of assuming "MSL buffer index == engine binding"
			// whenever a binding appears here.
			std::map<uint32, uint32> highBindingMslIndex;
			// Each stage's ShaderStageRecord::samplerIndexRemap /
			// textureIndexRemap, kept separate (index 0 = vertex, 1 =
			// fragment) rather than merged the way highBindingMslIndex is:
			// MSL's sampler/texture argument tables are per-stage, so the
			// same engine binding legitimately maps to a different slot in
			// each stage, and merging them would make one stage bind into
			// the other's slot.
			std::map<uint32, uint32> samplerMslIndex[2];
			std::map<uint32, uint32> textureMslIndex[2];
			// Engine binding -> declared array length (1 if not an array).
			// SendUniformInt() needs this to fill every MSL array element
			// and to stride consecutive texture/sampler slots - same role
			// as VulkanRenderDevice::ProgramRecord::samplerArraySizes.
			std::map<uint32, uint32> samplerArraySizes;
			ProgramRecord() : vertexShader(0), fragmentShader(0) {}
		};
		std::map<DeviceHandle, ProgramRecord> programs;
		DeviceHandle nextProgramHandle;

		// One MTLBuffer per handle (void* -> id<MTLBuffer> in the .mm).
		// No VMA/allocator member at all - MTLDevice's own
		// newBufferWithLength:options:/newBufferWithBytes:... calls are
		// the allocator; see CreateBuffer()'s comment on why the usual
		// staging-buffer path is likely unnecessary here.
		// alignedSlotSize/slotCount/currentSlot: see IsPerObjectDynamicBinding()'s
		// comment - a per-object UBO (ObjectMatrixUniforms etc) gets
		// rewritten once per object before the GPU has executed *any* of
		// this frame's draws, so a single-slot buffer means every draw
		// reads whichever object wrote last (confirmed: a real,
		// reproducible bug on the Vulkan backend before it gained the
		// identical fix this ports - "a two-object scene... reproducibly
		// renders only the second object"). isDynamicUniform buffers get
		// slotCount slots instead of 1; ReplaceUniformBuffer() advances
		// currentSlot before writing, and BindProgramUniformBuffers()
		// binds at that slot's byte offset instead of 0 - Metal's
		// setBuffer:offset:atIndex: takes a plain byte offset into one
		// larger buffer, direct equivalent of Vulkan's dynamic-descriptor
		// offset for the exact same problem.
		struct BufferRecord
		{
			void* buffer; // id<MTLBuffer>
			uint32 length;
			bool isDynamicUniform;
			uint32 alignedSlotSize;
			uint32 slotCount;
			uint32 currentSlot;
			BufferRecord() : buffer(NULL), length(0), isDynamicUniform(false), alignedSlotSize(0), slotCount(1), currentSlot(0) {}
		};
		std::map<DeviceHandle, BufferRecord> buffers;
		DeviceHandle nextBufferHandle;
		// Same binding-number convention as VulkanRenderDevice::
		// IsPerObjectDynamicBinding() (copied, not shared - see this
		// class's own file for why sharing one method across two
		// unrelated device classes isn't worth it for a handful of
		// constants) - these are PyrosShader.glsl's own UBO_BINDING
		// numbers, an engine-wide convention independent of backend.
		static bool IsPerObjectDynamicBinding(const uint32 bindingPoint);
		static const uint32 kFirstAutoUboBinding = 43;
		static const uint32 kMaxDynamicUboSlots = 65536;
		// Which buffer handle currently occupies a given uniform binding
		// index, globally - set by CreateUniformBuffer() (bindingPoint is a
		// stable, project-wide convention the same way it already is on
		// the Vulkan/GL side, e.g. "binding 8 is always this material's
		// MVP block"). DrawElements()/DrawElementsInstanced() read this
		// fresh per draw for whichever bindings the bound program's
		// ProgramRecord::bindingStageMask says it actually uses.
		std::map<uint32, DeviceHandle> uniformBufferByBindingPoint;
		// MSL shares one buffer-index namespace (0..30, Metal's guaranteed
		// minimum per stage) between per-vertex attribute buffers and
		// UBOs - unlike Vulkan, where they're different descriptor sets/
		// binding domains that can't collide by construction. PyrosShader.glsl's
		// own UBO_BINDING numbering (BIND_GlobalMatrices=0 through
		// BIND_ObjectLightCounts=23 - see that file's #define block) already
		// spans most of the low range, so vertex attribute buffers are
		// placed *above* it instead of at 0..N - the reverse of the more
		// obvious-looking choice, but the shader's own numbering isn't
		// engine-negotiable (Vulkan already depends on it) while this
		// offset is purely an internal MetalRenderDevice convention.
		// 24..30 (7 slots) is comfortably more than any mesh this engine
		// builds needs (almost always 1, occasionally 2 - e.g. an
		// instanced per-object transform buffer).
		static const uint32 kFirstVertexBufferIndex = 24;

		// Metal's per-stage sampler argument table is 16 entries
		// ([[sampler(0)]]..[[sampler(15)]]); the texture table allows 128.
		// See CompileShaderStage()'s resource remap (both advance by array
		// size so shadow-map arrays don't overlap).
		static const uint32 kMaxMslSamplersPerStage = 16;
		static const uint32 kMaxMslTexturesPerStage = 128;

		// One MTLTexture per handle, plus the small dirty-tracked sampler
		// descriptor mentioned on the Set*Filter/Wrap methods above -
		// deliberately no separate "TextureRecord::view"-style field the
		// way VulkanRenderDevice needs (GetOrCreateRenderTargetView() and
		// its per-nativeTextureTarget VkImageView cache exist only
		// because a VkImageView is a distinct object from its VkImage;
		// MTLTexture already *is* both, framebuffers attach the texture
		// directly, so there's nothing separate to lazily build/cache
		// here at all).
		struct TextureRecord
		{
			void* texture; // id<MTLTexture>
			uint32 width, height;
			uint32 samples;
			bool isCubemap;
			bool hasMipmap;    // requested (Texture::Mipmapping) - see mipsGenerated
			bool mipsGenerated; // GenerateMipmap() actually ran since the last (re)upload
			// Wrap/filter/compare state, applied lazily into samplerState -
			// see RebuildSamplerIfDirty()'s comment (mirrors
			// VulkanRenderDevice::TextureRecord's identical split: GL applies
			// these immediately per glTexParameter* call, Metal/Vulkan both
			// bake them into one immutable sampler object instead).
			uint32 wrapS, wrapT;
			uint32 minFilter, magFilter;
			bool compareModeEnabled;
			bool samplerDirty;
			void* samplerState; // id<MTLSamplerState>, rebuilt when samplerDirty
			TextureRecord()
				: texture(NULL), width(0), height(0), samples(1), isCubemap(false),
				  hasMipmap(false), mipsGenerated(false),
				  wrapS(TextureRepeat::Repeat), wrapT(TextureRepeat::Repeat),
				  minFilter(TextureFilter::Linear), magFilter(TextureFilter::Linear),
				  compareModeEnabled(false), samplerDirty(true), samplerState(NULL) {}
		};
		std::map<DeviceHandle, TextureRecord> textures;
		DeviceHandle nextTextureHandle;
		// Dual-purpose BindTextureToTarget() state machine, copied from
		// VulkanRenderDevice (see its identical comment on BindTextureToTarget()) -
		// most calls just select which texture subsequent Upload/SetWrap*/
		// SetFilter* calls configure (currentlyConfiguringTexture), but a
		// call immediately following ActivateTextureUnit() instead means
		// "this texture is what unit N should read from at render time"
		// (textureUnitBindings), consumed later by SendUniformInt() once
		// the material tells this backend which reflected sampler binding
		// that unit belongs to (see GetUniformLocation()'s comment for the
		// rest of this mechanism - unlike GL, a Metal/Vulkan sampler has no
		// real "location" to query, so GetUniformLocation() repurposes it
		// to mean "this name's reflected binding index" instead).
		uint32 currentTextureUnit;
		bool unitJustActivated;
		std::map<uint32, DeviceHandle> textureUnitBindings;
		DeviceHandle currentlyConfiguringTexture;
		// TranslateTextureTarget()'s cubemap-face sentinel - same value and
		// same reasoning as VulkanRenderDevice::CUBEMAP_FACE_TARGET_BASE
		// (an engine-wide "distinguish targets, not a real API token"
		// convention both backends independently need).
		static const uint32 kCubemapFaceTargetBase = 100;
		// Lazily (re)builds a texture's MTLSamplerState from its currently-
		// tracked wrap/filter/compare state - mirrors
		// VulkanRenderDevice::RebuildSamplerIfDirty()'s identical reasoning
		// (GL applies these immediately; Metal bakes them into one
		// immutable object instead, so this only runs when something
		// actually changed since the last build).
		bool RebuildSamplerIfDirty(TextureRecord &tex);

		// FBORecord's Metal equivalent - deliberately tiny compared to
		// VulkanRenderDevice's version. No VkRenderPass/VkFramebuffer
		// equivalent at all: BindFramebuffer()/AttachFramebufferTexture2D()
		// just record which TextureRecord handle is attached at which
		// slot into this struct; BeginRenderEncoderForCurrentTarget()
		// (private helper, see below) builds a fresh MTLRenderPassDescriptor
		// from these texture handles *every time* a render encoder opens
		// for this FBO - no persistent object to cache, invalidate on
		// resize (no InvalidateFramebuffersForTexture() equivalent
		// needed at all), or accidentally destroy out from under a
		// still-recording command buffer (the exact bug class fixed in
		// VulkanRenderDevice::BindFramebuffer() - see its comment history -
		// structurally can't recur here).
		//
		// Each attachment is recorded as its texture + target together -
		// target carries the same TranslateTextureTarget() sentinel
		// UploadTexture2D()/BindTextureToTarget() already use (1 = plain
		// 2D, kCubemapFaceTargetBase+faceIndex = one face of a cubemap,
		// e.g. a point light's shadow map) - MTLRenderPassAttachmentDescriptor's
		// `slice` property (faceIndex, 0 for a plain 2D texture) is how
		// Metal points a render pass at one face of a cube texture,
		// directly, with no separate single-face texture *view* object
		// needed the way some other operations require.
		struct FBOAttachmentRef
		{
			DeviceHandle texture;
			uint32 target;
			FBOAttachmentRef() : texture(0), target(1) {}
		};
		struct FBORecord
		{
			std::map<uint32, FBOAttachmentRef> colorAttachments; // slot -> attachment
			FBOAttachmentRef depthAttachment; // depthAttachment.texture == 0 means "none"
			bool preserveDepth;
			FBORecord() : preserveDepth(false) {}
		};
		std::map<DeviceHandle, FBORecord> fboRecords;
		DeviceHandle nextFBOHandle;
		DeviceHandle currentBoundFBO;
		DeviceHandle currentReadFBO;

		// Ends the currently-open render encoder (if any) - the nearest
		// equivalent of EndOffscreenRenderPassIfOpen()/vkCmdEndRenderPass,
		// needed for the same reason: Metal can't retarget an encoder's
		// attachments mid-pass either, a new target means a new encoder.
		void EndCurrentRenderEncoderIfOpen();
		// Builds a fresh MTLRenderPassDescriptor from `fbo` (or the
		// drawable, for fbo==0) and begins a new render command encoder
		// on currentCommandBuffer. No "finalize pending attachments"
		// step to mirror (see AttachFramebufferTexture2D()'s comment
		// above) - there's no render-pass object to defer building, so
		// every attach just updates FBORecord and the *next* real bind
		// builds the descriptor from whatever's there right then.
		void BeginRenderEncoderForTarget(const DeviceHandle fbo);
		// (Re)allocates depthTexture at drawableWidth x drawableHeight -
		// called by BindToLayer()/NotifySurfaceResized() (both know the
		// new size already). Depth32Float, private storage (never sampled
		// or read back on this milestone's swapchain path).
		void RebuildDepthTexture();
		// Binds whatever buffer currently occupies each of `programHandle`'s
		// reflected UBO bindings onto currentRenderEncoder, for whichever
		// stage(s) ProgramRecord::bindingStageMask says use it - the
		// per-draw equivalent of Vulkan's cached-descriptor-set bind (see
		// BindUniformBlockIfPresent()'s comment for why this is fresh
		// per-draw instead of write-once). Called from DrawArrays()/
		// DrawElements()/DrawElementsInstanced() right before the actual
		// draw call, mirroring VulkanRenderDevice's identical placement/
		// reasoning (a material's PreRender() may still be writing uniform
		// data between BindPipeline() and here).
		void BindProgramUniformBuffers(const DeviceHandle programHandle);

		// programHandle kept alongside the compiled pipeline state so
		// BindPipeline()/DrawElements() know which ProgramRecord's
		// attribute/binding reflection applies to whatever's currently
		// bound - a Vulkan VkPipeline has no equivalent back-reference
		// need (its descriptor set is looked up via ProgramRecord
		// directly, not through the pipeline), but this backend binds
		// buffers fresh per draw (see the comment on
		// uniformBufferByBindingPoint) and needs to know *which*
		// program's bindings to walk.
		struct PipelineRecord
		{
			void* pipelineState;     // id<MTLRenderPipelineState>
			void* depthStencilState; // id<MTLDepthStencilState>
			DeviceHandle programHandle;
			// Buffer index each VaoRecord::vertexBuffers[i] should bind at -
			// currently always i itself (see DrawElements()'s comment on
			// the shared vertex-buffer/UBO index namespace), kept as an
			// explicit field rather than an implicit assumption so a
			// future multi-buffer-layout pipeline has somewhere to record
			// a different assignment without changing DrawElements()'s
			// contract.
			uint32 vertexBufferCount;
			PipelineRecord() : pipelineState(NULL), depthStencilState(NULL), programHandle(0), vertexBufferCount(0) {}
		};
		std::map<DeviceHandle, PipelineRecord> pipelines;
		DeviceHandle nextPipelineHandle;

		// Persistent pipeline cache equivalent - MTLBinaryArchive
		// (macOS 11+) mirrors VkPipelineCache's "save compiled variants,
		// load them back next run" job closely enough to likely reuse
		// VulkanRenderDevice::PyrosCacheDir()'s on-disk location as-is.
		void* pipelineArchive; // id<MTLBinaryArchive>, nullable - absent on older OS versions

		Vec4 pendingClearColor;
		bool frameInProgress;

	};

};

#endif /* METAL_BACKEND */

#endif /* METALRENDERDEVICE_H */
