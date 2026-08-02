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

}

#endif /* VULKAN_BACKEND */
