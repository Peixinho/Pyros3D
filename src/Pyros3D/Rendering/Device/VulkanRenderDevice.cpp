//============================================================================
// Name        : VulkanRenderDevice.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IRenderDevice implementation backed by Vulkan - Step B
//               stub, see the header comment for scope.
//============================================================================

#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>

#ifdef VULKAN_BACKEND

#include <vector>

namespace p3d {

	VulkanRenderDevice::VulkanRenderDevice(const std::vector<const char*> &requiredInstanceExtensions)
		: instance(VK_NULL_HANDLE), surface(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE),
		  graphicsQueueFamily(0), presentQueueFamily(0), graphicsQueue(VK_NULL_HANDLE), presentQueue(VK_NULL_HANDLE),
		  swapchain(VK_NULL_HANDLE), swapchainFormat(VK_FORMAT_UNDEFINED), swapchainExtent{0, 0},
		  commandPool(VK_NULL_HANDLE), frameCommandBuffer(VK_NULL_HANDLE),
		  imageAvailableSemaphore(VK_NULL_HANDLE), renderFinishedSemaphore(VK_NULL_HANDLE), frameFence(VK_NULL_HANDLE)
	{
		if (volkInitialize() != VK_SUCCESS)
			return;

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Pyros3D";
		appInfo.apiVersion = VK_API_VERSION_1_0;

		std::vector<const char*> extensions = requiredInstanceExtensions;
		VkInstanceCreateFlags flags = 0;
#if defined(__APPLE__)
		// MoltenVK's ICD is a non-conformant "portability" implementation -
		// needs to be explicitly opted into since Vulkan SDK 1.3.216+.
		extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.flags = flags;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.empty() ? NULL : extensions.data();

		if (vkCreateInstance(&createInfo, NULL, &instance) == VK_SUCCESS)
			volkLoadInstance(instance);
	}

	VulkanRenderDevice::~VulkanRenderDevice()
	{
		if (device != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(device);

			if (frameFence != VK_NULL_HANDLE) vkDestroyFence(device, frameFence, NULL);
			if (imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
			if (renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
			if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, NULL);

			for (size_t i = 0; i < swapchainImageViews.size(); i++)
				vkDestroyImageView(device, swapchainImageViews[i], NULL);
			if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapchain, NULL);

			vkDestroyDevice(device, NULL);
		}
		if (surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance, surface, NULL);
		if (instance != VK_NULL_HANDLE)
			vkDestroyInstance(instance, NULL);
	}

	bool VulkanRenderDevice::InitializeSwapchain(VkSurfaceKHR newSurface, const uint32 width, const uint32 height)
	{
		if (instance == VK_NULL_HANDLE || newSurface == VK_NULL_HANDLE)
			return false;
		surface = newSurface;

		// Physical device: pick the first one with a queue family that
		// supports both graphics and presenting to this surface - true on
		// every single-GPU setup (including MoltenVK on Apple Silicon,
		// this class's only tested target so far), simplest correct choice
		// for this checkpoint. Multi-GPU/separate-queue-family setups are a
		// later refinement, not needed to prove the swapchain works at all.
		uint32 physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL);
		if (physicalDeviceCount == 0)
			return false;
		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

		bool foundQueueFamily = false;
		for (size_t d = 0; d < physicalDevices.size() && !foundQueueFamily; d++)
		{
			uint32 queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[d], &queueFamilyCount, NULL);
			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[d], &queueFamilyCount, queueFamilies.data());

			for (uint32 f = 0; f < queueFamilyCount; f++)
			{
				VkBool32 presentSupport = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[d], f, surface, &presentSupport);
				if ((queueFamilies[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
				{
					physicalDevice = physicalDevices[d];
					graphicsQueueFamily = presentQueueFamily = f;
					foundQueueFamily = true;
					break;
				}
			}
		}
		if (!foundQueueFamily)
			return false;

		// Logical device + queue.
		f32 queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		std::vector<const char*> deviceExtensions;
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#if defined(__APPLE__)
		// Required on MoltenVK alongside VK_KHR_portability_enumeration at
		// the instance level.
		deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device) != VK_SUCCESS)
			return false;
		volkLoadDevice(device);

		vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
		presentQueue = graphicsQueue;

		// Swapchain: pick the first available surface format, FIFO present
		// mode (the only mode every implementation is required to support -
		// simplest correct choice, MAILBOX/low-latency tuning is a later
		// refinement), and clamp the requested width/height to what the
		// surface actually allows.
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

		uint32 formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);
		if (formatCount == 0)
			return false;
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
		VkSurfaceFormatKHR chosenFormat = formats[0];
		for (size_t i = 0; i < formats.size(); i++)
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				chosenFormat = formats[i];
				break;
			}

		VkExtent2D extent;
		if (capabilities.currentExtent.width != 0xFFFFFFFF)
			extent = capabilities.currentExtent;
		else
		{
			extent.width = width < capabilities.minImageExtent.width ? capabilities.minImageExtent.width : (width > capabilities.maxImageExtent.width ? capabilities.maxImageExtent.width : width);
			extent.height = height < capabilities.minImageExtent.height ? capabilities.minImageExtent.height : (height > capabilities.maxImageExtent.height ? capabilities.maxImageExtent.height : height);
		}

		uint32 imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			imageCount = capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.surface = surface;
		swapchainCreateInfo.minImageCount = imageCount;
		swapchainCreateInfo.imageFormat = chosenFormat.format;
		swapchainCreateInfo.imageColorSpace = chosenFormat.colorSpace;
		swapchainCreateInfo.imageExtent = extent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.preTransform = capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain) != VK_SUCCESS)
			return false;

		swapchainFormat = chosenFormat.format;
		swapchainExtent = extent;

		uint32 actualImageCount = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, NULL);
		swapchainImages.resize(actualImageCount);
		vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, swapchainImages.data());

		swapchainImageViews.resize(actualImageCount);
		for (uint32 i = 0; i < actualImageCount; i++)
		{
			VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = swapchainImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = swapchainFormat;
			viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			if (vkCreateImageView(device, &viewInfo, NULL, &swapchainImageViews[i]) != VK_SUCCESS)
				return false;
		}

		// Command pool/buffer + sync objects for ClearAndPresent()'s
		// single-frame-in-flight loop.
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = graphicsQueueFamily;
		if (vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) != VK_SUCCESS)
			return false;

		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = commandPool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &frameCommandBuffer) != VK_SUCCESS)
			return false;

		VkSemaphoreCreateInfo semInfo = {};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(device, &semInfo, NULL, &imageAvailableSemaphore) != VK_SUCCESS) return false;
		if (vkCreateSemaphore(device, &semInfo, NULL, &renderFinishedSemaphore) != VK_SUCCESS) return false;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(device, &fenceInfo, NULL, &frameFence) != VK_SUCCESS) return false;

		return true;
	}

	bool VulkanRenderDevice::ClearAndPresent(const Vec4 &clearColor)
	{
		if (swapchain == VK_NULL_HANDLE)
			return false;

		vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &frameFence);

		uint32 imageIndex = 0;
		VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
			return false;

		vkResetCommandBuffer(frameCommandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(frameCommandBuffer, &beginInfo);

		VkImageMemoryBarrier toClearBarrier = {};
		toClearBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toClearBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		toClearBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toClearBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toClearBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toClearBarrier.image = swapchainImages[imageIndex];
		toClearBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		toClearBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(frameCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toClearBarrier);

		VkClearColorValue clearValue;
		clearValue.float32[0] = clearColor.x;
		clearValue.float32[1] = clearColor.y;
		clearValue.float32[2] = clearColor.z;
		clearValue.float32[3] = clearColor.w;
		VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdClearColorImage(frameCommandBuffer, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

		VkImageMemoryBarrier toPresentBarrier = toClearBarrier;
		toPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		toPresentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toPresentBarrier.dstAccessMask = 0;
		vkCmdPipelineBarrier(frameCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &toPresentBarrier);

		vkEndCommandBuffer(frameCommandBuffer);

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frameCommandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence) != VK_SUCCESS)
			return false;

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &imageIndex;
		VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
		return presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR;
	}

	CommandBufferHandle VulkanRenderDevice::BeginCommandBuffer() { return 0; }
	void VulkanRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd) {}

	uint32 VulkanRenderDevice::TranslateBufferBit(const uint32 bufferBits) { return 0; }
	void VulkanRenderDevice::Clear(const uint32 nativeBufferBits) {}
	void VulkanRenderDevice::SetClearColor(const Vec4 &color) {}

	void VulkanRenderDevice::SetDepthTest(const bool enabled, const uint32 mode) {}
	void VulkanRenderDevice::SetDepthMask(const bool enabled) {}
	void VulkanRenderDevice::PrepareDepthClear() {}

	void VulkanRenderDevice::SetStencilTestEnabled(const bool enabled) {}
	void VulkanRenderDevice::SetClearStencilValue() {}
	void VulkanRenderDevice::SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) {}
	void VulkanRenderDevice::SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) {}

	void VulkanRenderDevice::SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height) {}
	void VulkanRenderDevice::SetScissorTestEnabled(const bool enabled) {}

	void VulkanRenderDevice::SetWireFrame(const bool enabled) {}

	void VulkanRenderDevice::SetColorMask(const bool r, const bool g, const bool b, const bool a) {}

	void VulkanRenderDevice::SetPolygonOffsetEnabled(const bool enabled) {}
	void VulkanRenderDevice::SetPolygonOffset(const f32 factor, const f32 units) {}

	void VulkanRenderDevice::SetBlendingEnabled(const bool enabled) {}
	void VulkanRenderDevice::SetBlendFunction(const uint32 sfactor, const uint32 dfactor) {}
	void VulkanRenderDevice::SetBlendEquation(const uint32 mode) {}

	void VulkanRenderDevice::SetCullFaceMode(const uint32 cullFace) {}
	void VulkanRenderDevice::DisableCullFace() {}

	DeviceHandle VulkanRenderDevice::CreatePipeline(const PipelineDesc &desc) { return 0; }
	void VulkanRenderDevice::DestroyPipeline(const DeviceHandle pipeline) {}
	void VulkanRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline) {}

	void VulkanRenderDevice::EnableClipDistance(const uint32 index) {}
	void VulkanRenderDevice::DisableClipDistance(const uint32 index) {}

	void VulkanRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height) {}

	void VulkanRenderDevice::UseProgram(const uint32 program) {}
	DeviceHandle VulkanRenderDevice::CreateVertexArray() { return 0; }
	void VulkanRenderDevice::DeleteVertexArray(const DeviceHandle vao) {}
	void VulkanRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao) {}
	void VulkanRenderDevice::BindArrayBuffer(const uint32 buffer) {}
	void VulkanRenderDevice::BindElementBuffer(const uint32 buffer) {}
	void VulkanRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) {}
	void VulkanRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) {}
	void VulkanRenderDevice::DisableVertexAttribute(const int32 location) {}
	void VulkanRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor) {}
	void VulkanRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint) {}

	uint32 VulkanRenderDevice::TranslateDrawType(const uint32 engineDrawType) { return 0; }
	void VulkanRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) {}
	void VulkanRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount) {}
	void VulkanRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount) {}

	DeviceHandle VulkanRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint) { return 0; }
	void VulkanRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data) {}
	void VulkanRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data) {}
	void VulkanRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer) {}

	DeviceHandle VulkanRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) { return 0; }
	void VulkanRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) {}
	void VulkanRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length) {}
	void VulkanRenderDevice::DestroyBuffer(const DeviceHandle buffer) {}
	void *VulkanRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType) { return NULL; }
	void VulkanRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) {}

	uint32 VulkanRenderDevice::TranslateAttributeType(const uint32 engineType) { return 0; }

	std::string VulkanRenderDevice::BuildShaderSource(const std::string &definitions, const std::string &shaderBody) { return ""; }
	DeviceHandle VulkanRenderDevice::CreateShaderStage(const uint32 engineShaderType) { return 0; }
	bool VulkanRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog) { return false; }
	DeviceHandle VulkanRenderDevice::CreateProgram() { return 0; }
	void VulkanRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader) {}
	bool VulkanRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog) { return false; }
	bool VulkanRenderDevice::IsProgram(const DeviceHandle id) { return false; }
	bool VulkanRenderDevice::IsShaderStage(const DeviceHandle id) { return false; }
	void VulkanRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader) {}
	void VulkanRenderDevice::DeleteShaderStage(const DeviceHandle shader) {}
	void VulkanRenderDevice::DeleteProgram(const DeviceHandle program) {}

	int32 VulkanRenderDevice::GetUniformLocation(const uint32 program, const std::string &name) { return -1; }
	int32 VulkanRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name) { return -1; }

	void VulkanRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) {}

	void VulkanRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type) {}
	void VulkanRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode) {}

	DeviceHandle VulkanRenderDevice::CreateTextureObject() { return 0; }
	void VulkanRenderDevice::DestroyTextureObject(const DeviceHandle texture) {}
	void VulkanRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture) {}

	void VulkanRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data) {}
	void VulkanRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) {}
	void VulkanRenderDevice::GenerateMipmap(const uint32 target) {}

	void VulkanRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat) {}
	void VulkanRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat) {}
	void VulkanRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat) {}
	void VulkanRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter) {}
	void VulkanRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap) {}
	void VulkanRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) {}
	void VulkanRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 &color) {}
	void VulkanRenderDevice::SetTextureCompareMode(const uint32 target) {}
	void VulkanRenderDevice::SetPixelUnpackAlignment(const uint32 value) {}

	void VulkanRenderDevice::ActivateTextureUnit(const uint32 unit) {}

	void VulkanRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) {}
	uint32 VulkanRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) { return 0; }

	DeviceHandle VulkanRenderDevice::CreateFramebuffer() { return 0; }
	void VulkanRenderDevice::DestroyFramebuffer(const DeviceHandle fbo) {}
	uint32 VulkanRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess) { return 0; }
	void VulkanRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo) {}
	uint32 VulkanRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) { return 0; }
	void VulkanRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId) {}
	void VulkanRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) {}
	void VulkanRenderDevice::SetDrawBufferNone() {}
	void VulkanRenderDevice::SetReadBufferNone() {}
	void VulkanRenderDevice::SetDrawBufferBack() {}
	void VulkanRenderDevice::SetReadBufferBack() {}
	void VulkanRenderDevice::SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) {}
	uint32 VulkanRenderDevice::CheckFramebufferStatus() { return 0; }
	uint32 VulkanRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus) { return 0; }

	DeviceHandle VulkanRenderDevice::CreateRenderbuffer() { return 0; }
	void VulkanRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo) {}
	void VulkanRenderDevice::BindRenderbuffer(const DeviceHandle rbo) {}
	uint32 VulkanRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType) { return 0; }
	void VulkanRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) {}
	void VulkanRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) {}

	void VulkanRenderDevice::SetMultisampleEnabled(const bool enabled) {}
	void VulkanRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) {}

};

#endif /* VULKAN_BACKEND */
