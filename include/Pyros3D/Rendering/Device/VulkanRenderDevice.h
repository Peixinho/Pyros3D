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
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <map>

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

		virtual CommandBufferHandle BeginCommandBuffer();
		virtual void EndCommandBuffer(const CommandBufferHandle cmd);

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
			ProgramRecord() : vertexShader(0), fragmentShader(0), descriptorSetLayout(VK_NULL_HANDLE), pipelineLayout(VK_NULL_HANDLE) {}
		};
		std::map<DeviceHandle, ProgramRecord> programs;
		DeviceHandle nextProgramHandle;

		// Real VkPipeline objects, keyed by the handle CreatePipeline()
		// returns. Vertex input state is hardcoded to match
		// Primitive.cpp's always-interleaved (aPosition:vec3, aNormal:vec3,
		// aTexcoord:vec2) layout (stride 32, offsets 0/12/24) - the only
		// vertex format RotatingCube's Cube geometry (this backend's sole
		// validation target - see VULKAN_ROADMAP.md) ever produces.
		// Generalizing this (skinned meshes, instancing, tangent/bitangent)
		// needs either PipelineDesc to carry a real vertex layout or a
		// second reflection pass over the vertex stage's stage_inputs -
		// deliberately not guessed at without a second real mesh shape to
		// validate against.
		std::map<DeviceHandle, VkPipeline> pipelines;
		DeviceHandle nextPipelineHandle;

	};

};

#endif /* VULKAN_BACKEND */

#endif /* VULKANRENDERDEVICE_H */
