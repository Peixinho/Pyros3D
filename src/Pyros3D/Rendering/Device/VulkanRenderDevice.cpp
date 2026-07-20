//============================================================================
// Name        : VulkanRenderDevice.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IRenderDevice implementation backed by Vulkan - Step B
//               stub, see the header comment for scope.
//============================================================================

// Exactly one translation unit must define VMA_IMPLEMENTATION before
// including vk_mem_alloc.h (directly or, as here, transitively via
// VulkanRenderDevice.h) - this is that one. volk.h (also included via
// VulkanRenderDevice.h, and included first) already #defines
// VK_NO_PROTOTYPES before pulling in <vulkan/vulkan.h>, so vk_mem_alloc.h's
// own include of the same header doesn't redeclare the vk* functions volk
// itself provides as global function-pointer variables - VMA's default
// VMA_STATIC_VULKAN_FUNCTIONS=1 then "just works" by calling through
// those same global names.
#define VMA_IMPLEMENTATION
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>

#ifdef VULKAN_BACKEND

#include <Pyros3D/Rendering/SPIRV/ShaderCompiler.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <vector>
#include <cstring>

namespace p3d {

	VulkanRenderDevice::VulkanRenderDevice(const std::vector<const char*> &requiredInstanceExtensions)
		: instance(VK_NULL_HANDLE), surface(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE),
		  graphicsQueueFamily(0), presentQueueFamily(0), graphicsQueue(VK_NULL_HANDLE), presentQueue(VK_NULL_HANDLE),
		  swapchain(VK_NULL_HANDLE), swapchainFormat(VK_FORMAT_UNDEFINED), swapchainExtent{0, 0},
		  renderPass(VK_NULL_HANDLE), depthImage(VK_NULL_HANDLE), depthImageAllocation(VK_NULL_HANDLE),
		  depthImageView(VK_NULL_HANDLE), depthFormat(VK_FORMAT_UNDEFINED),
		  commandPool(VK_NULL_HANDLE), frameCommandBuffer(VK_NULL_HANDLE),
		  imageAvailableSemaphore(VK_NULL_HANDLE), frameFence(VK_NULL_HANDLE),
		  allocator(VK_NULL_HANDLE), nextBufferHandle(1), nextShaderStageHandle(1), nextProgramHandle(1), nextPipelineHandle(1)
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
		// VK_KHR_portability_subset (enabled as a *device* extension in
		// InitializeSwapchain() - required on MoltenVK) itself requires
		// this instance extension per its spec entry - missing it didn't
		// surface as a hard failure (MoltenVK tolerates it), only as a
		// validation error once validation layers actually got enabled
		// (VUID-vkCreateDevice-ppEnabledExtensionNames-01387) - not caught
		// by any earlier session's testing since none had run with
		// VK_LAYER_KHRONOS_validation actually loaded (needs VK_LAYER_PATH
		// set to Homebrew's layer dir on this machine, not just the layer
		// installed) despite VULKAN_ROADMAP.md stating validation-clean
		// output as part of the correctness bar from the start.
		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
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

			// Resource tables (buffers/shader modules/pipelines/pipeline
			// layouts) must be torn down before the allocator/device that
			// own their backing memory.
			for (std::map<DeviceHandle, VkPipeline>::iterator it = pipelines.begin(); it != pipelines.end(); it++)
				vkDestroyPipeline(device, it->second, NULL);
			pipelines.clear();
			for (std::map<DeviceHandle, ProgramRecord>::iterator it = programs.begin(); it != programs.end(); it++)
			{
				if (it->second.pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, it->second.pipelineLayout, NULL);
				if (it->second.descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, it->second.descriptorSetLayout, NULL);
			}
			programs.clear();
			for (std::map<DeviceHandle, BufferRecord>::iterator it = buffers.begin(); it != buffers.end(); it++)
				vmaDestroyBuffer(allocator, it->second.buffer, it->second.allocation);
			buffers.clear();
			for (std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.begin(); it != shaderStages.end(); it++)
				if (it->second.module != VK_NULL_HANDLE)
					vkDestroyShaderModule(device, it->second.module, NULL);
			shaderStages.clear();

			for (size_t i = 0; i < framebuffers.size(); i++)
				vkDestroyFramebuffer(device, framebuffers[i], NULL);
			if (renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, renderPass, NULL);
			if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, NULL);
			if (depthImage != VK_NULL_HANDLE) vmaDestroyImage(allocator, depthImage, depthImageAllocation);

			if (allocator != VK_NULL_HANDLE) vmaDestroyAllocator(allocator);

			if (frameFence != VK_NULL_HANDLE) vkDestroyFence(device, frameFence, NULL);
			if (imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
			for (size_t i = 0; i < renderFinishedSemaphores.size(); i++)
				vkDestroySemaphore(device, renderFinishedSemaphores[i], NULL);
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

		// VMA allocator - see the header comment on the `allocator` field.
		// volk.h defines VK_NO_PROTOTYPES before pulling in <vulkan/vulkan.h>
		// (see the comment on VMA_IMPLEMENTATION at the top of this file),
		// which pushes VMA into its dynamic-function-loading path
		// regardless of VMA_STATIC_VULKAN_FUNCTIONS's default - it asserts
		// unless vkGetInstanceProcAddr/vkGetDeviceProcAddr are supplied
		// explicitly, so hand them the same two volk already resolved
		// (global function-pointer variables by those names, populated by
		// volkInitialize()/volkLoadInstance() above); VMA uses those to
		// fetch every other function it needs internally.
		VmaVulkanFunctions vmaFunctions = {};
		vmaFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vmaFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = physicalDevice;
		allocatorInfo.device = device;
		allocatorInfo.instance = instance;
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
		allocatorInfo.pVulkanFunctions = &vmaFunctions;
		if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
			return false;

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

		// Depth buffer - one shared VkImage (single-frame-in-flight, see
		// the header comment) sized to the swapchain extent. D32_SFLOAT is
		// required to support VK_IMAGE_TILING_OPTIMAL as a depth-stencil
		// attachment on every Vulkan 1.0 implementation (the spec
		// guarantees it), so no format-support query/fallback chain is
		// needed the way color format selection above needed one.
		depthFormat = VK_FORMAT_D32_SFLOAT;
		VkImageCreateInfo depthImageInfo = {};
		depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
		depthImageInfo.format = depthFormat;
		depthImageInfo.extent = { swapchainExtent.width, swapchainExtent.height, 1 };
		depthImageInfo.mipLevels = 1;
		depthImageInfo.arrayLayers = 1;
		depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo depthAllocInfo = {};
		depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		depthAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		if (vmaCreateImage(allocator, &depthImageInfo, &depthAllocInfo, &depthImage, &depthImageAllocation, NULL) != VK_SUCCESS)
			return false;

		VkImageViewCreateInfo depthViewInfo = {};
		depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		depthViewInfo.image = depthImage;
		depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depthViewInfo.format = depthFormat;
		depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		if (vkCreateImageView(device, &depthViewInfo, NULL, &depthImageView) != VK_SUCCESS)
			return false;

		// Render pass - one color attachment (the swapchain image, cleared
		// and transitioned straight to PRESENT_SRC_KHR - this device still
		// has no separate "present" step beyond what the render pass itself
		// does once real rendering replaces ClearAndPresent()'s manual
		// barriers) plus one depth attachment (cleared, not stored - no
		// caller needs the depth buffer's contents after the pass).
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapchainFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

		VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
		if (vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass) != VK_SUCCESS)
			return false;

		// One framebuffer per swapchain image, sharing the single depth
		// image/view above.
		framebuffers.resize(actualImageCount);
		for (uint32 i = 0; i < actualImageCount; i++)
		{
			VkImageView fbAttachments[2] = { swapchainImageViews[i], depthImageView };
			VkFramebufferCreateInfo fbInfo = {};
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.renderPass = renderPass;
			fbInfo.attachmentCount = 2;
			fbInfo.pAttachments = fbAttachments;
			fbInfo.width = swapchainExtent.width;
			fbInfo.height = swapchainExtent.height;
			fbInfo.layers = 1;
			if (vkCreateFramebuffer(device, &fbInfo, NULL, &framebuffers[i]) != VK_SUCCESS)
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
		// One per swapchain image, not one shared - see the header comment
		// on renderFinishedSemaphores for why.
		renderFinishedSemaphores.resize(actualImageCount);
		for (uint32 i = 0; i < actualImageCount; i++)
			if (vkCreateSemaphore(device, &semInfo, NULL, &renderFinishedSemaphores[i]) != VK_SUCCESS) return false;

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
		submitInfo.pSignalSemaphores = &renderFinishedSemaphores[imageIndex];
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence) != VK_SUCCESS)
			return false;

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
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

	static VkCompareOp TranslateDepthTestVk(const uint32 mode)
	{
		switch (mode)
		{
		case DepthTest::Never: return VK_COMPARE_OP_NEVER;
		case DepthTest::Greater: return VK_COMPARE_OP_GREATER;
		case DepthTest::Equal: return VK_COMPARE_OP_EQUAL;
		case DepthTest::Always: return VK_COMPARE_OP_ALWAYS;
		case DepthTest::LEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case DepthTest::GEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case DepthTest::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
		case DepthTest::Less: default: return VK_COMPARE_OP_LESS;
		}
	}

	static VkBlendFactor TranslateBlendFactorVk(const uint32 factor)
	{
		switch (factor)
		{
		case BlendFunc::One: return VK_BLEND_FACTOR_ONE;
		case BlendFunc::Src_Color: return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFunc::One_Minus_Src_Color: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFunc::Dst_Color: return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFunc::One_Minus_Dst_Color: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFunc::Src_Alpha: return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFunc::One_Minus_Src_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFunc::Dst_Alpha: return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFunc::One_Minus_Dst_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFunc::Constant_Color: return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFunc::One_Minus_Constant_Color: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BlendFunc::Constant_Alpha: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case BlendFunc::One_Minus_Constant_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case BlendFunc::Src_Alpha_Saturate: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		case BlendFunc::Src1_Color: return VK_BLEND_FACTOR_SRC1_COLOR;
		case BlendFunc::One_Minus_Src1_Color: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case BlendFunc::Src1_Alpha: return VK_BLEND_FACTOR_SRC1_ALPHA;
		case BlendFunc::One_Minus_Src1_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
		case BlendFunc::Zero: default: return VK_BLEND_FACTOR_ZERO;
		}
	}

	static VkBlendOp TranslateBlendEquationVk(const uint32 eq)
	{
		switch (eq)
		{
		case BlendEq::Subtract: return VK_BLEND_OP_SUBTRACT;
		case BlendEq::Reverse_Subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendEq::Add: default: return VK_BLEND_OP_ADD;
		}
	}

	static VkCullModeFlags TranslateCullFaceVk(const uint32 cullFace)
	{
		switch (cullFace)
		{
		case CullFace::FrontFace: return VK_CULL_MODE_FRONT_BIT;
		case CullFace::DoubleSided: return VK_CULL_MODE_NONE;
		case CullFace::BackFace: default: return VK_CULL_MODE_BACK_BIT;
		}
	}

	DeviceHandle VulkanRenderDevice::CreatePipeline(const PipelineDesc &desc)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(desc.shaderProgram);
		if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || progIt == programs.end() || progIt->second.pipelineLayout == VK_NULL_HANDLE)
			return 0;
		std::map<DeviceHandle, ShaderStageRecord>::iterator vs = shaderStages.find(progIt->second.vertexShader);
		std::map<DeviceHandle, ShaderStageRecord>::iterator fs = shaderStages.find(progIt->second.fragmentShader);
		if (vs == shaderStages.end() || fs == shaderStages.end() || vs->second.module == VK_NULL_HANDLE || fs->second.module == VK_NULL_HANDLE)
			return 0;

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vs->second.module;
		stages[0].pName = "main";
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fs->second.module;
		stages[1].pName = "main";

		// Vertex input - hardcoded to Primitive.cpp's always-interleaved
		// (aPosition:vec3, aNormal:vec3, aTexcoord:vec2) layout, stride 32.
		// See the comment on the `pipelines` field in the header for why.
		VkVertexInputBindingDescription binding = {};
		binding.binding = 0;
		binding.stride = 32;
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription attributes[3] = {};
		attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };  // aPosition
		attributes[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12 }; // aNormal
		attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, 24 };    // aTexcoord

		VkPipelineVertexInputStateCreateInfo vertexInput = {};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.pVertexBindingDescriptions = &binding;
		vertexInput.vertexAttributeDescriptionCount = 3;
		vertexInput.pVertexAttributeDescriptions = attributes;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Viewport/scissor left dynamic (set per-command-buffer via
		// vkCmdSetViewport/vkCmdSetScissor once real draw recording exists)
		// rather than baked to swapchainExtent here, so a future swapchain
		// resize doesn't require rebuilding every pipeline.
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = desc.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = TranslateCullFaceVk(desc.cullFace);
		// Matches GLRenderDevice's default winding (GL's CCW-is-front
		// convention, never overridden anywhere in this codebase).
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp = TranslateDepthTestVk(desc.depthTestMode);

		VkPipelineColorBlendAttachmentState blendAttachment = {};
		blendAttachment.blendEnable = desc.blendingEnabled ? VK_TRUE : VK_FALSE;
		blendAttachment.srcColorBlendFactor = TranslateBlendFactorVk(desc.blendSrcFactor);
		blendAttachment.dstColorBlendFactor = TranslateBlendFactorVk(desc.blendDstFactor);
		blendAttachment.colorBlendOp = TranslateBlendEquationVk(desc.blendEquation);
		blendAttachment.srcAlphaBlendFactor = TranslateBlendFactorVk(desc.blendSrcFactor);
		blendAttachment.dstAlphaBlendFactor = TranslateBlendFactorVk(desc.blendDstFactor);
		blendAttachment.alphaBlendOp = TranslateBlendEquationVk(desc.blendEquation);
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &blendAttachment;

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = progIt->second.pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;

		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline) != VK_SUCCESS)
			return 0;

		DeviceHandle handle = nextPipelineHandle++;
		pipelines[handle] = pipeline;
		return handle;
	}

	void VulkanRenderDevice::DestroyPipeline(const DeviceHandle pipeline)
	{
		std::map<DeviceHandle, VkPipeline>::iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
			return;
		if (device != VK_NULL_HANDLE)
			vkDestroyPipeline(device, it->second, NULL);
		pipelines.erase(it);
	}

	void VulkanRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		// Not reachable yet - IRenderer doesn't call BindPipeline() (still
		// issuing the individual Set*/UseProgram calls below instead, all
		// no-ops on this backend) until it actually threads a real
		// per-frame VkCommandBuffer through RenderObject() - see
		// VULKAN_ROADMAP.md. cmd is currently always the meaningless
		// constant BeginCommandBuffer() below returns, not a real
		// VkCommandBuffer, so there's nothing correct to record into yet.
		std::map<DeviceHandle, VkPipeline>::iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
			return;
		(void)cmd;
	}

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

	// bindingPoint isn't tracked per-buffer here (nothing consumes it yet
	// - see the comment on CreatePipeline still being unimplemented) but
	// the parameter stays for interface parity and so CreateUniformBuffer's
	// call sites in IRenderer don't need to change again once descriptor
	// sets land.
	DeviceHandle VulkanRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint)
	{
		if (allocator == VK_NULL_HANDLE || sizeBytes == 0)
			return 0;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeBytes;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		BufferRecord record;
		record.size = sizeBytes;
		VmaAllocationInfo allocationInfo;
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &record.buffer, &record.allocation, &allocationInfo) != VK_SUCCESS)
			return 0;
		record.mapped = allocationInfo.pMappedData;

		// Zero-initialize - matches CreateUniformBuffer()'s existing GL
		// contract (IRenderer's ctor calls this with no initial data,
		// relying on the buffer starting zeroed until the first
		// ReplaceUniformBuffer()/UpdateUniformBuffer() call).
		if (record.mapped != NULL)
			memset(record.mapped, 0, sizeBytes);

		DeviceHandle handle = nextBufferHandle++;
		buffers[handle] = record;
		return handle;
	}

	void VulkanRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.mapped == NULL)
			return;
		memcpy((uchar*)it->second.mapped + offset, data, sizeBytes);
	}

	void VulkanRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data)
	{
		// No orphaning trick needed here the way GLRenderDevice's
		// ReplaceUniformBuffer() (see its comment) needs one for
		// glBufferSubData - this is a plain host-memory memcpy into
		// persistently-mapped, host-coherent memory; there's no implicit
		// CPU/GPU sync point to stall on the way glBufferSubData has.
		// (Real GPU/CPU synchronization for a buffer still being read by
		// an in-flight command buffer is a correctness concern that needs
		// per-frame buffering once real draw submission exists - not
		// reachable yet since nothing calls ReplaceUniformBuffer() through
		// a live render loop on this backend.)
		UpdateUniformBuffer(buffer, 0, sizeBytes, data);
	}

	void VulkanRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer)
	{
		DestroyBuffer(buffer);
	}

	DeviceHandle VulkanRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		if (allocator == VK_NULL_HANDLE || length == 0)
			return 0;

		// bufferType (GeometryBuffer.h's Buffer::Type::Index/Vertex/
		// Attribute) has no Vulkan-side distinction the way GL's
		// GL_ELEMENT_ARRAY_BUFFER/GL_ARRAY_BUFFER targets do - a VkBuffer
		// declares its allowed uses via usage flags at creation instead of
		// a bind target, so this covers every value with the union of
		// vertex+index usage (harmless to over-declare; validation layers
		// only warn on missing bits, never extra ones).
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = length;
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		BufferRecord record;
		record.size = length;
		VmaAllocationInfo allocationInfo;
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &record.buffer, &record.allocation, &allocationInfo) != VK_SUCCESS)
			return 0;
		record.mapped = allocationInfo.pMappedData;

		if (data != NULL && record.mapped != NULL)
			memcpy(record.mapped, data, length);

		DeviceHandle handle = nextBufferHandle++;
		buffers[handle] = record;
		return handle;
	}

	void VulkanRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || allocator == VK_NULL_HANDLE || length == 0)
			return;
		vmaDestroyBuffer(allocator, it->second.buffer, it->second.allocation);

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = length;
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		BufferRecord record;
		record.size = length;
		VmaAllocationInfo allocationInfo;
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &record.buffer, &record.allocation, &allocationInfo) != VK_SUCCESS)
		{
			buffers.erase(it);
			return;
		}
		record.mapped = allocationInfo.pMappedData;
		if (data != NULL && record.mapped != NULL)
			memcpy(record.mapped, data, length);

		it->second = record;
	}

	void VulkanRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.mapped == NULL)
			return;
		memcpy(it->second.mapped, data, length);
	}

	void VulkanRenderDevice::DestroyBuffer(const DeviceHandle buffer)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end())
			return;
		vmaDestroyBuffer(allocator, it->second.buffer, it->second.allocation);
		buffers.erase(it);
	}

	void *VulkanRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end())
			return NULL;
		// Already persistently mapped at creation time - nothing further
		// to do (VMA_ALLOCATION_CREATE_MAPPED_BIT above).
		return it->second.mapped;
	}

	void VulkanRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) {}

	// Not reachable yet - SetVertexAttribute() (GL: glVertexAttribPointer,
	// issued per-attribute at bind time) has no direct Vulkan equivalent;
	// vertex input layout is baked into VkPipelineVertexInputStateCreateInfo
	// at CreatePipeline() time instead, which isn't implemented yet either
	// (see the header comment). Left as a documented stub rather than
	// guessing at a translation table with no CreatePipeline() to consume it.
	uint32 VulkanRenderDevice::TranslateAttributeType(const uint32 engineType) { return 0; }

	// Mirrors GLRenderDevice::BuildShaderSource()'s per-profile #version
	// prefixing, but for the Vulkan/SPIR-V profile. No explicit
	// "#define VULKAN" needed here - SpirvShaderCompiler::Compile() sets
	// shaderc's target environment to shaderc_target_env_vulkan, which
	// predefines VULKAN itself (the same way the Vulkan GLSL spec has
	// glslang/shaderc predefine it for any Vulkan-target compile); adding
	// our own definition on top produced a "Macro redefined; different
	// substitutions" compile error since shaderc's predefined value isn't
	// an empty definition the way a manual "#define VULKAN\n" is. This is
	// exactly what activates PyrosShader.glsl's IO_LOCATION/UBO_BINDING/
	// SAMPLER_BINDING macros (see that file's header comment) - no action
	// needed on this end beyond routing the source through the Vulkan
	// target environment, which CompileShaderStage() already does.
	std::string VulkanRenderDevice::BuildShaderSource(const std::string &definitions, const std::string &shaderBody)
	{
		return std::string("#version 450\n") + definitions + std::string(" ") + shaderBody;
	}

	DeviceHandle VulkanRenderDevice::CreateShaderStage(const uint32 engineShaderType)
	{
		ShaderStageRecord record;
		record.engineShaderType = engineShaderType;
		record.module = VK_NULL_HANDLE;
		DeviceHandle handle = nextShaderStageHandle++;
		shaderStages[handle] = record;
		return handle;
	}

	bool VulkanRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog)
	{
		std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.find(shader);
		if (it == shaderStages.end() || device == VK_NULL_HANDLE)
			return false;

#ifdef SPIRV_TOOLING
		uint32 spirvStage = (it->second.engineShaderType == ShaderType::FragmentShader) ? SpirvShaderStage::Fragment : SpirvShaderStage::Vertex;
		if (!SpirvShaderCompiler::Compile(source, spirvStage, it->second.spirv, errorLog))
			return false;

		VkShaderModuleCreateInfo moduleInfo = {};
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.codeSize = it->second.spirv.size() * sizeof(uint32);
		moduleInfo.pCode = it->second.spirv.data();
		if (vkCreateShaderModule(device, &moduleInfo, NULL, &it->second.module) != VK_SUCCESS)
		{
			errorLog = "vkCreateShaderModule failed";
			return false;
		}
		return true;
#else
		// Built with BUILD_VULKAN_BACKEND=ON but BUILD_SPIRV_TOOLING=OFF -
		// there's no GLSL->SPIR-V path available at all in this build
		// configuration (see CMakeLists.txt); fail loudly rather than
		// silently producing an empty shader module.
		errorLog = "Vulkan backend built without SPIRV_TOOLING (shaderc/spirv-cross not found) - cannot compile GLSL to SPIR-V.";
		return false;
#endif
	}

	DeviceHandle VulkanRenderDevice::CreateProgram()
	{
		DeviceHandle handle = nextProgramHandle++;
		programs[handle] = ProgramRecord();
		return handle;
	}

	void VulkanRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(program);
		std::map<DeviceHandle, ShaderStageRecord>::iterator shaderIt = shaderStages.find(shader);
		if (progIt == programs.end() || shaderIt == shaderStages.end())
			return;
		if (shaderIt->second.engineShaderType == ShaderType::FragmentShader)
			progIt->second.fragmentShader = shader;
		else
			progIt->second.vertexShader = shader;
	}

	bool VulkanRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog)
	{
		// No real "link" step in Vulkan the way GL has one - each stage's
		// VkShaderModule already compiled independently in
		// CompileShaderStage(); matching VS outputs to FS inputs happens
		// implicitly via PyrosShader.glsl's shared LOC_v*/BIND_* macros
		// keeping both stages' declarations in sync. What *does* need
		// building here, since nothing else in this class needs it until
		// now: the VkDescriptorSetLayout + VkPipelineLayout CreatePipeline()
		// needs, derived by reflecting both stages' SPIR-V (see the
		// comment on ShaderStageRecord::spirv and ProgramRecord above) -
		// this is exactly what SpirvShaderCompiler::Reflect() (Phase 2)
		// was built for: deriving a descriptor set layout without hand-
		// authoring one per shader variant.
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return false;
		std::map<DeviceHandle, ShaderStageRecord>::iterator vs = shaderStages.find(it->second.vertexShader);
		std::map<DeviceHandle, ShaderStageRecord>::iterator fs = shaderStages.find(it->second.fragmentShader);
		bool vsOk = vs != shaderStages.end() && vs->second.module != VK_NULL_HANDLE;
		bool fsOk = fs != shaderStages.end() && fs->second.module != VK_NULL_HANDLE;
		if (!vsOk || !fsOk)
		{
			errorLog = "Program missing a compiled vertex and/or fragment stage";
			return false;
		}

#ifdef SPIRV_TOOLING
		// Merge both stages' reflected resources by binding index - a UBO
		// declared in both (none of PyrosShader.glsl's are today, but
		// nothing stops a future shader from doing so) must appear once in
		// the descriptor set layout with the union of both stages' bits in
		// VkDescriptorSetLayoutBinding::stageFlags, not twice.
		std::vector<SpirvResourceBinding> vsResources = SpirvShaderCompiler::Reflect(vs->second.spirv);
		std::vector<SpirvResourceBinding> fsResources = SpirvShaderCompiler::Reflect(fs->second.spirv);

		std::map<uint32, VkDescriptorSetLayoutBinding> bindingsByIndex;
		for (int stagePass = 0; stagePass < 2; stagePass++)
		{
			std::vector<SpirvResourceBinding> &resources = (stagePass == 0) ? vsResources : fsResources;
			VkShaderStageFlags stageFlag = (stagePass == 0) ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
			for (size_t i = 0; i < resources.size(); i++)
			{
				const SpirvResourceBinding &res = resources[i];
				std::map<uint32, VkDescriptorSetLayoutBinding>::iterator existing = bindingsByIndex.find(res.binding);
				if (existing != bindingsByIndex.end())
				{
					existing->second.stageFlags |= stageFlag;
					continue;
				}
				VkDescriptorSetLayoutBinding binding = {};
				binding.binding = res.binding;
				binding.descriptorType = (res.type == SpirvResourceType::SampledImage) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				binding.descriptorCount = 1;
				binding.stageFlags = stageFlag;
				bindingsByIndex[res.binding] = binding;
			}
		}

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
		for (std::map<uint32, VkDescriptorSetLayoutBinding>::iterator bIt = bindingsByIndex.begin(); bIt != bindingsByIndex.end(); bIt++)
			layoutBindings.push_back(bIt->second);

		VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
		setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayoutInfo.bindingCount = (uint32_t)layoutBindings.size();
		setLayoutInfo.pBindings = layoutBindings.empty() ? NULL : layoutBindings.data();
		if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, NULL, &it->second.descriptorSetLayout) != VK_SUCCESS)
		{
			errorLog = "vkCreateDescriptorSetLayout failed";
			return false;
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &it->second.descriptorSetLayout;
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &it->second.pipelineLayout) != VK_SUCCESS)
		{
			errorLog = "vkCreatePipelineLayout failed";
			return false;
		}
		return true;
#else
		errorLog = "Vulkan backend built without SPIRV_TOOLING - cannot reflect descriptor bindings.";
		return false;
#endif
	}

	bool VulkanRenderDevice::IsProgram(const DeviceHandle id) { return programs.find(id) != programs.end(); }
	bool VulkanRenderDevice::IsShaderStage(const DeviceHandle id) { return shaderStages.find(id) != shaderStages.end(); }

	void VulkanRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return;
		if (it->second.vertexShader == shader) it->second.vertexShader = 0;
		if (it->second.fragmentShader == shader) it->second.fragmentShader = 0;
	}

	void VulkanRenderDevice::DeleteShaderStage(const DeviceHandle shader)
	{
		std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.find(shader);
		if (it == shaderStages.end())
			return;
		if (it->second.module != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
			vkDestroyShaderModule(device, it->second.module, NULL);
		shaderStages.erase(it);
	}

	void VulkanRenderDevice::DeleteProgram(const DeviceHandle program)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return;
		if (device != VK_NULL_HANDLE)
		{
			if (it->second.pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, it->second.pipelineLayout, NULL);
			if (it->second.descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, it->second.descriptorSetLayout, NULL);
		}
		programs.erase(it);
	}

	// Loose (non-block) uniforms no longer exist in PyrosShader.glsl for
	// any material that supports UBOs (see IMaterial::SupportsUniformBlocks()
	// and IRenderer.cpp's SendGlobalUniforms()/SendModelUniforms()/
	// SendUserUniforms(), all UBO-backed now) - so for GenericShaderMaterial,
	// returning "not found" here is correct, not a stub cop-out: it makes
	// IRenderer's individual Shader::SendUniform() fallback loop (still
	// unconditionally attempted every call, same as GL) a no-op, exactly
	// matching what glGetUniformLocation would itself return for a name
	// that got absorbed into a UBO member instead of staying a loose
	// uniform. CustomShaderMaterial (which still declares its own loose
	// uniforms in user-authored shader files, outside PyrosShader.glsl)
	// is explicitly out of scope for RotatingCube's Vulkan validation path -
	// see VULKAN_ROADMAP.md.
	int32 VulkanRenderDevice::GetUniformLocation(const uint32 program, const std::string &name) { return -1; }

	// Unlike GL (where attribute locations are assigned by the driver at
	// link time and must be queried back), Vulkan/SPIR-V requires static
	// `layout(location = N)` on every input - PyrosShader.glsl's LOC_a*
	// macros (see that file) already pin every attribute name to a fixed
	// location for Vulkan builds, so this is a direct table lookup rather
	// than a real query. Keeps GetAttributeLocation()'s contract identical
	// across backends (IRenderer.cpp calls this the same way regardless
	// of which device is active).
	int32 VulkanRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name)
	{
		if (name == "aPosition") return 0;
		if (name == "aNormal") return 1;
		if (name == "aTexcoord") return 2;
		if (name == "aColor") return 3;
		if (name == "aSize") return 4;
		if (name == "aTangent") return 5;
		if (name == "aBitangent") return 6;
		if (name == "aBonesID") return 7;
		if (name == "aBonesWeight") return 8;
		if (name == "aInstancedTransform") return 9;
		return -1;
	}

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
