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
// Needed for VkPhysicalDevicePortabilitySubsetFeaturesKHR/
// VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR
// (queried/enabled in InitializeSwapchain() on Apple, for shadow-sampler
// comparison support - see that code's comment) - VK_KHR_portability_subset
// is still gated behind this macro in the Khronos headers even though
// it's a shipping, non-experimental extension on every real portability
// ICD (MoltenVK).
#define VK_ENABLE_BETA_EXTENSIONS
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <map>
#include <set>
#include <functional>

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
		bool IsVulkan() const { return true; }
		bool NeedsManualDisplayGamma() const { return true; }

		// Real fix for a real, reported bug: under a tiling window
		// manager, the OS resizes the window via the Accessibility API,
		// which bypasses the normal Cocoa resize-notification path SDL
		// listens to - SDL_WINDOWEVENT_RESIZED never fires, and even
		// SDL's own size-query functions (SDL_GetWindowSize(),
		// SDL_Vulkan_GetDrawableSize()) keep reporting the stale,
		// originally-requested size (confirmed via real debug prints
		// this session, not assumed). The swapchain itself doesn't have
		// this problem - vkGetPhysicalDeviceSurfaceCapabilitiesKHR's
		// currentExtent always reflects the real, current OS window
		// size, which is why RecreateSwapchain()'s self-heal path (on
		// VK_ERROR_OUT_OF_DATE_KHR) already resizes correctly. But
		// nothing told the *engine* side (DeferredRenderer's G-buffer
		// textures, the example's camera projection - all driven by
		// Width/Height, which only ever changes via the SDL resize path
		// that doesn't fire here) about the new size, leaving a real,
		// persistent mismatch between what the swapchain is actually
		// sized to and what everything drawing into it assumes - the
		// likely cause of "sometimes hangs, sometimes draws garbage/red
		// on resize under a tiling WM" (reported, not yet independently
		// reproduced on demand - this fixes the mechanism regardless).
		// Callers should poll this every frame (see SDL2VulkanContext::
		// GetEvents()) and diff against their own tracked size, since -
		// unlike SDL's queries - this always reflects reality.
		bool QueryRealSurfaceExtent(uint32 &width, uint32 &height) const;

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
		// TEMP-DIAG: reads back a depth texture's raw float content (e.g.
		// a shadow map) via a one-off staging-buffer copy - for directly
		// inspecting whether a shadow pass actually wrote real geometry
		// depth into its target, independent of anything sampling it
		// later. Not part of the real API surface, remove once the
		// directional-shadow investigation concludes.
		bool DebugReadDepthTexture(const DeviceHandle handle, std::vector<f32> &outDepths, uint32 &outWidth, uint32 &outHeight, const uint32 faceIndex = 0);

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
		virtual uint32 GetSwapchainGeneration() const { return swapchainGeneration; }
		// See IRenderDevice::NotifySurfaceResized()'s comment - calls the
		// same private RecreateSwapchain() the reactive OUT_OF_DATE/
		// SUBOPTIMAL path already uses, just triggered proactively instead
		// of waiting for a signal that may never come. No-op when already
		// at that extent - GetEvents polls every frame; without this a
		// tiny mismatch would vkDeviceWaitIdle+rebuild forever (~200 FPS).
		virtual void NotifySurfaceResized(const uint32 width, const uint32 height)
		{
			if (width == 0 || height == 0)
				return;
			if (swapchain != VK_NULL_HANDLE && width == swapchainExtent.width && height == swapchainExtent.height)
				return;
			RecreateSwapchain(width, height);
		}

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

		virtual Matrix TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip = false);
		virtual Matrix TranslateShadowBiasMatrix();

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
		virtual bool GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 &outBinding, std::string &outBlockName, uint32 &outSize, std::map<std::string, uint32> &outOffsets);

		virtual void SendUniformInt(const int32 handle, const int32 *data, const uint32 count);
		virtual void SendUniformFloat(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec2(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec3(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformVec4(const int32 handle, const f32 *data, const uint32 count);
		virtual void SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count);

		virtual void TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type);
		virtual void TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode);

		virtual void *GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType);
		// Frees a set handed out above. Defined beside it, in the only
		// translation unit that includes imgui_impl_vulkan.h.
		void ReleaseImGuiTextureID(void *descriptorSet);
		virtual DeviceHandle CreateTextureObject();
		virtual void DestroyTextureObject(const DeviceHandle texture);
		virtual void BindTextureToTarget(const uint32 target, const DeviceHandle texture);

		virtual void UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap);
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
		virtual void BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter);
		virtual void CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height);

		// Real ImGui-on-Vulkan integration - wraps ImGui_ImplVulkan_Init/
		// NewFrame/Shutdown so example code never links the vendored
		// ImGui_ImplVulkan_* symbols itself. Deliberately implemented here
		// (imgui_impl_vulkan.cpp is compiled as part of this library, see
		// the root CMakeLists.txt) rather than in example code: with
		// IMGUI_IMPL_VULKAN_USE_VOLK it shares this translation unit's
		// already-volkLoadDevice()'d function-pointer table - compiled
		// into a separate example binary instead, it would reference
		// that binary's own private, never-loaded copy of the same
		// globals and crash on the first Vulkan call (the identical,
		// previously-hit crash class WaitIdle()'s comment above
		// describes for inline Vulkan-calling methods in this header).
		// Deliberately does NOT touch ImGui_ImplSDL2_* - that backend
		// (imgui_impl_sdl2.cpp) has no volk/Vulkan-function dependency at
		// all and stays compiled per-example same as the GL path already
		// does; callers must call ImGui_ImplSDL2_InitForVulkan()/
		// ImGui_ImplSDL2_Shutdown() themselves. Call
		// ImGui_ImplSDL2_NewFrame() *before* NewImGuiVulkanFrame() so the
		// latter can correct DisplayFramebufferScale against the real
		// swapchain extent (SDL_Vulkan_GetDrawableSize can lag).
		bool InitImGuiVulkanBackend();
		void NewImGuiVulkanFrame();
		void ShutdownImGuiVulkanBackend();

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

		// Builds/rebuilds the swapchain + depth buffer + render pass +
		// framebuffers (everything keyed to the surface's current size/
		// format) - called by InitializeSwapchain() the first time and by
		// RecreateSwapchain() on every resize. See the .cpp definition's
		// comment for why oldSwapchain handling lives here.
		bool CreateSwapchainAndFramebuffers(const uint32 width, const uint32 height);
		// Destroys the current swapchain-size-dependent resources and
		// rebuilds them against the surface's now-current size - called
		// reactively from BeginFrame()/EndFrame() when a swapchain
		// operation reports VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR (see
		// the .cpp definition's comment for why this is the standard, and
		// only, way Vulkan surfaces a resize to the app).
		bool RecreateSwapchain(const uint32 width, const uint32 height);
		// Destroys and recreates one specific pool entry, forcing it back
		// to a known-unsignaled state - called whenever a frame is
		// abandoned after vkAcquireNextImageKHR() didn't return
		// VK_SUCCESS. Needed because rotating through a small pool alone
		// (see nextAcquireSemaphoreIndex's comment) isn't sufficient: a
		// long enough run of consecutive failed acquires during a
		// sustained resize can still wrap back around to a pool entry
		// that's still signaled from an earlier abandoned attempt
		// (reproduced: VUID-vkAcquireNextImageKHR-semaphore-01286 during
		// a resize burst even with a 3-entry pool). Destroying and
		// recreating is safe regardless of whether the driver actually
		// left it signaled or not - either way nothing pending on the
		// device still references it once the acquire call has returned.
		void ResetAcquireSemaphore(const uint32 index);

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
		// Bumped every time CreateSwapchainAndFramebuffers() builds a new
		// `renderPass` (initial creation and every resize) - see
		// IRenderDevice::GetSwapchainGeneration()'s comment for why a
		// pipeline that targets the swapchain directly needs to know this.
		uint32 swapchainGeneration;
		VkImage depthImage;
		VmaAllocation depthImageAllocation;
		VkImageView depthImageView;
		VkFormat depthFormat;
		std::vector<VkFramebuffer> framebuffers;

		// Double-buffered frames in flight so the CPU can record frame N+1
		// while the GPU finishes frame N (single-buffer was leaving MoltenVK
		// ~5× behind GL on a one-cube forward demo).
		static const uint32 MAX_FRAMES_IN_FLIGHT = 2;
		VkCommandPool commandPool;
		VkCommandBuffer frameCommandBuffers[MAX_FRAMES_IN_FLIGHT];
		// Alias of frameCommandBuffers[currentFrameSlot] for the open frame -
		// BeginFrame() assigns it; existing record paths keep using this name.
		VkCommandBuffer frameCommandBuffer;
		uint32 currentFrameSlot;
		// A small rotating pool, not one shared semaphore - see
		// nextAcquireSemaphoreIndex's comment for why one semaphore isn't
		// enough even in this single-frame-in-flight model, unlike
		// renderFinishedSemaphores (which has a different, already-solved
		// reason to be plural - see its own comment below).
		std::vector<VkSemaphore> imageAvailableSemaphores;
		// Advances by one on *every* vkAcquireNextImageKHR call, success
		// or failure alike - found necessary via
		// VUID-vkAcquireNextImageKHR-semaphore-01286 ("semaphore must not
		// be currently signaled"): VK_SUBOPTIMAL_KHR (and, on this
		// backend's observed MoltenVK behavior, VK_ERROR_OUT_OF_DATE_KHR
		// too) still signals the semaphore per spec even though the
		// caller is expected to treat the acquire as unusable and skip
		// the frame - which this backend does, skipping the submit that
		// would otherwise have consumed that signal via a wait. Reusing
		// the *same* semaphore on the next call then violates "must not
		// be currently signaled" (a real, reproduced validation error
		// during a resize burst, not hypothetical). Rotating through a
		// small pool instead means every acquire call always uses a
		// fresh, guaranteed-unsignaled semaphore regardless of whether
		// the previous attempt's signal was ever consumed.
		uint32 nextAcquireSemaphoreIndex;
		// Which pool entry the *in-progress* frame's BeginFrame() actually
		// used - EndFrame()'s vkQueueSubmit must wait on that exact one,
		// not whatever nextAcquireSemaphoreIndex has since advanced to.
		uint32 currentFrameAcquireSemaphoreIndex;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		VkFence frameFences[MAX_FRAMES_IN_FLIGHT];
		// Convenience for paths that wait "until GPU is idle enough to
		// touch shared resources" - equals frameFences[currentFrameSlot]
		// only while a frame is open; prefer WaitAllFrameFences() for
		// offscreen pre-frame sync.
		VkFence frameFence;

		// Batched asset uploads (UploadTexture2D / GenerateMipmap). One
		// command buffer + one fence for a whole load burst instead of
		// submit+wait per texture. Flushed before the first frame that
		// can sample them (BeginFrame) and before WaitIdle/teardown.
		VkCommandBuffer transferCommandBuffer;
		bool transferCommandBufferRecording;
		VkFence transferFence;
		struct PendingStagingBuffer
		{
			VkBuffer buffer;
			VmaAllocation allocation;
		};
		std::vector<PendingStagingBuffer> pendingStagingBuffers;
		VkDeviceSize pendingStagingBytes;

		// Persistent VkPipelineCache (loaded/saved under ~/.cache/pyros3d).
		VkPipelineCache pipelineCache;

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

		// Invoked by EndFrame() with the still-recording frameCommandBuffer,
		// immediately before the render pass closes - the only point at
		// which a caller can inject additional draw commands (e.g. ImGui)
		// into the main swapchain pass. Set by InitImGuiVulkanBackend(),
		// cleared by ShutdownImGuiVulkanBackend(); empty (no-op) otherwise.
		std::function<void(VkCommandBuffer)> UIRenderHook;

		// True between a successful InitImGuiVulkanBackend() and the next
		// ShutdownImGuiVulkanBackend(). Gates RebuildImGuiVulkanPipeline()
		// below (called from RecreateSwapchain() on every resize) so a
		// device that never initialized the ImGui backend doesn't pay for
		// (or crash on) touching it.
		bool imguiVulkanBackendActive;

		// Rebuilds ImGui's Vulkan pipeline against the just-recreated
		// `renderPass` - called from RecreateSwapchain() right after it
		// rebuilds `renderPass` itself, a no-op unless
		// imguiVulkanBackendActive. Defined in VulkanImGuiBackend.cpp
		// (not here) so this file - already compiled without any ImGui
		// awareness - doesn't need to include imgui_impl_vulkan.h.
		void RebuildImGuiVulkanPipeline();

		// Which command buffer BindPipeline()/DrawElements()/
		// DrawElementsInstanced()/SetViewport() actually record into -
		// frameCommandBuffer during a real swapchain frame,
		// offscreenCommandBuffer during an offscreen FBO pass (shadow
		// maps - see BindFramebuffer()). The two are mutually exclusive
		// in time (shadow passes run in IRenderer::PreRender(), strictly
		// before RenderScene()'s BeginFrame()), so reusing one pointer-
		// like member for "whichever is currently live" avoids
		// duplicating every draw-path method for the offscreen case.
		VkCommandBuffer activeCommandBuffer;
		// Real per-session command buffer for offscreen FBO rendering
		// (shadow maps) - separate from frameCommandBuffer since a
		// shadow pass must be able to record+submit+wait *before* the
		// swapchain frame's own command buffer even exists yet
		// (BeginFrame() hasn't run). Allocated once in
		// InitializeSwapchain(), reused (vkResetCommandBuffer) per
		// FrameBuffer::Bind()/UnBind() session - one submit per session
		// (per light), not per attached face, so a point light's 6 faces
		// share a single submit+wait instead of six.
		VkCommandBuffer offscreenCommandBuffer;
		// True while a vkCmdBeginRenderPass is currently open on
		// offscreenCommandBuffer - mirrors frameInProgress's role for
		// the swapchain case; BindPipeline()/DrawElements()/etc check
		// (frameInProgress || offscreenPassOpen) instead of
		// frameInProgress alone.
		bool offscreenPassOpen;
		// True from the first BeginOffscreenRenderPassForTarget() that
		// starts recording until FlushOffscreenCommandBuffer() submits.
		// UnBind (BindFramebuffer 0) ends the open render pass but keeps
		// recording so G-buffer → CopyDepthTexture → lighting (and nested
		// Capture FBO restores) batch into one submit. Distinct from
		// offscreenPassOpen, which toggles per render pass within that CB.
		bool offscreenCommandBufferRecording;
		// A ring of offscreen submission slots rather than one command
		// buffer + one fence. With a single slot, every offscreen session
		// had to block the CPU in vkWaitForFences before it could
		// vkResetCommandBuffer and record the next one - so a deferred
		// frame's shadow / capture / G-buffer / lighting sessions ran as
		// four serialized CPU->GPU round-trips with no overlap at all.
		// That was 84% of the Vulkan main thread (sampled on GrassField:
		// 3073 of 3670 samples inside vkWaitForFences), and it is why the
		// deferred and shadowed demos sat at 21-23ms against GL's 3-5ms
		// while the unshadowed ones hit vsync fine.
		//
		// Rotating slots lets the CPU record session N+1 while the GPU is
		// still running session N. GPU-side *ordering* is unchanged: see
		// offscreenChainSemaphore - each submit waits on the previous
		// one's semaphore, so passes still execute in the order they were
		// recorded, which is what a G-buffer written by one session and
		// sampled by the next relies on. Only the CPU's wait is gone.
		static const uint32 OFFSCREEN_SLOTS = 4;
		struct OffscreenSlot
		{
			VkCommandBuffer cmd;
			VkFence fence;
			VkSemaphore done;
			// Submitted, fence not yet waited on the CPU. Its command
			// buffer must not be reset and its fence must not be reset
			// while this is set.
			bool fenceInFlight;
			OffscreenSlot() : cmd(VK_NULL_HANDLE), fence(VK_NULL_HANDLE), done(VK_NULL_HANDLE), fenceInFlight(false) {}
		};
		OffscreenSlot offscreenSlots[OFFSCREEN_SLOTS];
		// Slot currently recording, or most recently submitted.
		uint32 offscreenSlotIndex;
		// The `done` semaphore of the most recent offscreen submit that
		// nothing has waited on yet, or VK_NULL_HANDLE. Exactly one waiter
		// ever takes it - the next offscreen submit (which is what keeps
		// offscreen work ordered), or EndFrame if the frame's own draws
		// are next. A binary semaphore that is signaled and never waited
		// cannot legally be signaled again, so this must be consumed
		// before its slot is reused; WaitOffscreenSlot() does that with an
		// empty submit when it has to.
		VkSemaphore offscreenChainSemaphore;
		// Cleared each BeginFrame; set after EnsureHostMappedBufferWritable
		// so multiple STREAM buffer updates in one tick share one wait.
		bool hostMappedBuffersSafeThisFrame;
		// Which FBO handle BindFramebuffer(access, fbo!=0) most recently
		// selected - AttachFramebufferTexture2D() operates on this.
		// 0 = none (matches BindFramebuffer(access, 0)'s GL "unbind to
		// the default framebuffer" semantics).
		DeviceHandle currentBoundFBO;
		// Separate from currentBoundFBO above - GL's glBlitFramebuffer()
		// (what BlitFramebuffer() mirrors) reads from whatever's bound to
		// the *separate* GL_READ_FRAMEBUFFER target, independent of
		// GL_DRAW_FRAMEBUFFER (currentBoundFBO's real equivalent here).
		// Only ever set by a BindFramebuffer() call whose nativeAccess is
		// FBOAccess::Read - a Read-only bind never begins a render pass
		// or touches currentBoundFBO/the offscreen command buffer at all
		// (see BindFramebuffer()'s comment), matching GL's own behavior
		// (binding something for reading doesn't make it the render
		// target). 0 = none.
		DeviceHandle currentReadFBO;

		// One real offscreen render target, keyed by the handle
		// CreateFramebuffer() returns. Only depth-only, texture-backed
		// FBOs are implemented - every light's shadow FBO setup
		// (DirectionalLight/PointLight/SpotLight .cpp) uses exactly this
		// shape (a single Depth_Attachment, TextureType::Texture) on its
		// live code path; the Renderbuffer-backed alternative in each of
		// those files is gated `#if defined(GLLEGACY)`, confirmed never
		// defined anywhere in this project's CMake, so not implemented
		// here either.
		// One pending, not-yet-built attachment slot - accumulated by
		// AttachFramebufferTexture2D() calls that arrive *outside* an
		// already-active Bind() session (wasAlreadyBound=false - see
		// IRenderDevice.h's comment on that parameter), keyed by
		// nativeAttachmentFormat so a G-buffer's several AddAttach() calls
		// (Depth_Attachment, Color_Attachment0, Color_Attachment1, ...)
		// each claim a distinct slot instead of overwriting one another.
		// Finalized into a real multi-attachment VkRenderPass+VkFramebuffer
		// the first time this FBO is genuinely bound for rendering (see
		// BindFramebuffer()'s comment) - deferred because Vulkan needs
		// every attachment's format known upfront to build a render pass,
		// but a caller-supplied multi-attachment FrameBuffer's individual
		// AddAttach() calls each only know about *one* attachment at a
		// time.
		struct PendingAttachment
		{
			uint32 format; // FrameBufferAttachmentFormat::* (Depth_Attachment or Color_AttachmentN)
			uint32 target; // TranslateTextureTarget()'s native token (plain 2D, or a cubemap face)
			DeviceHandle textureId;
		};

		struct FBORecord
		{
			// Built lazily, once, either from the first
			// AttachFramebufferTexture2D() call that arrives with
			// wasAlreadyBound=true (the original depth-only shadow-map
			// case - unchanged), or from BindFramebuffer()'s first *real*
			// bind once enough PendingAttachment entries have accumulated
			// (the multi-attachment G-buffer/color-render-target case).
			VkRenderPass renderPass;
			uint32 width, height;
			// Slots accumulated so far, used to build renderPass the first
			// time (see PendingAttachment's comment) - deliberately *not*
			// cleared once that happens, unlike earlier versions of this
			// mechanism: BuildCombinedFramebuffer() also needs this list
			// later, any time InvalidateFramebuffersForTexture() tears down
			// framebuffersByTarget[0] because one of these attachments'
			// textures got resized. A *second* AttachFramebufferTexture2D()
			// call for a format already present after renderPass is built
			// (wasAlreadyBound=true, e.g. a point light's next cubemap
			// face) goes through the existing per-target immediate-retarget
			// path instead, never touching this again.
			std::vector<PendingAttachment> pendingAttachments;
			// How many of renderPass's attachments are color (vs the one
			// optional depth) - CreatePipeline() needs this to size its
			// VkPipelineColorBlendStateCreateInfo/VkPipelineColorBlendAttachmentState
			// array to match, since Vulkan requires that count to equal
			// the target render pass's color attachment count exactly.
			uint32 colorAttachmentCount;
			bool hasDepthAttachment;
			// One VkFramebuffer per distinct render target within this
			// FBO - almost always exactly one entry (directional/spot: a
			// single 2D depth map, attached once), but up to six for a
			// point light's cubemap (each face is a separate 2D view of
			// the same cube image, attached in turn via repeated
			// AttachFramebufferTexture2D() calls - see PointLight's
			// PreRender() loop), keyed by the `nativeTextureTarget`
			// TranslateTextureTarget() produced for that face/target.
			std::map<uint32, VkFramebuffer> framebuffersByTarget;
			// Which entry in framebuffersByTarget was attached most
			// recently - directional/spot lights call
			// AttachFramebufferTexture2D() exactly once, at setup time
			// (FrameBuffer::Init()), and never again; every subsequent
			// frame's IRenderer::PreRender() only calls Bind()/UnBind()
			// (FrameBuffer::Bind()/UnBind(), this backend's BindFramebuffer())
			// around the shadow-casting draws, with no re-attach at all.
			// BindFramebuffer(fbo!=0) uses this to know which existing
			// framebuffer to re-begin rendering into on those frames,
			// instead of only ever beginning a render pass from inside
			// AttachFramebufferTexture2D() (which would never run again
			// after the first frame for these two light types - found
			// the hard way via a shadow-casting draw silently never
			// happening past frame 1, no error, just zero effect).
			uint32 lastTarget;
			// VK_SAMPLE_COUNT_1_BIT unless every attachment here is a real
			// multisample texture (see TextureRecord::samples) -
			// BuildMultiAttachmentRenderPass() reads each attachment's own
			// sample count and requires them to agree (Vulkan render
			// passes need one uniform sample count across all attachments
			// in a subpass); CreatePipeline() reads this back to set
			// VkPipelineMultisampleStateCreateInfo::rasterizationSamples
			// to match, same "resolve from currentBoundFBO at creation
			// time" pattern targetRenderPass/targetColorAttachmentCount
			// already use.
			VkSampleCountFlagBits samples;
			// See IRenderDevice::SetFramebufferPreserveDepth(). When set, this
			// FBO's render pass loads its depth attachment instead of clearing
			// it, and leaves it in DEPTH_STENCIL_ATTACHMENT_OPTIMAL so the next
			// frame's CopyDepthTexture() finds the layout it expects.
			bool preserveDepth;
			FBORecord() : renderPass(VK_NULL_HANDLE), width(0), height(0), colorAttachmentCount(0), hasDepthAttachment(false), lastTarget(0), samples(VK_SAMPLE_COUNT_1_BIT), preserveDepth(false) {}
		};
		std::map<DeviceHandle, FBORecord> fboRecords;
		DeviceHandle nextFBOHandle;

		// A depth-only render pass used *only* for building shadow-casting
		// pipelines (PipelineDesc::isShadowPass) - not tied to any one
		// FBORecord/light, since every shadow map on this backend uses
		// the exact same shape (one VK_FORMAT_D32_SFLOAT attachment, see
		// TranslateTextureFormat()). Vulkan bakes a specific VkRenderPass's
		// attachment shape into a pipeline at creation time, and using
		// the swapchain's color+depth render pass (this class's default
		// `renderPass` member, further down) for a shadow-casting draw
		// is a real compatibility mismatch - caught the hard way via
		// VUID-vkCmdDrawIndexed-renderPass-02684 the first time a real
		// shadow pass tried to draw with a pipeline built against it.
		// Built lazily via BuildDepthOnlyRenderPass() on first use; every
		// individual light's own FBORecord::renderPass (built the same
		// way, from the same shadow-map format) is render-pass-compatible
		// with this one, so a pipeline built against this template can
		// validly be used within any of them.
		VkRenderPass shadowPipelineRenderPass;

		// Lazily builds fbo.renderPass (depth-only, LOAD_OP_CLEAR,
		// STORE_OP_STORE so the shadow map survives to be sampled in the
		// main pass, finalLayout=SHADER_READ_ONLY_OPTIMAL) if not
		// already built. Returns false only on a real Vulkan failure.
		bool BuildDepthOnlyRenderPass(FBORecord &fbo, const VkFormat depthFormat);
		// Builds fbo.renderPass from fbo.pendingAttachments (0+ color +
		// at most 1 depth, in accumulation order for color) - the general
		// case BuildDepthOnlyRenderPass() above predates (a G-buffer's
		// several attachments, or a post-effect's single color render
		// target). Each color attachment: LOAD_OP_CLEAR/STORE_OP_STORE,
		// finalLayout=SHADER_READ_ONLY_OPTIMAL (same "ready to sample
		// immediately after" reasoning as the depth-only case - deferred
		// shading/post-effects always read these back as textures in a
		// later pass). Sets fbo.colorAttachmentCount/hasDepthAttachment
		// for CreatePipeline()'s color-blend-state sizing. Returns false
		// only on a real Vulkan failure (including "no pending
		// attachments at all", which should never happen given the
		// caller only invokes this once pendingAttachments is non-empty).
		bool BuildMultiAttachmentRenderPass(FBORecord &fbo);
		// (Re)builds fbo.framebuffersByTarget[0] from fbo.pendingAttachments'
		// *current* texture views - factored out of BindFramebuffer()'s
		// finalize path so the same logic can also rebuild a combined
		// framebuffer that InvalidateFramebuffersForTexture() tore down
		// (see its comment). Requires fbo.renderPass to already exist;
		// returns false on a real Vulkan failure or if any attachment's
		// texture/view is missing.
		bool BuildCombinedFramebuffer(FBORecord &fbo);
		// Destroys any FBORecord's cached VkFramebuffer(s) that reference
		// `texture` - called from UploadTexture2D() right before it
		// destroys+recreates a texture's VkImage/VkImageView on a size
		// change. A VkFramebuffer bakes in specific VkImageView handles at
		// creation time (VkFramebufferCreateInfo::pAttachments); once the
		// view they point at is destroyed, using that framebuffer again is
		// exactly VUID-VkRenderPassBeginInfo-framebuffer-parameter ("pAttachments[0]
		// VkImageView ... is invalid"), found via every IEffect's own FBO
		// (Color_Attachment0 texture, built at Init()-time and then resized
		// once to match the real window size - a HiDPI-driven resize event
		// firing right after window creation on macOS) crashing on its
		// first real bind after that resize. Only clears
		// framebuffersByTarget, not renderPass or pendingAttachments - the
		// render pass's attachment *format* doesn't change on a resize,
		// only the image/view, and pendingAttachments (kept populated
		// after the FBO's first build, not cleared - see BindFramebuffer())
		// is exactly what BuildCombinedFramebuffer() needs to rebuild the
		// framebuffer lazily on the FBO's next real bind.
		void InvalidateFramebuffersForTexture(const DeviceHandle texture);
		// Ends whatever offscreen render pass is currently open
		// (vkCmdEndRenderPass only - does not submit) - called by
		// AttachFramebufferTexture2D() when re-attaching a new target
		// within an already-open FrameBuffer::Bind() session (a point
		// light's 2nd-6th cubemap face), since Vulkan can't swap a
		// render pass's target attachment mid-pass.
		void EndOffscreenRenderPassIfOpen();
		// Ends+submits+waits on offscreenCommandBuffer if a session is
		// currently recording - the same submit/fence sequence
		// BindFramebuffer()'s fbo==0 branch already does, factored out so
		// BlitFramebuffer() can call it too. See BlitFramebuffer()'s own
		// comment on why it needs this: a Write-bound destination FBO
		// (e.g. resolvedFBO, bound only to be a blit target, never
		// actually drawn into) still records a real "begin render pass
		// (clears to the render pass's hardcoded clear value) ... end
		// render pass" into offscreenCommandBuffer at Bind() time, same as
		// any other FBO - it just never submits until something later
		// ends the session. Left unflushed, that recorded-but-not-yet-
		// executed clear runs *after* BlitFramebuffer()'s own separate,
		// synchronously-submitted command buffer already wrote real
		// resolved content into the same image, silently wiping it back
		// out - a real bug (blank/black resolvedColor) found running
		// MSAATest, the first thing to ever bind an FBO purely as a blit
		// destination.
		void FlushOffscreenCommandBuffer();
		// Drains every offscreen slot. The broad barrier - for teardown,
		// buffer reallocation, and anything that has to know no offscreen
		// GPU work is outstanding. Not for the record path: that only
		// needs the one slot it is about to reuse, via WaitOffscreenSlot.
		void WaitOffscreenSubmitIfPending();
		void WaitOffscreenSlot(OffscreenSlot &slot);
		void WaitAllFrameFences();
		// Wait in-flight frame/offscreen work before destroying or
		// reallocating a host-mapped buffer that may still be bound.
		void EnsureHostMappedBufferWritable();
		// If a deferred offscreen batch is still recording but the draw
		// target is the swapchain (currentBoundFBO==0 inside BeginFrame),
		// flush that batch and point activeCommandBuffer at the frame CB.
		void EnsureFrameCommandBufferForSwapchainDraw();
		// Records into transferCommandBuffer (begins it on first use).
		VkCommandBuffer BeginOrGetTransferCommandBuffer();
		// Submits pending UploadTexture2D/GenerateMipmap work and frees
		// staging buffers. Safe no-op when nothing is pending.
		void FlushPendingTransfers();
		void CreatePipelineCache();
		void DestroyPipelineCache();
		// Sets deviceIdleSinceLastSubmit=false then vkQueueSubmit.
		VkResult SubmitGraphics(uint32 submitCount, const VkSubmitInfo *infos, VkFence fence);
		// Begins recording offscreenCommandBuffer if this is the first
		// render pass in the current Bind()/UnBind() session, then
		// vkCmdBeginRenderPass()s `targetFramebuffer` (already resolved
		// by the caller) and sets a default full-target viewport/scissor.
		// Shared by AttachFramebufferTexture2D() (building a target for
		// the first time, or a point light's next cubemap face) and
		// BindFramebuffer() (re-entering an already-built target on a
		// later frame, when no attach call happens again - see
		// FBORecord::lastTarget's comment for why that path exists).
		void BeginOffscreenRenderPassForTarget(FBORecord &fbo, const VkFramebuffer targetFramebuffer);

		// Vertex buffer(s) / index buffer for a "VAO" handle - see the
		// header comment on the `pipelines` field below for the broader
		// vertex-input limitation this is part of. GL's VAO bakes in
		// attribute pointers/enables *and* the bound array+element buffers;
		// this backend's vertex *attribute layout* is already baked into
		// the pipeline (CreatePipeline()'s VkVertexInputState, built from
		// PipelineDesc::vertexLayout - one VkVertexInputBindingDescription
		// per AttributeBuffer the mesh's Geometry has), so all a "VAO"
		// needs to remember here is which buffer(s) to bind at draw time,
		// in the same order LinkProgram()'s/CreatePipeline()'s bindings
		// were declared. Built up the same way GL builds a VAO -
		// BindVertexArray(cmd, vao) selects which one BindArrayBuffer()/
		// BindElementBuffer() write into next (mirroring glBindVertexArray()
		// making those the implicit target of subsequent glBindBuffer()
		// calls) - and later just re-selected (via the same
		// BindVertexArray() call) as the active one for DrawElements()/
		// DrawElementsInstanced() to read from.
		// vertexBuffers is a *vector*, not a single handle, because a mesh
		// can have more than one AttributeBuffer - e.g. ParticlesExample's
		// ParticleEmitter adds a second, separately-updated per-instance
		// buffer (divisor=1) alongside the base Plane geometry's own
		// buffer, via IRenderingInstancedComponent::AddBuffer(). Each
		// AttributeBuffer gets its own VkVertexInputBindingDescription
		// (binding 0, 1, 2...) in pipeline creation, in the exact order
		// IRenderer::BindMesh() calls BindArrayBuffer() for each one - a
		// single-buffer field here silently dropped every buffer but the
		// last one bound (found via ParticlesExample rendering nothing at
		// all on Vulkan while working fine on GL, which has no equivalent
		// "one current buffer" limitation).
		struct VaoRecord
		{
			std::vector<DeviceHandle> vertexBuffers;
			DeviceHandle indexBuffer;
			VaoRecord() : indexBuffer(0) {}
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
			// STREAM/DYNAMIC vertex+attribute buffers: ring of
			// (MAX_FRAMES_IN_FLIGHT+1) host-mapped VkBuffers. Update()
			// advances writeIndex and retargets buffer/mapped so Draw sees
			// the fresh slot without a CPU fence wait (Update runs before
			// BeginFrame's fence wait, so a 2-slot ping-pong still races;
			// +1 covers that ordering). Static buffers leave ringCount=0.
			static const uint32 kMaxStreamRing = MAX_FRAMES_IN_FLIGHT + 1;
			uint32 streamRingCount;
			uint32 streamWriteIndex;
			VkBuffer streamBuffers[kMaxStreamRing];
			VmaAllocation streamAllocations[kMaxStreamRing];
			void* streamMapped[kMaxStreamRing];
			// The following four fields are only meaningful for a uniform
			// buffer created at one of IsPerObjectDynamicBinding()'s
			// binding points - see that function's comment for the "why".
			// isDynamicUniform=false (the common case: vertex/index
			// buffers, and every per-frame-constant UBO) leaves them at
			// their default/unused values.
			bool isDynamicUniform;
			// `size` above rounded up to a multiple of
			// minUniformBufferOffsetAlignment - the actual byte stride
			// between consecutive slots (VkDescriptorBufferInfo::range
			// still uses the unpadded `size`, since that's the shader
			// block's real declared size - the padding exists only to
			// satisfy vkCmdBindDescriptorSets' dynamic-offset alignment
			// rule, it's never meant to be read as data).
			VkDeviceSize alignedSlotSize;
			uint32 slotCount;
			// Which slot the most recent ReplaceUniformBuffer() call
			// wrote to - BindCurrentPipelineDescriptorSets() reads this
			// to build vkCmdBindDescriptorSets' pDynamicOffsets for
			// whichever draw comes next, which by construction is always
			// the object/material-switch that slot's data belongs to
			// (ReplaceUniformBuffer() is always called immediately before
			// the pipeline/descriptor bind + draw it's meant for - see
			// IRenderer.cpp's SendGlobalUniforms()/BindMesh() ordering).
			uint32 currentSlot;
			// ReplaceUniformBuffer() calls since BeginFrame() reset -
			// used to detect mid-frame ring wrap (GPU still references
			// earlier slots in this command buffer).
			uint32 writesThisFrame;
			BufferRecord() : buffer(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), mapped(NULL), size(0),
				streamRingCount(0), streamWriteIndex(0),
				isDynamicUniform(false), alignedSlotSize(0), slotCount(1), currentSlot(0), writesThisFrame(0)
			{
				for (uint32 i = 0; i < kMaxStreamRing; i++)
				{
					streamBuffers[i] = VK_NULL_HANDLE;
					streamAllocations[i] = VK_NULL_HANDLE;
					streamMapped[i] = NULL;
				}
			}
		};
		std::map<DeviceHandle, BufferRecord> buffers;
		DeviceHandle nextBufferHandle;
		bool AllocHostVisibleVertexBuffer(uint32 allocLength, VkBuffer *outBuffer, VmaAllocation *outAllocation, void **outMapped);
		void DestroyBufferRecordResources(BufferRecord &rec);

		// True for the handful of UBO binding points PyrosShader.glsl
		// declares that genuinely vary *within* a single frame - either
		// per-object (rewritten for every mesh drawn: ObjectMatrixUniforms,
		// BoneMatrices, VelocityObjectUniforms, ObjectLightCounts, and
		// LightsBlock - "each object only gets its nearby lights", see
		// IRenderer.cpp's comment) or per-material-switch
		// (MaterialUniforms, "re-uploaded when the active material
		// changes" - still multiple times a frame in any scene using more
		// than one material). Every other UBO binding
		// (GlobalMatrices/DirectionalShadowBlock/PointShadowBlock/
		// SpotShadowBlock/VertexFrameUniforms/VelocityFrameUniforms/
		// AmbientLightUniforms) is written at most once per frame and
		// stays a plain, non-dynamic descriptor.
		//
		// Why this list exists at all: a plain (non-dynamic)
		// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER descriptor only ever points at
		// one fixed buffer location - vkCmdDrawIndexed doesn't read a
		// UBO's content until the whole command buffer is submitted and
		// the GPU actually executes it, which happens *after* every
		// object in the frame has already been CPU-side memcpy'd into
		// that same buffer via ReplaceUniformBuffer(). With a single,
		// repeatedly-overwritten buffer, every draw in the frame ends up
		// reading whatever the *last* object wrote - confirmed directly:
		// a two-object scene (any material, no lights even) reproducibly
		// renders only the second object, regardless of which one is
		// positioned where. GL never had this problem because its
		// immediate-mode execution model runs each draw call as it's
		// issued, using whatever uniform value is current *right then*.
		// The fix: these bindings get their own multi-slot buffer (one
		// slot per ReplaceUniformBuffer() call, wrapping around - safe
		// given this backend's single-frame-in-flight model, see
		// BeginFrame()'s fence wait) and a
		// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor, with
		// vkCmdBindDescriptorSets' pDynamicOffsets selecting the right
		// slot at the moment each individual draw is recorded - so the
		// *recorded command* captures which slot belongs to it, not just
		// "whatever's in the buffer" at some later, shared execution time.
		// First binding number CompileShaderStage() hands out to
		// AutoFixForVulkan()'s synthesized UBOs (nextAutoUboBinding's
		// initial value, VulkanRenderDevice.cpp's constructor) - chosen to
		// sit one past the highest engine-reserved binding used below
		// (42), so every auto-synthesized binding is >= this value and
		// every engine-reserved one is <, with no overlap either way.
		static const uint32 kFirstAutoUboBinding = 43;

		static bool IsPerObjectDynamicBinding(const uint32 bindingPoint)
		{
			switch (bindingPoint)
			{
			case 0:  // BIND_GlobalMatrices - rewritten once per shadow-
			         // casting light's depth pass (IRenderer.cpp's
			         // InitRender() reassigns ProjectionMatrix/ViewMatrix
			         // to each light and calls RenderObject(), which
			         // re-uploads this UBO) *and* once more for the main
			         // camera pass, all within a single frame - the exact
			         // multi-write-per-frame pattern the other bindings
			         // below needed dynamic offsets for. Missed originally
			         // because it looks like a once-per-frame UBO from the
			         // main pass alone.
			case 1:  // BIND_LightsBlock
			case 21: // BIND_AmbientLightUniforms - IRenderer::GlobalLight is
			         // a per-instance member, but every IRenderer instance
			         // that draws a uniform-block material within the same
			         // frame (the main SceneEditor viewport, AxisHelper's own
			         // tiny gizmo viewport, camera/thumbnail preview
			         // renderers) shares this one buffer - the exact
			         // single-shared-buffer, multiple-writes-before-submit
			         // race this list exists to solve for every binding
			         // below. Missed originally because it looks like a
			         // once-per-frame UBO from the main pass alone (same
			         // blind spot as binding 0's). Symptom: whichever
			         // IRenderer instance rendered *last* in a frame silently
			         // won this binding for every draw already recorded
			         // against it that frame, regardless of which instance's
			         // ambient value that draw was actually meant to see -
			         // e.g. AxisHelper's fixed gizmo ambient overwriting the
			         // real scene's ambient on the main viewport's own
			         // objects, or the scene ambient never visibly applying
			         // at all once the axis gizmo/preview renderers overwrote
			         // it.
			case 16: // BIND_VertexFrameUniforms - IslandDemo's water
			         // multipass Enable/DisableClipPlane + SetClipPlane0
			         // rewrites this three times per frame (reflection,
			         // refraction, main). Each offscreen UnBind flushes
			         // today, but keep it dynamic so a future batched
			         // path can't stomp clip-enable under an in-flight
			         // draw (symptom: island triangles discard-flicker).
			case 18: // BIND_ObjectMatrixUniforms
			case 19: // BIND_BoneMatrices
			case 20: // BIND_VelocityObjectUniforms
			case 22: // BIND_MaterialUniforms
			case 23: // BIND_ObjectLightCounts
			case 40: // WaterVertParams (IslandDemo WaterMaterial)
			case 41: // WaterFragParams (IslandDemo WaterMaterial)
			// DeferredRenderer's second-pass lighting materials (see
			// IMaterial.h's comment on extraUniforms[2]) - each of these
			// is rewritten once per light of that light's type within a
			// single frame (secondpassPoint.glsl/secondpassSpot.glsl draw
			// once per point/spot light via the same shared material), the
			// exact same "single shared buffer, multiple writes before
			// submit" race this list exists to solve. Ambient/LastPass
			// (27/37) only ever draw once per frame today but are included
			// for uniformity/future-proofing - dynamic-vs-not is invisible
			// to anything except CreateUniformBuffer()'s allocation size.
			case 27: // AmbientFragParams
			case 32: // DirectionalFragParams
			case 33: // PointVertParams
			case 34: // SpotVertParams
			case 37: // LastPassFragParams
			case 38: // PointFragParams
			case 39: // SpotFragParams
			case 42: // ParticleVertParams (ParticlesExample) - carries a
			         // per-object uModelMatrix, and both of the example's
			         // ParticleEmitters share one ParticleMaterial instance
			         // (hence one CreateUniformBuffer() call, one shared
			         // buffer) - the exact same single-shared-buffer,
			         // multiple-writes-before-submit race as the rest of
			         // this list. Missed when this material was first wired
			         // up because the bug was masked by two unrelated, more
			         // fundamental gaps (a second vertex buffer binding
			         // never getting bound at all, and a zero-length
			         // initial buffer allocation Vulkan silently refused) -
			         // fixing those was necessary before this one could
			         // even become visible: symptom was "only one emitter
			         // ever renders" (matches this binding's whole class of
			         // bug exactly, see the comment above), not caught by
			         // validation since a stale-but-valid ModelMatrix isn't
			         // a validation-detectable error.
				return true;
			default:
				// Every AutoFixForVulkan-synthesized UBO (CustomShaderMaterial
				// shaders with no explicit UBO_BINDING macros, e.g.
				// particleSystem.glsl) is, by construction, exactly the same
				// "loose per-draw uniforms wrapped into one UBO" shape as the
				// fixed bindings enumerated above - and just as capable of
				// being written multiple times in a frame before any of
				// those writes are actually consumed by the GPU (e.g. once
				// for a shadow-casting instanced component's shadow-pass
				// draw, once more for its main-pass draw, same
				// uProjectionMatrix/uViewMatrix race case 0 exists for).
				// Unlike the fixed list, these bindings are assigned at
				// shader-compile time, not known in advance - so instead of
				// enumerating them individually, anything at or past the
				// reserved starting point is covered categorically. Found
				// via a real, reproducible bug: p3d::ParticleSystem's
				// particles rendering at the wrong screen position on
				// Vulkan only (GL, with no such single-shared-buffer
				// hazard, was unaffected) - this UBO was the one binding in
				// its whole program still using a plain, non-dynamic
				// buffer.
				return bindingPoint >= kFirstAutoUboBinding;
			}
		}
		// Cap on per-draw dynamic UBO ring depth. CreateUniformBuffer()
		// also caps total ring bytes (~64MB) so BoneMatrices (large slots)
		// get fewer entries than ObjectMatrix. Mid-frame wrap reuses a
		// slot still referenced by an earlier vkCmdDraw - Vulkan then
		// shows static geometry (walls) "jumping" to later ModelMatrices;
		// GL hides it via driver sync/orphaning. Cross-frame wrap is fine:
		// BeginFrame waits on frameFence first.
		static const uint32 DYNAMIC_UBO_SLOT_COUNT = 65536;
		// Queried once from the physical device - vkCmdBindDescriptorSets'
		// dynamic offsets must be a multiple of this.
		VkDeviceSize minUniformBufferOffsetAlignment;

		// Queried once from the physical device - the bitmask of sample
		// counts every color AND depth attachment format can agree on
		// simultaneously (framebufferColorSampleCounts &
		// framebufferDepthSampleCounts), since a multisample FBO built by
		// this backend always attaches both. MoltenVK's real value varies
		// by Apple Silicon generation - never trust a caller's requested
		// sample count blindly, see ClampSampleCount()'s comment.
		VkSampleCountFlags supportedSampleCounts;
		static VkSampleCountFlagBits ClampSampleCount(const VkSampleCountFlags supported, const uint32 requested);

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
			// Set by CompileShaderStage() when SpirvShaderCompiler::
			// AutoFixForVulkan() (see ShaderCompiler.h) found this stage's
			// source had loose non-opaque uniforms and wrapped them into a
			// synthesized UBO - mirrors SpirvAutoUboResult's fields directly
			// rather than storing that type (same SPIRV_TOOLING-optionality
			// reasoning as `spirv` above). GetAutoUniformBlockLayout() reports
			// this back to callers (CustomShaderMaterial's constructor) so
			// IMaterial::extraUniforms[] can be auto-populated with no
			// hand-authored wiring - see that method's comment.
			bool autoUboHasBlock;
			uint32 autoUboBinding;
			std::string autoUboBlockName;
			uint32 autoUboSize;
			std::map<std::string, uint32> autoUboOffsets;
			ShaderStageRecord() : engineShaderType(0), module(VK_NULL_HANDLE), autoUboHasBlock(false), autoUboBinding(0), autoUboSize(0) {}
		};
		std::map<DeviceHandle, ShaderStageRecord> shaderStages;
		DeviceHandle nextShaderStageHandle;

		// Next globally-unique UBO binding CompileShaderStage() hands to
		// SpirvShaderCompiler::AutoFixForVulkan() for a stage's synthesized
		// loose-uniform block, if it turns out to need one - see
		// IMaterial::ExtraUniformsBlock's comment on why this has to be a
		// persistent, ever-incrementing counter (bindings flow through
		// this device's *global* uniformBufferByBindingPoint map, not a
		// per-program one) and never reused, even across stages that
		// didn't end up needing a block. Starts one past every binding
		// this engine's own shipped shaders hand-assign today (see
		// PyrosShader.glsl/post-effects/DeferredRenderer/the 3 example
		// materials' UBO_BINDING numbers) - if a future hand-authored
		// shader claims a new fixed number in that range, bump this to
		// stay past it.
		uint32 nextAutoUboBinding;

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
			// Which of reflectedBindings' UBO descriptors have already
			// been written (vkUpdateDescriptorSets) into descriptorSet -
			// BindUniformBlockIfPresent() is called from BindMesh(), once
			// per (mesh, shader) *cache miss*, so it runs again for every
			// distinct mesh that first uses this program, not just once
			// per program - but the content written (which UBO buffer
			// backs a given binding) never varies by mesh, it's a
			// program-level fact. Re-writing it anyway is more than just
			// wasted work: this descriptor set is shared per-program
			// (see the comment on descriptorSet above), so if an earlier
			// mesh sharing this program already drew (and thus already
			// vkCmdBindDescriptorSets'd this exact set) earlier in the
			// same still-recording command buffer - a real case, not
			// hypothetical: IRenderer's shared shadowMaterial is used by
			// every shadow-casting mesh, so a scene with more than one
			// (the common case) hits this on the very first frame a
			// second mesh casts a shadow - re-writing it now violates
			// "no update after bind without VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT"
			// and corrupts the command buffer's recording state entirely
			// (caught via VUID-vkCmdBindPipeline-commandBuffer-recording
			// cascading into every subsequent command in that buffer).
			// Tracking which bindings are already written and skipping
			// them is correct, not just a workaround - the value being
			// written is unconditionally the same either way.
			std::set<uint32> writtenBindings;
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
			// Declared array length per sampler binding (e.g.
			// PyrosShader.glsl's `uPointShadowMaps[4]` -> 4; a plain
			// non-array sampler like `uColormap` -> 1, same as if this
			// map had no entry at all). SendUniformInt() writes this many
			// descriptors (all pointing at the same texture) rather than
			// just element 0, so every entry the shader's array *type*
			// declares is left valid - PyrosShader.glsl's PCFPOINT/PCFSPOT
			// calls only ever read index 0 with a compile-time-constant
			// literal, but Vulkan validation still requires every
			// declared array element to be a valid descriptor
			// (VUID-vkCmdDrawIndexed-None-08114) regardless of whether
			// the shader dynamically indexes past it.
			std::map<uint32, uint32> samplerArraySizes;
			// SAMPLER_KIND_* bitmask per sampler binding, from SPIR-V
			// reflection - which fallback image a binding needs when
			// nothing bound a real one. See BindCurrentPipelineDescriptorSets().
			std::map<uint32, uint32> samplerKinds;
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

		// Fallback ("dummy") images for sampler bindings a pipeline
		// declares but nothing ever binds a real texture to. GL tolerates
		// that - an unbound sampler just reads black - so materials have
		// always been free to compile a shader variant whose sampler they
		// never fill (a PBRMap material with no metallic/roughness map, a
		// light that casts no shadow). Vulkan requires every descriptor a
		// bound pipeline statically references to be valid at draw time
		// (VUID-vkCmdDrawIndexed-None-08114), even behind a branch the
		// shader never takes.
		//
		// Four, not one: a descriptor must match the *declared* sampler
		// type, so a 2D image cannot back a samplerCube and a plain
		// sampler cannot back a sampler2DShadow/samplerCubeShadow.
		// DeferredRenderer already hit this and hand-rolled two of these
		// as members of its own (dummyShadow2D/dummyShadowCube); this is
		// the same idea applied generically, from reflection, so a new
		// shader variant can't reintroduce the bug one call site at a time.
		// Indexed by the SAMPLER_KIND_* bitmask.
		enum { SAMPLER_KIND_CUBE = 1, SAMPLER_KIND_DEPTH = 2, SAMPLER_KIND_COUNT = 4 };
		DeviceHandle fallbackSamplerTextures[SAMPLER_KIND_COUNT];
		DeviceHandle GetOrCreateFallbackTexture(const uint32 samplerKind);
		// Writes a type-matching fallback into every sampler binding of the
		// current pipeline's set that nothing has written yet.
		void FillUnwrittenSamplerDescriptors(ProgramRecord &prog, const VkDescriptorSet samplerSet);

		// Dirty-check for SendUniformInt()'s sampler-descriptor writes -
		// keyed by (pipeline, binding), remembers the VkImageView last
		// written there. Without this, SendUniformInt() called
		// vkUpdateDescriptorSets() unconditionally on every single
		// RenderObject() call, even when the texture unit's actual
		// VkImageView hadn't changed since the previous draw using the
		// same pipeline - harmless-looking, but a real bug once a scene
		// draws the *same* (mesh, shader, target) pipeline many times in
		// one frame with unchanged texture bindings (e.g. DeferredRenderer's
		// second-pass lighting materials, redrawn once per light of that
		// type - 100 point lights in DeferredRendering's example scene):
		// each redundant vkUpdateDescriptorSets() call mutates a
		// VkDescriptorSet already referenced by the *previous* light's
		// not-yet-submitted command buffer (this backend submits once per
		// frame, not per object - see CommandBufferHandle's comment in
		// IRenderDevice.h), which is exactly what
		// VUID-vkCmdBindDescriptorSets-commandBuffer-recording and its
		// cascade of "destroyed or updated without UPDATE_AFTER_BIND"
		// follow-on errors flag - found via a real validation-layer run
		// of DeferredRendering, not derived. Skipping genuinely-identical
		// rewrites is the correct fix for this specific case (same
		// texture bound every draw) and a strict improvement for the
		// general case (fewer descriptor writes overall); it does not
		// solve a *different*-texture-per-draw pattern sharing one
		// pipeline within a frame, which would need real
		// UPDATE_AFTER_BIND descriptor-indexing support - not needed by
		// any current caller.
		std::map<std::pair<DeviceHandle, uint32>, VkImageView> lastWrittenSamplerView;

		// MUST be called immediately before destroying any VkImageView that
		// could have been written into a sampler descriptor. The cache above
		// keys on the raw VkImageView handle, and a handle is only unique
		// while its object is alive - MoltenVK readily hands the exact same
		// address back out for a view created right after one is destroyed
		// (a resize destroys and recreates all ~12 render-target views in a
		// fixed order, which is close to a best case for allocator reuse).
		// When that happens the "is this already written?" check compares
		// new-handle == old-handle, concludes the descriptor is current, and
		// skips vkUpdateDescriptorSets() - leaving the descriptor set still
		// pointing at the *destroyed* view. The next draw then samples freed
		// memory, which faults the GPU and surfaces as
		// VK_ERROR_DEVICE_LOST from the following frame's vkWaitForFences
		// (reproduced live: every resize step clean, device lost on the
		// very next frame). Validation layers hid it - they change
		// allocation patterns enough that handles usually don't get
		// recycled, and a recycled handle refers to a genuinely live object
		// so no VUID fires either way.
		void ForgetSamplerDescriptorsForView(const VkImageView view);

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
			// Sampling view - VK_IMAGE_VIEW_TYPE_CUBE (all 6 layers) if
			// isCubemap, else VK_IMAGE_VIEW_TYPE_2D. What SendUniformInt()
			// writes into a descriptor for `samplerCube`/`sampler2D`/
			// `sampler2DShadow`/`samplerCubeShadow` GLSL uniforms alike.
			VkImageView view;
			VkSampler sampler;
			uint32 width, height;
			// 1 unless UploadTexture2D() was called with willMipmap=true,
			// in which case it's floor(log2(max(width,height)))+1 (the
			// full chain down to a 1x1 level) - decided once, at image-
			// creation time, since Vulkan (unlike GL) can't extend a
			// VkImage's level count after creation. GenerateMipmap()
			// actually fills levels 1..mipLevels-1 via cascading blits;
			// `view`'s subresourceRange.levelCount already covers all of
			// them regardless of whether GenerateMipmap() has run yet
			// (matches GL's own contract - image() is still "valid",
			// just not blurred/filtered by mip level, until mips are
			// actually generated).
			uint32 mipLevels;
			// True once UploadTexture2D() has actually staged real pixel
			// data into level 0 (its data!=NULL branch) - false for a
			// CreateEmptyTexture()-only texture (every render target:
			// G-buffer attachments, post-effect RTTs, shadow maps),
			// which leaves level 0 in VK_IMAGE_LAYOUT_UNDEFINED until
			// something actually renders into it. GenerateMipmap() reads
			// this to know whether level 0 is really in
			// SHADER_READ_ONLY_OPTIMAL (safe to blit *from*) - found via
			// a real validation-layer run (VUID-vkCmdDraw-None-09600):
			// CreateEmptyTexture()'s own Mipmapping=true default means
			// GenerateMipmap() runs immediately on every render target
			// too, not just real LoadTexture()/UpdateData() uploads, and
			// assuming level 0 was already SHADER_READ_ONLY_OPTIMAL then
			// was a real bug (barrier lying about the actual oldLayout).
			bool baseLevelHasRealData;
			// True once GenerateMipmap() has actually finished blitting
			// real content into levels 1..mipLevels-1 - *not* the same as
			// hasMipmap (which just reflects the requested min-filter
			// mode, e.g. LinearMipmapLinear, regardless of whether any
			// mip data actually exists yet). Drives
			// RebuildSamplerIfDirty()'s maxLod: a texture whose mips were
			// requested (hasMipmap) but never generated (a pure render
			// target - see GenerateMipmap()'s baseLevelHasRealData check)
			// still needs maxLod clamped to 0, or the driver's automatic
			// LOD selection can pick a level >0 at any time (screen-space
			// derivatives, not something the engine controls per-draw)
			// and sample genuinely undefined image memory - found via a
			// real validation-layer run (VUID-vkCmdDraw-None-09600) on
			// SSAOExample/ScreenSpaceReflection, both post-effect chains
			// whose IEffect::attachment render targets default to
			// Mipmapping=true the same as every other texture.
			bool mipsGenerated;
			// False until something has actually put this image into a
			// real layout - an upload's transfer barrier, or a render pass
			// that has it as an attachment. A freshly created image is
			// VK_IMAGE_LAYOUT_UNDEFINED, and sampling an UNDEFINED image is
			// invalid (VUID-vkCmdDraw-None-09600) even though the
			// descriptor itself is perfectly well-formed - which is why
			// this is a *separate* problem from the unwritten-descriptor
			// one fallbackSamplerTextures solves, and why it bites exactly
			// the textures that pass through CreateEmptyTexture() and get
			// sampled before anything draws into them (post-effect
			// attachments on their first frame; see also
			// DeferredRenderer's previousFrameColorTexture, which
			// hand-rolls a warm-up render pass for this same reason).
			// SendUniformInt() transitions any image still flagged here
			// before writing its descriptor. Contents stay undefined,
			// which they already were.
			bool layoutInitialized;
			// True between an upload recording its staging copy + barrier
			// and that transfer actually being submitted. Uploads are
			// batched into one command buffer and flushed at BeginFrame(),
			// which is fine for an asset loaded before it is ever drawn -
			// but a texture uploaded *during* a frame that also samples it
			// (a post-effect building its noise texture in its constructor,
			// mid demo-switch) is read while its copy is still unsubmitted:
			// the image is legitimately still VK_IMAGE_LAYOUT_UNDEFINED at
			// draw time (VUID-vkCmdDraw-None-09600), even though the
			// barrier that fixes it has already been recorded.
			bool pendingUpload;
			VkFormat format;
			uint32 wrapS, wrapT;     // TextureRepeat::* values
			uint32 minFilter, magFilter; // TextureFilter::* values
			bool hasMipmap;
			bool samplerDirty;
			// True once any CreateEmptyTexture()/LoadTexture() call for
			// this handle targeted a cubemap face (TranslateTextureTarget()
			// output, not TextureType::Texture) - see UploadTexture2D()'s
			// comment. image is then a 6-layer, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
			// image instead of a plain 2D one.
			bool isCubemap;
			// True for TextureDataType::DepthComponent* - drives
			// SetTextureCompareMode()'s effect on RebuildSamplerIfDirty()
			// (shadow-comparison sampling only makes sense for a depth
			// format) and CreatePipeline()... no, unused there; kept here
			// since format alone doesn't reliably distinguish "depth
			// texture used as a render target" from "some other texture
			// that happens to share a single-channel float format".
			bool isDepthTexture;
			bool compareModeEnabled;
			// Render-*target* views, as opposed to `view` above (always a
			// full/complete *sampling* view). A cubemap depth texture
			// used as a shadow map needs a separate 2D view *per face*
			// (baseArrayLayer=face, one render pass per face - see
			// FBORecord::framebuffersByTarget) to be usable as a
			// depth-attachment target at all; a plain 2D depth texture's
			// render-target view is the same as its sampling view (no
			// separate entry needed - see GetOrCreateRenderTargetView()).
			// Keyed by the same `nativeTextureTarget`
			// TranslateTextureTarget() produced for that face/target.
			std::map<uint32, VkImageView> renderTargetViewsByTarget;
			// VK_SAMPLE_COUNT_1_BIT unless this came from
			// UploadTexture2DMultisample() - a real (already
			// device-capability-clamped, see ClampSampleCount()) multisample
			// image otherwise. Never mixed with mipLevels>1 (Vulkan doesn't
			// support multisample+mipmapped images) or isCubemap (this
			// engine never creates a multisample cubemap).
			VkSampleCountFlagBits samples;
			TextureRecord()
				: image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), view(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE),
				  width(0), height(0), mipLevels(1), baseLevelHasRealData(false), mipsGenerated(false), format(VK_FORMAT_R8G8B8A8_UNORM),
				  wrapS(TextureRepeat::Repeat), wrapT(TextureRepeat::Repeat),
				  minFilter(TextureFilter::Linear), magFilter(TextureFilter::Linear),
				  hasMipmap(false), samplerDirty(true), isCubemap(false), isDepthTexture(false), compareModeEnabled(false),
				  samples(VK_SAMPLE_COUNT_1_BIT) {}
		};
		std::map<DeviceHandle, TextureRecord> textures;
		// ImTextureID (VkDescriptorSet) per texture, for the render-target
		// viewer. Cached because ImGui_ImplVulkan_AddTexture() allocates a
		// descriptor set per call - doing that every frame would exhaust
		// ImGui's pool in seconds.
		//
		// The view and sampler it was built against are kept alongside it,
		// because caching on the texture handle alone is not enough: the
		// sampler is rebuilt lazily whenever a filter/wrap setter dirties it
		// and views are recreated on resize, and a descriptor still pointing
		// at the destroyed one fails VUID-vkCmdDrawIndexed-None-08114 the
		// next time ImGui draws. Rebuilt when either has changed.
		struct ImGuiTextureBinding
		{
			void *set;          // VkDescriptorSet
			VkImageView view;
			VkSampler sampler;
			ImGuiTextureBinding() : set(NULL), view(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE) {}
		};
		std::map<DeviceHandle, ImGuiTextureBinding> imguiTextureIDs;
		// Moves an image that has never been written out of
		// VK_IMAGE_LAYOUT_UNDEFINED so it can legally be sampled - see
		// TextureRecord::layoutInitialized. Declared here rather than
		// beside GetOrCreateFallbackTexture() (its sibling in purpose)
		// because TextureRecord is only defined this far down the class.
		void EnsureSampledLayout(TextureRecord &tex);
		DeviceHandle nextTextureHandle;
		// Lazily (re)builds tex.sampler from its wrap/filter state if
		// dirty - see the comment on TextureRecord::samplerDirty. Returns
		// false (logging) only on a real vkCreateSampler failure; true
		// otherwise, including the already-clean-and-valid case.
		bool RebuildSamplerIfDirty(TextureRecord &tex);
		// Binds currentPipeline's owning program's UBO set (set=0) and
		// the pipeline's own sampler set (set=1, if any) -
		// vkCmdBindDescriptorSets(), called from DrawElements()/
		// DrawElementsInstanced() right before the actual draw rather
		// than from BindPipeline() - see BindPipeline()'s comment for
		// why binding it earlier (before SendGlobalUniforms()/
		// SendUserUniforms() have written this object's texture/shadow-
		// map descriptors via SendUniformInt()) is invalid.
		void BindCurrentPipelineDescriptorSets();
		// Lazily creates `descriptorPool` (idempotent - returns true
		// immediately if it already exists) - see its header comment for
		// sizing. Was inlined only in BindUniformBlockIfPresent(); factored
		// out so CreatePipeline() can call it too - a pipeline's own
		// sampler set (set=1) is allocated from this same pool, but
		// CreatePipeline() runs *before* BindUniformBlockIfPresent() in
		// RenderObject()'s call order (BindMesh() first, uniform-sending
		// after), so for any material that never sends a "regular" UBO
		// block (every CustomShaderMaterial, since SupportsUniformBlocks()
		// defaults false) the pool didn't exist yet the first time a
		// pipeline needed it - found via ParticlesExample (the only
		// example whose *entire* scene is CustomShaderMaterial objects,
		// so nothing else incidentally created the pool first).
		bool EnsureDescriptorPool();
		// Returns (creating if needed) the VkImageView usable as a
		// depth-attachment render target for `nativeTextureTarget` - see
		// TextureRecord::renderTargetViewsByTarget. VK_NULL_HANDLE only
		// on a real vkCreateImageView failure.
		VkImageView GetOrCreateRenderTargetView(TextureRecord &tex, const uint32 nativeTextureTarget);
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
