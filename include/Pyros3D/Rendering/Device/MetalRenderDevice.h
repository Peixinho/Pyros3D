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
#include <vector>
#include <map>
#include <string>

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

		void WaitIdle();
		bool IsVulkan() const { return false; }

		// Minimal "hello window" frame loop - acquires the next drawable,
		// clears it to clearColor, and presents. Not part of IRenderDevice,
		// same reasoning as VulkanRenderDevice::ClearAndPresent() (see its
		// comment): exists so BindToLayer() has something concrete to
		// verify against before the real per-frame path (BeginFrame() +
		// CreatePipeline()/CreateBuffer()/DrawElements() + EndFrame(),
		// driven by IRenderer once those are real) exists. This is this
		// backend's actual first-light milestone - see MetalHelloWindow.
		bool ClearAndPresent(const Vec4 &clearColor);

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
		virtual void BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint);

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

		// One MTLBuffer per handle (void* -> id<MTLBuffer> in the .mm).
		// No VMA/allocator member at all - MTLDevice's own
		// newBufferWithLength:options:/newBufferWithBytes:... calls are
		// the allocator; see CreateBuffer()'s comment on why the usual
		// staging-buffer path is likely unnecessary here.
		struct BufferRecord
		{
			void* buffer; // id<MTLBuffer>
			uint32 length;
			bool isDynamicUniform;
			uint32 writesThisFrame;
			BufferRecord() : buffer(NULL), length(0), isDynamicUniform(false), writesThisFrame(0) {}
		};
		std::map<DeviceHandle, BufferRecord> buffers;
		DeviceHandle nextBufferHandle;

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
			bool samplerDirty;
			void* samplerState; // id<MTLSamplerState>, rebuilt when samplerDirty
			TextureRecord() : texture(NULL), width(0), height(0), samples(1), samplerDirty(true), samplerState(NULL) {}
		};
		std::map<DeviceHandle, TextureRecord> textures;
		DeviceHandle nextTextureHandle;
		DeviceHandle currentActiveTextureUnit;

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
		struct FBORecord
		{
			std::map<uint32, DeviceHandle> colorAttachments; // slot -> texture handle
			DeviceHandle depthAttachment;
			bool preserveDepth;
			FBORecord() : depthAttachment(0), preserveDepth(false) {}
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

		std::map<DeviceHandle, void*> pipelines; // id<MTLRenderPipelineState>
		DeviceHandle nextPipelineHandle;
		std::map<DeviceHandle, void*> depthStencilStates; // id<MTLDepthStencilState>, keyed same as pipelines

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
