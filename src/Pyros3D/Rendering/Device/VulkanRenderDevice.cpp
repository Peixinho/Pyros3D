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
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <vector>
#include <cstring>
#include <cstdio>

namespace p3d {

	// See TranslateTextureTarget()'s comment - a cube face's
	// mode/subMode is this plus the face index (0..5, matching
	// TextureType::CubemapPositive_X..CubemapNegative_Z's enum order).
	static const uint32 CUBEMAP_FACE_TARGET_BASE = 100;

	VulkanRenderDevice::VulkanRenderDevice(const std::vector<const char*> &requiredInstanceExtensions)
		: instance(VK_NULL_HANDLE), surface(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE),
		  graphicsQueueFamily(0), presentQueueFamily(0), graphicsQueue(VK_NULL_HANDLE), presentQueue(VK_NULL_HANDLE),
		  swapchain(VK_NULL_HANDLE), swapchainFormat(VK_FORMAT_UNDEFINED), swapchainExtent{0, 0},
		  renderPass(VK_NULL_HANDLE), depthImage(VK_NULL_HANDLE), depthImageAllocation(VK_NULL_HANDLE),
		  depthImageView(VK_NULL_HANDLE), depthFormat(VK_FORMAT_UNDEFINED),
		  commandPool(VK_NULL_HANDLE), frameCommandBuffer(VK_NULL_HANDLE),
		  nextAcquireSemaphoreIndex(0), currentFrameAcquireSemaphoreIndex(0), frameFence(VK_NULL_HANDLE),
		  pendingClearColor(0.f, 0.f, 0.f, 1.f),
		  captureRequested(false), capturedWidth(0), capturedHeight(0), capturedRedByteOffset(0), capturedFrameValid(false),
		  frameInProgress(false), currentImageIndex(0),
		  activeCommandBuffer(VK_NULL_HANDLE), offscreenCommandBuffer(VK_NULL_HANDLE), offscreenPassOpen(false), offscreenCommandBufferRecording(false), currentBoundFBO(0),
		  nextFBOHandle(1), shadowPipelineRenderPass(VK_NULL_HANDLE),
		  nextVaoHandle(1), currentVao(0), currentPipeline(0),
		  allocator(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE),
		  nextBufferHandle(1), minUniformBufferOffsetAlignment(256), nextShaderStageHandle(1), nextProgramHandle(1), currentProgram(0), nextPipelineHandle(1),
		  nextTextureHandle(1), currentlyConfiguringTexture(0), unitJustActivated(false), currentTextureUnit(0)
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
			// own their backing memory. Descriptor sets are owned by
			// descriptorPool (allocated without
			// VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT), so
			// destroying the pool below frees every program's set - no
			// separate vkFreeDescriptorSets loop needed.
			for (std::map<DeviceHandle, VkPipeline>::iterator it = pipelines.begin(); it != pipelines.end(); it++)
				vkDestroyPipeline(device, it->second, NULL);
			pipelines.clear();
			if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptorPool, NULL);
			for (std::map<DeviceHandle, ProgramRecord>::iterator it = programs.begin(); it != programs.end(); it++)
			{
				if (it->second.pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, it->second.pipelineLayout, NULL);
				if (it->second.descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, it->second.descriptorSetLayout, NULL);
				if (it->second.samplerSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, it->second.samplerSetLayout, NULL);
			}
			programs.clear();
			// FBO framebuffers reference texture image *views* - must be
			// destroyed before the textures loop below destroys those
			// views.
			for (std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.begin(); it != fboRecords.end(); it++)
			{
				for (std::map<uint32, VkFramebuffer>::iterator fIt = it->second.framebuffersByTarget.begin(); fIt != it->second.framebuffersByTarget.end(); fIt++)
					vkDestroyFramebuffer(device, fIt->second, NULL);
				if (it->second.renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, it->second.renderPass, NULL);
			}
			fboRecords.clear();
			for (std::map<DeviceHandle, TextureRecord>::iterator it = textures.begin(); it != textures.end(); it++)
			{
				if (it->second.sampler != VK_NULL_HANDLE) vkDestroySampler(device, it->second.sampler, NULL);
				if (it->second.view != VK_NULL_HANDLE) vkDestroyImageView(device, it->second.view, NULL);
				for (std::map<uint32, VkImageView>::iterator rtIt = it->second.renderTargetViewsByTarget.begin(); rtIt != it->second.renderTargetViewsByTarget.end(); rtIt++)
					vkDestroyImageView(device, rtIt->second, NULL);
				if (it->second.image != VK_NULL_HANDLE) vmaDestroyImage(allocator, it->second.image, it->second.allocation);
			}
			textures.clear();
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
			if (shadowPipelineRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, shadowPipelineRenderPass, NULL);
			if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, NULL);
			if (depthImage != VK_NULL_HANDLE) vmaDestroyImage(allocator, depthImage, depthImageAllocation);

			if (allocator != VK_NULL_HANDLE) vmaDestroyAllocator(allocator);

			if (frameFence != VK_NULL_HANDLE) vkDestroyFence(device, frameFence, NULL);
			for (size_t i = 0; i < imageAvailableSemaphores.size(); i++)
				vkDestroySemaphore(device, imageAvailableSemaphores[i], NULL);
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

	// Builds (or rebuilds) everything keyed to the swapchain's size/format:
	// the swapchain itself, its images/image views, the depth buffer, the
	// render pass, and one framebuffer per swapchain image. Shared between
	// InitializeSwapchain() (first call - swapchain/renderPass/framebuffers
	// are all VK_NULL_HANDLE/empty already) and RecreateSwapchain() (resize -
	// the caller destroys the old dependents first, but leaves the old
	// `swapchain` handle itself alive so it can be passed here as
	// VkSwapchainCreateInfoKHR::oldSwapchain, the standard Vulkan resize
	// idiom - some drivers reuse the old swapchain's resources more
	// efficiently when given this hint, and it's required to be valid
	// during the old-to-new handoff on at least one real-world driver).
	// This function destroys that old handle itself, once the new one
	// exists.
	bool VulkanRenderDevice::CreateSwapchainAndFramebuffers(const uint32 width, const uint32 height)
	{
		VkSwapchainKHR oldSwapchain = swapchain;

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
		// TRANSFER_SRC (on top of COLOR_ATTACHMENT for rendering and
		// TRANSFER_DST for ClearAndPresent()'s vkCmdClearColorImage) lets
		// EndFrame() read a frame back on request - see RequestFrameCapture()/
		// GetCapturedFrame() - purely additive capability, every driver
		// that supports presenting to this surface at all supports this
		// too (it's not an optional/queryable feature the way some other
		// usage bits are).
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.preTransform = capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = oldSwapchain;

		VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
		if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &newSwapchain) != VK_SUCCESS)
			return false;
		swapchain = newSwapchain;

		if (oldSwapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(device, oldSwapchain, NULL);

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

		return true;
	}

	// Tears down and rebuilds everything CreateSwapchainAndFramebuffers()
	// creates, in response to the surface's actual size having changed
	// underneath us (window resize) - detected reactively via
	// VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR from vkAcquireNextImageKHR/
	// vkQueuePresentKHR in BeginFrame()/EndFrame(), the standard Vulkan way
	// to learn a resize happened (there is no "resize" callback in the API
	// itself - the app finds out when a swapchain operation tells it its
	// swapchain no longer matches the surface). Before this existed, a
	// window resize left the swapchain (and its depth buffer/framebuffers)
	// stuck at its original size while the surface itself had already
	// changed underneath it - MoltenVK/Metal in particular does not fail
	// outright, it just keeps presenting a fixed-size image into a
	// differently-sized drawable, which reads as a distorted/wrong-looking
	// frustum even though the camera's own aspect ratio (recomputed
	// correctly on the CPU side by the example's OnResize()) was never the
	// problem.
	void VulkanRenderDevice::ResetAcquireSemaphore(const uint32 index)
	{
		if (index >= imageAvailableSemaphores.size())
			return;
		if (imageAvailableSemaphores[index] != VK_NULL_HANDLE)
			vkDestroySemaphore(device, imageAvailableSemaphores[index], NULL);
		VkSemaphoreCreateInfo semInfo = {};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkCreateSemaphore(device, &semInfo, NULL, &imageAvailableSemaphores[index]);
	}

	bool VulkanRenderDevice::RecreateSwapchain(const uint32 width, const uint32 height)
	{
		if (device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
			return false;

		// A live interactive resize drag can report a momentarily
		// degenerate (0x0) surface extent - a minimized window, or just a
		// transient in-between state mid-drag on some platforms. Bail
		// *before* touching anything: this used to unconditionally
		// destroy the framebuffers/depth buffer/render pass first and
		// only then attempt to rebuild them, so hitting this case even
		// once left the device with an empty `framebuffers` and no
		// `renderPass` and no way to ever recover, since nothing but a
		// swapchain-operation failure ever calls this function again -
		// every following BeginFrame() would then race through
		// vkAcquireNextImageKHR() against a swapchain whose dependents no
		// longer exist, rendering nothing, forever. That reads to a user
		// as "resizing hangs the app" even though the process itself
		// isn't deadlocked - the window just permanently stops updating.
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
		if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0)
			return false;

		vkDeviceWaitIdle(device);

		for (size_t i = 0; i < framebuffers.size(); i++)
			vkDestroyFramebuffer(device, framebuffers[i], NULL);
		framebuffers.clear();

		if (renderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(device, renderPass, NULL);
		renderPass = VK_NULL_HANDLE;

		if (depthImageView != VK_NULL_HANDLE)
			vkDestroyImageView(device, depthImageView, NULL);
		depthImageView = VK_NULL_HANDLE;
		if (depthImage != VK_NULL_HANDLE)
			vmaDestroyImage(allocator, depthImage, depthImageAllocation);
		depthImage = VK_NULL_HANDLE;
		depthImageAllocation = VK_NULL_HANDLE;

		for (size_t i = 0; i < swapchainImageViews.size(); i++)
			vkDestroyImageView(device, swapchainImageViews[i], NULL);
		swapchainImageViews.clear();
		swapchainImages.clear();

		// CreateSwapchainAndFramebuffers() reads the current `swapchain`
		// member as the old handle to hand to VkSwapchainCreateInfoKHR::
		// oldSwapchain, and destroys it once the new one exists - not
		// destroyed here.
		if (!CreateSwapchainAndFramebuffers(width, height))
			return false;

		// renderFinishedSemaphores is sized/created once in
		// InitializeSwapchain(), indexed by acquired swapchain image
		// index everywhere it's used (EndFrame() etc) - if the recreated
		// swapchain ever comes back with a *different* image count than
		// before (driver-dependent, rare but not impossible - surface
		// capabilities' min/maxImageCount can vary with size on some
		// platforms), those indexed accesses would read past the end of
		// this vector. Resize to match and create any newly-needed ones;
		// destroy any now-excess ones.
		if (renderFinishedSemaphores.size() != swapchainImages.size())
		{
			for (size_t i = swapchainImages.size(); i < renderFinishedSemaphores.size(); i++)
				vkDestroySemaphore(device, renderFinishedSemaphores[i], NULL);
			size_t oldCount = renderFinishedSemaphores.size();
			renderFinishedSemaphores.resize(swapchainImages.size());
			VkSemaphoreCreateInfo semInfo = {};
			semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			for (size_t i = oldCount; i < renderFinishedSemaphores.size(); i++)
				if (vkCreateSemaphore(device, &semInfo, NULL, &renderFinishedSemaphores[i]) != VK_SUCCESS)
					return false;
		}

		return true;
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

		// Needed by every per-object/per-material-switch UBO's dynamic
		// offsets (see BufferRecord::alignedSlotSize's comment) -
		// vkCmdBindDescriptorSets requires each dynamic offset to be a
		// multiple of this. Queried from the real device rather than
		// trusting the constructor's 256-byte default (a common value,
		// but not spec-guaranteed) now that a physical device is
		// actually selected.
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		minUniformBufferOffsetAlignment = deviceProperties.limits.minUniformBufferOffsetAlignment;

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

#if defined(__APPLE__)
		// MoltenVK's portability-subset ICD treats comparison samplers
		// (VkSamplerCreateInfo::compareEnable, needed for sampler2DShadow/
		// samplerCubeShadow hardware PCF - see RebuildSamplerIfDirty())
		// as an *optional* feature, off by default - found the hard way
		// via VUID-VkDescriptorImageInfo-mutableComparisonSamplers-04450
		// the first time a real shadow sampler was ever written to a
		// descriptor. Query what's actually supported and request exactly
		// that (not a blind VK_TRUE - would fail vkCreateDevice on a
		// portability ICD that genuinely doesn't support it) by reusing
		// the same queried struct as the enable request, the standard
		// Vulkan idiom for this.
		VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures = {};
		portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
		VkPhysicalDeviceFeatures2 features2 = {};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &portabilityFeatures;
		// KHR-suffixed, not the unsuffixed Vulkan 1.1+ core entry point -
		// this instance only requests VK_API_VERSION_1_0 (see appInfo
		// above), relying on VK_KHR_get_physical_device_properties2
		// (already an enabled instance extension - see the comment on
		// its addition further up) for this functionality instead.
		vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &features2);
		deviceCreateInfo.pNext = &portabilityFeatures;
#endif

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

		if (!CreateSwapchainAndFramebuffers(width, height))
			return false;

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
		// See the header comment on offscreenCommandBuffer - a separate,
		// dedicated command buffer for offscreen FBO passes (shadow maps),
		// since those must record+submit+wait before the swapchain
		// frame's own command buffer even exists yet.
		if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &offscreenCommandBuffer) != VK_SUCCESS)
			return false;

		VkSemaphoreCreateInfo semInfo = {};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		// See the header comment on nextAcquireSemaphoreIndex for why this
		// is a small rotating pool rather than one shared semaphore. 3 is
		// comfortably more than the 1 frame ever actually in flight here -
		// it just needs to be large enough that a burst of back-to-back
		// failed acquires (a resize) never wraps back onto a semaphore
		// still holding an unconsumed signal from a couple of calls ago.
		imageAvailableSemaphores.resize(3);
		for (size_t i = 0; i < imageAvailableSemaphores.size(); i++)
			if (vkCreateSemaphore(device, &semInfo, NULL, &imageAvailableSemaphores[i]) != VK_SUCCESS) return false;
		// One per swapchain image, not one shared - see the header comment
		// on renderFinishedSemaphores for why.
		renderFinishedSemaphores.resize(swapchainImages.size());
		for (size_t i = 0; i < swapchainImages.size(); i++)
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

		// Bounded timeout - see BeginFrame()'s comment on
		// FRAME_WAIT_TIMEOUT_NS for why UINT64_MAX here can hang the
		// process during a window resize.
		static const uint64_t FRAME_WAIT_TIMEOUT_NS = 2000000000ULL;
		if (vkWaitForFences(device, 1, &frameFence, VK_TRUE, FRAME_WAIT_TIMEOUT_NS) != VK_SUCCESS)
			return false;

		// Do not reset frameFence until acquire has actually succeeded -
		// see BeginFrame()'s comment on the same sequencing bug (resetting
		// unconditionally here left the fence permanently unsignaled the
		// moment acquire ever failed once, hanging every future call).
		uint32 imageIndex = 0;
		// See nextAcquireSemaphoreIndex's comment - must advance on every
		// attempt, success or failure, not just successful ones.
		uint32 acquireSemIndex = nextAcquireSemaphoreIndex;
		nextAcquireSemaphoreIndex = (nextAcquireSemaphoreIndex + 1) % (uint32)imageAvailableSemaphores.size();
		VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, FRAME_WAIT_TIMEOUT_NS, imageAvailableSemaphores[acquireSemIndex], VK_NULL_HANDLE, &imageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
		{
			ResetAcquireSemaphore(acquireSemIndex);
			RecreateSwapchain(swapchainExtent.width, swapchainExtent.height);
			return false;
		}
		if (acquireResult != VK_SUCCESS)
		{
			ResetAcquireSemaphore(acquireSemIndex);
			return false;
		}
		vkResetFences(device, 1, &frameFence);

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
		submitInfo.pWaitSemaphores = &imageAvailableSemaphores[acquireSemIndex];
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

	void VulkanRenderDevice::WaitIdle()
	{
		if (device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(device);
	}

	void VulkanRenderDevice::RequestFrameCapture()
	{
		captureRequested = true;
	}

	bool VulkanRenderDevice::GetCapturedFrame(std::vector<uint8_t> &outPixels, uint32 &outWidth, uint32 &outHeight, uint32 &outRedByteOffset)
	{
		if (!capturedFrameValid)
			return false;
		outPixels = capturedPixels;
		outWidth = capturedWidth;
		outHeight = capturedHeight;
		outRedByteOffset = capturedRedByteOffset;
		return true;
	}

	bool VulkanRenderDevice::DebugReadDepthTexture(const DeviceHandle handle, std::vector<f32> &outDepths, uint32 &outWidth, uint32 &outHeight, const uint32 faceIndex)
	{
		std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(handle);
		if (texIt == textures.end() || texIt->second.image == VK_NULL_HANDLE)
			return false;
		TextureRecord &tex = texIt->second;

		VkDeviceSize bufferSize = (VkDeviceSize)tex.width * tex.height * sizeof(f32);
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo stagingAllocInfo = {};
		stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAllocation = VK_NULL_HANDLE;
		VmaAllocationInfo stagingAllocationInfo;
		if (vmaCreateBuffer(allocator, &bufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, &stagingAllocationInfo) != VK_SUCCESS)
			return false;

		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = commandPool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd) != VK_SUCCESS)
		{
			vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
			return false;
		}

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);

		// Shadow maps sit in SHADER_READ_ONLY_OPTIMAL between frames (the
		// depth-only render pass's finalLayout - see
		// BuildDepthOnlyRenderPass()) - transition to TRANSFER_SRC, copy,
		// then back, so sampling still works normally afterward.
		VkImageMemoryBarrier toSrc = {};
		toSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toSrc.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toSrc.image = tex.image;
		toSrc.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, faceIndex, 1 };
		toSrc.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toSrc);

		VkBufferImageCopy region = {};
		region.imageSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, faceIndex, 1 };
		region.imageExtent = { tex.width, tex.height, 1 };
		vkCmdCopyImageToBuffer(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

		VkImageMemoryBarrier toShaderRead = toSrc;
		toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &toShaderRead);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence fence = VK_NULL_HANDLE;
		vkCreateFence(device, &fenceInfo, NULL, &fence);
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
		if (fence != VK_NULL_HANDLE)
		{
			vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
			vkDestroyFence(device, fence, NULL);
		}
		else
		{
			vkQueueWaitIdle(graphicsQueue);
		}
		vkFreeCommandBuffers(device, commandPool, 1, &cmd);

		outWidth = tex.width;
		outHeight = tex.height;
		outDepths.resize((size_t)tex.width * tex.height);
		memcpy(outDepths.data(), stagingAllocationInfo.pMappedData, (size_t)bufferSize);

		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		return true;
	}

	bool VulkanRenderDevice::DrawFrame(const DeviceHandle pipeline, const DeviceHandle program, const DeviceHandle vertexBuffer, const DeviceHandle indexBuffer, const uint32 indexCount, const Vec4 &clearColor)
	{
		if (swapchain == VK_NULL_HANDLE)
			return false;
		std::map<DeviceHandle, VkPipeline>::iterator pipelineIt = pipelines.find(pipeline);
		std::map<DeviceHandle, BufferRecord>::iterator vboIt = buffers.find(vertexBuffer);
		std::map<DeviceHandle, BufferRecord>::iterator iboIt = buffers.find(indexBuffer);
		if (pipelineIt == pipelines.end() || vboIt == buffers.end() || iboIt == buffers.end())
			return false;
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(program);

		// Bounded timeout - see BeginFrame()'s comment on
		// FRAME_WAIT_TIMEOUT_NS for why UINT64_MAX here can hang the
		// process during a window resize.
		static const uint64_t FRAME_WAIT_TIMEOUT_NS = 2000000000ULL;
		if (vkWaitForFences(device, 1, &frameFence, VK_TRUE, FRAME_WAIT_TIMEOUT_NS) != VK_SUCCESS)
			return false;

		// Do not reset frameFence until acquire has actually succeeded -
		// see BeginFrame()'s comment on the same sequencing bug (resetting
		// unconditionally here left the fence permanently unsignaled the
		// moment acquire ever failed once, hanging every future call).
		uint32 imageIndex = 0;
		// See nextAcquireSemaphoreIndex's comment - must advance on every
		// attempt, success or failure, not just successful ones.
		uint32 acquireSemIndex = nextAcquireSemaphoreIndex;
		nextAcquireSemaphoreIndex = (nextAcquireSemaphoreIndex + 1) % (uint32)imageAvailableSemaphores.size();
		VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, FRAME_WAIT_TIMEOUT_NS, imageAvailableSemaphores[acquireSemIndex], VK_NULL_HANDLE, &imageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
		{
			ResetAcquireSemaphore(acquireSemIndex);
			RecreateSwapchain(swapchainExtent.width, swapchainExtent.height);
			return false;
		}
		if (acquireResult != VK_SUCCESS)
		{
			ResetAcquireSemaphore(acquireSemIndex);
			return false;
		}
		vkResetFences(device, 1, &frameFence);

		vkResetCommandBuffer(frameCommandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(frameCommandBuffer, &beginInfo);

		VkClearValue clearValues[2] = {};
		clearValues[0].color = { { clearColor.x, clearColor.y, clearColor.z, clearColor.w } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBegin = {};
		renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBegin.renderPass = renderPass;
		renderPassBegin.framebuffer = framebuffers[imageIndex];
		renderPassBegin.renderArea.extent = swapchainExtent;
		renderPassBegin.clearValueCount = 2;
		renderPassBegin.pClearValues = clearValues;
		vkCmdBeginRenderPass(frameCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineIt->second);

		// CreatePipeline() left viewport/scissor dynamic (see its comment) -
		// set them here to the full swapchain extent every frame.
		VkViewport viewport = { 0.0f, 0.0f, (f32)swapchainExtent.width, (f32)swapchainExtent.height, 0.0f, 1.0f };
		vkCmdSetViewport(frameCommandBuffer, 0, 1, &viewport);
		VkRect2D scissor = { { 0, 0 }, swapchainExtent };
		vkCmdSetScissor(frameCommandBuffer, 0, 1, &scissor);

		if (progIt != programs.end() && progIt->second.descriptorSet != VK_NULL_HANDLE)
			vkCmdBindDescriptorSets(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, progIt->second.pipelineLayout, 0, 1, &progIt->second.descriptorSet, 0, NULL);

		VkBuffer vbo = vboIt->second.buffer;
		VkDeviceSize vboOffset = 0;
		vkCmdBindVertexBuffers(frameCommandBuffer, 0, 1, &vbo, &vboOffset);
		// __INDEX_C_TYPE__ (Global.h) is uint32 - matches VK_INDEX_TYPE_UINT32.
		vkCmdBindIndexBuffer(frameCommandBuffer, iboIt->second.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(frameCommandBuffer, indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(frameCommandBuffer);
		vkEndCommandBuffer(frameCommandBuffer);

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphores[acquireSemIndex];
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

	// See the header comment on `frameInProgress`. Every per-object
	// BeginCommandBuffer()/EndCommandBuffer() pair IRenderer already
	// issues (RenderObject()/BindMesh()/EndRender()) becomes a cheap
	// reference to the one real, already-open frameCommandBuffer once
	// BeginFrame() has run - if it hasn't (e.g. the shadow sub-pass inside
	// PreRender(), called before RenderScene()/BeginFrame() - see
	// IRenderDevice.h's comment), this returns 0, and every device method
	// that receives a 0 cmd (BindVertexArray/DrawElements/etc, all guarded
	// on `frameInProgress` too) safely no-ops instead of touching a
	// command buffer no one began recording into.
	// True during either a real swapchain frame or an offscreen FBO pass
	// (shadow maps) - see the header comment on activeCommandBuffer for
	// why both share this single-active-pass model rather than needing
	// separate parallel state.
	CommandBufferHandle VulkanRenderDevice::BeginCommandBuffer() { return (frameInProgress || offscreenPassOpen) ? 1 : 0; }
	void VulkanRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd) {}

	void VulkanRenderDevice::BeginFrame()
	{
		if (swapchain == VK_NULL_HANDLE || frameInProgress)
			return;

		// Self-heal: if a previous RecreateSwapchain() call bailed out
		// (e.g. a momentarily degenerate 0x0 extent mid-resize - see its
		// comment) or otherwise left the device without a render pass/
		// framebuffers, retry every tick rather than only reacting to
		// vkAcquireNextImageKHR()'s result below - acquiring against a
		// technically-still-valid swapchain whose dependents don't exist
		// is exactly the "resize permanently breaks rendering" failure
		// mode this exists to close off.
		if (framebuffers.empty() || renderPass == VK_NULL_HANDLE)
		{
			RecreateSwapchain(swapchainExtent.width, swapchainExtent.height);
			return;
		}

		// Bounded, not UINT64_MAX: on macOS/MoltenVK, acquiring/waiting
		// with no timeout at all can genuinely block the whole process
		// during an active window resize drag - the CAMetalLayer can
		// stop handing back drawables for the duration of the resize
		// (this is a real, observed platform behavior, not
		// hypothetical - a resize was seen to hang the process outright
		// before this fix). A few-second bound means a stalled resize
		// degrades to "skip this frame, try again next tick" instead of
		// a frozen process; it's far longer than any real single-frame
		// GPU workload this backend does, so it never fires under normal
		// operation.
		static const uint64_t FRAME_WAIT_TIMEOUT_NS = 2000000000ULL;
		if (vkWaitForFences(device, 1, &frameFence, VK_TRUE, FRAME_WAIT_TIMEOUT_NS) != VK_SUCCESS)
			return;

		// Do *not* reset frameFence until we know this frame is actually
		// going to submit and re-signal it (right before
		// vkResetCommandBuffer below, once acquire has already
		// succeeded). Resetting it unconditionally here - the previous
		// version of this function did - meant any early-return between
		// here and the real submit (acquire failing, which happens
		// constantly during a resize) left the fence permanently
		// unsignaled with nothing left to ever signal it again: every
		// later BeginFrame() call would then block for the *full*
		// timeout on a fence that can never become signaled, return, and
		// immediately be called again by the main loop - forever. From
		// the outside (and to a user) that reads as the app permanently
		// hanging on resize, not "gracefully skipping a frame" - reproduced
		// and confirmed via `lldb -p <pid> -o "bt all"` while stuck:
		// the main thread was parked inside this exact vkWaitForFences
		// call, indefinitely.
		uint32 imageIndex = 0;
		// See nextAcquireSemaphoreIndex's comment - must advance on every
		// attempt, success or failure. currentFrameAcquireSemaphoreIndex
		// (used by EndFrame()'s submit) only gets updated below once
		// acquire actually succeeds.
		uint32 acquireSemIndex = nextAcquireSemaphoreIndex;
		nextAcquireSemaphoreIndex = (nextAcquireSemaphoreIndex + 1) % (uint32)imageAvailableSemaphores.size();
		VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, FRAME_WAIT_TIMEOUT_NS, imageAvailableSemaphores[acquireSemIndex], VK_NULL_HANDLE, &imageIndex);
		// VK_ERROR_OUT_OF_DATE_KHR (surface changed size/properties enough
		// that this swapchain can no longer be used at all) and
		// VK_SUBOPTIMAL_KHR (still usable, but no longer an exact match -
		// also worth rebuilding, or the mismatch just persists forever)
		// are both the surface telling us a resize happened - see
		// RecreateSwapchain()'s comment. Skip this frame either way; the
		// next BeginFrame() call retries against the freshly-rebuilt
		// swapchain. VK_TIMEOUT (drawable not ready yet, still mid-resize)
		// is handled the same way - just try again next tick. frameFence
		// is still signaled from the last real completed frame in every
		// one of these cases, so the *next* call's vkWaitForFences above
		// returns immediately rather than blocking again.
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
		{
			ResetAcquireSemaphore(acquireSemIndex);
			RecreateSwapchain(swapchainExtent.width, swapchainExtent.height);
			return;
		}
		if (acquireResult != VK_SUCCESS)
		{
			ResetAcquireSemaphore(acquireSemIndex);
			return;
		}
		vkResetFences(device, 1, &frameFence);
		currentFrameAcquireSemaphoreIndex = acquireSemIndex;
		currentImageIndex = imageIndex;
		currentVao = 0;
		currentPipeline = 0;

		vkResetCommandBuffer(frameCommandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(frameCommandBuffer, &beginInfo);

		VkClearValue clearValues[2] = {};
		clearValues[0].color = { { pendingClearColor.x, pendingClearColor.y, pendingClearColor.z, pendingClearColor.w } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBegin = {};
		renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBegin.renderPass = renderPass;
		renderPassBegin.framebuffer = framebuffers[imageIndex];
		renderPassBegin.renderArea.extent = swapchainExtent;
		renderPassBegin.clearValueCount = 2;
		renderPassBegin.pClearValues = clearValues;
		vkCmdBeginRenderPass(frameCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

		// CreatePipeline() left viewport/scissor dynamic (see its comment) -
		// set them here once, to the full swapchain extent, rather than
		// per-draw (every BindPipeline() would otherwise need to redo this).
		VkViewport viewport = { 0.0f, 0.0f, (f32)swapchainExtent.width, (f32)swapchainExtent.height, 0.0f, 1.0f };
		vkCmdSetViewport(frameCommandBuffer, 0, 1, &viewport);
		VkRect2D scissor = { { 0, 0 }, swapchainExtent };
		vkCmdSetScissor(frameCommandBuffer, 0, 1, &scissor);

		activeCommandBuffer = frameCommandBuffer;
		frameInProgress = true;
	}

	void VulkanRenderDevice::EndFrame()
	{
		if (!frameInProgress)
			return;
		frameInProgress = false;
		activeCommandBuffer = VK_NULL_HANDLE;
		currentVao = 0;

		vkCmdEndRenderPass(frameCommandBuffer);

		// Capture happens *before* vkEndCommandBuffer/present - see the
		// header comment on RequestFrameCapture() for why post-present is
		// invalid. The render pass's finalLayout already transitioned the
		// image to PRESENT_SRC_KHR at vkCmdEndRenderPass() just above
		// (baked into InitializeSwapchain()'s VkAttachmentDescription), so
		// the copy needs its own barrier there and back, all still within
		// this same frameCommandBuffer/submission - no separate
		// acquire/ownership concerns since the image is still "ours"
		// until vkQueuePresentKHR() runs, below.
		VkBuffer captureStagingBuffer = VK_NULL_HANDLE;
		VmaAllocation captureStagingAllocation = VK_NULL_HANDLE;
		void* captureStagingMapped = NULL;
		bool capturingThisFrame = captureRequested && allocator != VK_NULL_HANDLE &&
			(swapchainFormat == VK_FORMAT_B8G8R8A8_UNORM || swapchainFormat == VK_FORMAT_B8G8R8A8_SRGB ||
			 swapchainFormat == VK_FORMAT_R8G8B8A8_UNORM || swapchainFormat == VK_FORMAT_R8G8B8A8_SRGB);
		if (capturingThisFrame)
		{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = (VkDeviceSize)swapchainExtent.width * swapchainExtent.height * 4;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo stagingAllocInfo = {};
			stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

			VmaAllocationInfo stagingInfo;
			if (vmaCreateBuffer(allocator, &bufferInfo, &stagingAllocInfo, &captureStagingBuffer, &captureStagingAllocation, &stagingInfo) == VK_SUCCESS)
			{
				captureStagingMapped = stagingInfo.pMappedData;

				VkImageMemoryBarrier toTransferBarrier = {};
				toTransferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				toTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				toTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				toTransferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				toTransferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				toTransferBarrier.image = swapchainImages[currentImageIndex];
				toTransferBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				toTransferBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				toTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				vkCmdPipelineBarrier(frameCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toTransferBarrier);

				VkBufferImageCopy region = {};
				region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
				region.imageExtent = { swapchainExtent.width, swapchainExtent.height, 1 };
				vkCmdCopyImageToBuffer(frameCommandBuffer, swapchainImages[currentImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, captureStagingBuffer, 1, &region);

				VkImageMemoryBarrier toPresentBarrier = toTransferBarrier;
				toPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				toPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				toPresentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				toPresentBarrier.dstAccessMask = 0;
				vkCmdPipelineBarrier(frameCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &toPresentBarrier);
			}
			else
			{
				capturingThisFrame = false;
			}
		}

		vkEndCommandBuffer(frameCommandBuffer);

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrameAcquireSemaphoreIndex];
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frameCommandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentImageIndex];
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence) != VK_SUCCESS)
		{
			if (captureStagingBuffer != VK_NULL_HANDLE)
				vmaDestroyBuffer(allocator, captureStagingBuffer, captureStagingAllocation);
			return;
		}

		if (capturingThisFrame)
		{
			// Block until this frame's GPU work (including the copy above)
			// actually finishes before reading the staging buffer's mapped
			// memory - diagnostic-only, so a stall here is acceptable
			// (this is not on any normal, non-capturing frame's path).
			vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);

			capturedPixels.resize((size_t)swapchainExtent.width * swapchainExtent.height * 4);
			memcpy(capturedPixels.data(), captureStagingMapped, capturedPixels.size());
			capturedWidth = swapchainExtent.width;
			capturedHeight = swapchainExtent.height;
			capturedRedByteOffset = (swapchainFormat == VK_FORMAT_B8G8R8A8_UNORM || swapchainFormat == VK_FORMAT_B8G8R8A8_SRGB) ? 2 : 0;
			capturedFrameValid = true;
			captureRequested = false;

			vmaDestroyBuffer(allocator, captureStagingBuffer, captureStagingAllocation);
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentImageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &currentImageIndex;
		vkQueuePresentKHR(presentQueue, &presentInfo);
	}

	uint32 VulkanRenderDevice::TranslateBufferBit(const uint32 bufferBits) { return 0; }
	void VulkanRenderDevice::Clear(const uint32 nativeBufferBits) {}
	void VulkanRenderDevice::SetClearColor(const Vec4 &color) { pendingClearColor = color; }

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

	// Maps a Buffer::Attribute::Type (GeometryBuffer.h) to the VkFormat a
	// single vertex-input location of that type should read as. Matrix is
	// handled by CreatePipeline() emitting 4 consecutive locations, each
	// one vec4 "row" - this returns the per-row format for that case.
	// Int/Short are unverified against this backend: GL's equivalent path
	// (glVertexAttribPointer, non-integer variant) implicitly converts the
	// fetched integer to float before it reaches a float-typed shader
	// input, which plain SINT/UINT VkFormats do not do - no mesh this
	// backend has been validated against uses either type, so this is a
	// best-effort mapping, not a confirmed-correct one.
	static VkFormat TranslateAttributeFormatVk(const uint32 type)
	{
		switch (type)
		{
		case Buffer::Attribute::Type::Float: return VK_FORMAT_R32_SFLOAT;
		case Buffer::Attribute::Type::Vec2: return VK_FORMAT_R32G32_SFLOAT;
		case Buffer::Attribute::Type::Vec3: return VK_FORMAT_R32G32B32_SFLOAT;
		case Buffer::Attribute::Type::Vec4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case Buffer::Attribute::Type::Matrix: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case Buffer::Attribute::Type::Int: return VK_FORMAT_R32_SINT;
		case Buffer::Attribute::Type::Short: return VK_FORMAT_R16_SINT;
		default: return VK_FORMAT_R32G32B32_SFLOAT;
		}
	}

	static VkSamplerAddressMode TranslateTextureRepeatVk(const uint32 repeat)
	{
		switch (repeat)
		{
		case TextureRepeat::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TextureRepeat::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case TextureRepeat::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TextureRepeat::Repeat: default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	static void TranslateTextureFilterVk(const uint32 filter, VkFilter &outFilter, VkSamplerMipmapMode &outMipmapMode)
	{
		switch (filter)
		{
		case TextureFilter::Nearest: outFilter = VK_FILTER_NEAREST; outMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
		case TextureFilter::LinearMipmapLinear: outFilter = VK_FILTER_LINEAR; outMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
		case TextureFilter::LinearMipmapNearest: outFilter = VK_FILTER_LINEAR; outMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
		case TextureFilter::NearestMipmapNearest: outFilter = VK_FILTER_NEAREST; outMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
		case TextureFilter::NearestMipmapLinear: outFilter = VK_FILTER_NEAREST; outMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
		case TextureFilter::Linear: default: outFilter = VK_FILTER_LINEAR; outMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
		}
	}

	DeviceHandle VulkanRenderDevice::CreatePipeline(const PipelineDesc &desc)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(desc.shaderProgram);
		if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || progIt == programs.end() || progIt->second.pipelineLayout == VK_NULL_HANDLE)
		{
			fprintf(stderr, "VulkanRenderDevice::CreatePipeline: FAILED - device=%p renderPass=%p programFound=%d pipelineLayout=%p (shaderProgram handle=%u)\n",
				(void*)device, (void*)renderPass, progIt != programs.end(),
				progIt != programs.end() ? (void*)progIt->second.pipelineLayout : (void*)0, desc.shaderProgram);
			return 0;
		}
		std::map<DeviceHandle, ShaderStageRecord>::iterator vs = shaderStages.find(progIt->second.vertexShader);
		std::map<DeviceHandle, ShaderStageRecord>::iterator fs = shaderStages.find(progIt->second.fragmentShader);
		if (vs == shaderStages.end() || fs == shaderStages.end() || vs->second.module == VK_NULL_HANDLE || fs->second.module == VK_NULL_HANDLE)
		{
			fprintf(stderr, "VulkanRenderDevice::CreatePipeline: FAILED - vertexShader/fragmentShader stage lookup or module missing (vs found=%d, fs found=%d)\n",
				vs != shaderStages.end(), fs != shaderStages.end());
			return 0;
		}

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vs->second.module;
		stages[0].pName = "main";
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fs->second.module;
		stages[1].pName = "main";

		// Vertex input - built from the mesh's actual attribute layout
		// (PipelineDesc::vertexLayout, populated by IRenderer::BindMesh()
		// from RenderingMesh::Geometry->Attributes) rather than hardcoded
		// to any one mesh's layout. Attribute *locations* are resolved by
		// name against this program's reflected vertex-stage inputs
		// (attributeLocations, built in LinkProgram()) - Vulkan has no
		// runtime "get location by name" the way GL's glGetAttribLocation
		// does, so this is the equivalent lookup, just done once here
		// instead of per-mesh at runtime.
		if (desc.vertexLayout.empty())
		{
			fprintf(stderr, "VulkanRenderDevice::CreatePipeline: FAILED - PipelineDesc::vertexLayout is empty (caller must populate it from the mesh's actual attribute layout)\n");
			return 0;
		}

		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
		for (size_t bufferIndex = 0; bufferIndex < desc.vertexLayout.size(); bufferIndex++)
		{
			const VertexBufferLayoutDesc &bufferLayout = desc.vertexLayout[bufferIndex];

			VkVertexInputBindingDescription bindingDesc = {};
			bindingDesc.binding = (uint32_t)bufferIndex;
			bindingDesc.stride = bufferLayout.stride;
			// Vulkan's inputRate is per-*binding*, GL's divisor is per-
			// *attribute* - taking the first attribute's divisor assumes
			// a buffer doesn't mix per-vertex and per-instance attributes,
			// true for every mesh this backend has been validated
			// against so far (documented simplification, not a general
			// solution - matches this file's existing precedent for
			// flagging known-narrow assumptions rather than silently
			// guessing).
			bool isInstanced = !bufferLayout.attributes.empty() && bufferLayout.attributes[0].divisor > 0;
			bindingDesc.inputRate = isInstanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
			bindings.push_back(bindingDesc);

			for (size_t attrIndex = 0; attrIndex < bufferLayout.attributes.size(); attrIndex++)
			{
				const VertexAttributeDesc &attr = bufferLayout.attributes[attrIndex];
				std::map<std::string, uint32>::const_iterator locIt = progIt->second.attributeLocations.find(attr.name);
				if (locIt == progIt->second.attributeLocations.end())
					continue; // shader doesn't declare this attribute - matches GL's "location < 0" skip in IRenderer::BindMesh()

				// A Matrix attribute occupies 4 consecutive locations, one
				// vec4 ("row") each - mirrors GL's SetVertexAttribute(location+1/+2/+3, ...)
				// handling for Buffer::Attribute::Type::Matrix in BindMesh().
				uint32 componentCount = (attr.type == Buffer::Attribute::Type::Matrix) ? 4 : 1;
				VkFormat format = TranslateAttributeFormatVk(attr.type);
				for (uint32 c = 0; c < componentCount; c++)
				{
					VkVertexInputAttributeDescription attrDesc = {};
					attrDesc.location = locIt->second + c;
					attrDesc.binding = (uint32_t)bufferIndex;
					attrDesc.format = format;
					attrDesc.offset = attr.offset + c * 16;
					attributes.push_back(attrDesc);
				}
			}
		}

		VkPipelineVertexInputStateCreateInfo vertexInput = {};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = (uint32_t)bindings.size();
		vertexInput.pVertexBindingDescriptions = bindings.empty() ? NULL : bindings.data();
		vertexInput.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
		vertexInput.pVertexAttributeDescriptions = attributes.empty() ? NULL : attributes.data();

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
		// GL's front-face winding is CCW, never overridden anywhere in
		// this codebase, and Vulkan keeps that same meaning here - despite
		// TranslateProjectionMatrix() flipping clip-space Y for Vulkan's
		// NDC convention. An earlier version of this line compensated
		// with VK_FRONT_FACE_CLOCKWISE on the theory that the Y-flip
		// mirrors every triangle's apparent winding; that reasoning
		// checked out on paper (twice) but was empirically wrong - it
		// silently back-face-culled every mesh's near-camera faces
		// (RotatingCube rendered as a hollow shell: correct silhouette
		// bbox, ~25% interior fill, only grazing-angle side faces
		// surviving). Confirmed via a GL-vs-Vulkan pixel-readback
		// comparison (avg luminance + bounding box of an identical scene
		// swept through 8 rotation angles): VK_FRONT_FACE_COUNTER_CLOCKWISE
		// gives an exact match to GL at every angle; CLOCKWISE does not.
		// Trust that measurement over the paper derivation.
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

		// A depth-only render pass's subpass has zero color attachments -
		// the color-blend state's attachment count must match that
		// exactly for render-pass compatibility (see the comment on
		// PipelineDesc::isShadowPass/shadowPipelineRenderPass for why
		// this distinction exists at all).
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = desc.isShadowPass ? 0 : 1;
		colorBlending.pAttachments = desc.isShadowPass ? NULL : &blendAttachment;

		VkRenderPass targetRenderPass = renderPass;
		if (desc.isShadowPass)
		{
			if (shadowPipelineRenderPass == VK_NULL_HANDLE)
			{
				FBORecord templateFbo;
				if (!BuildDepthOnlyRenderPass(templateFbo, VK_FORMAT_D32_SFLOAT))
				{
					fprintf(stderr, "VulkanRenderDevice::CreatePipeline: failed to build the shared shadow-pipeline render pass\n");
					return 0;
				}
				shadowPipelineRenderPass = templateFbo.renderPass;
			}
			targetRenderPass = shadowPipelineRenderPass;
		}

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
		pipelineInfo.renderPass = targetRenderPass;
		pipelineInfo.subpass = 0;

		VkPipeline pipeline;
		VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline);
		if (result != VK_SUCCESS)
		{
			fprintf(stderr, "VulkanRenderDevice::CreatePipeline: vkCreateGraphicsPipelines FAILED with VkResult=%d\n", (int)result);
			return 0;
		}

		DeviceHandle handle = nextPipelineHandle++;
		pipelines[handle] = pipeline;
		pipelineToProgram[handle] = desc.shaderProgram;

		// This pipeline's own sampler descriptor set (set=1) - see the
		// comment on ProgramRecord::samplerSetLayout for why every
		// pipeline gets its own instead of sharing one per program.
		// descriptorPool is created (see the sizing comment on that field)
		// by BindUniformBlockIfPresent(), which BindMesh() always calls
		// before ever reaching CreatePipeline() - if it's somehow still
		// NULL here, skip rather than guess at a second creation path.
		if (descriptorPool != VK_NULL_HANDLE)
		{
			VkDescriptorSetAllocateInfo samplerSetAllocInfo = {};
			samplerSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			samplerSetAllocInfo.descriptorPool = descriptorPool;
			samplerSetAllocInfo.descriptorSetCount = 1;
			samplerSetAllocInfo.pSetLayouts = &progIt->second.samplerSetLayout;
			VkDescriptorSet samplerSet = VK_NULL_HANDLE;
			if (vkAllocateDescriptorSets(device, &samplerSetAllocInfo, &samplerSet) == VK_SUCCESS)
				pipelineSamplerSets[handle] = samplerSet;
			else
				fprintf(stderr, "VulkanRenderDevice::CreatePipeline: vkAllocateDescriptorSets (sampler set) failed for pipeline %u\n", handle);
		}

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
		pipelineToProgram.erase(pipeline);
		// The VkDescriptorSet itself isn't individually freed - descriptorPool
		// was created without VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		// so every set it ever allocated is only reclaimed by
		// vkDestroyDescriptorPool (the device destructor) - just drop the
		// bookkeeping entry here.
		pipelineSamplerSets.erase(pipeline);
	}

	void VulkanRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		if (!(frameInProgress || offscreenPassOpen) || cmd == 0)
			return;
		std::map<DeviceHandle, VkPipeline>::iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
		{
			fprintf(stderr, "VulkanRenderDevice::BindPipeline: pipeline handle %u not found (CreatePipeline likely failed earlier) - draw calls will be skipped this bind\n", pipeline);
			currentPipeline = 0;
			return;
		}
		currentPipeline = pipeline;
		vkCmdBindPipeline(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, it->second);

		// Deliberately *not* binding descriptor sets here (moved to
		// DrawElements()/DrawElementsInstanced(), right before the
		// actual draw) - see the comment there for why: this call runs
		// on mesh/material *switch*, before Material->PreRender()/
		// SendGlobalUniforms()/SendUserUniforms() have had a chance to
		// write this object's texture/shadow-map descriptors via
		// SendUniformInt(). Binding a descriptor set here and then
		// updating its contents afterward - still within the same
		// not-yet-submitted command buffer - is invalid without the
		// VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT feature (not
		// enabled here): VUID-vkCmdBindPipeline-commandBuffer-recording
		// caught this the hard way, the first time a shadow-casting
		// object's descriptor write happened after its pipeline (and,
		// with the old code, its descriptor sets) were already bound.
	}

	void VulkanRenderDevice::EnableClipDistance(const uint32 index) {}
	void VulkanRenderDevice::DisableClipDistance(const uint32 index) {}

	// Real per-draw viewport/scissor override - used by the shadow pass's
	// per-cascade directional-light viewport regions (up to 4 quadrants
	// of one shadow atlas texture, see IRenderer::PreRender()'s
	// DIRECTIONAL case) via vkCmdSetViewport/vkCmdSetScissor as dynamic
	// state (CreatePipeline() already declares both dynamic - see its
	// comment). BeginFrame()/AttachFramebufferTexture2D() already set a
	// default full-target viewport when a pass begins, so most draws
	// never need to call this at all - it only matters when a caller
	// wants something narrower.
	void VulkanRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height)
	{
		if (!(frameInProgress || offscreenPassOpen) || activeCommandBuffer == VK_NULL_HANDLE)
			return;
		VkViewport viewport = { (f32)x, (f32)y, (f32)width, (f32)height, 0.0f, 1.0f };
		VkRect2D scissor = { { (int32_t)x, (int32_t)y }, { width, height } };
		vkCmdSetViewport(activeCommandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(activeCommandBuffer, 0, 1, &scissor);
	}

	// Mirrors GL's implicit "current program" state (glUseProgram) -
	// GetUniformLocation()/SendUniformInt() below need to know which
	// program's reflected sampler bindings to resolve against, and
	// receive no program argument themselves (matching IRenderDevice's
	// existing GL-shaped contract, where glUniform1i et al also only ever
	// operate on whatever program is currently in use).
	void VulkanRenderDevice::UseProgram(const uint32 program) { currentProgram = program; }
	// See the header comment on VaoRecord for the overall design - this
	// only allocates a handle and a zeroed record; BindArrayBuffer()/
	// BindElementBuffer() fill it in afterward, the same way
	// glGenVertexArrays() alone doesn't populate anything either.
	DeviceHandle VulkanRenderDevice::CreateVertexArray()
	{
		DeviceHandle handle = nextVaoHandle++;
		vaos[handle] = VaoRecord();
		return handle;
	}

	void VulkanRenderDevice::DeleteVertexArray(const DeviceHandle vao)
	{
		vaos.erase(vao);
	}

	// Selects `vao` as the implicit target for BindArrayBuffer()/
	// BindElementBuffer() (mirrors glBindVertexArray() making a VAO the
	// implicit target of subsequent glBindBuffer() calls) *and* as the
	// buffer pair DrawElements()/DrawElementsInstanced() will read from -
	// both BindMesh() (building a fresh VAO) and RenderObject() (re-binding
	// an already-cached one before drawing) call this the same way GL's
	// glBindVertexArray() serves both purposes identically. `cmd` is
	// unused - see BeginCommandBuffer()'s comment; a 0 vao (GL's "unbind")
	// just clears the selection, same as GL leaving nothing meaningfully
	// bound.
	void VulkanRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao)
	{
		currentVao = vao;
	}

	void VulkanRenderDevice::BindArrayBuffer(const uint32 buffer)
	{
		if (currentVao == 0)
			return;
		std::map<DeviceHandle, VaoRecord>::iterator it = vaos.find(currentVao);
		if (it != vaos.end())
			it->second.vertexBuffer = buffer;
	}

	void VulkanRenderDevice::BindElementBuffer(const uint32 buffer)
	{
		if (currentVao == 0)
			return;
		std::map<DeviceHandle, VaoRecord>::iterator it = vaos.find(currentVao);
		if (it != vaos.end())
			it->second.indexBuffer = buffer;
	}
	void VulkanRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) {}
	void VulkanRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) {}
	void VulkanRenderDevice::DisableVertexAttribute(const int32 location) {}
	void VulkanRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor) {}
	void VulkanRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(program);
		if (device == VK_NULL_HANDLE || progIt == programs.end() || progIt->second.descriptorSetLayout == VK_NULL_HANDLE)
			return;
		// blockName is unused - unlike GL (which looks a block up by name
		// via glGetUniformBlockIndex, since a shader could in principle
		// bind a block to any binding point at runtime), this backend's
		// binding points are already static in the shader (see
		// PyrosShader.glsl's UBO_BINDING/BIND_* macros) and reflected
		// directly by binding index in LinkProgram() - blockName has
		// nothing left to resolve. Kept as a parameter only because it's
		// part of IRenderDevice's shared interface (GLRenderDevice still
		// needs it).
		(void)blockName;
		if (progIt->second.reflectedBindings.find(bindingPoint) == progIt->second.reflectedBindings.end())
			return; // shader doesn't declare a block at this binding - matches GL's no-op contract
		if (progIt->second.writtenBindings.find(bindingPoint) != progIt->second.writtenBindings.end())
			return; // already written for this program - see the comment on ProgramRecord::writtenBindings for why re-writing it is unsafe, not just wasteful

		std::map<uint32, DeviceHandle>::iterator bufIt = uniformBufferByBindingPoint.find(bindingPoint);
		if (bufIt == uniformBufferByBindingPoint.end())
			return; // no CreateUniformBuffer() at this binding point yet
		std::map<DeviceHandle, BufferRecord>::iterator recIt = buffers.find(bufIt->second);
		if (recIt == buffers.end())
			return;

		// Lazily create the shared descriptor pool - see the header
		// comment on descriptorPool for the sizing rationale. Sized for
		// up to 1024 sets total: one UBO set per program (few - one per
		// distinct shader variant) plus one sampler set per *pipeline*
		// (see ProgramRecord::samplerSetLayout's comment for why samplers
		// need per-pipeline, not per-program, granularity) - a real but
		// generous cap, not dynamically growable; a scene with more than
		// ~1024 distinct (mesh,shader) pairs would need this revisited.
		if (descriptorPool == VK_NULL_HANDLE)
		{
			// See IsPerObjectDynamicBinding()'s comment - up to 6 of a
			// program's UBO bindings are VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			// instead of plain UNIFORM_BUFFER, so the pool needs a
			// reservation for that type too.
			VkDescriptorPoolSize poolSizes[3] = {};
			poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			poolSizes[0].descriptorCount = 64;
			poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSizes[1].descriptorCount = 4096;
			poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			poolSizes[2].descriptorCount = 64;

			VkDescriptorPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.maxSets = 1024;
			poolInfo.poolSizeCount = 3;
			poolInfo.pPoolSizes = poolSizes;
			if (vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool) != VK_SUCCESS)
				return;
		}

		// Lazily allocate this program's descriptor set - see the header
		// comment on ProgramRecord::descriptorSet.
		if (progIt->second.descriptorSet == VK_NULL_HANDLE)
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &progIt->second.descriptorSetLayout;
			if (vkAllocateDescriptorSets(device, &allocInfo, &progIt->second.descriptorSet) != VK_SUCCESS)
				return;
		}

		// offset stays 0 regardless of isDynamicUniform - that's the
		// descriptor's *base* offset; the actual current slot gets added
		// on top of this at bind time via vkCmdBindDescriptorSets'
		// pDynamicOffsets (BindCurrentPipelineDescriptorSets()), not baked
		// in here. range uses the unpadded `size` either way (the
		// shader's real declared block size - see BufferRecord::
		// alignedSlotSize's comment on why the padding isn't part of it).
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = recIt->second.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = recIt->second.size;

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = progIt->second.descriptorSet;
		write.dstBinding = bindingPoint;
		write.descriptorCount = 1;
		write.descriptorType = recIt->second.isDynamicUniform ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
		progIt->second.writtenBindings.insert(bindingPoint);
	}

	// Unused by DrawElements()/DrawElementsInstanced() below - Vulkan bakes
	// primitive topology into the pipeline (CreatePipeline() hardcodes
	// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST), not the draw call, so the
	// translated value this returns is never actually read for this
	// backend; kept returning 0 rather than a real translation table since
	// nothing consumes the result.
	// See the comment on this method in IRenderDevice.h. Combines both
	// corrections Vulkan's NDC convention needs relative to GL's (which is
	// what Matrix::PerspectiveMatrix()/OrthoMatrix() build) into one
	// matrix, left-multiplied onto the projection matrix so the shader's
	// existing `uProjectionMatrix * uViewMatrix * ModelMatrix * pos`
	// order doesn't need to change at all:
	//   - Z: GL's clip Z range is [-1,1]; Vulkan's is [0,1]. Remapped via
	//     z' = 0.5*z + 0.5*w (row 3 below).
	//   - Y: GL's NDC Y+ points up the framebuffer; Vulkan's points down.
	//     Flipped via negating row 2's Y coefficient.
	// Found the hard way: every draw this session had validation-clean,
	// crash-free output, but this session's first actual pixel-level
	// check (reading back the swapchain, not just checking for errors)
	// showed 0% of the expected color on screen - Vulkan's stricter clip
	// volume (no negative Z, unlike GL) was discarding essentially the
	// entire scene before rasterization, silently (clipping isn't a
	// validation error, it's correct behavior given the mismatched
	// convention).
	Matrix VulkanRenderDevice::TranslateProjectionMatrix(const Matrix &projectionMatrix)
	{
		// Matrix's constructor takes arguments column-by-column
		// (n11,n21,n31,n41 = column 1, ...; see Matrix.h), not row-by-row -
		// this is the column-major encoding of the row-major mathematical
		// matrix described in the header comment on this method:
		//   [1  0  0   0 ]
		//   [0 -1  0   0 ]
		//   [0  0  0.5 0.5]
		//   [0  0  0   1 ]
		static const Matrix clipCorrection(
			1.f, 0.f, 0.f, 0.f,
			0.f, -1.f, 0.f, 0.f,
			0.f, 0.f, 0.5f, 0.f,
			0.f, 0.f, 0.5f, 1.f
		);
		return clipCorrection * projectionMatrix;
	}

	// See the comment on this method in IRenderDevice.h - X/Y remapped
	// clip[-1,1]->[0,1] exactly like Matrix::BIAS, but Z passes through
	// unchanged (TranslateProjectionMatrix() above already remapped it):
	//   [0.5  0    0   0.5]
	//   [0    0.5  0   0.5]
	//   [0    0    1   0  ]
	//   [0    0    0   1  ]
	Matrix VulkanRenderDevice::TranslateShadowBiasMatrix()
	{
		static const Matrix xyOnlyBias(
			0.5f, 0.f, 0.f, 0.f,
			0.f, 0.5f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.5f, 0.5f, 0.f, 1.f
		);
		return xyOnlyBias;
	}

	uint32 VulkanRenderDevice::TranslateDrawType(const uint32 engineDrawType) { return 0; }

	// Not implemented - nothing on this backend's validation path
	// (RotatingCube) uses non-indexed draws.
	void VulkanRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) {}

	void VulkanRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount)
	{
		if (!(frameInProgress || offscreenPassOpen) || cmd == 0 || currentVao == 0)
			return;
		if (currentPipeline == 0)
		{
			fprintf(stderr, "VulkanRenderDevice::DrawElements: skipped draw - no valid pipeline is currently bound\n");
			return;
		}
		std::map<DeviceHandle, VaoRecord>::iterator vaoIt = vaos.find(currentVao);
		if (vaoIt == vaos.end())
			return;
		std::map<DeviceHandle, BufferRecord>::iterator vboIt = buffers.find(vaoIt->second.vertexBuffer);
		std::map<DeviceHandle, BufferRecord>::iterator iboIt = buffers.find(vaoIt->second.indexBuffer);
		if (vboIt == buffers.end() || iboIt == buffers.end())
			return;

		BindCurrentPipelineDescriptorSets();

		VkBuffer vbo = vboIt->second.buffer;
		VkDeviceSize vboOffset = 0;
		vkCmdBindVertexBuffers(activeCommandBuffer, 0, 1, &vbo, &vboOffset);
		// __INDEX_C_TYPE__ (Global.h) is uint32 - matches VK_INDEX_TYPE_UINT32.
		vkCmdBindIndexBuffer(activeCommandBuffer, iboIt->second.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(activeCommandBuffer, indexCount, 1, 0, 0, 0);
	}

	void VulkanRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount)
	{
		if (!(frameInProgress || offscreenPassOpen) || cmd == 0 || currentVao == 0)
			return;
		if (currentPipeline == 0)
		{
			fprintf(stderr, "VulkanRenderDevice::DrawElementsInstanced: skipped draw - no valid pipeline is currently bound\n");
			return;
		}
		std::map<DeviceHandle, VaoRecord>::iterator vaoIt = vaos.find(currentVao);
		if (vaoIt == vaos.end())
			return;
		std::map<DeviceHandle, BufferRecord>::iterator vboIt = buffers.find(vaoIt->second.vertexBuffer);
		std::map<DeviceHandle, BufferRecord>::iterator iboIt = buffers.find(vaoIt->second.indexBuffer);
		if (vboIt == buffers.end() || iboIt == buffers.end())
			return;

		BindCurrentPipelineDescriptorSets();

		VkBuffer vbo = vboIt->second.buffer;
		VkDeviceSize vboOffset = 0;
		vkCmdBindVertexBuffers(activeCommandBuffer, 0, 1, &vbo, &vboOffset);
		vkCmdBindIndexBuffer(activeCommandBuffer, iboIt->second.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(activeCommandBuffer, indexCount, instanceCount, 0, 0, 0);
	}

	void VulkanRenderDevice::BindCurrentPipelineDescriptorSets()
	{
		std::map<DeviceHandle, DeviceHandle>::iterator progHandleIt = pipelineToProgram.find(currentPipeline);
		if (progHandleIt == pipelineToProgram.end())
			return;
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(progHandleIt->second);
		if (progIt == programs.end())
			return;
		if (progIt->second.descriptorSet != VK_NULL_HANDLE)
		{
			// Dynamic offsets must be supplied in the same order the
			// descriptor set layout's bindings were given at creation
			// time (CreatePipeline()'s reflection loop iterates
			// bindingsByIndex - a std::map, so ascending binding index -
			// reflectedBindings is a std::set, so iterating it gives that
			// same order for free), filtered to just the dynamic ones -
			// every plain, non-dynamic UBO binding contributes nothing
			// here regardless of where it falls in that order. Fixed-size
			// stack array, not a std::vector - this runs on every single
			// draw call, and IsPerObjectDynamicBinding()'s list is capped
			// at 7 entries, so there's no reason to heap-allocate here.
			uint32_t dynamicOffsets[7];
			uint32_t dynamicOffsetCount = 0;
			for (std::set<uint32>::iterator bIt = progIt->second.reflectedBindings.begin(); bIt != progIt->second.reflectedBindings.end(); bIt++)
			{
				if (!IsPerObjectDynamicBinding(*bIt))
					continue;
				std::map<uint32, DeviceHandle>::iterator bufHandleIt = uniformBufferByBindingPoint.find(*bIt);
				if (bufHandleIt == uniformBufferByBindingPoint.end())
					continue;
				std::map<DeviceHandle, BufferRecord>::iterator bufRecIt = buffers.find(bufHandleIt->second);
				if (bufRecIt == buffers.end())
					continue;
				if (dynamicOffsetCount >= 7)
					break; // can't happen - IsPerObjectDynamicBinding() only ever recognizes 7 binding points
				dynamicOffsets[dynamicOffsetCount++] = (uint32_t)((VkDeviceSize)bufRecIt->second.currentSlot * bufRecIt->second.alignedSlotSize);
			}
			vkCmdBindDescriptorSets(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, progIt->second.pipelineLayout, 0, 1, &progIt->second.descriptorSet,
				dynamicOffsetCount, dynamicOffsetCount > 0 ? dynamicOffsets : NULL);
		}
		std::map<DeviceHandle, VkDescriptorSet>::iterator samplerSetIt = pipelineSamplerSets.find(currentPipeline);
		if (samplerSetIt != pipelineSamplerSets.end() && samplerSetIt->second != VK_NULL_HANDLE)
			vkCmdBindDescriptorSets(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, progIt->second.pipelineLayout, 1, 1, &samplerSetIt->second, 0, NULL);
	}

	DeviceHandle VulkanRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint)
	{
		if (allocator == VK_NULL_HANDLE || sizeBytes == 0)
			return 0;

		// See IsPerObjectDynamicBinding()'s comment - these specific
		// binding points get a real multi-slot buffer (one slot per
		// ReplaceUniformBuffer() call) instead of the single `sizeBytes`
		// allocation every other UBO uses.
		bool dynamic = IsPerObjectDynamicBinding(bindingPoint);
		VkDeviceSize alignedSlotSize = sizeBytes;
		uint32 slotCount = 1;
		if (dynamic)
		{
			VkDeviceSize align = minUniformBufferOffsetAlignment > 0 ? minUniformBufferOffsetAlignment : 256;
			alignedSlotSize = ((VkDeviceSize)sizeBytes + align - 1) / align * align;
			slotCount = DYNAMIC_UBO_SLOT_COUNT;
		}
		VkDeviceSize totalSize = alignedSlotSize * slotCount;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = totalSize;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		BufferRecord record;
		record.size = sizeBytes;
		record.isDynamicUniform = dynamic;
		record.alignedSlotSize = alignedSlotSize;
		record.slotCount = slotCount;
		record.currentSlot = 0;
		VmaAllocationInfo allocationInfo;
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &record.buffer, &record.allocation, &allocationInfo) != VK_SUCCESS)
			return 0;
		record.mapped = allocationInfo.pMappedData;

		// Zero-initialize - matches CreateUniformBuffer()'s existing GL
		// contract (IRenderer's ctor calls this with no initial data,
		// relying on the buffer starting zeroed until the first
		// ReplaceUniformBuffer()/UpdateUniformBuffer() call).
		if (record.mapped != NULL)
			memset(record.mapped, 0, (size_t)totalSize);

		DeviceHandle handle = nextBufferHandle++;
		buffers[handle] = record;
		// See the comment on uniformBufferByBindingPoint in the header -
		// this is what BindUniformBlockIfPresent() looks up later.
		uniformBufferByBindingPoint[bindingPoint] = handle;
		return handle;
	}

	// Never advances a dynamic UBO's current slot - see
	// ReplaceUniformBuffer()'s comment for why that's its job, not this
	// one's. Writes at (current slot's base) + offset, so a caller that
	// needs to split one logical object/material-switch's worth of data
	// across multiple calls (the way DirectionalShadowUBO's two
	// PreRender()-time writes do, though that one isn't a dynamic binding)
	// can call ReplaceUniformBuffer() once to start it, then
	// UpdateUniformBuffer() for the rest, all landing in the same slot.
	void VulkanRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.mapped == NULL)
			return;
		BufferRecord &rec = it->second;
		VkDeviceSize slotBase = rec.isDynamicUniform ? (VkDeviceSize)rec.currentSlot * rec.alignedSlotSize : 0;
		memcpy((uchar*)rec.mapped + slotBase + offset, data, sizeBytes);
	}

	void VulkanRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data)
	{
		// No orphaning trick needed here the way GLRenderDevice's
		// ReplaceUniformBuffer() (see its comment) needs one for
		// glBufferSubData - this is a plain host-memory memcpy into
		// persistently-mapped, host-coherent memory; there's no implicit
		// CPU/GPU sync point to stall on the way glBufferSubData has.
		//
		// For a dynamic UBO (see IsPerObjectDynamicBinding()'s comment),
		// this call is the start of a new logical write - advance to the
		// next slot *before* writing, so this object/material-switch gets
		// a slot no other still-relevant draw this frame is using.
		// BindCurrentPipelineDescriptorSets() reads currentSlot right
		// before the matching draw is recorded, so the two stay in sync
		// as long as callers keep doing what IRenderer.cpp already does:
		// finish sending an object's uniforms before drawing it.
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it != buffers.end() && it->second.isDynamicUniform)
			it->second.currentSlot = (it->second.currentSlot + 1) % it->second.slotCount;
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

		// Vertex attribute locations - see the comment on
		// ProgramRecord::attributeLocations. Only the vertex stage has
		// externally-named "attribute" inputs in this engine's model.
		it->second.attributeLocations.clear();
		std::vector<SpirvStageInput> vsInputs = SpirvShaderCompiler::ReflectStageInputs(vs->second.spirv);
		for (size_t i = 0; i < vsInputs.size(); i++)
			it->second.attributeLocations[vsInputs[i].name] = vsInputs[i].location;

		// Split by resource type into two independent binding maps - UBOs
		// (set=0, descriptorSetLayout - shared per-program, one set is
		// correct since every draw using a program's UBOs shares the same
		// content) and samplers (set=1, samplerSetLayout - see the
		// comment on ProgramRecord::samplerSetLayout for why these can't
		// share one set per program the way UBOs do).
		std::map<uint32, VkDescriptorSetLayoutBinding> bindingsByIndex;
		std::map<uint32, VkDescriptorSetLayoutBinding> samplerBindingsByIndex;
		it->second.samplerBindings.clear();
		for (int stagePass = 0; stagePass < 2; stagePass++)
		{
			std::vector<SpirvResourceBinding> &resources = (stagePass == 0) ? vsResources : fsResources;
			VkShaderStageFlags stageFlag = (stagePass == 0) ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
			for (size_t i = 0; i < resources.size(); i++)
			{
				const SpirvResourceBinding &res = resources[i];
				std::map<uint32, VkDescriptorSetLayoutBinding> &targetMap = (res.type == SpirvResourceType::SampledImage) ? samplerBindingsByIndex : bindingsByIndex;
				if (res.type == SpirvResourceType::SampledImage)
				{
					it->second.samplerBindings[res.name] = res.binding;
					it->second.samplerArraySizes[res.binding] = res.arraySize;
				}
				std::map<uint32, VkDescriptorSetLayoutBinding>::iterator existing = targetMap.find(res.binding);
				if (existing != targetMap.end())
				{
					existing->second.stageFlags |= stageFlag;
					continue;
				}
				VkDescriptorSetLayoutBinding binding = {};
				binding.binding = res.binding;
				// See IsPerObjectDynamicBinding()'s comment - the handful
				// of UBOs that genuinely vary within a single frame need
				// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC (paired with
				// CreateUniformBuffer()'s multi-slot allocation and
				// BindCurrentPipelineDescriptorSets()'s per-draw dynamic
				// offset) instead of a plain UNIFORM_BUFFER descriptor,
				// which only ever points at one fixed buffer location.
				binding.descriptorType = (res.type == SpirvResourceType::SampledImage) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					: (IsPerObjectDynamicBinding(res.binding) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				// Must match the shader's own declared array length (e.g.
				// PyrosShader.glsl's `uPointShadowMaps[4]`) exactly - a
				// hardcoded 1 here fails pipeline creation outright
				// (VUID-VkGraphicsPipelineCreateInfo-layout-07991) for any
				// material using an array-typed sampler, which every
				// PointShadow/SpotShadow material does.
				binding.descriptorCount = res.arraySize;
				binding.stageFlags = stageFlag;
				targetMap[res.binding] = binding;
			}
		}

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
		for (std::map<uint32, VkDescriptorSetLayoutBinding>::iterator bIt = bindingsByIndex.begin(); bIt != bindingsByIndex.end(); bIt++)
		{
			layoutBindings.push_back(bIt->second);
			it->second.reflectedBindings.insert(bIt->first);
		}
		std::vector<VkDescriptorSetLayoutBinding> samplerLayoutBindings;
		for (std::map<uint32, VkDescriptorSetLayoutBinding>::iterator bIt = samplerBindingsByIndex.begin(); bIt != samplerBindingsByIndex.end(); bIt++)
		{
			samplerLayoutBindings.push_back(bIt->second);
			it->second.reflectedSamplerBindings.insert(bIt->first);
		}

		VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
		setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayoutInfo.bindingCount = (uint32_t)layoutBindings.size();
		setLayoutInfo.pBindings = layoutBindings.empty() ? NULL : layoutBindings.data();
		if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, NULL, &it->second.descriptorSetLayout) != VK_SUCCESS)
		{
			errorLog = "vkCreateDescriptorSetLayout failed";
			return false;
		}

		// Always create a (possibly zero-binding, still valid) sampler
		// set layout - keeps CreatePipeline()/BindPipeline() uniform
		// across textured and non-textured programs alike, rather than
		// special-casing "this program has no samplers".
		VkDescriptorSetLayoutCreateInfo samplerSetLayoutInfo = {};
		samplerSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		samplerSetLayoutInfo.bindingCount = (uint32_t)samplerLayoutBindings.size();
		samplerSetLayoutInfo.pBindings = samplerLayoutBindings.empty() ? NULL : samplerLayoutBindings.data();
		if (vkCreateDescriptorSetLayout(device, &samplerSetLayoutInfo, NULL, &it->second.samplerSetLayout) != VK_SUCCESS)
		{
			errorLog = "vkCreateDescriptorSetLayout (sampler set) failed";
			return false;
		}

		VkDescriptorSetLayout setLayouts[2] = { it->second.descriptorSetLayout, it->second.samplerSetLayout };
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 2;
		pipelineLayoutInfo.pSetLayouts = setLayouts;
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
			if (it->second.samplerSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, it->second.samplerSetLayout, NULL);
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
	// One real exception to the "-1 for anything absorbed into a UBO"
	// contract described above: sampler uniforms (e.g. "uColormap") are
	// *not* absorbed into a UBO - they stay declared as loose
	// `uniform sampler2D` in PyrosShader.glsl (opaque types are exempt
	// from Vulkan's "non-opaque uniforms outside a block" rule - see
	// VULKAN_ROADMAP.md's Phase 2 blocker list), just with a static
	// `layout(binding=N)`. GenericShaderMaterial still sends this name's
	// value as a plain int uniform (GL's mechanism for telling a sampler
	// which texture *unit* to read from - see AddTexture()/SetColorMap()
	// in GenericShaderMaterial.cpp) - repurposed here: returning the
	// reflected binding number (instead of -1) as this "location" means
	// the exact same SendUserUniforms() call that already runs
	// unconditionally reaches SendUniformInt() with everything it needs
	// to update this program's/pipeline's sampler descriptor - see that
	// method's comment for the other half of this mechanism.
	int32 VulkanRenderDevice::GetUniformLocation(const uint32 program, const std::string &name)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return -1;
		std::map<std::string, uint32>::iterator samplerIt = it->second.samplerBindings.find(name);
		if (samplerIt != it->second.samplerBindings.end())
			return (int32)samplerIt->second;
		return -1;
	}

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

	// Lazily (re)builds a texture's VkSampler from its currently-tracked
	// wrap/filter state (see the comment on TextureRecord) - GL applies
	// SetTextureWrapS/SetTextureMinFilter/etc immediately via glTexParameter*,
	// but Vulkan bakes all of a sampler's state into one immutable object
	// created up front, so this only actually runs the first time a
	// texture is used after any wrap/filter setter touched it
	// (samplerDirty), not on every setter call.
	bool VulkanRenderDevice::RebuildSamplerIfDirty(TextureRecord &tex)
	{
		if (!tex.samplerDirty)
			return tex.sampler != VK_NULL_HANDLE;
		if (tex.sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, tex.sampler, NULL);
			tex.sampler = VK_NULL_HANDLE;
		}

		VkFilter filter;
		VkSamplerMipmapMode mipmapMode;
		TranslateTextureFilterVk(tex.minFilter, filter, mipmapMode);
		VkFilter magFilter;
		VkSamplerMipmapMode unusedMipmapMode;
		TranslateTextureFilterVk(tex.magFilter, magFilter, unusedMipmapMode);

		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = magFilter;
		samplerInfo.minFilter = filter;
		samplerInfo.mipmapMode = mipmapMode;
		samplerInfo.addressModeU = TranslateTextureRepeatVk(tex.wrapS);
		samplerInfo.addressModeV = TranslateTextureRepeatVk(tex.wrapT);
		samplerInfo.addressModeW = samplerInfo.addressModeV;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = tex.hasMipmap ? VK_LOD_CLAMP_NONE : 0.0f;
		// Hardware depth-compare sampling for sampler2DShadow/
		// samplerCubeShadow (every shadow map - see
		// Texture::EnableCompareMode(), called by every light's
		// EnableCastShadows()). LESS_OR_EQUAL matches GL's default
		// GL_TEXTURE_COMPARE_FUNC (GL_LEQUAL) - PyrosShader.glsl's PCF
		// functions (PCFDIRECTIONAL/PCFPOINT/PCFSPOT) were written
		// against that convention and never override it.
		samplerInfo.compareEnable = tex.compareModeEnabled ? VK_TRUE : VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		if (vkCreateSampler(device, &samplerInfo, NULL, &tex.sampler) != VK_SUCCESS)
		{
			fprintf(stderr, "VulkanRenderDevice: vkCreateSampler failed\n");
			return false;
		}
		tex.samplerDirty = false;
		return true;
	}

	// The other half of the mechanism described in GetUniformLocation()'s
	// comment: GenericShaderMaterial's texture-uniform sending
	// (SendUserUniforms(), unconditionally already run every draw) ends
	// up calling this with `handle` = a sampler's reflected descriptor
	// binding (not a real "uniform location" - Vulkan samplers have no
	// such thing) and `data[0]` = the texture *unit* Texture::Bind() just
	// activated it at (see textureUnitBindings, populated by
	// ActivateTextureUnit()/BindTextureToTarget()'s render-time pairing).
	// Resolves unit -> real texture -> writes it into the *current
	// pipeline's* sampler descriptor set at that binding - see the
	// comment on ProgramRecord::samplerSetLayout for why this is
	// per-pipeline rather than per-program.
	void VulkanRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count)
	{
		if (handle < 0 || count == 0 || device == VK_NULL_HANDLE)
			return;
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(currentProgram);
		if (progIt == programs.end())
			return;
		if (progIt->second.reflectedSamplerBindings.find((uint32)handle) == progIt->second.reflectedSamplerBindings.end())
			return; // not a sampler binding for this program - not a texture-unit send this backend handles

		std::map<uint32, DeviceHandle>::iterator unitIt = textureUnitBindings.find((uint32)data[0]);
		if (unitIt == textureUnitBindings.end())
			return; // nothing actually bound at that unit
		std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(unitIt->second);
		if (texIt == textures.end() || texIt->second.view == VK_NULL_HANDLE)
			return; // texture handle stale, or UploadTexture2D() never ran (no image yet)

		std::map<DeviceHandle, VkDescriptorSet>::iterator setIt = pipelineSamplerSets.find(currentPipeline);
		if (setIt == pipelineSamplerSets.end() || setIt->second == VK_NULL_HANDLE)
			return; // no current pipeline / it has no sampler set (CreatePipeline() failed, or descriptorPool wasn't ready yet)

		if (!RebuildSamplerIfDirty(texIt->second))
			return;

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texIt->second.view;
		imageInfo.sampler = texIt->second.sampler;

		// If this binding is a shader-declared array (e.g.
		// `uPointShadowMaps[4]` - see ProgramRecord::samplerArraySizes'
		// comment), write the same descriptor into every element, not
		// just element 0: PyrosShader.glsl's PCF helpers only ever read
		// index 0 with a literal constant, but Vulkan still requires
		// every element the *type* declares to be a valid descriptor the
		// moment any element is dynamically accessed
		// (VUID-vkCmdDrawIndexed-None-08114) - leaving elements 1..N-1
		// never-written means they're never valid.
		uint32 arraySize = 1;
		std::map<uint32, uint32>::iterator arrIt = progIt->second.samplerArraySizes.find((uint32)handle);
		if (arrIt != progIt->second.samplerArraySizes.end())
			arraySize = arrIt->second;

		std::vector<VkDescriptorImageInfo> imageInfos(arraySize, imageInfo);

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = setIt->second;
		write.dstBinding = (uint32)handle;
		write.dstArrayElement = 0;
		write.descriptorCount = arraySize;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = imageInfos.data();
		vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
	}
	void VulkanRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) {}
	void VulkanRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) {}

	// GL's TranslateTextureFormat() fills three separate GL tokens
	// (internalFormat/format/type) because glTexImage2D needs them
	// separately (how the GPU stores it vs. how the *input* data is laid
	// out). Vulkan images have one storage VkFormat and no separate
	// input-format/type concept - this just packs a VkFormat into
	// `internalFormat` (read back by UploadTexture2D() below) and leaves
	// `format`/`type` unused (0). Covers the formats this backend's real
	// texture paths actually need: Texture::LoadTexture() (via stb_image
	// forced to 4 channels - see its `stbi_load_from_memory(..., 4)` call -
	// always produces TextureDataType::RGBA), every light's shadow map
	// (always TextureDataType::DepthComponent, see
	// DirectionalLight/PointLight/SpotLight.cpp's EnableCastShadows()),
	// and a few other straightforward same-byte-layout extras; anything
	// else (compressed formats) falls back to RGBA8 as a best-effort
	// default, untested since no example on this backend uses them.
	// UploadTexture2D()/CreateTextureObject() branch on whether the
	// returned VkFormat is one of the depth ones below (there's no
	// separate "this is a depth texture" flag threaded through this
	// call - the format itself is the signal, same as GL's own
	// glTexImage2D(..., GL_DEPTH_COMPONENT, ...) convention).
	void VulkanRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type)
	{
		format = 0;
		type = 0;
		switch (engineDataType)
		{
		case TextureDataType::RGBA: internalFormat = (uint32)VK_FORMAT_R8G8B8A8_UNORM; break;
		case TextureDataType::BGRA: internalFormat = (uint32)VK_FORMAT_B8G8R8A8_UNORM; break;
		case TextureDataType::R8: internalFormat = (uint32)VK_FORMAT_R8_UNORM; break;
		case TextureDataType::RG8: internalFormat = (uint32)VK_FORMAT_R8G8_UNORM; break;
		case TextureDataType::RGBA16F: internalFormat = (uint32)VK_FORMAT_R16G16B16A16_SFLOAT; break;
		case TextureDataType::RGBA32F: internalFormat = (uint32)VK_FORMAT_R32G32B32A32_SFLOAT; break;
		case TextureDataType::R16F: internalFormat = (uint32)VK_FORMAT_R16_SFLOAT; break;
		case TextureDataType::R32F: internalFormat = (uint32)VK_FORMAT_R32_SFLOAT; break;
		// A widely-supported 32-bit float depth format, no stencil -
		// every shadow FBO in this engine is depth-only (see FBORecord's
		// comment), so there's no need to pick one of the depth+stencil
		// variants some GPUs require instead of a depth-only one; a
		// depth-only format is core-spec-required to be supported for
		// this exact optimal-tiling+attachment+sampled usage combination
		// (VK_FORMAT_D32_SFLOAT specifically, per the Vulkan spec's
		// mandatory format support tables), so no runtime format-support
		// query/fallback is needed here.
		case TextureDataType::DepthComponent:
		case TextureDataType::DepthComponent16:
		case TextureDataType::DepthComponent24:
		case TextureDataType::DepthComponent32:
			internalFormat = (uint32)VK_FORMAT_D32_SFLOAT;
			break;
		default: internalFormat = (uint32)VK_FORMAT_R8G8B8A8_UNORM; break;
		}
	}

	// Plain 2D textures and cubemap faces (point-light shadow maps - see
	// PointLight.cpp's EnableCastShadows(), which calls
	// CreateEmptyTexture()/attaches with TextureType::CubemapPositive_X
	// + i for i in 0..5) are implemented; multisample targets are not
	// (no example on this backend uses one - post-effects/MSAA is Phase
	// 6 scope). mode/subMode come back as a small integer sentinel
	// distinct for each real target (1 for plain 2D,
	// CUBEMAP_FACE_TARGET_BASE+faceIndex for a cube face) rather than a
	// real Vulkan token, since callers (BindTextureToTarget()/
	// UploadTexture2D()/AttachFramebufferTexture2D()) only need to tell
	// targets apart, not translate to anything GL-shaped; 0 for anything
	// else (multisample), matching the "unimplemented" sentinel this
	// always returned.
	void VulkanRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode)
	{
		if (engineTextureType == TextureType::Texture)
			mode = subMode = 1;
		else if (engineTextureType <= TextureType::CubemapNegative_Z)
			mode = subMode = CUBEMAP_FACE_TARGET_BASE + engineTextureType;
		else
			mode = subMode = 0;
	}

	DeviceHandle VulkanRenderDevice::CreateTextureObject()
	{
		DeviceHandle handle = nextTextureHandle++;
		textures[handle] = TextureRecord();
		return handle;
	}

	void VulkanRenderDevice::DestroyTextureObject(const DeviceHandle texture)
	{
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(texture);
		if (it == textures.end())
			return;
		if (device != VK_NULL_HANDLE)
		{
			if (it->second.sampler != VK_NULL_HANDLE) vkDestroySampler(device, it->second.sampler, NULL);
			if (it->second.view != VK_NULL_HANDLE) vkDestroyImageView(device, it->second.view, NULL);
		}
		if (allocator != VK_NULL_HANDLE && it->second.image != VK_NULL_HANDLE)
			vmaDestroyImage(allocator, it->second.image, it->second.allocation);
		textures.erase(it);
		if (currentlyConfiguringTexture == texture)
			currentlyConfiguringTexture = 0;
	}

	// Dual-purpose, mirroring GL's own dual-purpose glBindTexture(): most
	// calls (from Texture.cpp's Upload/SetWrap*/SetFilter* configuration
	// sequences) just select which texture subsequent calls configure,
	// tracked via currentlyConfiguringTexture. But Texture::Bind()/
	// Unbind() call this immediately after ActivateTextureUnit() - a
	// pairing this backend needs to tell apart from configuration binds,
	// since it means something different: "this texture is what unit N
	// should read from at render time" (recorded into
	// textureUnitBindings, consumed later by SendUniformInt() - see its
	// comment for the rest of the mechanism), not "configure this
	// texture". unitJustActivated (set only by ActivateTextureUnit(),
	// consumed by the very next call here regardless of outcome) is what
	// distinguishes the two.
	void VulkanRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture)
	{
		(void)target;
		if (unitJustActivated)
		{
			unitJustActivated = false;
			if (texture != 0)
				textureUnitBindings[currentTextureUnit] = texture;
			else
				textureUnitBindings.erase(currentTextureUnit);
			return;
		}
		currentlyConfiguringTexture = texture;
	}

	// Uploads pixel data into currentlyConfiguringTexture (see
	// BindTextureToTarget()), creating the backing VkImage/VkImageView
	// the first time this runs for a given texture (or if width/height/
	// format changed - a real resize, matching Texture::Resize()'s GL
	// behavior of just re-calling this). `internalFormat` is the VkFormat
	// TranslateTextureFormat() packed above; `target`/`format`/`type` are
	// unused (see that method's comment). Levels beyond 0 (mipmaps
	// supplied pre-generated by the caller, e.g. LoadTexture()'s DDS
	// path) are accepted into the same image if it already exists at the
	// right size, but this backend's images are always created with a
	// single mip level (GenerateMipmap() is a no-op) - untested territory,
	// no example on this backend loads pre-mipmapped DDS content.
	static bool IsDepthFormatVk(const VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT;
	}

	void VulkanRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data)
	{
		(void)format; (void)type;
		if (currentlyConfiguringTexture == 0 || device == VK_NULL_HANDLE || allocator == VK_NULL_HANDLE || width == 0 || height == 0)
			return;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end())
			return;
		TextureRecord &tex = it->second;
		VkFormat wantedFormat = (VkFormat)internalFormat;
		bool isDepth = IsDepthFormatVk(wantedFormat);
		// A cubemap face target (TranslateTextureTarget()'s
		// CUBEMAP_FACE_TARGET_BASE+faceIndex encoding) - see
		// PointLight.cpp's EnableCastShadows(), which calls this six
		// times (once per face) on the *same* Texture/handle, matching
		// GL's own "one texture object, six glTexImage2D calls with
		// different face targets" model.
		bool isCubemapTarget = target >= CUBEMAP_FACE_TARGET_BASE;
		uint32 faceIndex = isCubemapTarget ? (target - CUBEMAP_FACE_TARGET_BASE) : 0;

		// (Re)create the image only on the *first* face's call for a
		// cubemap (isCubemap already true + image already exists means
		// this is face 1-5 reusing the same 6-layer image), or whenever
		// a plain 2D texture's size/format actually changed.
		bool needsCreate = tex.image == VK_NULL_HANDLE || tex.width != width || tex.height != height || tex.format != wantedFormat || (isCubemapTarget && !tex.isCubemap);
		if (needsCreate)
		{
			if (tex.view != VK_NULL_HANDLE) { vkDestroyImageView(device, tex.view, NULL); tex.view = VK_NULL_HANDLE; }
			for (std::map<uint32, VkImageView>::iterator rtIt = tex.renderTargetViewsByTarget.begin(); rtIt != tex.renderTargetViewsByTarget.end(); rtIt++)
				vkDestroyImageView(device, rtIt->second, NULL);
			tex.renderTargetViewsByTarget.clear();
			if (tex.image != VK_NULL_HANDLE) { vmaDestroyImage(allocator, tex.image, tex.allocation); tex.image = VK_NULL_HANDLE; }

			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.flags = isCubemapTarget ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = wantedFormat;
			imageInfo.extent = { width, height, 1 };
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = isCubemapTarget ? 6 : 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			// Depth formats are never uploaded via staging buffer (every
			// depth texture on this backend is a shadow map, rendered
			// into directly as a depth attachment - see
			// AttachFramebufferTexture2D()), so no TRANSFER_DST_BIT for
			// those; color formats keep it for LoadTexture()'s staging
			// upload below. TRANSFER_SRC_BIT on depth textures is purely
			// for DebugReadDepthTexture()'s diagnostic readback (same
			// reasoning as the swapchain gaining it for
			// RequestFrameCapture()) - additive, no cost to any real
			// shadow-map usage.
			imageInfo.usage = (isDepth ? (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) : VK_IMAGE_USAGE_TRANSFER_DST_BIT) | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &tex.image, &tex.allocation, NULL) != VK_SUCCESS)
			{
				fprintf(stderr, "VulkanRenderDevice::UploadTexture2D: vmaCreateImage failed (%ux%u, format=%d, cubemap=%d)\n", width, height, (int)wantedFormat, isCubemapTarget);
				tex.image = VK_NULL_HANDLE;
				return;
			}

			VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = tex.image;
			viewInfo.viewType = isCubemapTarget ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = wantedFormat;
			viewInfo.subresourceRange = { (VkImageAspectFlags)(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 1, 0, isCubemapTarget ? 6u : 1u };
			if (vkCreateImageView(device, &viewInfo, NULL, &tex.view) != VK_SUCCESS)
			{
				fprintf(stderr, "VulkanRenderDevice::UploadTexture2D: vkCreateImageView failed\n");
				vmaDestroyImage(allocator, tex.image, tex.allocation);
				tex.image = VK_NULL_HANDLE;
				return;
			}

			tex.width = width;
			tex.height = height;
			tex.format = wantedFormat;
			tex.isCubemap = isCubemapTarget;
			tex.isDepthTexture = isDepth;
		}

		if (data == NULL)
			return; // CreateEmptyTexture()'s case (every shadow map) - image exists, nothing to upload yet
		if (isDepth)
		{
			fprintf(stderr, "VulkanRenderDevice::UploadTexture2D: ignoring non-NULL data for a depth-format texture - depth textures on this backend are only ever populated by rendering into them (see AttachFramebufferTexture2D()), not uploaded\n");
			return;
		}

		// One-time-submit staging upload, since texture loading typically
		// happens outside any per-frame command buffer (asset loading
		// before the render loop even starts) - allocate a temporary
		// command buffer, record a barrier+copy+barrier, submit, wait,
		// and free it, all synchronously. Not the most efficient (no
		// overlap with anything else), but correct and simple - matches
		// this backend's existing "correctness over throughput" bar for
		// a first real implementation (see e.g. RequestFrameCapture()'s
		// similarly synchronous design).
		VkDeviceSize uploadSize = (VkDeviceSize)width * height * 4; // every supported format here is <=4 bytes/texel; see the comment on TranslateTextureFormat()
		VkBufferCreateInfo stagingInfo = {};
		stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingInfo.size = uploadSize;
		stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo stagingAllocInfo = {};
		stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAllocation = VK_NULL_HANDLE;
		VmaAllocationInfo stagingAllocationInfo;
		if (vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, &stagingAllocationInfo) != VK_SUCCESS)
		{
			fprintf(stderr, "VulkanRenderDevice::UploadTexture2D: staging vmaCreateBuffer failed\n");
			return;
		}
		memcpy(stagingAllocationInfo.pMappedData, data, (size_t)uploadSize);

		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = commandPool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;
		VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
		if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &uploadCmd) != VK_SUCCESS)
		{
			vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
			return;
		}

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(uploadCmd, &beginInfo);

		VkImageMemoryBarrier toDst = {};
		toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toDst.image = tex.image;
		// baseArrayLayer=faceIndex (0 for a plain 2D texture) - a
		// cubemap color upload (e.g. a future envmap/skybox path, not
		// exercised by anything on this backend yet - shadow maps are
		// depth-only and never reach this staging-upload code at all,
		// see the isDepth early-return above) must only transition/copy
		// into the one face layer being uploaded, not layer 0 always.
		toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, faceIndex, 1 };
		toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toDst);

		VkBufferImageCopy copyRegion = {};
		copyRegion.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, faceIndex, 1 };
		copyRegion.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(uploadCmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		VkImageMemoryBarrier toShaderRead = toDst;
		toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &toShaderRead);

		vkEndCommandBuffer(uploadCmd);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &uploadCmd;
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence uploadFence = VK_NULL_HANDLE;
		vkCreateFence(device, &fenceInfo, NULL, &uploadFence);
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadFence);
		if (uploadFence != VK_NULL_HANDLE)
		{
			vkWaitForFences(device, 1, &uploadFence, VK_TRUE, UINT64_MAX);
			vkDestroyFence(device, uploadFence, NULL);
		}
		else
		{
			vkQueueWaitIdle(graphicsQueue);
		}

		vkFreeCommandBuffers(device, commandPool, 1, &uploadCmd);
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
	}
	void VulkanRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) {}
	void VulkanRenderDevice::GenerateMipmap(const uint32 target) {}

	// All of these operate on currentlyConfiguringTexture (see
	// BindTextureToTarget()'s comment) - GL applies wrap/filter state
	// immediately via glTexParameter*, but Vulkan bakes it into an
	// immutable VkSampler, so these just accumulate state and mark
	// samplerDirty; RebuildSamplerIfDirty() (called from SendUniformInt(),
	// the point a texture is actually about to be used) does the real
	// work, once, not on every setter call.
	void VulkanRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.wrapS = engineRepeat;
		it->second.samplerDirty = true;
	}
	void VulkanRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.wrapT = engineRepeat;
		it->second.samplerDirty = true;
	}
	// Third-axis wrap (cubemap/3D-texture only) - not implemented, since
	// only plain 2D textures are (see TranslateTextureTarget()'s comment).
	void VulkanRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat) { (void)target; (void)engineRepeat; }
	void VulkanRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.magFilter = engineFilter;
		it->second.samplerDirty = true;
	}
	void VulkanRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.minFilter = engineFilter;
		// Real mipmap *generation* isn't implemented (GenerateMipmap() is
		// a no-op, images are always created with 1 mip level - see
		// UploadTexture2D()) - hasMipmap only affects the sampler's LOD
		// clamp range here, harmless when there's genuinely only one
		// level to sample from either way.
		it->second.hasMipmap = hasMipmap;
		it->second.samplerDirty = true;
	}
	// Mip range clamping - moot without real mipmap generation (see
	// UploadTexture2D()'s comment); not implemented.
	void VulkanRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) { (void)target; (void)baseLevel; (void)maxLevel; }
	// VkSamplerCreateInfo's border color is a fixed enum (transparent/
	// opaque black or white), not an arbitrary Vec4 the way GL's
	// GL_TEXTURE_BORDER_COLOR is - no example on this backend uses
	// ClampToBorder wrapping, so not implemented rather than guessed at.
	void VulkanRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 &color) { (void)target; (void)color; }
	// Shadow-sampler compare mode (sampler2DShadow/samplerCubeShadow) -
	// needed for shadow mapping, which has no framebuffer target on this
	// backend yet at all (Phase 6 scope, see VULKAN_ROADMAP.md) - not
	// implemented.
	// Enables hardware depth-compare sampling (sampler2DShadow/
	// samplerCubeShadow) on currentlyConfiguringTexture - see the
	// comment on RebuildSamplerIfDirty()'s samplerInfo.compareEnable.
	void VulkanRenderDevice::SetTextureCompareMode(const uint32 target)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.compareModeEnabled = true;
		it->second.samplerDirty = true;
	}
	// GL_UNPACK_ALIGNMENT has no Vulkan equivalent - vkCmdCopyBufferToImage's
	// staging buffer is always tightly packed (see UploadTexture2D()).
	void VulkanRenderDevice::SetPixelUnpackAlignment(const uint32 value) { (void)value; }

	// See the comment on BindTextureToTarget() for the render-time-bind
	// pairing this sets up.
	void VulkanRenderDevice::ActivateTextureUnit(const uint32 unit)
	{
		currentTextureUnit = unit;
		unitJustActivated = true;
	}

	void VulkanRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) {}
	uint32 VulkanRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) { return 0; }

	DeviceHandle VulkanRenderDevice::CreateFramebuffer()
	{
		DeviceHandle handle = nextFBOHandle++;
		fboRecords[handle] = FBORecord();
		return handle;
	}

	void VulkanRenderDevice::DestroyFramebuffer(const DeviceHandle fbo)
	{
		std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.find(fbo);
		if (it == fboRecords.end())
			return;
		if (device != VK_NULL_HANDLE)
		{
			for (std::map<uint32, VkFramebuffer>::iterator fIt = it->second.framebuffersByTarget.begin(); fIt != it->second.framebuffersByTarget.end(); fIt++)
				vkDestroyFramebuffer(device, fIt->second, NULL);
			if (it->second.renderPass != VK_NULL_HANDLE)
				vkDestroyRenderPass(device, it->second.renderPass, NULL);
		}
		fboRecords.erase(it);
		if (currentBoundFBO == fbo)
			currentBoundFBO = 0;
	}

	bool VulkanRenderDevice::BuildDepthOnlyRenderPass(FBORecord &fbo, const VkFormat depthFormat)
	{
		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// So the shadow map is immediately ready to sample once this
		// render pass ends - no separate transition needed before the
		// main pass reads it.
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference depthRef = {};
		depthRef.attachment = 0;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pDepthStencilAttachment = &depthRef;

		// Both directions: this pass must wait for any *previous* frame's
		// sampling of this same shadow map to finish before writing a
		// new one (dependencies[0]), and whoever samples it afterward -
		// the main pass, later in the same frame - must wait for this
		// write plus the layout transition to SHADER_READ_ONLY_OPTIMAL
		// to complete first (dependencies[1]).
		VkSubpassDependency dependencies[2] = {};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &depthAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies;

		if (vkCreateRenderPass(device, &renderPassInfo, NULL, &fbo.renderPass) != VK_SUCCESS)
		{
			fprintf(stderr, "VulkanRenderDevice::BuildDepthOnlyRenderPass: vkCreateRenderPass failed\n");
			return false;
		}
		return true;
	}

	VkImageView VulkanRenderDevice::GetOrCreateRenderTargetView(TextureRecord &tex, const uint32 nativeTextureTarget)
	{
		if (!tex.isCubemap)
			return tex.view; // plain 2D depth texture - render-target view == sampling view, no separate one needed

		std::map<uint32, VkImageView>::iterator it = tex.renderTargetViewsByTarget.find(nativeTextureTarget);
		if (it != tex.renderTargetViewsByTarget.end())
			return it->second;

		// A full VK_IMAGE_VIEW_TYPE_CUBE view (like tex.view) can't be
		// used as a depth-attachment render target - need a 2D view of
		// exactly the one array layer this face occupies instead.
		uint32 faceIndex = nativeTextureTarget - CUBEMAP_FACE_TARGET_BASE;
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = tex.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = tex.format;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, faceIndex, 1 };
		VkImageView view = VK_NULL_HANDLE;
		if (vkCreateImageView(device, &viewInfo, NULL, &view) != VK_SUCCESS)
		{
			fprintf(stderr, "VulkanRenderDevice::GetOrCreateRenderTargetView: vkCreateImageView failed\n");
			return VK_NULL_HANDLE;
		}
		tex.renderTargetViewsByTarget[nativeTextureTarget] = view;
		return view;
	}

	// FBOAccess::Read_Write/Read/Write - Vulkan has no read/write-only
	// framebuffer *binding* distinction the way GL's GL_READ_FRAMEBUFFER/
	// GL_DRAW_FRAMEBUFFER targets do, so this just passes the engine
	// value through unchanged; BindFramebuffer() below ignores it beyond
	// "is this a real bind (fbo!=0) or an unbind (fbo==0)".
	uint32 VulkanRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess) { return engineAccess; }

	// Real offscreen-pass session start/end - see the header comments on
	// activeCommandBuffer/offscreenCommandBuffer/offscreenPassOpen/
	// currentBoundFBO for the design. fbo!=0 (Bind()) just records which
	// FBO is now selected - AttachFramebufferTexture2D() does the actual
	// render-pass-begin work, since only it knows which texture/face to
	// target. fbo==0 (UnBind()) ends whatever's open and submits+waits
	// synchronously (matches UploadTexture2D()'s established "correctness
	// over throughput" immediate-submit precedent) - one submit per
	// Bind()/UnBind() session, not per attached face.
	void VulkanRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo)
	{
		(void)nativeAccess;
		if (fbo != 0)
		{
			currentBoundFBO = fbo;
			// If this FBO already has a built render pass + at least one
			// attached target (directional/spot lights: true on every
			// frame after the first - see FBORecord::lastTarget's
			// comment), re-begin rendering into the most recently
			// attached target right now, since no AttachFramebufferTexture2D()
			// call is coming this time to do it. A brand-new FBO with
			// nothing attached yet (point lights' first-ever face, or
			// this FBO's very first Init()) has no renderPass yet - fall
			// through and let the upcoming AttachFramebufferTexture2D()
			// call build+begin it instead, unchanged.
			std::map<DeviceHandle, FBORecord>::iterator fboIt = fboRecords.find(fbo);
			if (fboIt != fboRecords.end() && fboIt->second.renderPass != VK_NULL_HANDLE)
			{
				std::map<uint32, VkFramebuffer>::iterator fbIt = fboIt->second.framebuffersByTarget.find(fboIt->second.lastTarget);
				if (fbIt != fboIt->second.framebuffersByTarget.end())
					BeginOffscreenRenderPassForTarget(fboIt->second, fbIt->second);
			}
			return;
		}

		currentBoundFBO = 0;
		if (!offscreenCommandBufferRecording)
			return; // AttachFramebufferTexture2D() never actually ran (e.g. attachment failed) - nothing to submit

		EndOffscreenRenderPassIfOpen();
		vkEndCommandBuffer(offscreenCommandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &offscreenCommandBuffer;
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence offscreenFence = VK_NULL_HANDLE;
		vkCreateFence(device, &fenceInfo, NULL, &offscreenFence);
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, offscreenFence);
		if (offscreenFence != VK_NULL_HANDLE)
		{
			vkWaitForFences(device, 1, &offscreenFence, VK_TRUE, UINT64_MAX);
			vkDestroyFence(device, offscreenFence, NULL);
		}
		else
		{
			vkQueueWaitIdle(graphicsQueue);
		}

		activeCommandBuffer = VK_NULL_HANDLE;
		offscreenCommandBufferRecording = false;
		currentVao = 0;
		currentPipeline = 0;
	}

	// Passes the engine's FrameBufferAttachmentFormat::* value straight
	// through - AttachFramebufferTexture2D() only cares whether this is
	// Depth_Attachment (the only attachment type this backend implements
	// - see FBORecord's header comment for why Renderbuffer-backed/color
	// attachments aren't), not a translated native token.
	uint32 VulkanRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) { return engineAttachmentFormat; }

	void VulkanRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId)
	{
		if (currentBoundFBO == 0 || device == VK_NULL_HANDLE)
			return;
		if (nativeAttachmentFormat != FrameBufferAttachmentFormat::Depth_Attachment)
		{
			fprintf(stderr, "VulkanRenderDevice::AttachFramebufferTexture2D: only Depth_Attachment is implemented - attachment format %u ignored\n", nativeAttachmentFormat);
			return;
		}
		std::map<DeviceHandle, FBORecord>::iterator fboIt = fboRecords.find(currentBoundFBO);
		if (fboIt == fboRecords.end())
			return;
		std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(textureId);
		if (texIt == textures.end() || texIt->second.image == VK_NULL_HANDLE)
		{
			fprintf(stderr, "VulkanRenderDevice::AttachFramebufferTexture2D: texture handle %u has no image yet (CreateEmptyTexture() must run before FrameBuffer::Init()/AddAttach())\n", textureId);
			return;
		}

		// Re-attaching a new target within an already-open session (a
		// point light's 2nd-6th cubemap face) - end the previous face's
		// render pass first; Vulkan can't retarget a render pass's
		// attachment mid-pass.
		EndOffscreenRenderPassIfOpen();

		if (fboIt->second.renderPass == VK_NULL_HANDLE)
		{
			if (!BuildDepthOnlyRenderPass(fboIt->second, texIt->second.format))
				return;
			fboIt->second.width = texIt->second.width;
			fboIt->second.height = texIt->second.height;
		}

		VkImageView targetView = GetOrCreateRenderTargetView(texIt->second, nativeTextureTarget);
		if (targetView == VK_NULL_HANDLE)
			return;

		std::map<uint32, VkFramebuffer>::iterator fbIt = fboIt->second.framebuffersByTarget.find(nativeTextureTarget);
		VkFramebuffer targetFramebuffer;
		if (fbIt == fboIt->second.framebuffersByTarget.end())
		{
			VkFramebufferCreateInfo fbInfo = {};
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.renderPass = fboIt->second.renderPass;
			fbInfo.attachmentCount = 1;
			fbInfo.pAttachments = &targetView;
			fbInfo.width = fboIt->second.width;
			fbInfo.height = fboIt->second.height;
			fbInfo.layers = 1;
			if (vkCreateFramebuffer(device, &fbInfo, NULL, &targetFramebuffer) != VK_SUCCESS)
			{
				fprintf(stderr, "VulkanRenderDevice::AttachFramebufferTexture2D: vkCreateFramebuffer failed\n");
				return;
			}
			fboIt->second.framebuffersByTarget[nativeTextureTarget] = targetFramebuffer;
		}
		else
		{
			targetFramebuffer = fbIt->second;
		}

		fboIt->second.lastTarget = nativeTextureTarget;
		BeginOffscreenRenderPassForTarget(fboIt->second, targetFramebuffer);
	}

	void VulkanRenderDevice::BeginOffscreenRenderPassForTarget(FBORecord &fbo, const VkFramebuffer targetFramebuffer)
	{
		// Begin recording the session's command buffer, once - the
		// first time this FBO actually has something to render into
		// within this Bind()/UnBind() session.
		if (!offscreenCommandBufferRecording)
		{
			vkResetCommandBuffer(offscreenCommandBuffer, 0);
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			vkBeginCommandBuffer(offscreenCommandBuffer, &beginInfo);
			activeCommandBuffer = offscreenCommandBuffer;
			offscreenCommandBufferRecording = true;
			currentVao = 0;
			currentPipeline = 0;
		}

		VkClearValue clearValue;
		clearValue.depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderPassBegin = {};
		renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBegin.renderPass = fbo.renderPass;
		renderPassBegin.framebuffer = targetFramebuffer;
		renderPassBegin.renderArea.extent = { fbo.width, fbo.height };
		renderPassBegin.clearValueCount = 1;
		renderPassBegin.pClearValues = &clearValue;
		vkCmdBeginRenderPass(offscreenCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = { 0.0f, 0.0f, (f32)fbo.width, (f32)fbo.height, 0.0f, 1.0f };
		VkRect2D scissor = { { 0, 0 }, { fbo.width, fbo.height } };
		vkCmdSetViewport(offscreenCommandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(offscreenCommandBuffer, 0, 1, &scissor);

		offscreenPassOpen = true;
	}

	void VulkanRenderDevice::EndOffscreenRenderPassIfOpen()
	{
		if (!offscreenPassOpen)
			return;
		vkCmdEndRenderPass(offscreenCommandBuffer);
		offscreenPassOpen = false;
	}

	// GLLEGACY-only in every light's own shadow-FBO setup (confirmed
	// never defined in this project's CMake) - not implemented.
	void VulkanRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) { (void)nativeAttachmentFormat; (void)renderbuffer; }
	// GL_DRAW_BUFFER/GL_READ_BUFFER state has no Vulkan equivalent - a
	// render pass's attachments (and which subpass writes to which) are
	// fixed at creation time, not toggled per-draw. No-ops.
	void VulkanRenderDevice::SetDrawBufferNone() {}
	void VulkanRenderDevice::SetReadBufferNone() {}
	void VulkanRenderDevice::SetDrawBufferBack() {}
	void VulkanRenderDevice::SetReadBufferBack() {}
	void VulkanRenderDevice::SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) { (void)colorAttachmentIndices; }
	// Real Vulkan failures (vkCreateRenderPass/vkCreateFramebuffer/etc)
	// are already reported via fprintf at the exact call site that
	// failed (BuildDepthOnlyRenderPass()/AttachFramebufferTexture2D()) -
	// returning Complete here unconditionally avoids FrameBuffer::
	// CheckFBOStatus()'s echo()-based status print (which needs _DEBUG
	// to show anything anyway - see the glcheck_verification gotcha)
	// contradicting or duplicating that.
	uint32 VulkanRenderDevice::CheckFramebufferStatus() { return FBOStatus::Complete; }
	uint32 VulkanRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus) { return nativeStatus; }

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
