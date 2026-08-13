//============================================================================
// Name        : VulkanImGuiBackend.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VulkanRenderDevice::InitImGuiVulkanBackend()/
//               NewImGuiVulkanFrame()/ShutdownImGuiVulkanBackend()/
//               RebuildImGuiVulkanPipeline() - kept out of the already
//               very large VulkanRenderDevice.cpp, and in its own
//               translation unit so that file doesn't need any ImGui
//               awareness. See VulkanRenderDevice.h's comment on
//               InitImGuiVulkanBackend() for why imgui_impl_vulkan.cpp is
//               compiled into this library (see the root CMakeLists.txt)
//               rather than per-example: with IMGUI_IMPL_VULKAN_USE_VOLK
//               (defined build-wide when the Vulkan backend is on) it
//               shares this translation-unit-graph's already-
//               volkLoadDevice()'d function-pointer table - compiled into
//               a separate example binary instead, it would reference
//               that binary's own private, never-loaded copy of the same
//               globals and crash on the first Vulkan call.
//============================================================================

#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>

#ifdef VULKAN_BACKEND

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <algorithm>

namespace p3d {

	bool VulkanRenderDevice::InitImGuiVulkanBackend()
	{
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = instance;
		init_info.PhysicalDevice = physicalDevice;
		init_info.Device = device;
		init_info.QueueFamily = graphicsQueueFamily;
		init_info.Queue = graphicsQueue;
		// ImGui-owned pool, not this device's own `descriptorPool` - that
		// one was deliberately built without
		// VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT (see its
		// header comment), unsafe for ImGui's own alloc/free-on-font-
		// rebuild usage.
		init_info.DescriptorPoolSize = 64;
		init_info.MinImageCount = (uint32_t)std::max<size_t>(2, swapchainImages.size());
		init_info.ImageCount = (uint32_t)swapchainImages.size();
		init_info.PipelineInfoMain.RenderPass = renderPass;
		init_info.UseDynamicRendering = false; // traditional VkRenderPass, not VK_KHR_dynamic_rendering

		if (!ImGui_ImplVulkan_Init(&init_info))
			return false;

		UIRenderHook = [](VkCommandBuffer cmd) {
			// GetDrawData() returns NULL whenever ImGui::Render() hasn't
			// finalized a frame yet - true for every example still on the
			// old Begin+DrawUI+Render-after-RenderScene() RenderImGui()
			// pattern (this hook fires *inside* RenderScene(), before that
			// call), not just before the very first frame. Only
			// DemoLauncher calls PrepareImGuiFrame() early enough to avoid
			// this. Dereferencing the NULL unconditionally segfaulted in
			// ImGui_ImplVulkan_RenderDrawData() on frame 1 of every other
			// example.
			ImDrawData* drawData = ImGui::GetDrawData();
			if (drawData)
				ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
		};
		imguiVulkanBackendActive = true;

		return true;
	}

	void VulkanRenderDevice::NewImGuiVulkanFrame()
	{
		ImGui_ImplVulkan_NewFrame();

		// Must run *after* ImGui_ImplSDL2_NewFrame(): that sets DisplaySize
		// from SDL_GetWindowSize and FramebufferScale from
		// SDL_Vulkan_GetDrawableSize. Drawable size can lag the real Vulkan
		// surface extent (tiling WMs / MoltenVK - see SDL2VulkanContext's
		// QueryRealSurfaceExtent poll), while our swapchain always matches
		// capabilities.currentExtent. ImGui_ImplVulkan_RenderDrawData
		// scales clip rects by DisplaySize*FramebufferScale - if that
		// product disagrees with swapchainExtent, the whole UI (and the
		// scene Image inside it) stretches/clips into garbage after resize.
		ImGuiIO &io = ImGui::GetIO();
		if (swapchainExtent.width > 0 && swapchainExtent.height > 0 &&
			io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f)
		{
			io.DisplayFramebufferScale = ImVec2(
				(float)swapchainExtent.width / io.DisplaySize.x,
				(float)swapchainExtent.height / io.DisplaySize.y);
		}
	}

	void VulkanRenderDevice::ShutdownImGuiVulkanBackend()
	{
		if (!imguiVulkanBackendActive)
			return;

		// ImGui's Vulkan pipeline/descriptor pool are still-referenced GPU
		// resources at this point - must not tear them down while a
		// submission using them could still be in flight.
		WaitIdle();

		UIRenderHook = nullptr;
		imguiVulkanBackendActive = false;

		ImGui_ImplVulkan_Shutdown();
	}

	void VulkanRenderDevice::RebuildImGuiVulkanPipeline()
	{
		if (!imguiVulkanBackendActive)
			return;

		ImGui_ImplVulkan_SetMinImageCount((uint32_t)std::max<size_t>(2, swapchainImages.size()));

		ImGui_ImplVulkan_PipelineInfo pipelineInfo = {};
		pipelineInfo.RenderPass = renderPass;
		ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
	}

	// Debug UI only - see IRenderDevice::GetImGuiTextureID(). Implemented in
	// this file rather than VulkanRenderDevice.cpp because this is the
	// translation unit that has imgui_impl_vulkan.h; the device deliberately
	// keeps ImGui out of its main source (see InitImGuiVulkanBackend()).
	void VulkanRenderDevice::ReleaseImGuiTextureID(void *descriptorSet)
	{
		if (descriptorSet != NULL)
			ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)descriptorSet);
	}

	void *VulkanRenderDevice::GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType)
	{
		if (texture == 0 || device == VK_NULL_HANDLE || engineTextureType != TextureType::Texture)
			return NULL;

		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(texture);
		if (it == textures.end() || it->second.view == VK_NULL_HANDLE)
			return NULL;
		// ImGui samples with a plain 2D sampler, so a cube view would be
		// read as the wrong type entirely.
		if (it->second.isCubemap)
			return NULL;

		// The sampler is built lazily on first use; a render target that has
		// never been sampled by the scene may not have one yet.
		RebuildSamplerIfDirty(it->second);
		if (it->second.sampler == VK_NULL_HANDLE)
			return NULL;

		// Same hazard as SendUniformInt: LoadTexture during an ImGui frame
		// (Assets thumbnails) only records the upload. ImGui draws in
		// EndFrame before BeginFrame's transfer flush — sampling an
		// unsubmitted / UNDEFINED image crashes MoltenVK. Flush here.
		if (it->second.pendingUpload)
			FlushPendingTransfers();
		EnsureSampledLayout(it->second);

		ImGuiTextureBinding &binding = imguiTextureIDs[texture];
		if (binding.set != NULL)
		{
			if (binding.view == it->second.view && binding.sampler == it->second.sampler)
				return binding.set;
			// Rebuilt underneath us - drop the stale set rather than let a
			// draw reference a destroyed view or sampler.
			ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)binding.set);
			binding.set = NULL;
		}

		// SHADER_READ_ONLY_OPTIMAL is what the target is in by the time ImGui
		// draws: ImGui renders last, after every pass that wrote this target
		// has finished and left it readable.
		VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(it->second.sampler, it->second.view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (set == VK_NULL_HANDLE)
			return NULL;
		binding.set = (void*)set;
		binding.view = it->second.view;
		binding.sampler = it->second.sampler;
		return binding.set;
	}

}

#endif /* VULKAN_BACKEND */
