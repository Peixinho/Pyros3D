//============================================================================
// Name        : VulkanRenderDevice.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IRenderDevice implementation backed by Vulkan. Vulkan
//               roadmap Phase 5 - see VULKAN_ROADMAP.md. Real so far:
//               instance creation (Step B), device selection/logical
//               device/swapchain creation (Step D, via
//               InitializeSwapchain() - deliberately not part of the
//               constructor, since a VkSurfaceKHR can only be created once
//               a window exists, and this class must not depend on any
//               particular windowing library - see the comment on
//               InitializeSwapchain() below). Also real: buffer/uniform
//               buffer creation via VMA, and shader compilation (GLSL ->
//               SPIR-V via SpirvShaderCompiler -> VkShaderModule). Still
//               unimplemented: CreatePipeline (needs a real VkRenderPass
//               and descriptor set layout, deferred until IRenderer
//               actually drives per-frame rendering through this device -
//               see VULKAN_ROADMAP.md), and everything texture/framebuffer
//               related (out of scope for RotatingCube's texture-free
//               validation path).
//               Only built when CMake finds the Vulkan SDK + vulkan-volk
//               (see CMakeLists.txt's BUILD_VULKAN_BACKEND option); the
//               whole header is compiled out otherwise so a build without
//               the toolchain never sees a declaration it can't link
//               against - same pattern as ShaderCompiler.h/SPIRV_TOOLING.
//============================================================================

#ifndef VULKANRENDERDEVICE_H
#define VULKANRENDERDEVICE_H

#ifdef VULKAN_BACKEND

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <map>
#include <set>

namespace p3d {

	class PYROS3D_API VulkanRenderDevice : public IRenderDevice {

	public:

		// requiredInstanceExtensions: whatever the windowing layer says it
		// needs to later create a VkSurfaceKHR against this instance (e.g.
		// SDL2VulkanContext::GetRequiredInstanceExtensions()) - this class
		// takes them as plain strings rather than depending on any
		// particular windowing library itself (engine code must not depend
		// on examples/ code). Pass an empty vector for the no-window,
		// compile/link-verification-only use case Step B's smoke test used.
		VulkanRenderDevice(const std::vector<const char*> &requiredInstanceExtensions = std::vector<const char*>());
		virtual ~VulkanRenderDevice();

		VkInstance GetInstance() const { return instance; }

		// Blocks until every submitted command on this device's queues has
		// finished executing. The destructor already calls this before
		// tearing anything down, but any caller destroying resources
		// (pipelines, buffers, programs) *between* a DrawFrame()/
		// ClearAndPresent() call and shutdown needs to call this first
		// too - frameFence only proves the *previous* frame's submission
		// completed (waited on at the top of the *next* DrawFrame()/
		// ClearAndPresent() call), not the most recent one, so destroying
		// anything right after the last frame without this is a real
		// use-after-free-on-the-GPU risk, not just a validation-layer
		// nitpick (caught the hard way via
		// VUID-vkDestroyPipeline-pipeline-00765/VUID-vkDestroyBuffer-buffer-00922
		// in this session's own smoke test). Deliberately declared here
		// but *defined in the .cpp*, not inline - every Vulkan-calling
		// method on this class must be, since volk's function pointers
		// (e.g. vkDeviceWaitIdle) are plain global variables populated by
		// volkLoadDevice() inside VulkanRenderDevice.cpp's translation
		// unit (compiled into libPyrosEngine.dylib); an inline definition
		// here would get its own out-of-line copy compiled into whatever
		// *other* binary includes this header (an example, a test), which
		// would reference *that* binary's own, separate, never-loaded
		// copy of the same global symbol - crashed with a wild-pointer
		// call the hard way discovering this (see VULKAN_ROADMAP.md).
		void WaitIdle();

		// Second-phase init, deliberately separate from the constructor:
		// selects a physical device compatible with the given surface,
		// creates the logical device + queues, and creates a swapchain
		// sized width x height. The caller creates the VkSurfaceKHR itself
		// (e.g. via SDL2VulkanContext::CreateSurface(GetInstance(), &surface))
		// since surface creation is inherently windowing-library-specific -
		// this class only ever touches the resulting VkSurfaceKHR handle,
		// never the window. Returns false (and leaves this device without a
		// swapchain) on failure.
		bool InitializeSwapchain(VkSurfaceKHR surface, const uint32 width, const uint32 height);

		// Minimal "hello window" frame loop - acquires the next swapchain
		// image, clears it to clearColor, and presents. Not part of
		// IRenderDevice (no GL equivalent, and IRenderer doesn't drive
		// per-frame swapchain acquire/submit/present yet - see
		// VULKAN_ROADMAP.md Phase 5's "next" section) - exists so
		// InitializeSwapchain() has something concrete to verify against
		// besides "the calls didn't return an error".
		bool ClearAndPresent(const Vec4 &clearColor);

		// Real render-pass-based indexed draw, the "hello cube" milestone
		// - acquires the next swapchain image, begins the render pass
		// built in InitializeSwapchain() (clearing color+depth), binds
		// `pipeline` and its program's descriptor set (see
		// BindUniformBlockIfPresent()), binds `vertexBuffer`/`indexBuffer`
		// (as produced by GeometryBuffer/CreateBuffer()), draws
		// `indexCount` indices, ends the render pass, and presents. Not
		// part of IRenderDevice for the same reason ClearAndPresent()
		// isn't (see its comment) - IRenderer doesn't drive a real
		// per-frame command buffer through this backend yet.
		bool DrawFrame(const DeviceHandle pipeline, const DeviceHandle program, const DeviceHandle vertexBuffer, const DeviceHandle indexBuffer, const uint32 indexCount, const Vec4 &clearColor);

		// Diagnostic-only, not part of IRenderDevice: requests that the
		// *next* EndFrame() call copy the frame it just rendered into
		// host-readable memory (retrieved afterward via GetCapturedFrame())
		// before presenting it - so a caller without a real display (this
		// environment can't screenshot) can verify actual pixel content
		// instead of only "no crash, no validation error". Deliberately
		// captures *before* presenting, not after: a presentable image's
		// contents belong to the presentation engine from vkQueuePresentKHR()
		// until it's reacquired (which isn't guaranteed to hand back the
		// same image), so reading it back post-present is invalid - this
		// was tried the naive way first and caught by validation
		// (VUID-VkImageMemoryBarrier-oldLayout-01212 and
		// UNASSIGNED-non-acquired-swapchain-image-used) before being fixed
		// to capture pre-present instead, within the same frame's already-
		// open command buffer and submission.
		void RequestFrameCapture();
		// Returns the pixels captured by the most recent RequestFrameCapture()
		// + EndFrame() pair, as tightly-packed 8-bit-per-channel RGBA
		// (outRedByteOffset is 0 or 2 depending on the swapchain's actual
		// channel order - B8G8R8A8 is the common case). False if no
		// capture has completed yet.
		bool GetCapturedFrame(std::vector<uint8_t> &outPixels, uint32 &outWidth, uint32 &outHeight, uint32 &outRedByteOffset);

		virtual CommandBufferHandle BeginCommandBuffer();
		virtual void EndCommandBuffer(const CommandBufferHandle cmd);
		virtual void BeginFrame();
		virtual void EndFrame();

		virtual uint32 TranslateBufferBit(const uint32 bufferBits);
		virtual void Clear(const uint32 nativeBufferBits);
		virtual void SetClearColor(const Vec4 &color);

		virtual void SetDepthTest(const bool enabled, const uint32 mode);
		virtual void SetDepthMask(const bool enabled);
		virtual void PrepareDepthClear();

		virtual void SetStencilTestEnabled(const bool enabled);
		virtual void SetClearStencilValue();
		virtual void SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask);
		virtual void SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass);

		virtual void SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height);
		virtual void SetScissorTestEnabled(const bool enabled);

		virtual void SetWireFrame(const bool enabled);

		virtual void SetColorMask(const bool r, const bool g, const bool b, const bool a);

		virtual void SetPolygonOffsetEnabled(const bool enabled);
		virtual void SetPolygonOffset(const f32 factor, const f32 units);

		virtual void SetBlendingEnabled(const bool enabled);
		virtual void SetBlendFunction(const uint32 sfactor, const uint32 dfactor);
		virtual void SetBlendEquation(const uint32 mode);

		virtual void SetCullFaceMode(const uint32 cullFace);
		virtual void DisableCullFace();

		virtual DeviceHandle CreatePipeline(const PipelineDesc &desc);
		virtual void DestroyPipeline(const DeviceHandle pipeline);
		virtual void BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline);

		virtual void EnableClipDistance(const uint32 index);
		virtual void DisableClipDistance(const uint32 index);

		virtual void SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height);

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
		virtual void BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint);

		virtual Matrix TranslateProjectionMatrix(const Matrix &projectionMatrix);

		virtual uint32 TranslateDrawType(const uint32 engineDrawType);
		virtual void DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count);
		virtual void DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount);
		virtual void DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount);

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

		virtual void SendUniformInt(const int32 handle, const int32 *data, const uint32 count);
		virtual void SendUniformFloat(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec2(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec3(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec4(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count);

		virtual void TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type);
		virtual void TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode);

		virtual DeviceHandle CreateTextureObject();
		virtual void DestroyTextureObject(const DeviceHandle texture);
		virtual void BindTextureToTarget(const uint32 target, const DeviceHandle texture);

		virtual void UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data);
		virtual void UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height);
		virtual void GenerateMipmap(const uint32 target);

		virtual void SetTextureWrapS(const uint32 target, const uint32 engineRepeat);
		virtual void SetTextureWrapT(const uint32 target, const uint32 engineRepeat);
		virtual void SetTextureWrapR(const uint32 target, const uint32 engineRepeat);
		virtual void SetTextureMagFilter(const uint32 target, const uint32 engineFilter);
		virtual void SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap);
		virtual void SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel);
		virtual void SetTextureBorderColor(const uint32 target, const Vec4 &color);
		virtual void SetTextureCompareMode(const uint32 target);
		virtual void SetPixelUnpackAlignment(const uint32 value);

		virtual void ActivateTextureUnit(const uint32 unit);

		virtual void ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer);
		virtual uint32 GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height);

		virtual DeviceHandle CreateFramebuffer();
		virtual void DestroyFramebuffer(const DeviceHandle fbo);
		virtual uint32 TranslateFramebufferAccess(const uint32 engineAccess);
		virtual void BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo);
		virtual uint32 TranslateFramebufferAttachment(const uint32 engineAttachmentFormat);
		virtual void AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId);
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
		virtual void BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter);

	private:

		VkInstance instance;

		// Set by InitializeSwapchain(); all VK_NULL_HANDLE/0 until then.
		VkSurfaceKHR surface;
		VkPhysicalDevice physicalDevice;
		VkDevice device;
		uint32 graphicsQueueFamily, presentQueueFamily;
		VkQueue graphicsQueue, presentQueue;
		VkSwapchainKHR swapchain;
		VkFormat swapchainFormat;
		VkExtent2D swapchainExtent;
		std::vector<VkImage> swapchainImages;
		std::vector<VkImageView> swapchainImageViews;

		// Depth buffer + render pass + one framebuffer per swapchain image -
		// created in InitializeSwapchain() alongside everything else that
		// depends on swapchainFormat/swapchainExtent. One shared depth
		// image is safe across every framebuffer (only one frame is ever
		// in flight at a time - see the sync objects below), matching this
		// step's single-frame-in-flight scope everywhere else. A single
		// hardcoded render pass (one color + one depth attachment) is
		// enough for every PipelineDesc CreatePipeline() sees so far - a
		// real engine would need render-pass variants for e.g. deferred
		// G-buffer's multiple color attachments, out of scope until Phase 6.
		VkRenderPass renderPass;
		VkImage depthImage;
		VmaAllocation depthImageAllocation;
		VkImageView depthImageView;
		VkFormat depthFormat;
		std::vector<VkFramebuffer> framebuffers;

		// Minimal single-frame-in-flight sync (no double/triple buffering
		// of these yet - good enough for the "does presenting work at all"
		// checkpoint this step is verifying, not production frame pacing).
		// renderFinishedSemaphore is one-per-swapchain-image, not singular
		// like imageAvailableSemaphore/frameFence below - a single shared
		// one caused a real validation error
		// (VUID-vkQueueSubmit-pSignalSemaphores-00067, only ever surfaced
		// once validation layers were actually enabled for the first time):
		// the semaphore signaled by frame N's vkQueueSubmit is still being
		// consumed by the presentation engine when frame N+1's submit
		// tries to signal the same semaphore again, since presenting is
		// asynchronous and isn't covered by frameFence (which only tracks
		// GPU completion of the submitted commands, not the swapchain's
		// present). Indexing by the acquired image index - the standard
		// fix - avoids this since a given swapchain image can't be
		// re-acquired until its previous present completes.
		VkCommandPool commandPool;
		VkCommandBuffer frameCommandBuffer;
		VkSemaphore imageAvailableSemaphore;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		VkFence frameFence;

		// Set by SetClearColor(), read by BeginFrame() when it builds the
		// render pass's VkClearValue - GL sets this as persistent state
		// (glClearColor) ahead of a separate Clear() call, but Vulkan has
		// no equivalent of "clear now" outside of a render pass's
		// LOAD_OP_CLEAR at vkCmdBeginRenderPass time, so this backend's
		// Clear() is (and stays) a no-op - the actual clear always happens
		// via whatever color was set here by the time BeginFrame() runs.
		// ForwardRenderer::RenderScene() calls SetClearColor() (via
		// DrawBackground()) before BeginFrame() for exactly this reason -
		// see that function's comment.
		Vec4 pendingClearColor;

		// See RequestFrameCapture()/GetCapturedFrame().
		bool captureRequested;
		std::vector<uint8_t> capturedPixels;
		uint32 capturedWidth, capturedHeight, capturedRedByteOffset;
		bool capturedFrameValid;

		// Set by BeginFrame(), cleared by EndFrame() - see the comment on
		// IRenderDevice::BeginFrame()/EndFrame(). While true,
		// BeginCommandBuffer() just returns a constant, cheap, meaningful
		// handle representing "the current frame's already-open
		// frameCommandBuffer/render pass" instead of re-acquiring - every
		// per-object BeginCommandBuffer()/EndCommandBuffer() call pair
		// IRenderer already issues (RenderObject()/BindMesh()/EndRender())
		// becomes free once a frame is open, exactly like it already is on
		// GL, just for a different reason (GL has no command buffer at
		// all; this has one real one, shared for the whole frame).
		bool frameInProgress;
		uint32 currentImageIndex;

		// Vertex buffer / index buffer pair for a "VAO" handle - see the
		// header comment on the `pipelines` field below for the broader
		// vertex-input limitation this is part of. GL's VAO bakes in
		// attribute pointers/enables *and* the bound array+element buffers;
		// this backend's vertex *attribute layout* is already baked into
		// the pipeline (CreatePipeline()'s hardcoded VkVertexInputState),
		// so all a "VAO" needs to remember here is which two buffers to
		// bind at draw time. Built up the same way GL builds a VAO -
		// BindVertexArray(cmd, vao) selects which one BindArrayBuffer()/
		// BindElementBuffer() write into next (mirroring glBindVertexArray()
		// making those the implicit target of subsequent glBindBuffer()
		// calls) - and later just re-selected (via the same
		// BindVertexArray() call) as the active one for DrawElements()/
		// DrawElementsInstanced() to read from.
		struct VaoRecord
		{
			DeviceHandle vertexBuffer, indexBuffer;
			VaoRecord() : vertexBuffer(0), indexBuffer(0) {}
		};
		std::map<DeviceHandle, VaoRecord> vaos;
		DeviceHandle nextVaoHandle;
		// 0 = none selected (matches BindVertexArray(cmd, 0)'s GL "unbind"
		// semantics - BindArrayBuffer()/BindElementBuffer()/DrawElements()
		// all silently no-op while this is 0, same as GL silently doing
		// nothing useful with no VAO bound).
		DeviceHandle currentVao;
		// 0 = no pipeline bound in the currently-open frame. Tracked so
		// DrawElements()/DrawElementsInstanced() can refuse to issue a
		// vkCmdDrawIndexed() when BindPipeline() silently failed to find
		// its handle (e.g. because CreatePipeline() earlier returned 0) -
		// otherwise the draw is genuinely undefined behavior on real
		// drivers (VUID-vkCmdDrawIndexed-None-08606), not just a validation
		// warning; skipping it fails safe instead of crashing.
		DeviceHandle currentPipeline;

		// Created alongside the logical device in InitializeSwapchain() -
		// every CreateBuffer()/CreateUniformBuffer() call below goes through
		// this rather than hand-rolled vkAllocateMemory (VMA handles the
		// actual-device-memory-vs-buffer-count multiplexing Vulkan requires
		// but GL never exposed, e.g. the ~4096 discrete allocation limit
		// many drivers enforce). NULL until InitializeSwapchain() runs -
		// CreateBuffer()/CreateUniformBuffer() called before that point (as
		// Shaders.cpp/GeometryBuffer.cpp's asset-loading paths might, ahead
		// of any window/device existing) fail gracefully (return 0).
		VmaAllocator allocator;

		// Host-visible/coherent, persistently mapped - simplest correct
		// choice for a first working path (device-local + staging-buffer
		// upload is a real perf improvement, deliberately deferred per
		// VULKAN_ROADMAP.md Phase 5 Step D's scope). One record per
		// CreateBuffer()/CreateUniformBuffer() handle; buffers and uniform
		// buffers share this table and DeviceHandle namespace since nothing
		// downstream needs to tell them apart by handle alone (unlike GL,
		// where the two are different concepts entirely - VkBuffer doesn't
		// distinguish "vertex/index buffer" from "uniform buffer", only
		// how it's bound later does).
		struct BufferRecord
		{
			VkBuffer buffer;
			VmaAllocation allocation;
			void* mapped; // persistently mapped pointer, valid for the buffer's whole lifetime
			uint32 size;
		};
		std::map<DeviceHandle, BufferRecord> buffers;
		DeviceHandle nextBufferHandle;

		// bindingPoint -> uniform buffer handle, populated by
		// CreateUniformBuffer() (which already receives the binding point
		// GL's glBindBufferBase would use). BindUniformBlockIfPresent()
		// looks a binding up here to know which VkBuffer to point a
		// program's descriptor set at - GL's equivalent
		// (glUniformBlockBinding) only takes a binding *point*, not a
		// buffer, because GL keeps a single global binding-point ->
		// buffer table (glBindBufferBase) separate from the program; this
		// is this backend's equivalent of that table.
		std::map<uint32, DeviceHandle> uniformBufferByBindingPoint;

		// Created lazily by the first BindUniformBlockIfPresent() call -
		// sized generously up front (see .cpp) for up to 1024 sets total:
		// one UBO set per program (few) plus one sampler set per
		// *pipeline* (see ProgramRecord::samplerSetLayout for why samplers
		// need per-pipeline granularity) - a real but generous fixed cap,
		// not dynamically growable.
		VkDescriptorPool descriptorPool;

		// One VkShaderModule per CreateShaderStage()/CompileShaderStage()
		// pair - engineShaderType is stashed at CreateShaderStage() time
		// since CompileShaderStage() needs to know which SpirvShaderStage
		// to compile as (see SpirvShaderCompiler::Compile()), and GL's
		// CreateShaderStage() already takes that same parameter, so no
		// interface change was needed to plumb it through.
		struct ShaderStageRecord
		{
			uint32 engineShaderType; // ShaderType::VertexShader/FragmentShader
			VkShaderModule module; // VK_NULL_HANDLE until CompileShaderStage() succeeds
			// Kept around (not just the module) so LinkProgram() can
			// reflect it - SpirvResourceBinding itself is only declared
			// under SPIRV_TOOLING (see ShaderCompiler.h), so this stores
			// the raw words instead of a SPIRV_TOOLING-dependent type,
			// keeping this header buildable with BUILD_VULKAN_BACKEND=ON
			// and BUILD_SPIRV_TOOLING=OFF (LinkProgram() just fails
			// gracefully in that configuration, same as CompileShaderStage()
			// already does).
			std::vector<uint32> spirv;
		};
		std::map<DeviceHandle, ShaderStageRecord> shaderStages;
		DeviceHandle nextShaderStageHandle;

		// A "program" is the (vertex module, fragment module) pair plus
		// the descriptor set layout + pipeline layout LinkProgram() derives
		// from reflecting both stages' SPIR-V (see the comment on
		// ShaderStageRecord::spirv, and SpirvResourceBinding in
		// ShaderCompiler.h) - Vulkan has no equivalent of a linked GL
		// program object; the actual "link" step happens at VkPipeline
		// creation (CreatePipeline()), which is what consumes
		// pipelineLayout. Every one of PyrosShader.glsl's UBO bindings
		// (see its BIND_* macros) has no explicit `set=N` qualifier, so
		// SPIR-V/spirv-cross default every one of them to set 0 - this
		// only ever builds a single VkDescriptorSetLayout per program,
		// never multiple sets, which matches that.
		struct ProgramRecord
		{
			DeviceHandle vertexShader, fragmentShader;
			VkDescriptorSetLayout descriptorSetLayout;
			VkPipelineLayout pipelineLayout;
			// Allocated lazily by the first BindUniformBlockIfPresent()
			// call for this program (see descriptorPool above) - one set
			// per program is enough for this backend's current scope
			// (every draw using a given program shares the same handful
			// of UBOs; nothing here yet supports per-draw-varying textures/
			// buffers, which would need one set per draw or per-material
			// instead).
			VkDescriptorSet descriptorSet;
			// Which binding indices LinkProgram() actually found reflected
			// (i.e. which ones exist in descriptorSetLayout) -
			// BindUniformBlockIfPresent() checks membership before writing
			// a descriptor, mirroring GL's BindUniformBlockIfPresent
			// no-op-if-the-shader-doesn't-declare-this-block contract
			// (writing to a binding vkUpdateDescriptorSets doesn't know
			// about would be invalid, not a harmless no-op, the way GL's
			// glUniformBlockBinding on a nonexistent block name is).
			std::set<uint32> reflectedBindings;
			// Vertex attribute name -> location, reflected from the
			// vertex stage's compiled SPIR-V in LinkProgram() (see
			// SpirvShaderCompiler::ReflectStageInputs()). CreatePipeline()
			// looks names up here (matching PipelineDesc::VertexAttributeDesc::name,
			// itself sourced from the mesh's VertexAttribute::Name) instead
			// of GL's runtime glGetAttribLocation() equivalent, which
			// Vulkan has no counterpart for.
			std::map<std::string, uint32> attributeLocations;
			// Second descriptor set (set=1) for this program's sampler
			// resources only (set=0, descriptorSetLayout/descriptorSet
			// above, stays UBO-only) - deliberately *not* sharing one set
			// per program the way UBOs do, because textures vary per
			// *material*, not per program: two materials using the same
			// shader but different textures would otherwise silently both
			// render with whichever texture was bound last, since
			// vkUpdateDescriptorSets mutates a set's contents in place and
			// every draw referencing that set (regardless of when it was
			// recorded into a command buffer) reads whatever's there when
			// the GPU actually executes it, not what was there at record
			// time. Solved instead by giving each *pipeline* its own
			// sampler descriptor set (pipelineSamplerSets below) - pipelines
			// are already effectively per-(mesh,shader), at least as fine-
			// grained as per-material for any real scene, since
			// RenderingMesh::PipelineCache is keyed per-mesh, not shared
			// globally even when two meshes share one Material instance.
			VkDescriptorSetLayout samplerSetLayout;
			// Sampler uniform name (e.g. "uColormap") -> the binding
			// PyrosShader.glsl's SAMPLER_BINDING/BIND_* macros gave it,
			// reflected the same way attributeLocations is. Repurposes
			// GetUniformLocation()'s return value: for a name that
			// reflects to a SampledImage resource, VulkanRenderDevice
			// returns this binding number instead of -1, so the existing
			// GenericShaderMaterial/SendUserUniforms() call sequence that
			// already sends a texture's "unit" as a plain int uniform
			// (GL's mechanism for telling a sampler which texture unit to
			// read from) can be repurposed, unmodified, as the trigger for
			// updating this program's/pipeline's sampler descriptor
			// instead - see SendUniformInt()'s comment for the full
			// mechanism.
			std::map<std::string, uint32> samplerBindings;
			// Membership check mirroring reflectedBindings, but for
			// samplerSetLayout/samplerBindings instead of
			// descriptorSetLayout/UBOs.
			std::set<uint32> reflectedSamplerBindings;
			ProgramRecord() : vertexShader(0), fragmentShader(0), descriptorSetLayout(VK_NULL_HANDLE), pipelineLayout(VK_NULL_HANDLE), descriptorSet(VK_NULL_HANDLE), samplerSetLayout(VK_NULL_HANDLE) {}
		};
		std::map<DeviceHandle, ProgramRecord> programs;
		DeviceHandle nextProgramHandle;
		// Set by UseProgram() - which program's reflected data
		// (attributeLocations/samplerBindings/etc) SendUniform*() and
		// GetUniformLocation() calls should resolve against. Mirrors GL's
		// own implicit "current program" state (glUseProgram), which every
		// glUniform* call already relies on the same way.
		DeviceHandle currentProgram;

		// Real VkPipeline objects, keyed by the handle CreatePipeline()
		// returns. Vertex input state is built dynamically per pipeline
		// from PipelineDesc::vertexLayout (see CreatePipeline()) - no
		// longer hardcoded to any one mesh's layout.
		std::map<DeviceHandle, VkPipeline> pipelines;
		DeviceHandle nextPipelineHandle;
		// pipeline handle -> the program it was built from (PipelineDesc's
		// shaderProgram) - BindPipeline() only receives a pipeline handle,
		// but also needs to bind that program's descriptor set
		// (vkCmdBindDescriptorSets), so this is how it looks the program
		// back up.
		std::map<DeviceHandle, DeviceHandle> pipelineToProgram;
		// This pipeline's own sampler descriptor set (set=1) - see the
		// comment on ProgramRecord::samplerSetLayout for why this is
		// per-pipeline rather than per-program. Allocated in
		// CreatePipeline() using the owning program's samplerSetLayout;
		// written to by SendUniformInt() once BindMesh()/Material::PreRender()
		// actually bind a texture. currentPipeline (below) is which of
		// these SendUniformInt() should target.
		std::map<DeviceHandle, VkDescriptorSet> pipelineSamplerSets;

		// Real VkImage/VkImageView/VkSampler-backed texture, keyed by the
		// handle CreateTextureObject() returns. GL's texture API is an
		// incremental state machine (bind a target, then configure it via
		// separate SetTextureWrapS/SetTextureMinFilter/UploadTexture2D
		// calls) - Vulkan needs an atomic image+view+sampler, so this
		// record accumulates GL-style state (format/size/wrap/filter)
		// across calls and lazily (re)builds the real objects once enough
		// is known (the image+view right after the first UploadTexture2D,
		// since that's when format/size/data become available; the
		// sampler on first use after any wrap/filter setter, tracked via
		// samplerDirty).
		struct TextureRecord
		{
			VkImage image;
			VmaAllocation allocation;
			VkImageView view;
			VkSampler sampler;
			uint32 width, height;
			VkFormat format;
			uint32 wrapS, wrapT;     // TextureRepeat::* values
			uint32 minFilter, magFilter; // TextureFilter::* values
			bool hasMipmap;
			bool samplerDirty;
			TextureRecord()
				: image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), view(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE),
				  width(0), height(0), format(VK_FORMAT_R8G8B8A8_UNORM),
				  wrapS(TextureRepeat::Repeat), wrapT(TextureRepeat::Repeat),
				  minFilter(TextureFilter::Linear), magFilter(TextureFilter::Linear),
				  hasMipmap(false), samplerDirty(true) {}
		};
		std::map<DeviceHandle, TextureRecord> textures;
		DeviceHandle nextTextureHandle;
		// Lazily (re)builds tex.sampler from its wrap/filter state if
		// dirty - see the comment on TextureRecord::samplerDirty. Returns
		// false (logging) only on a real vkCreateSampler failure; true
		// otherwise, including the already-clean-and-valid case.
		bool RebuildSamplerIfDirty(TextureRecord &tex);
		// Which texture BindTextureToTarget()/UploadTexture2D()/
		// SetTextureWrapS()/etc are currently configuring - GL's own
		// "operate on whatever's bound to this target" state, mirrored
		// here since Texture.cpp's call sequence (bind, configure,
		// unbind) is identical regardless of backend. 0 = none (matches
		// GL's BindTextureToTarget(target, 0) unbind).
		DeviceHandle currentlyConfiguringTexture;
		// True for exactly one BindTextureToTarget() call after
		// ActivateTextureUnit() - the pairing Texture::Bind()/Unbind()
		// uses at *render* time (as opposed to the configuration-time
		// bind/unbind pairs above, which never call ActivateTextureUnit)
		// - distinguishes "bind this texture for rendering at unit N"
		// from "select this texture to configure its wrap/filter state",
		// since both go through the same BindTextureToTarget() call.
		bool unitJustActivated;
		uint32 currentTextureUnit;
		// Texture unit -> texture handle, populated by the render-time
		// bind pairing above (Texture::Bind()/Unbind()). SendUniformInt()
		// reads this when a sampler's "unit" int uniform is sent, to
		// resolve which real texture a descriptor write should point at -
		// see the comment on ProgramRecord::samplerBindings.
		std::map<uint32, DeviceHandle> textureUnitBindings;

	};

};

#endif /* VULKAN_BACKEND */

#endif /* VULKANRENDERDEVICE_H */
