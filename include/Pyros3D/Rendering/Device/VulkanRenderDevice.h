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
//               InitializeSwapchain() below). Everything else
//               (CreatePipeline, CreateBuffer, shader compilation, command
//               recording) is still unimplemented pending further work.
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
#include <vector>

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

		// Minimal single-frame-in-flight sync (no double/triple buffering
		// of these yet - good enough for the "does presenting work at all"
		// checkpoint this step is verifying, not production frame pacing).
		VkCommandPool commandPool;
		VkCommandBuffer frameCommandBuffer;
		VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
		VkFence frameFence;

	};

};

#endif /* VULKAN_BACKEND */

#endif /* VULKANRENDERDEVICE_H */
