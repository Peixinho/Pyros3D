//============================================================================
// Name        : MetalImGuiBackend.mm
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : MetalRenderDevice::InitImGuiMetalBackend()/NewImGuiMetalFrame()/
//               ShutdownImGuiMetalBackend() - kept out of MetalRenderDevice.mm,
//               in its own translation unit, so that file doesn't need any
//               ImGui awareness (same reasoning as VulkanImGuiBackend.cpp's
//               identical split). imgui_impl_metal.mm is compiled into this
//               library (see the root CMakeLists.txt/cmake/PyrosBackend.cmake)
//               so example code never links the vendored ImGui_ImplMetal_*
//               symbols itself. Deliberately does NOT touch ImGui_ImplSDL2_* -
//               that backend has no Metal dependency at all and stays
//               compiled per-example same as the GL/Vulkan paths already do.
//============================================================================

#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>

#ifdef METAL_BACKEND

#import <Metal/Metal.h>
#import "imgui.h"
#import "imgui_impl_metal.h"

namespace p3d {

	bool MetalRenderDevice::InitImGuiMetalBackend()
	{
		if (device == NULL)
			return false;

		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;

			if (!ImGui_ImplMetal_Init(mtlDevice))
				return false;

			// Must match MetalRenderDevice::swapchainPixelFormat /
			// CAMetalLayer.pixelFormat (BGRA8Unorm): ImGui_ImplMetal_
			// NewFrame() keys its pipeline cache off this dummy's format.
			MTLTextureDescriptor* dummyDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
				width:1 height:1 mipmapped:NO];
			dummyDesc.usage = MTLTextureUsageRenderTarget;
			dummyDesc.storageMode = MTLStorageModePrivate;
			id<MTLTexture> dummyTex = [mtlDevice newTextureWithDescriptor:dummyDesc];
			if (dummyTex == nil)
			{
				ImGui_ImplMetal_Shutdown();
				return false;
			}
			imguiDummyColorTexture = (void*)CFBridgingRetain(dummyTex);
		}

		UIRenderHook = [](void* commandBuffer, void* encoder)
		{
			// GetDrawData() returns NULL whenever ImGui::Render() hasn't
			// finalized a frame yet - same guard as
			// VulkanRenderDevice::InitImGuiVulkanBackend()'s identical
			// UIRenderHook (see its comment): true for every example still
			// on the old Begin+DrawUI+Render-after-RenderScene() pattern.
			ImDrawData* drawData = ImGui::GetDrawData();
			if (drawData == NULL || commandBuffer == NULL || encoder == NULL)
				return;
			@autoreleasepool
			{
				ImGui_ImplMetal_RenderDrawData(drawData,
					(__bridge id<MTLCommandBuffer>)commandBuffer,
					(__bridge id<MTLRenderCommandEncoder>)encoder);
			}
		};
		imguiMetalBackendActive = true;
		return true;
	}

	void MetalRenderDevice::NewImGuiMetalFrame()
	{
		if (!imguiMetalBackendActive || device == NULL)
			return;
		@autoreleasepool
		{
			id<MTLTexture> dummyTex = (__bridge id<MTLTexture>)imguiDummyColorTexture;
			MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
			rpd.colorAttachments[0].texture = dummyTex;
			// depthTexture is real and persistent for the whole session
			// (RebuildDepthTexture(), called by BindToLayer()/
			// NotifySurfaceResized()) - reusing it here, rather than a
			// second dummy, means the pipeline ImGui bakes from this
			// descriptor already matches EndFrame()'s real depth format,
			// exactly the render-pass-compatibility class of bug
			// CreatePipeline()'s isShadowPass handling exists to avoid
			// elsewhere in this backend.
			if (depthTexture != NULL)
				rpd.depthAttachment.texture = (__bridge id<MTLTexture>)depthTexture;
			ImGui_ImplMetal_NewFrame(rpd);
		}
	}

	void MetalRenderDevice::ShutdownImGuiMetalBackend()
	{
		if (!imguiMetalBackendActive)
			return;

		// ImGui's Metal pipeline/buffers are still-referenced GPU resources
		// at this point - same WaitIdle()-before-teardown requirement as
		// VulkanRenderDevice::ShutdownImGuiVulkanBackend()'s identical call.
		WaitIdle();

		UIRenderHook = nullptr;
		imguiMetalBackendActive = false;

		ImGui_ImplMetal_Shutdown();

		if (imguiDummyColorTexture != NULL)
		{
			CFBridgingRelease(imguiDummyColorTexture);
			imguiDummyColorTexture = NULL;
		}
	}

}

#endif /* METAL_BACKEND */
