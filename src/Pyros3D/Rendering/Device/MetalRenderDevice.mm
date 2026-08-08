//============================================================================
// Name        : MetalRenderDevice.mm
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See MetalRenderDevice.h for scope. Real so far: device/
//               queue/layer/depth-buffer setup, the real BeginFrame()/
//               EndFrame() per-frame path (+ ClearAndPresent(), the
//               original bring-up milestone), the GLSL->SPIR-V->MSL shader
//               path, buffers/uniform-buffers, CreatePipeline()/
//               BindPipeline(), and DrawArrays()/DrawElements()/
//               DrawElementsInstanced(). Framebuffers/textures/samplers
//               (anything targeting an offscreen render target, not the
//               swapchain) are still stubs - logs once via LogStub() and
//               returns a safe default. Compiled with ARC (-fobjc-arc, set
//               in PyrosBackend.cmake for this file only) so id<MTLXxx>
//               locals are memory-managed normally; every Metal-typed
//               *member* is still a plain void* (see the header comment on
//               why) and crosses that boundary via CFBridgingRetain()/
//               CFBridgingRelease()/(__bridge Type) - retain on store,
//               release on destroy, plain non-owning cast to read.
//============================================================================

#include "Pyros3D/Rendering/Device/MetalRenderDevice.h"

#ifdef METAL_BACKEND

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <set>
#include <string>

#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#ifdef METAL_SHADER_TOOLING
#include <Pyros3D/Rendering/SPIRV/ShaderCompiler.h>
#include <spirv_cross/spirv_msl.hpp>
#endif

namespace {

	// Logs each not-yet-implemented method exactly once (not every call) -
	// most of these are on hot paths that would otherwise flood stderr the
	// moment anything real drives this device.
	void LogStub(const char* fn)
	{
		static std::set<std::string> warned;
		if (warned.insert(fn).second)
			fprintf(stderr, "MetalRenderDevice::%s: not implemented yet (framebuffers/textures/samplers - the swapchain draw path is real)\n", fn);
	}

	MTLVertexFormat TranslateVertexFormatMSL(p3d::uint32 engineType)
	{
		using namespace p3d;
		switch (engineType)
		{
			case Buffer::Attribute::Type::Float:  return MTLVertexFormatFloat;
			case Buffer::Attribute::Type::Vec2:   return MTLVertexFormatFloat2;
			case Buffer::Attribute::Type::Vec3:   return MTLVertexFormatFloat3;
			case Buffer::Attribute::Type::Vec4:   return MTLVertexFormatFloat4;
			case Buffer::Attribute::Type::Int:    return MTLVertexFormatInt;
			// Matrix (4 consecutive Vec4 attribute slots, one engine
			// "attribute" spanning several MSL vertex-descriptor
			// attributes) isn't reachable via this single-format
			// translation - no shipped shader's vertex input uses one
			// (RenderingInstancedComponent's per-instance transform is
			// its own separate concern, not modeled through
			// VertexAttributeDesc today). Fall through to Float3 rather
			// than silently mis-sizing a real attribute if that ever
			// changes - CreatePipeline() failing loudly beats rendering
			// garbage.
			default:
				fprintf(stderr, "MetalRenderDevice: TranslateVertexFormatMSL: unhandled engine attribute type %u, defaulting to Float3\n", engineType);
				return MTLVertexFormatFloat3;
		}
	}

	MTLCompareFunction TranslateDepthFuncMSL(p3d::uint32 mode)
	{
		using namespace p3d;
		switch (mode)
		{
			case DepthTest::Less:     return MTLCompareFunctionLess;
			case DepthTest::Never:    return MTLCompareFunctionNever;
			case DepthTest::Greater:  return MTLCompareFunctionGreater;
			case DepthTest::Equal:    return MTLCompareFunctionEqual;
			case DepthTest::Always:   return MTLCompareFunctionAlways;
			case DepthTest::LEqual:   return MTLCompareFunctionLessEqual;
			case DepthTest::GEqual:   return MTLCompareFunctionGreaterEqual;
			case DepthTest::NotEqual: return MTLCompareFunctionNotEqual;
			default:                  return MTLCompareFunctionLess;
		}
	}

	MTLBlendFactor TranslateBlendFactorMSL(p3d::uint32 factor)
	{
		using namespace p3d;
		switch (factor)
		{
			case BlendFunc::Zero:                     return MTLBlendFactorZero;
			case BlendFunc::One:                      return MTLBlendFactorOne;
			case BlendFunc::Src_Color:                return MTLBlendFactorSourceColor;
			case BlendFunc::One_Minus_Src_Color:      return MTLBlendFactorOneMinusSourceColor;
			case BlendFunc::Dst_Color:                return MTLBlendFactorDestinationColor;
			case BlendFunc::One_Minus_Dst_Color:      return MTLBlendFactorOneMinusDestinationColor;
			case BlendFunc::Src_Alpha:                return MTLBlendFactorSourceAlpha;
			case BlendFunc::One_Minus_Src_Alpha:      return MTLBlendFactorOneMinusSourceAlpha;
			case BlendFunc::Dst_Alpha:                return MTLBlendFactorDestinationAlpha;
			case BlendFunc::One_Minus_Dst_Alpha:      return MTLBlendFactorOneMinusDestinationAlpha;
			case BlendFunc::Constant_Color:           return MTLBlendFactorBlendColor;
			case BlendFunc::One_Minus_Constant_Color: return MTLBlendFactorOneMinusBlendColor;
			case BlendFunc::Constant_Alpha:           return MTLBlendFactorBlendAlpha;
			case BlendFunc::One_Minus_Constant_Alpha: return MTLBlendFactorOneMinusBlendAlpha;
			case BlendFunc::Src_Alpha_Saturate:       return MTLBlendFactorSourceAlphaSaturated;
			case BlendFunc::Src1_Color:                return MTLBlendFactorSource1Color;
			case BlendFunc::One_Minus_Src1_Color:      return MTLBlendFactorOneMinusSource1Color;
			case BlendFunc::Src1_Alpha:                return MTLBlendFactorSource1Alpha;
			case BlendFunc::One_Minus_Src1_Alpha:      return MTLBlendFactorOneMinusSource1Alpha;
			default:                                  return MTLBlendFactorOne;
		}
	}

	MTLBlendOperation TranslateBlendEquationMSL(p3d::uint32 eq)
	{
		using namespace p3d;
		switch (eq)
		{
			case BlendEq::Add:              return MTLBlendOperationAdd;
			case BlendEq::Subtract:         return MTLBlendOperationSubtract;
			case BlendEq::Reverse_Subtract: return MTLBlendOperationReverseSubtract;
			default:                        return MTLBlendOperationAdd;
		}
	}

	// Metal has no 3-component texture formats any more than Vulkan does -
	// TranslateTextureFormat() reuses this exact sentinel-in-the-`format`-
	// out-param convention from VulkanRenderDevice::VULKAN_UPLOAD_PAD_RGB_TO_RGBA
	// (same reasoning, same trick, different backend) so UploadTexture2D()
	// knows to expand a 3-byte-per-texel source buffer into 4 before
	// calling replaceRegion:.
	const p3d::uint32 kMetalUploadPadRgbToRgba = 1;

	// Real per-texel byte size of whatever TranslateTextureFormat() picked -
	// needed for both the RGB->RGBA padding above and computing
	// replaceRegion:'s bytesPerRow. Mirrors VulkanRenderDevice::BytesPerTexelVk()'s
	// identical table (found there via a real crash - see that function's
	// comment - so this is the already-debugged answer, not a guess).
	p3d::uint32 BytesPerTexelMSL(MTLPixelFormat format)
	{
		switch (format)
		{
			case MTLPixelFormatR8Unorm: return 1;
			case MTLPixelFormatRG8Unorm: return 2;
			case MTLPixelFormatR16Float: return 2;
			case MTLPixelFormatRGBA8Unorm:
			case MTLPixelFormatBGRA8Unorm:
			case MTLPixelFormatR32Float:
				return 4;
			case MTLPixelFormatRGBA16Float: return 8;
			case MTLPixelFormatRGBA32Float: return 16;
			default: return 4;
		}
	}

	p3d::uint32 ComputeMipLevelsMSL(p3d::uint32 width, p3d::uint32 height)
	{
		p3d::uint32 dim = width > height ? width : height;
		p3d::uint32 levels = 1;
		while (dim > 1) { dim >>= 1; levels++; }
		return levels;
	}

	void TranslateTextureFilterMSL(p3d::uint32 engineFilter, MTLSamplerMinMagFilter &outFilter, MTLSamplerMipFilter &outMipFilter)
	{
		using namespace p3d;
		switch (engineFilter)
		{
			case TextureFilter::Nearest:                outFilter = MTLSamplerMinMagFilterNearest; outMipFilter = MTLSamplerMipFilterNotMipmapped; break;
			case TextureFilter::Linear:                 outFilter = MTLSamplerMinMagFilterLinear;  outMipFilter = MTLSamplerMipFilterNotMipmapped; break;
			case TextureFilter::LinearMipmapLinear:     outFilter = MTLSamplerMinMagFilterLinear;  outMipFilter = MTLSamplerMipFilterLinear; break;
			case TextureFilter::LinearMipmapNearest:    outFilter = MTLSamplerMinMagFilterLinear;  outMipFilter = MTLSamplerMipFilterNearest; break;
			case TextureFilter::NearestMipmapNearest:   outFilter = MTLSamplerMinMagFilterNearest; outMipFilter = MTLSamplerMipFilterNearest; break;
			case TextureFilter::NearestMipmapLinear:    outFilter = MTLSamplerMinMagFilterNearest; outMipFilter = MTLSamplerMipFilterLinear; break;
			default:                                    outFilter = MTLSamplerMinMagFilterLinear;  outMipFilter = MTLSamplerMipFilterNotMipmapped; break;
		}
	}

	MTLSamplerAddressMode TranslateTextureRepeatMSL(p3d::uint32 engineRepeat)
	{
		using namespace p3d;
		switch (engineRepeat)
		{
			case TextureRepeat::Clamp:         return MTLSamplerAddressModeClampToEdge;
			case TextureRepeat::ClampToBorder: return MTLSamplerAddressModeClampToBorderColor;
			case TextureRepeat::ClampToEdge:   return MTLSamplerAddressModeClampToEdge;
			case TextureRepeat::Repeat:        return MTLSamplerAddressModeRepeat;
			default:                           return MTLSamplerAddressModeRepeat;
		}
	}

}

namespace p3d {

	MetalRenderDevice::MetalRenderDevice()
		: device(NULL), commandQueue(NULL), metalLayer(NULL), drawableWidth(0), drawableHeight(0),
		  depthTexture(NULL),
		  imguiMetalBackendActive(false), imguiDummyColorTexture(NULL),
		  frameBoundarySemaphore(NULL), currentCommandBuffer(NULL), currentRenderEncoder(NULL),
		  currentDrawable(NULL), lastSubmittedCommandBuffer(NULL),
		  nextVaoHandle(1), currentVao(0), currentPipeline(0),
		  nextShaderStageHandle(1), nextAutoUboBinding(kFirstAutoUboBinding), nextProgramHandle(1),
		  nextBufferHandle(1), nextTextureHandle(1),
		  currentTextureUnit(0), unitJustActivated(false), currentlyConfiguringTexture(0),
		  nextFBOHandle(1), currentBoundFBO(0), currentReadFBO(0),
		  nextPipelineHandle(1), pipelineArchive(NULL),
		  pendingClearColor(0.f, 0.f, 0.f, 1.f), frameInProgress(false)
	{
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
			if (mtlDevice == nil)
			{
				fprintf(stderr, "MetalRenderDevice: MTLCreateSystemDefaultDevice() returned nil - no supported GPU\n");
				return;
			}
			device = (void*)CFBridgingRetain(mtlDevice);

			id<MTLCommandQueue> queue = [mtlDevice newCommandQueue];
			if (queue == nil)
			{
				fprintf(stderr, "MetalRenderDevice: newCommandQueue failed\n");
				return;
			}
			commandQueue = (void*)CFBridgingRetain(queue);

			// Two in-flight frames, same as VulkanRenderDevice::MAX_FRAMES_IN_FLIGHT -
			// see the header comment on frameBoundarySemaphore for why this
			// single semaphore replaces that backend's whole fence array.
			dispatch_semaphore_t sem = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
			frameBoundarySemaphore = (void*)CFBridgingRetain(sem);
		}
	}

	MetalRenderDevice::~MetalRenderDevice()
	{
		// Same ordering requirement as VulkanRenderDevice's destructor
		// (see WaitIdle()'s comment there) - nothing below may run while
		// the GPU could still be reading a resource this destructor is
		// about to release.
		WaitIdle();

		@autoreleasepool
		{
			for (std::map<DeviceHandle, PipelineRecord>::iterator it = pipelines.begin(); it != pipelines.end(); ++it)
			{
				if (it->second.pipelineState != NULL) CFBridgingRelease(it->second.pipelineState);
				if (it->second.depthStencilState != NULL) CFBridgingRelease(it->second.depthStencilState);
			}
			pipelines.clear();

			for (std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.begin(); it != shaderStages.end(); ++it)
			{
				if (it->second.function != NULL) CFBridgingRelease(it->second.function);
			}
			shaderStages.clear();

			for (std::map<DeviceHandle, BufferRecord>::iterator it = buffers.begin(); it != buffers.end(); ++it)
			{
				if (it->second.buffer != NULL) CFBridgingRelease(it->second.buffer);
			}
			buffers.clear();

			if (depthTexture != NULL) { CFBridgingRelease(depthTexture); depthTexture = NULL; }
			if (currentRenderEncoder != NULL) { CFBridgingRelease(currentRenderEncoder); currentRenderEncoder = NULL; }
			if (currentCommandBuffer != NULL) { CFBridgingRelease(currentCommandBuffer); currentCommandBuffer = NULL; }
			if (currentDrawable != NULL) { CFBridgingRelease(currentDrawable); currentDrawable = NULL; }
			if (lastSubmittedCommandBuffer != NULL) { CFBridgingRelease(lastSubmittedCommandBuffer); lastSubmittedCommandBuffer = NULL; }
			if (frameBoundarySemaphore != NULL) { CFBridgingRelease(frameBoundarySemaphore); frameBoundarySemaphore = NULL; }
			if (commandQueue != NULL) { CFBridgingRelease(commandQueue); commandQueue = NULL; }
			if (device != NULL) { CFBridgingRelease(device); device = NULL; }
			// metalLayer is a borrowed pointer (SDL/its NSView owns the real
			// CAMetalLayer - see BindToLayer()'s comment) - never retained,
			// so never released here either.
		}
	}

	void MetalRenderDevice::RebuildDepthTexture()
	{
		if (device == NULL || drawableWidth == 0 || drawableHeight == 0)
			return;
		@autoreleasepool
		{
			if (depthTexture != NULL) { CFBridgingRelease(depthTexture); depthTexture = NULL; }
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
				width:drawableWidth height:drawableHeight mipmapped:NO];
			texDesc.usage = MTLTextureUsageRenderTarget;
			texDesc.storageMode = MTLStorageModePrivate;
			id<MTLTexture> tex = [mtlDevice newTextureWithDescriptor:texDesc];
			if (tex != nil)
				depthTexture = (void*)CFBridgingRetain(tex);
			else
				fprintf(stderr, "MetalRenderDevice::RebuildDepthTexture: newTextureWithDescriptor failed\n");
		}
	}

	bool MetalRenderDevice::BindToLayer(void* layerPtr, const uint32 width, const uint32 height)
	{
		if (device == NULL || commandQueue == NULL)
		{
			fprintf(stderr, "MetalRenderDevice::BindToLayer: device wasn't constructed successfully\n");
			return false;
		}
		if (layerPtr == NULL)
		{
			fprintf(stderr, "MetalRenderDevice::BindToLayer: null CAMetalLayer\n");
			return false;
		}

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)layerPtr;
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			layer.device = mtlDevice;
			layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
			// This engine only ever samples an offscreen render target back
			// as a texture (see IRenderDevice::CopyDepthTexture()'s comment
			// on the same distinction on the Vulkan side) - the swapchain/
			// drawable itself is never read back that way, so
			// framebufferOnly's perf benefit (lets the driver skip a
			// present-compatible memory layout) is free to take.
			layer.framebufferOnly = YES;
			layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
		}

		metalLayer = layerPtr;
		drawableWidth = width;
		drawableHeight = height;
		RebuildDepthTexture();
		return true;
	}

	void MetalRenderDevice::NotifySurfaceResized(const uint32 width, const uint32 height)
	{
		if (width == 0 || height == 0 || metalLayer == NULL)
			return;
		if (width == drawableWidth && height == drawableHeight)
			return;

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
		}
		drawableWidth = width;
		drawableHeight = height;
		RebuildDepthTexture();
	}

	void MetalRenderDevice::WaitIdle()
	{
		if (lastSubmittedCommandBuffer == NULL)
			return;
		@autoreleasepool
		{
			id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)lastSubmittedCommandBuffer;
			[cmdBuf waitUntilCompleted];
		}
	}

	bool MetalRenderDevice::ClearAndPresent(const Vec4 &clearColor)
	{
		if (device == NULL || commandQueue == NULL || metalLayer == NULL)
			return false;

		dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			if (drawable == nil)
			{
				dispatch_semaphore_signal(sem);
				return false;
			}

			id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
			id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];

			MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
			rpd.colorAttachments[0].texture = drawable.texture;
			rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
			rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
			rpd.colorAttachments[0].clearColor = MTLClearColorMake(clearColor.x, clearColor.y, clearColor.z, clearColor.w);

			id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpd];
			[encoder endEncoding];

			[cmdBuf presentDrawable:drawable];

			[cmdBuf addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull finishedBuffer) {
				(void)finishedBuffer;
				dispatch_semaphore_signal(sem);
			}];

			if (lastSubmittedCommandBuffer != NULL)
			{
				CFBridgingRelease(lastSubmittedCommandBuffer);
				lastSubmittedCommandBuffer = NULL;
			}
			lastSubmittedCommandBuffer = (void*)CFBridgingRetain(cmdBuf);

			[cmdBuf commit];
		}
		return true;
	}

	// =====================================================================
	// Real per-frame path
	// =====================================================================

	// Same reasoning as VulkanRenderDevice's identical one-liners: every
	// per-object BeginCommandBuffer()/EndCommandBuffer() call IRenderer
	// already issues becomes free once a real frame is open - there's one
	// real command buffer (currentCommandBuffer) for the whole frame, this
	// just returns a cheap, meaningful "is one currently open" handle.
	CommandBufferHandle MetalRenderDevice::BeginCommandBuffer() { return frameInProgress ? 1 : 0; }
	void MetalRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd) { (void)cmd; }

	void MetalRenderDevice::BeginFrame()
	{
		if (frameInProgress || device == NULL || commandQueue == NULL || metalLayer == NULL)
			return;

		// Frame throttling - see the header comment on
		// frameBoundarySemaphore.
		dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			if (drawable == nil)
			{
				dispatch_semaphore_signal(sem);
				return;
			}
			currentDrawable = (void*)CFBridgingRetain(drawable);

			id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
			id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
			currentCommandBuffer = (void*)CFBridgingRetain(cmdBuf);

			MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
			rpd.colorAttachments[0].texture = drawable.texture;
			rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
			rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
			rpd.colorAttachments[0].clearColor = MTLClearColorMake(pendingClearColor.x, pendingClearColor.y, pendingClearColor.z, pendingClearColor.w);

			if (depthTexture != NULL)
			{
				id<MTLTexture> depthTex = (__bridge id<MTLTexture>)depthTexture;
				rpd.depthAttachment.texture = depthTex;
				rpd.depthAttachment.loadAction = MTLLoadActionClear;
				rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
				rpd.depthAttachment.clearDepth = 1.0;
			}

			id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpd];
			currentRenderEncoder = (void*)CFBridgingRetain(encoder);

			// PyrosShader.glsl's geometry is authored CCW-front (OpenGL's
			// convention). TranslateProjectionMatrix() no longer negates Y
			// (see its own comment - that was compensating for a Vulkan-only
			// quirk that doesn't apply to Metal), so winding survives
			// untouched from authoring to here - Metal's own default
			// (MTLWindingClockwise) is the wrong one to leave in place.
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];

			// Full-target default viewport at frame start - same
			// convention as VulkanRenderDevice::BeginFrame()'s comment.
			MTLViewport viewport = { 0.0, 0.0, (double)drawableWidth, (double)drawableHeight, 0.0, 1.0 };
			[encoder setViewport:viewport];
		}

		currentVao = 0;
		currentPipeline = 0;
		frameInProgress = true;
	}

	void MetalRenderDevice::EndFrame()
	{
		if (!frameInProgress)
			return;

		@autoreleasepool
		{
			// Last chance to record additional draw commands (ImGui) into
			// the still-open swapchain render pass - see UIRenderHook's
			// header comment. Mirrors VulkanRenderDevice::EndFrame()'s
			// identical placement, right before ending the pass.
			if (UIRenderHook) UIRenderHook(currentCommandBuffer, currentRenderEncoder);

			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder endEncoding];
			CFBridgingRelease(currentRenderEncoder);
			currentRenderEncoder = NULL;

			id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
			id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)currentDrawable;
			[cmdBuf presentDrawable:drawable];

			dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
			[cmdBuf addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull finishedBuffer) {
				(void)finishedBuffer;
				dispatch_semaphore_signal(sem);
			}];

			if (lastSubmittedCommandBuffer != NULL)
			{
				CFBridgingRelease(lastSubmittedCommandBuffer);
				lastSubmittedCommandBuffer = NULL;
			}
			lastSubmittedCommandBuffer = (void*)CFBridgingRetain(cmdBuf);

			[cmdBuf commit];

			CFBridgingRelease(currentCommandBuffer);
			currentCommandBuffer = NULL;
			CFBridgingRelease(currentDrawable);
			currentDrawable = NULL;
		}

		frameInProgress = false;
	}

	uint32 MetalRenderDevice::TranslateBufferBit(const uint32 bufferBits) { (void)bufferBits; return 0; }
	// Real no-op, not a stub - see VulkanRenderDevice::Clear()'s identical
	// reasoning: the clear already happens via BeginFrame()'s render pass
	// load action (pendingClearColor, set by SetClearColor() below),
	// there's no separate "clear now" operation outside a render pass on
	// this backend either.
	void MetalRenderDevice::Clear(const uint32 nativeBufferBits) { (void)nativeBufferBits; }
	void MetalRenderDevice::SetClearColor(const Vec4 &color) { pendingClearColor = color; }

	// =====================================================================
	// State that's really pipeline state on Metal (see the header comment
	// on this group) - CreatePipeline() is what actually consumes it, via
	// the PipelineDesc IRenderer builds directly from the active
	// material's own settings, not by tracking these calls. Real, empty
	// no-ops here, not stubs - confirmed against a live CppApiDemo run
	// through IRenderer: it calls SetDepthTest()/SetDepthMask()/
	// PrepareDepthClear() (and the rest of this group) unconditionally,
	// every object, same as it always has for GL/Vulkan - VulkanRenderDevice's
	// identical empty bodies are the proof this is the correct, intentional
	// contract, not a gap: GL is the only backend where these calls are
	// live immediate-mode state rather than inert.
	// =====================================================================

	void MetalRenderDevice::SetDepthTest(const bool enabled, const uint32 mode) { (void)enabled; (void)mode; }
	void MetalRenderDevice::SetDepthMask(const bool enabled) { (void)enabled; }
	void MetalRenderDevice::PrepareDepthClear() {}

	void MetalRenderDevice::SetStencilTestEnabled(const bool enabled) { (void)enabled; }
	void MetalRenderDevice::SetClearStencilValue() {}
	void MetalRenderDevice::SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) { (void)func; (void)ref; (void)mask; }
	void MetalRenderDevice::SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) { (void)sfail; (void)dpfail; (void)dppass; }

	void MetalRenderDevice::SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height)
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			MTLScissorRect rect;
			rect.x = (NSUInteger)x;
			rect.y = (NSUInteger)y;
			rect.width = (NSUInteger)width;
			rect.height = (NSUInteger)height;
			[encoder setScissorRect:rect];
		}
	}
	void MetalRenderDevice::SetScissorTestEnabled(const bool enabled) { (void)enabled; }

	void MetalRenderDevice::SetWireFrame(const bool enabled)
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder setTriangleFillMode:enabled ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
		}
	}

	void MetalRenderDevice::SetColorMask(const bool r, const bool g, const bool b, const bool a) { (void)r; (void)g; (void)b; (void)a; }

	void MetalRenderDevice::SetPolygonOffsetEnabled(const bool enabled) { (void)enabled; }
	void MetalRenderDevice::SetPolygonOffset(const f32 factor, const f32 units)
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder setDepthBias:units slopeScale:factor clamp:0.0f];
		}
	}

	void MetalRenderDevice::SetBlendingEnabled(const bool enabled) { (void)enabled; }
	void MetalRenderDevice::SetBlendFunction(const uint32 sfactor, const uint32 dfactor) { (void)sfactor; (void)dfactor; }
	void MetalRenderDevice::SetBlendEquation(const uint32 mode) { (void)mode; }

	void MetalRenderDevice::SetCullFaceMode(const uint32 cullFace)
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			// IMaterial.h's CullFace::BackFace/FrontFace/DoubleSided - engine
			// values, not GL/Vulkan-native tokens (this method never went
			// through a Translate*() step on any backend).
			switch (cullFace)
			{
				case 0: [encoder setCullMode:MTLCullModeBack]; break;
				case 1: [encoder setCullMode:MTLCullModeFront]; break;
				default: [encoder setCullMode:MTLCullModeNone]; break;
			}
		}
	}
	void MetalRenderDevice::DisableCullFace()
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder setCullMode:MTLCullModeNone];
		}
	}

	// =====================================================================
	// Pipelines
	// =====================================================================

	DeviceHandle MetalRenderDevice::CreatePipeline(const PipelineDesc &desc)
	{
		if (device == NULL)
			return 0;
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(desc.shaderProgram);
		if (progIt == programs.end())
		{
			fprintf(stderr, "MetalRenderDevice::CreatePipeline: program handle %u not found\n", desc.shaderProgram);
			return 0;
		}
		std::map<DeviceHandle, ShaderStageRecord>::iterator vs = shaderStages.find(progIt->second.vertexShader);
		std::map<DeviceHandle, ShaderStageRecord>::iterator fs = shaderStages.find(progIt->second.fragmentShader);
		if (vs == shaderStages.end() || fs == shaderStages.end() || vs->second.function == NULL || fs->second.function == NULL)
		{
			fprintf(stderr, "MetalRenderDevice::CreatePipeline: program %u has no compiled vertex+fragment function pair\n", desc.shaderProgram);
			return 0;
		}
		if (desc.vertexLayout.empty() && !desc.noVertexInput)
		{
			fprintf(stderr, "MetalRenderDevice::CreatePipeline: empty vertexLayout and noVertexInput not set\n");
			return 0;
		}

		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLFunction> vertexFn = (__bridge id<MTLFunction>)vs->second.function;
			id<MTLFunction> fragmentFn = (__bridge id<MTLFunction>)fs->second.function;

			MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
			pipelineDesc.vertexFunction = vertexFn;
			pipelineDesc.fragmentFunction = fragmentFn;
			// desc.isShadowPass (see its header comment - same flag
			// VulkanRenderDevice's shadowPipelineRenderPass exists for)
			// means this pipeline only ever runs inside a depth-only
			// offscreen render pass (IRenderer::PreRender()'s shadow
			// maps), which has no color attachment at all - a pipeline
			// declaring colorAttachments[0].pixelFormat there is a
			// render-pass-compatibility mismatch, same shape as the
			// VUID-vkCmdDrawIndexed-renderPass-02684 bug that comment
			// describes, just caught differently (Metal doesn't reject
			// the draw the way Vulkan's validation layer does - it just
			// silently never writes real depth, leaving every shadow
			// map at its clear value and every receiving surface fully
			// "in shadow").
			//
			// A pipeline targeting a real *offscreen* FBO (DeferredRenderer's
			// G-buffer: 4 simultaneous color attachments, each a different
			// format - RGBA8/RGBA8/RGBA32F/RGBA8 per render_host.lua's
			// makeTarget() calls) needs each colorAttachments[slot]'s real
			// pixel format too, same render-pass-compatibility reasoning -
			// found the same way as the shadow bug: Metal doesn't reject
			// pipeline creation for this mismatch the way Vulkan's
			// validation layer would, it just silently never stores
			// whatever the fragment shader wrote to the slots this pipeline
			// left at MTLPixelFormatInvalid, leaving every DeferredRenderer
			// demo's screen black despite the G-buffer write, lighting
			// accumulation, and swapchain composite/present sequence all
			// otherwise completing without error (confirmed identical scene
			// renders correctly on Vulkan - this is Metal-specific).
			// currentBoundFBO is always the target this pipeline is about
			// to draw into: CreatePipeline() only ever runs from
			// IRenderer::BindMesh()/PostEffectsManager, both called with
			// the real target already bound, matching how a Vulkan
			// pipeline's VkRenderPass is always known at creation time too.
			if (desc.isShadowPass)
			{
				pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatInvalid;
			}
			else
			{
				std::map<DeviceHandle, FBORecord>::iterator fboIt = fboRecords.find(currentBoundFBO);
				if (currentBoundFBO != 0 && fboIt != fboRecords.end() && !fboIt->second.colorAttachments.empty())
				{
					for (std::map<uint32, FBOAttachmentRef>::iterator cIt = fboIt->second.colorAttachments.begin(); cIt != fboIt->second.colorAttachments.end(); cIt++)
					{
						std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(cIt->second.texture);
						if (texIt == textures.end() || texIt->second.texture == NULL)
							continue;
						id<MTLTexture> attachmentTex = (__bridge id<MTLTexture>)texIt->second.texture;
						pipelineDesc.colorAttachments[cIt->first].pixelFormat = attachmentTex.pixelFormat;
					}
				}
				else
				{
					pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
				}
			}
			pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

			if (desc.blendingEnabled)
			{
				pipelineDesc.colorAttachments[0].blendingEnabled = YES;
				pipelineDesc.colorAttachments[0].rgbBlendOperation = TranslateBlendEquationMSL(desc.blendEquation);
				pipelineDesc.colorAttachments[0].alphaBlendOperation = TranslateBlendEquationMSL(desc.blendEquation);
				pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = TranslateBlendFactorMSL(desc.blendSrcFactor);
				pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = TranslateBlendFactorMSL(desc.blendSrcFactor);
				pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = TranslateBlendFactorMSL(desc.blendDstFactor);
				pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = TranslateBlendFactorMSL(desc.blendDstFactor);
			}

			if (!desc.noVertexInput)
			{
				MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor vertexDescriptor];
				for (size_t bufferIdx = 0; bufferIdx < desc.vertexLayout.size(); bufferIdx++)
				{
					const IRenderDevice::VertexBufferLayoutDesc &layout = desc.vertexLayout[bufferIdx];
					for (size_t a = 0; a < layout.attributes.size(); a++)
					{
						const IRenderDevice::VertexAttributeDesc &attr = layout.attributes[a];
						std::map<std::string, uint32>::iterator locIt = progIt->second.attributeLocations.find(attr.name);
						if (locIt == progIt->second.attributeLocations.end())
							continue; // shader doesn't use this attribute - matches GL's -1-location no-op, not an error
						uint32 location = locIt->second;
						vertexDesc.attributes[location].format = TranslateVertexFormatMSL(attr.type);
						vertexDesc.attributes[location].offset = attr.offset;
						// See the header comment on kFirstVertexBufferIndex -
						// vertex attribute buffers live above the UBO
						// binding range, not at 0..N, to avoid colliding
						// with PyrosShader.glsl's own UBO_BINDING numbers
						// in MSL's single shared buffer-index namespace.
						vertexDesc.attributes[location].bufferIndex = (NSUInteger)(kFirstVertexBufferIndex + bufferIdx);
					}
					vertexDesc.layouts[kFirstVertexBufferIndex + bufferIdx].stride = layout.stride;
					vertexDesc.layouts[kFirstVertexBufferIndex + bufferIdx].stepFunction = MTLVertexStepFunctionPerVertex;
					// A buffer's attributes share one step rate on Metal,
					// unlike GL/Vulkan's per-attribute divisor - true for
					// every AttributeBuffer this engine builds today (one
					// buffer per divisor value - see
					// IRenderingInstancedComponent::AddBuffer()).
					if (!layout.attributes.empty() && layout.attributes[0].divisor > 0)
					{
						vertexDesc.layouts[kFirstVertexBufferIndex + bufferIdx].stepFunction = MTLVertexStepFunctionPerInstance;
						vertexDesc.layouts[kFirstVertexBufferIndex + bufferIdx].stepRate = layout.attributes[0].divisor;
					}
				}
				pipelineDesc.vertexDescriptor = vertexDesc;
			}

			NSError* nsError = nil;
			id<MTLRenderPipelineState> pipelineState = [mtlDevice newRenderPipelineStateWithDescriptor:pipelineDesc error:&nsError];
			if (pipelineState == nil)
			{
				fprintf(stderr, "MetalRenderDevice::CreatePipeline: %s\n", nsError ? [[nsError localizedDescription] UTF8String] : "newRenderPipelineStateWithDescriptor failed");
				return 0;
			}

			MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
			depthDesc.depthCompareFunction = desc.depthTest ? TranslateDepthFuncMSL(desc.depthTestMode) : MTLCompareFunctionAlways;
			depthDesc.depthWriteEnabled = desc.depthTest && desc.depthWrite;
			id<MTLDepthStencilState> depthState = [mtlDevice newDepthStencilStateWithDescriptor:depthDesc];

			PipelineRecord record;
			record.pipelineState = (void*)CFBridgingRetain(pipelineState);
			record.depthStencilState = (void*)CFBridgingRetain(depthState);
			record.programHandle = desc.shaderProgram;
			record.vertexBufferCount = (uint32)desc.vertexLayout.size();

			DeviceHandle handle = nextPipelineHandle++;
			pipelines[handle] = record;
			return handle;
		}
	}

	void MetalRenderDevice::DestroyPipeline(const DeviceHandle pipeline)
	{
		std::map<DeviceHandle, PipelineRecord>::iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
			return;
		if (it->second.pipelineState != NULL) CFBridgingRelease(it->second.pipelineState);
		if (it->second.depthStencilState != NULL) CFBridgingRelease(it->second.depthStencilState);
		pipelines.erase(it);
	}

	void MetalRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		(void)cmd;
		if (currentRenderEncoder == NULL)
			return;
		std::map<DeviceHandle, PipelineRecord>::iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
		{
			fprintf(stderr, "MetalRenderDevice::BindPipeline: pipeline handle %u not found (CreatePipeline likely failed earlier) - draw calls will be skipped this bind\n", pipeline);
			currentPipeline = 0;
			return;
		}
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)it->second.pipelineState];
			[encoder setDepthStencilState:(__bridge id<MTLDepthStencilState>)it->second.depthStencilState];
		}
		currentPipeline = pipeline;
	}

	void MetalRenderDevice::EnableClipDistance(const uint32 index) { (void)index; }
	void MetalRenderDevice::DisableClipDistance(const uint32 index) { (void)index; }

	void MetalRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height)
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			MTLViewport viewport = { (double)x, (double)y, (double)width, (double)height, 0.0, 1.0 };
			[encoder setViewport:viewport];
		}
	}

	// =====================================================================
	// Vertex state - the fake-VAO shim (see the header comment on
	// BindVertexArray()). SetVertexAttribute()/SetFloatVertexAttribute()/
	// DisableVertexAttribute()/SetVertexAttributeDivisor() are real no-ops,
	// not stubs - vertex layout is entirely CreatePipeline()'s
	// MTLVertexDescriptor job, same as Vulkan's identical no-op set (see
	// its own comment on these four).
	// =====================================================================

	void MetalRenderDevice::UseProgram(const uint32 program) { (void)program; }
	DeviceHandle MetalRenderDevice::CreateVertexArray()
	{
		DeviceHandle handle = nextVaoHandle++;
		vaos[handle] = VaoRecord();
		return handle;
	}
	void MetalRenderDevice::DeleteVertexArray(const DeviceHandle vao)
	{
		vaos.erase(vao);
		if (currentVao == vao)
			currentVao = 0;
	}
	void MetalRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao) { (void)cmd; currentVao = vao; }
	void MetalRenderDevice::BindArrayBuffer(const uint32 buffer)
	{
		if (currentVao == 0)
			return;
		std::map<DeviceHandle, VaoRecord>::iterator it = vaos.find(currentVao);
		if (it != vaos.end())
			it->second.vertexBuffers.push_back(buffer);
	}
	void MetalRenderDevice::BindElementBuffer(const uint32 buffer)
	{
		if (currentVao == 0)
			return;
		std::map<DeviceHandle, VaoRecord>::iterator it = vaos.find(currentVao);
		if (it != vaos.end())
			it->second.indexBuffer = buffer;
	}
	void MetalRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) { (void)location; (void)typeCount; (void)nativeType; (void)stride; (void)offset; }
	void MetalRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) { (void)location; (void)componentCount; (void)stride; (void)offset; }
	void MetalRenderDevice::DisableVertexAttribute(const int32 location) { (void)location; }
	void MetalRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor) { (void)location; (void)divisor; }

	// No descriptor-set-equivalent object to lazily build/write here (see
	// the header comment on uniformBufferByBindingPoint) - DrawArrays()/
	// DrawElements()/DrawElementsInstanced() bind whatever buffer
	// currently occupies `bindingPoint` fresh, every draw, for any binding
	// the bound program's own SPIR-V reflection says it actually declares.
	// Nothing left to do here except match GL's no-op-if-absent contract,
	// so a caller that always calls this defensively (same as the GL/
	// Vulkan paths) doesn't need a Metal-specific branch.
	void MetalRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint) { (void)program; (void)blockName; (void)bindingPoint; }

	// Metal's clip space is NOT identical to Vulkan's: both remap Z to
	// [0,1], but Metal's NDC Y axis points *up*, matching OpenGL - it's
	// only Vulkan whose NDC Y points down (a well-documented Vulkan-
	// specific quirk). The fixed-function NDC-to-viewport step already
	// handles converting NDC-Y-up into top-left-origin/Y-down framebuffer
	// coordinates on both GL and Metal identically - that's not something
	// a GL-authored projection matrix needs to compensate for. The
	// previous version of this function copied Vulkan's Y-negation
	// unconditionally, which took an already-correct GL-convention Y and
	// flipped it again - every ForwardRenderer scene rendered directly to
	// the swapchain came out upside down (confirmed via direct pixel
	// sampling: PhysicsStress's walled arena and falling spheres, plumb
	// wrong relative to each other). Only Z needs remapping here.
	Matrix MetalRenderDevice::TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip)
	{
		(void)skipYFlip;
		// Matrix's constructor takes arguments column-by-column, not
		// row-by-row (see VulkanRenderDevice::TranslateProjectionMatrix()'s
		// identical comment) - this is the column-major encoding of:
		//   [1  0  0   0 ]
		//   [0  1  0   0 ]
		//   [0  0  0.5 0.5]
		//   [0  0  0   1 ]
		static const Matrix clipCorrection(
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 0.5f, 0.f,
			0.f, 0.f, 0.5f, 1.f
		);
		return clipCorrection * projectionMatrix;
	}
	Matrix MetalRenderDevice::TranslateShadowBiasMatrix()
	{
		static const Matrix xyOnlyBias(
			0.5f, 0.f, 0.f, 0.f,
			0.f, 0.5f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.5f, 0.5f, 0.f, 1.f
		);
		return xyOnlyBias;
	}

	// Unused - Metal bakes primitive topology into the pipeline
	// (CreatePipeline() hardcodes MTLPrimitiveTypeTriangle in
	// DrawElements()/DrawElementsInstanced() directly), not the draw call,
	// same reasoning as VulkanRenderDevice's identical stub.
	uint32 MetalRenderDevice::TranslateDrawType(const uint32 engineDrawType) { (void)engineDrawType; return 0; }

	void MetalRenderDevice::BindProgramUniformBuffers(const DeviceHandle programHandle)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(programHandle);
		if (progIt == programs.end() || currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			for (std::map<uint32, uint32>::iterator bIt = progIt->second.bindingStageMask.begin(); bIt != progIt->second.bindingStageMask.end(); ++bIt)
			{
				const uint32 bindingPoint = bIt->first;
				const uint32 stageMask = bIt->second;
				std::map<uint32, DeviceHandle>::iterator bufHandleIt = uniformBufferByBindingPoint.find(bindingPoint);
				if (bufHandleIt == uniformBufferByBindingPoint.end())
					continue;
				std::map<DeviceHandle, BufferRecord>::iterator bufIt = buffers.find(bufHandleIt->second);
				if (bufIt == buffers.end() || bufIt->second.buffer == NULL)
					continue;
				id<MTLBuffer> buf = (__bridge id<MTLBuffer>)bufIt->second.buffer;
				// currentSlot's byte offset for a per-object dynamic UBO
				// (see the header comment on BufferRecord) - captured at
				// the moment *this draw* is recorded, same as Vulkan's
				// dynamic descriptor offset, so a later ReplaceUniformBuffer()
				// advancing to a new slot for the next object can't affect
				// a draw already recorded against this one.
				NSUInteger slotOffset = bufIt->second.isDynamicUniform ? (NSUInteger)bufIt->second.currentSlot * bufIt->second.alignedSlotSize : 0;
				// uniformBufferByBindingPoint (above) is keyed by the engine
				// binding point regardless of backend, but the *compiled
				// shader* may read this UBO from a different, compacted MSL
				// buffer index - see ProgramRecord::highBindingMslIndex's
				// comment (CompileShaderStage() remaps any binding >=
				// kFirstVertexBufferIndex, since Metal caps buffer indices
				// at 30).
				NSUInteger mslIndex = bindingPoint;
				std::map<uint32, uint32>::iterator remapIt = progIt->second.highBindingMslIndex.find(bindingPoint);
				if (remapIt != progIt->second.highBindingMslIndex.end())
					mslIndex = remapIt->second;
				if (stageMask & 1u)
					[encoder setVertexBuffer:buf offset:slotOffset atIndex:mslIndex];
				if (stageMask & 2u)
					[encoder setFragmentBuffer:buf offset:slotOffset atIndex:mslIndex];
			}
		}
	}

	void MetalRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count)
	{
		(void)nativeDrawType;
		if (currentRenderEncoder == NULL || currentPipeline == 0)
			return;
		std::map<DeviceHandle, PipelineRecord>::iterator pipeIt = pipelines.find(currentPipeline);
		if (pipeIt == pipelines.end())
			return;
		BindProgramUniformBuffers(pipeIt->second.programHandle);
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:first vertexCount:count];
		}
	}

	void MetalRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount)
	{
		(void)cmd; (void)nativeDrawType;
		if (currentRenderEncoder == NULL || currentVao == 0 || currentPipeline == 0)
			return;
		std::map<DeviceHandle, VaoRecord>::iterator vaoIt = vaos.find(currentVao);
		std::map<DeviceHandle, PipelineRecord>::iterator pipeIt = pipelines.find(currentPipeline);
		if (vaoIt == vaos.end() || pipeIt == pipelines.end() || vaoIt->second.vertexBuffers.empty())
			return;
		std::map<DeviceHandle, BufferRecord>::iterator iboIt = buffers.find(vaoIt->second.indexBuffer);
		if (iboIt == buffers.end() || iboIt->second.buffer == NULL)
			return;

		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;

			for (size_t i = 0; i < vaoIt->second.vertexBuffers.size(); i++)
			{
				std::map<DeviceHandle, BufferRecord>::iterator vboIt = buffers.find(vaoIt->second.vertexBuffers[i]);
				if (vboIt == buffers.end() || vboIt->second.buffer == NULL)
					return;
				id<MTLBuffer> vbo = (__bridge id<MTLBuffer>)vboIt->second.buffer;
				[encoder setVertexBuffer:vbo offset:0 atIndex:(NSUInteger)(kFirstVertexBufferIndex + i)];
			}

			BindProgramUniformBuffers(pipeIt->second.programHandle);

			// __INDEX_C_TYPE__ (Global.h) is uint32 - matches
			// MTLIndexTypeUInt32, same as VulkanRenderDevice's identical
			// comment on its own DrawElements().
			id<MTLBuffer> ibo = (__bridge id<MTLBuffer>)iboIt->second.buffer;
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
				indexCount:indexCount
				indexType:MTLIndexTypeUInt32
				indexBuffer:ibo
				indexBufferOffset:0];
		}
	}

	void MetalRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount)
	{
		(void)cmd; (void)nativeDrawType;
		if (currentRenderEncoder == NULL || currentVao == 0 || currentPipeline == 0)
			return;
		std::map<DeviceHandle, VaoRecord>::iterator vaoIt = vaos.find(currentVao);
		std::map<DeviceHandle, PipelineRecord>::iterator pipeIt = pipelines.find(currentPipeline);
		if (vaoIt == vaos.end() || pipeIt == pipelines.end() || vaoIt->second.vertexBuffers.empty())
			return;
		std::map<DeviceHandle, BufferRecord>::iterator iboIt = buffers.find(vaoIt->second.indexBuffer);
		if (iboIt == buffers.end() || iboIt->second.buffer == NULL)
			return;

		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;

			for (size_t i = 0; i < vaoIt->second.vertexBuffers.size(); i++)
			{
				std::map<DeviceHandle, BufferRecord>::iterator vboIt = buffers.find(vaoIt->second.vertexBuffers[i]);
				if (vboIt == buffers.end() || vboIt->second.buffer == NULL)
					return;
				id<MTLBuffer> vbo = (__bridge id<MTLBuffer>)vboIt->second.buffer;
				[encoder setVertexBuffer:vbo offset:0 atIndex:(NSUInteger)(kFirstVertexBufferIndex + i)];
			}

			BindProgramUniformBuffers(pipeIt->second.programHandle);

			id<MTLBuffer> ibo = (__bridge id<MTLBuffer>)iboIt->second.buffer;
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
				indexCount:indexCount
				indexType:MTLIndexTypeUInt32
				indexBuffer:ibo
				indexBufferOffset:0
				instanceCount:instanceCount];
		}
	}

	// =====================================================================
	// Uniform buffers / buffers - plain MTLBuffer throughout, shared
	// storage mode (unified memory - see the header comment on
	// CreateUniformBuffer() for why no VMA/staging-buffer equivalent is
	// needed). `.contents` is always CPU-writable for a shared-storage
	// buffer, so Update/Replace/Map* are all direct memcpy/pointer-return,
	// no separate map/unmap ceremony.
	// =====================================================================

	// Copied from VulkanRenderDevice::IsPerObjectDynamicBinding() verbatim
	// (see the header comment on why this isn't shared code) - these are
	// PyrosShader.glsl's own UBO_BINDING numbers, not anything Vulkan-
	// specific, so the same list applies here unchanged. Bindings this
	// backend doesn't reach yet (DeferredRenderer/water materials, 27-42)
	// are still included for exact parity - harmless if never actually
	// created, and one less thing to get wrong later when they are.
	bool MetalRenderDevice::IsPerObjectDynamicBinding(const uint32 bindingPoint)
	{
		switch (bindingPoint)
		{
		case 0:  // BIND_GlobalMatrices
		case 1:  // BIND_LightsBlock
		case 16: // BIND_VertexFrameUniforms
		case 18: // BIND_ObjectMatrixUniforms
		case 19: // BIND_BoneMatrices
		case 20: // BIND_VelocityObjectUniforms
		case 22: // BIND_MaterialUniforms
		case 23: // BIND_ObjectLightCounts
		case 27: case 32: case 33: case 34: case 37: case 38: case 39:
		case 40: case 41: case 42:
			return true;
		default:
			return bindingPoint >= kFirstAutoUboBinding;
		}
	}

	DeviceHandle MetalRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint)
	{
		if (device == NULL || sizeBytes == 0)
			return 0;

		bool dynamic = IsPerObjectDynamicBinding(bindingPoint);
		uint32 alignedSlotSize = sizeBytes;
		uint32 slotCount = 1;
		if (dynamic)
		{
			// See the header comment on BufferRecord - same sizing
			// formula as VulkanRenderDevice::CreateUniformBuffer() (256-byte
			// align, ~64MB budget per UBO, 4096..65536 slots). Metal has
			// no queryable "minimum uniform buffer offset alignment" the
			// way vkGetPhysicalDeviceProperties does; 256 is the common,
			// safely-conservative convention (comfortably above the
			// actual per-type alignment Metal structs need).
			const uint32 align = 256;
			alignedSlotSize = (sizeBytes + align - 1) / align * align;
			const uint64_t kMaxDynamicUboBytes = 64ull * 1024ull * 1024ull;
			uint64_t computedSlots = kMaxDynamicUboBytes / alignedSlotSize;
			slotCount = (uint32)(computedSlots < 4096ull ? 4096ull : computedSlots);
			if (slotCount > kMaxDynamicUboSlots)
				slotCount = kMaxDynamicUboSlots;
		}

		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLBuffer> buf = [mtlDevice newBufferWithLength:(NSUInteger)alignedSlotSize * slotCount options:MTLResourceStorageModeShared];
			if (buf == nil)
				return 0;
			memset(buf.contents, 0, (size_t)alignedSlotSize * slotCount);

			BufferRecord record;
			record.buffer = (void*)CFBridgingRetain(buf);
			record.length = sizeBytes;
			record.isDynamicUniform = dynamic;
			record.alignedSlotSize = alignedSlotSize;
			record.slotCount = slotCount;
			record.currentSlot = 0;
			DeviceHandle handle = nextBufferHandle++;
			buffers[handle] = record;
			uniformBufferByBindingPoint[bindingPoint] = handle;
			return handle;
		}
	}
	void MetalRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL || data == NULL)
			return;
		@autoreleasepool
		{
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
			// Writes into the *current* slot, same as
			// VulkanRenderDevice::UpdateUniformBuffer() - never advances it
			// (that's ReplaceUniformBuffer()'s job, see its comment), so a
			// caller that splits one logical write across multiple calls
			// (offset varying, slot fixed) lands them all in the same slot.
			size_t slotBase = it->second.isDynamicUniform ? (size_t)it->second.currentSlot * it->second.alignedSlotSize : 0;
			memcpy((char*)buf.contents + slotBase + offset, data, sizeBytes);
		}
	}
	void MetalRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data)
	{
		// Advance to the next slot *before* writing - see the header
		// comment on BufferRecord for why a single shared slot silently
		// corrupts any scene with more than one object sharing this
		// binding. Non-dynamic buffers (slotCount==1) wrap back to the
		// same slot 0 every time, identical to today's single-buffer
		// behavior.
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it != buffers.end() && it->second.isDynamicUniform)
			it->second.currentSlot = (it->second.currentSlot + 1) % it->second.slotCount;
		UpdateUniformBuffer(buffer, 0, sizeBytes, data);
	}
	void MetalRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer)
	{
		for (std::map<uint32, DeviceHandle>::iterator it = uniformBufferByBindingPoint.begin(); it != uniformBufferByBindingPoint.end(); )
		{
			if (it->second == buffer)
			{
				std::map<uint32, DeviceHandle>::iterator eraseIt = it;
				++it;
				uniformBufferByBindingPoint.erase(eraseIt);
			}
			else ++it;
		}
		DestroyBuffer(buffer);
	}

	DeviceHandle MetalRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		(void)bufferType; (void)bufferDraw;
		if (device == NULL)
			return 0;
		// A zero-length buffer is a real, legitimate state (e.g.
		// ParticlesExample's emitter starting at 0 particles) - same
		// reasoning/placeholder-length fix as VulkanRenderDevice's
		// identical comment on its own CreateBuffer().
		uint32 allocLength = (length == 0) ? 4 : length;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLBuffer> buf = (data != NULL)
				? [mtlDevice newBufferWithBytes:data length:allocLength options:MTLResourceStorageModeShared]
				: [mtlDevice newBufferWithLength:allocLength options:MTLResourceStorageModeShared];
			if (buf == nil)
				return 0;
			BufferRecord record;
			record.buffer = (void*)CFBridgingRetain(buf);
			record.length = length;
			DeviceHandle handle = nextBufferHandle++;
			buffers[handle] = record;
			return handle;
		}
	}
	void MetalRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		(void)bufferType; (void)bufferDraw;
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || device == NULL)
			return;
		@autoreleasepool
		{
			if (it->second.buffer != NULL) { CFBridgingRelease(it->second.buffer); it->second.buffer = NULL; }
			uint32 allocLength = (length == 0) ? 4 : length;
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLBuffer> buf = (data != NULL)
				? [mtlDevice newBufferWithBytes:data length:allocLength options:MTLResourceStorageModeShared]
				: [mtlDevice newBufferWithLength:allocLength options:MTLResourceStorageModeShared];
			if (buf != nil)
				it->second.buffer = (void*)CFBridgingRetain(buf);
			it->second.length = length;
		}
	}
	void MetalRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length)
	{
		(void)bufferType;
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL || data == NULL)
			return;
		@autoreleasepool
		{
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
			memcpy(buf.contents, data, length);
		}
	}
	void MetalRenderDevice::DestroyBuffer(const DeviceHandle buffer)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end())
			return;
		if (it->second.buffer != NULL)
			CFBridgingRelease(it->second.buffer);
		buffers.erase(it);
	}
	void *MetalRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType)
	{
		(void)bufferType; (void)mappingType;
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL)
			return NULL;
		id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
		return buf.contents;
	}
	void MetalRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) { (void)buffer; (void)bufferType; }

	// Unreachable - see SetVertexAttribute()'s comment; vertex attribute
	// component type is resolved once at CreatePipeline() time via
	// TranslateVertexFormatMSL() (this file's anonymous-namespace helper),
	// not through this seam.
	uint32 MetalRenderDevice::TranslateAttributeType(const uint32 engineType) { (void)engineType; return 0; }

	// =====================================================================
	// Shaders: GLSL -> SPIR-V (SpirvShaderCompiler/shaderc, shared with the
	// Vulkan backend) -> MSL source (SPIRV-Cross) -> MTLLibrary/MTLFunction.
	// =====================================================================

	// Mirrors VulkanRenderDevice::BuildShaderSource() exactly - the
	// GLSL->SPIR-V step is identical on both backends (shaderc's Vulkan
	// target environment predefines VULKAN, activating PyrosShader.glsl's
	// IO_LOCATION/UBO_BINDING/SAMPLER_BINDING macros - see that file's
	// header comment); only what happens to the *resulting SPIR-V*
	// differs (SPIRV-Cross MSL backend here, vkCreateShaderModule there).
	std::string MetalRenderDevice::BuildShaderSource(const std::string &definitions, const std::string &shaderBody)
	{
		return std::string("#version 450\n") + definitions + std::string(" ") + shaderBody;
	}

	DeviceHandle MetalRenderDevice::CreateShaderStage(const uint32 engineShaderType)
	{
		ShaderStageRecord record;
		record.engineShaderType = engineShaderType;
		DeviceHandle handle = nextShaderStageHandle++;
		shaderStages[handle] = record;
		return handle;
	}

	bool MetalRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog)
	{
		std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.find(shader);
		if (it == shaderStages.end() || device == NULL)
			return false;

#ifdef METAL_SHADER_TOOLING
		uint32 spirvStage = (it->second.engineShaderType == ShaderType::FragmentShader) ? SpirvShaderStage::Fragment : SpirvShaderStage::Vertex;

		// CustomShaderMaterial-authored shaders (particleSystem.glsl and
		// friends) use loose uniforms with no explicit layout(binding=) -
		// fine for GL, meaningless for a descriptor-free binding model
		// like Vulkan's or this one. AutoFixForVulkan() (SPIRV/ShaderCompiler.cpp)
		// is already backend-agnostic despite the name - it just rewrites
		// the loose uniforms into one explicit-binding UBO block - so this
		// mirrors VulkanRenderDevice::CompileShaderStage()'s identical
		// call verbatim instead of reimplementing it. The synthesized
		// binding (kFirstAutoUboBinding=43) lands well above
		// kFirstVertexBufferIndex, so the existing highBindingRemap loop
		// below compacts it into a real MSL buffer slot the same way it
		// already does for lastPass.glsl/secondpass*.glsl's hardcoded
		// high bindings - no separate remap path needed for this one.
		std::string compileSource = source;
		bool usedAutoFix = false;
		if (SpirvShaderCompiler::NeedsAutoFixForVulkan(source))
		{
			SpirvAutoUboResult autoUbo;
			std::string autoFixErr;
			uint32 candidateBinding = nextAutoUboBinding;
			std::string autoBlockName = std::string("AutoUBO_") + std::to_string(shader) + "_" + std::to_string(spirvStage);
			if (!SpirvShaderCompiler::AutoFixForVulkan(compileSource, spirvStage, candidateBinding, autoBlockName, autoUbo, autoFixErr))
			{
				errorLog = autoFixErr;
				return false;
			}
			usedAutoFix = true;
			if (autoUbo.hasBlock)
			{
				nextAutoUboBinding++;
				it->second.autoUboHasBlock = true;
				it->second.autoUboBinding = autoUbo.binding;
				it->second.autoUboBlockName = autoUbo.blockName;
				it->second.autoUboSize = autoUbo.size;
				it->second.autoUboOffsets = autoUbo.offsets;
			}
		}

		if (!SpirvShaderCompiler::Compile(compileSource, spirvStage, it->second.spirv, errorLog))
		{
			if (usedAutoFix)
				return false;
			SpirvAutoUboResult autoUbo;
			std::string autoFixErr;
			uint32 candidateBinding = nextAutoUboBinding;
			std::string autoBlockName = std::string("AutoUBO_") + std::to_string(shader) + "_" + std::to_string(spirvStage);
			compileSource = source;
			if (!SpirvShaderCompiler::AutoFixForVulkan(compileSource, spirvStage, candidateBinding, autoBlockName, autoUbo, autoFixErr))
			{
				errorLog = autoFixErr;
				return false;
			}
			if (autoUbo.hasBlock)
			{
				nextAutoUboBinding++;
				it->second.autoUboHasBlock = true;
				it->second.autoUboBinding = autoUbo.binding;
				it->second.autoUboBlockName = autoUbo.blockName;
				it->second.autoUboSize = autoUbo.size;
				it->second.autoUboOffsets = autoUbo.offsets;
			}
			errorLog.clear();
			if (!SpirvShaderCompiler::Compile(compileSource, spirvStage, it->second.spirv, errorLog))
				return false;
		}

		@autoreleasepool
		{
			std::string mslSource;
			try
			{
				spirv_cross::CompilerMSL mslCompiler(it->second.spirv);
				spirv_cross::CompilerMSL::Options mslOptions;
				mslOptions.set_msl_version(2, 1);
				mslCompiler.set_msl_options(mslOptions);

				// Without this, SPIRV-Cross assigns MSL buffer/texture/
				// sampler indices in its own declaration-order sequence
				// starting from 0 - it does NOT preserve the original
				// GLSL/SPIR-V `layout(binding=N)` value by default. This
				// engine's own binding-point convention (CreateUniformBuffer()'s
				// bindingPoint param, matched at draw time by
				// BindProgramUniformBuffers() using the *original* SPIR-V
				// binding via SpirvShaderCompiler::Reflect()) would then
				// silently disagree with whatever index the shader actually
				// reads from - every uniform read as garbage/zeroed memory,
				// with no compile or runtime error at all (found the hard
				// way: MetalHelloWindow's cube compiled and drew, with a
				// correct MVP written to the right *engine* binding, and
				// still rendered nothing - the vertex shader was reading
				// buffer index 0, not 8, because nothing told SPIRV-Cross
				// to keep them in sync). Force every uniform buffer's MSL
				// index to equal its original SPIR-V binding explicitly -
				// EXCEPT bindings >= kFirstVertexBufferIndex, which get a
				// compacted index instead (see the loop below): Metal only
				// allows [[buffer(0)]] through [[buffer(30)]], but several
				// deferred/post-effect shaders (lastPass.glsl,
				// secondpassSpot.glsl, PostEffects/Effects/*.cpp, ...) were
				// authored against Vulkan's much larger descriptor space and
				// hardcode UBO_BINDING values up to 39 - a direct passthrough
				// makes newLibraryWithSource: fail with "'buffer' attribute
				// parameter is out of bounds" (found running DemoLauncher's
				// deferred-by-default demos - every one of them hit this).
				// PyrosShader.glsl's own materials never use a binding that
				// high (max is 23), so this remap is a no-op for them.
				spirv_cross::ShaderResources resources = mslCompiler.get_shader_resources();
				uint32 nextCompactHighBinding = kFirstVertexBufferIndex;
				for (size_t i = 0; i < resources.uniform_buffers.size(); i++)
				{
					const spirv_cross::Resource &res = resources.uniform_buffers[i];
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
					if (binding.binding >= kFirstVertexBufferIndex)
					{
						uint32 compact = nextCompactHighBinding++;
						it->second.highBindingRemap[binding.binding] = compact;
						binding.msl_buffer = compact;
					}
					else
					{
						binding.msl_buffer = binding.binding;
					}
					mslCompiler.add_msl_resource_binding(binding);
				}
				// Same reasoning for sampled images (textures aren't
				// exercised by this milestone's shader, but the next
				// texture-sampling material this backend compiles would
				// hit the exact same silent-garbage-read bug without this).
				// Metal's texture/sampler argument tables are separate from
				// the buffer one used above, so none of these ever need the
				// same >=kFirstVertexBufferIndex remap - no post-effect
				// shader samples more than a handful of textures.
				for (size_t i = 0; i < resources.sampled_images.size(); i++)
				{
					const spirv_cross::Resource &res = resources.sampled_images[i];
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
					binding.msl_texture = binding.binding;
					binding.msl_sampler = binding.binding;
					mslCompiler.add_msl_resource_binding(binding);
				}

				mslSource = mslCompiler.compile();
			}
			catch (const std::exception &e)
			{
				errorLog = std::string("SPIRV-Cross MSL compile failed: ") + e.what();
				return false;
			}
			NSString* mslNSString = [NSString stringWithUTF8String:mslSource.c_str()];
			NSError* nsError = nil;
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLLibrary> library = [mtlDevice newLibraryWithSource:mslNSString options:nil error:&nsError];
			if (library == nil)
			{
				errorLog = nsError ? std::string([[nsError localizedDescription] UTF8String]) : "newLibraryWithSource failed";
				return false;
			}
			// SPIRV-Cross names every stage's entry point "main0" by
			// default (MSL doesn't allow a function literally named
			// "main").
			id<MTLFunction> function = [library newFunctionWithName:@"main0"];
			if (function == nil)
			{
				errorLog = "MSL library has no main0 entry point";
				return false;
			}
			it->second.function = (void*)CFBridgingRetain(function);
		}
		return true;
#else
		errorLog = "Metal backend built without METAL_SHADER_TOOLING (shaderc/spirv-cross-msl not found) - cannot compile GLSL to MSL.";
		return false;
#endif
	}

	DeviceHandle MetalRenderDevice::CreateProgram()
	{
		DeviceHandle handle = nextProgramHandle++;
		programs[handle] = ProgramRecord();
		return handle;
	}
	void MetalRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader)
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
	bool MetalRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return false;
		std::map<DeviceHandle, ShaderStageRecord>::iterator vs = shaderStages.find(it->second.vertexShader);
		std::map<DeviceHandle, ShaderStageRecord>::iterator fs = shaderStages.find(it->second.fragmentShader);
		bool vsOk = vs != shaderStages.end() && vs->second.function != NULL;
		bool fsOk = fs != shaderStages.end() && fs->second.function != NULL;
		if (!vsOk || !fsOk)
		{
			errorLog = "Program missing a compiled vertex and/or fragment stage";
			return false;
		}

#ifdef METAL_SHADER_TOOLING
		// No VkDescriptorSetLayout-equivalent to build (see
		// BindUniformBlockIfPresent()'s comment) - just reflect attribute
		// locations (CreatePipeline()'s MTLVertexDescriptor) and which
		// binding indices exist / which stage(s) use each (DrawElements()'s
		// BindProgramUniformBuffers()).
		it->second.attributeLocations.clear();
		std::vector<SpirvStageInput> vsInputs = SpirvShaderCompiler::ReflectStageInputs(vs->second.spirv);
		for (size_t i = 0; i < vsInputs.size(); i++)
			it->second.attributeLocations[vsInputs[i].name] = vsInputs[i].location;

		it->second.bindingStageMask.clear();
		it->second.samplerBindings.clear();
		it->second.samplerStageMask.clear();
		// Merge both stages' CompileShaderStage()-computed remaps (see the
		// header comment on ShaderStageRecord::highBindingRemap) - in
		// practice a given high engine binding only ever appears in one of
		// the two stages (post-effect UBOs like LastPassFragParams are
		// fragment-only), so there's nothing to reconcile between them.
		it->second.highBindingMslIndex.clear();
		it->second.highBindingMslIndex.insert(vs->second.highBindingRemap.begin(), vs->second.highBindingRemap.end());
		it->second.highBindingMslIndex.insert(fs->second.highBindingRemap.begin(), fs->second.highBindingRemap.end());
		std::vector<SpirvResourceBinding> vsResources = SpirvShaderCompiler::Reflect(vs->second.spirv);
		std::vector<SpirvResourceBinding> fsResources = SpirvShaderCompiler::Reflect(fs->second.spirv);
		for (int stagePass = 0; stagePass < 2; stagePass++)
		{
			std::vector<SpirvResourceBinding> &resources = (stagePass == 0) ? vsResources : fsResources;
			uint32 stageBit = (stagePass == 0) ? 1u : 2u;
			for (size_t i = 0; i < resources.size(); i++)
			{
				const SpirvResourceBinding &res = resources[i];
				if (res.type == SpirvResourceType::SampledImage)
				{
					// Separate MSL argument table from UBOs (setFragmentTexture:
					// vs setFragmentBuffer:) - see the header comment on
					// ProgramRecord::samplerBindings for why a texture and a
					// UBO safely sharing one numeric binding (PyrosShader.glsl
					// does this - e.g. BIND_uMetallicRoughnessmap and
					// BIND_VertexFrameUniforms are both 16) isn't a collision
					// here the way it would be in a single shared namespace.
					it->second.samplerBindings[res.name] = res.binding;
					it->second.samplerStageMask[res.binding] |= stageBit;
				}
				else
				{
					it->second.bindingStageMask[res.binding] |= stageBit;
				}
			}
		}
#endif
		return true;
	}
	bool MetalRenderDevice::IsProgram(const DeviceHandle handle) { return programs.find(handle) != programs.end(); }
	bool MetalRenderDevice::IsShaderStage(const DeviceHandle handle) { return shaderStages.find(handle) != shaderStages.end(); }
	void MetalRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return;
		if (it->second.vertexShader == shader) it->second.vertexShader = 0;
		if (it->second.fragmentShader == shader) it->second.fragmentShader = 0;
	}
	void MetalRenderDevice::DeleteShaderStage(const DeviceHandle shader)
	{
		std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.find(shader);
		if (it == shaderStages.end())
			return;
		if (it->second.function != NULL)
			CFBridgingRelease(it->second.function);
		shaderStages.erase(it);
	}
	void MetalRenderDevice::DeleteProgram(const DeviceHandle program) { programs.erase(program); }

	// Unlike GL, a Metal sampler has no real "uniform location" to query -
	// repurposed (same as VulkanRenderDevice's identical override) to mean
	// "this name's reflected sampler binding index", consumed by
	// SendUniformInt() below. Every non-sampler loose uniform this engine
	// ever sends is UBO-backed by this point (see CompileShaderStage()'s
	// AutoFixForVulkan rejection), so a sampler name is the only thing
	// this can meaningfully return.
	int32 MetalRenderDevice::GetUniformLocation(const uint32 program, const std::string &name)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return -1;
		std::map<std::string, uint32>::iterator samplerIt = it->second.samplerBindings.find(name);
		return samplerIt == it->second.samplerBindings.end() ? -1 : (int32)samplerIt->second;
	}
	int32 MetalRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return -1;
		std::map<std::string, uint32>::iterator locIt = it->second.attributeLocations.find(name);
		return locIt == it->second.attributeLocations.end() ? -1 : (int32)locIt->second;
	}
	// See ShaderStageRecord::autoUboHasBlock's comment - matches
	// VulkanRenderDevice::GetAutoUniformBlockLayout() verbatim: a lookup
	// into whichever of the program's two stages CompileShaderStage()
	// stashed this on.
	bool MetalRenderDevice::GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 &outBinding, std::string &outBlockName, uint32 &outSize, std::map<std::string, uint32> &outOffsets)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(program);
		if (progIt == programs.end())
			return false;
		DeviceHandle stageHandle = (engineShaderType == ShaderType::FragmentShader) ? progIt->second.fragmentShader : progIt->second.vertexShader;
		std::map<DeviceHandle, ShaderStageRecord>::iterator stageIt = shaderStages.find(stageHandle);
		if (stageIt == shaderStages.end() || !stageIt->second.autoUboHasBlock)
			return false;
		outBinding = stageIt->second.autoUboBinding;
		outBlockName = stageIt->second.autoUboBlockName;
		outSize = stageIt->second.autoUboSize;
		outOffsets = stageIt->second.autoUboOffsets;
		return true;
	}

	// The other half of GetUniformLocation()'s mechanism (see its comment):
	// `handle` is a sampler's reflected binding index, `data[0]` is the
	// texture *unit* Texture::Bind() just activated it at (see
	// textureUnitBindings, populated by ActivateTextureUnit()/
	// BindTextureToTarget()'s render-time pairing). Resolves unit -> real
	// texture and binds it directly onto the currently-open render
	// encoder - simpler than VulkanRenderDevice's equivalent (which writes
	// into a descriptor set cached across draws, since this call happens
	// before the draw that needs it): Metal's setFragmentTexture:/
	// setFragmentSamplerState: are themselves just per-encoder state that
	// persists until overwritten, exactly like setVertexBuffer:, so
	// binding immediately here needs no deferral to draw time at all.
	void MetalRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count)
	{
		if (handle < 0 || count == 0 || currentRenderEncoder == NULL || currentPipeline == 0)
			return;
		std::map<DeviceHandle, PipelineRecord>::iterator pipeIt = pipelines.find(currentPipeline);
		if (pipeIt == pipelines.end())
			return;
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(pipeIt->second.programHandle);
		if (progIt == programs.end())
			return;
		std::map<uint32, uint32>::iterator maskIt = progIt->second.samplerStageMask.find((uint32)handle);
		if (maskIt == progIt->second.samplerStageMask.end())
			return; // program doesn't declare a sampler at this binding - matches GL's no-op contract

		std::map<uint32, DeviceHandle>::iterator unitIt = textureUnitBindings.find((uint32)data[0]);
		if (unitIt == textureUnitBindings.end())
			return; // no Texture::Bind() at this unit (yet, or ever)
		std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(unitIt->second);
		if (texIt == textures.end() || texIt->second.texture == NULL)
			return;

		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			id<MTLTexture> tex = (__bridge id<MTLTexture>)texIt->second.texture;
			RebuildSamplerIfDirty(texIt->second);
			id<MTLSamplerState> sampler = (__bridge id<MTLSamplerState>)texIt->second.samplerState;
			const uint32 stageMask = maskIt->second;
			if (stageMask & 1u)
			{
				[encoder setVertexTexture:tex atIndex:(NSUInteger)handle];
				if (sampler != nil) [encoder setVertexSamplerState:sampler atIndex:(NSUInteger)handle];
			}
			if (stageMask & 2u)
			{
				[encoder setFragmentTexture:tex atIndex:(NSUInteger)handle];
				if (sampler != nil) [encoder setFragmentSamplerState:sampler atIndex:(NSUInteger)handle];
			}
		}
	}
	void MetalRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }

	// =====================================================================
	// Textures/samplers - plain 2D and cubemap faces (point/spot-light-style
	// shadow maps aside - those need real framebuffer support too, still a
	// stub below). Multisample targets are not implemented, matching
	// VulkanRenderDevice's identical scope note: no example needs one yet.
	// =====================================================================

	// format/type stay unused for everything except the RGB->RGBA padding
	// sentinel (see kMetalUploadPadRgbToRgba's comment) - same as
	// VulkanRenderDevice::TranslateTextureFormat(), and for the same
	// reason: this backend's upload path doesn't need GL's separate
	// format/type tokens once internalFormat alone says everything.
	void MetalRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type)
	{
		format = 0;
		type = 0;
		switch (engineDataType)
		{
			case TextureDataType::RGBA: internalFormat = (uint32)MTLPixelFormatRGBA8Unorm; break;
			case TextureDataType::BGRA: internalFormat = (uint32)MTLPixelFormatBGRA8Unorm; break;
			case TextureDataType::R8: internalFormat = (uint32)MTLPixelFormatR8Unorm; break;
			case TextureDataType::RG8: internalFormat = (uint32)MTLPixelFormatRG8Unorm; break;
			case TextureDataType::RGBA16F: internalFormat = (uint32)MTLPixelFormatRGBA16Float; break;
			case TextureDataType::RGBA32F: internalFormat = (uint32)MTLPixelFormatRGBA32Float; break;
			case TextureDataType::RGBA16I: internalFormat = (uint32)MTLPixelFormatRGBA16Sint; break;
			case TextureDataType::RGBA32I: internalFormat = (uint32)MTLPixelFormatRGBA32Sint; break;
			case TextureDataType::R16F: internalFormat = (uint32)MTLPixelFormatR16Float; break;
			case TextureDataType::R32F: internalFormat = (uint32)MTLPixelFormatR32Float; break;
			case TextureDataType::R16I: internalFormat = (uint32)MTLPixelFormatR16Sint; break;
			case TextureDataType::R32I: internalFormat = (uint32)MTLPixelFormatR32Sint; break;
			case TextureDataType::RG16F: internalFormat = (uint32)MTLPixelFormatRG16Float; break;
			case TextureDataType::RG32F: internalFormat = (uint32)MTLPixelFormatRG32Float; break;
			case TextureDataType::RG16I: internalFormat = (uint32)MTLPixelFormatRG16Sint; break;
			case TextureDataType::RG32I: internalFormat = (uint32)MTLPixelFormatRG32Sint; break;
			// No swizzle set up for these (replicate-into-RGB GL legacy
			// modes) any more than VulkanRenderDevice bothers to - same
			// "nothing on this backend uses either yet" reasoning.
			case TextureDataType::LUMINANCE: internalFormat = (uint32)MTLPixelFormatR8Unorm; break;
			case TextureDataType::LUMINANCE_ALPHA: internalFormat = (uint32)MTLPixelFormatRG8Unorm; break;
			// Metal has no 3-component texture formats either - same
			// upload-time-padding answer as Vulkan.
			case TextureDataType::RGB8: internalFormat = (uint32)MTLPixelFormatRGBA8Unorm; format = kMetalUploadPadRgbToRgba; break;
			case TextureDataType::BGR: internalFormat = (uint32)MTLPixelFormatBGRA8Unorm; format = kMetalUploadPadRgbToRgba; break;
			case TextureDataType::RGB16F: internalFormat = (uint32)MTLPixelFormatRGBA16Float; format = kMetalUploadPadRgbToRgba; break;
			case TextureDataType::RGB32F: internalFormat = (uint32)MTLPixelFormatRGBA32Float; format = kMetalUploadPadRgbToRgba; break;
			case TextureDataType::RGB16I: internalFormat = (uint32)MTLPixelFormatRGBA16Sint; format = kMetalUploadPadRgbToRgba; break;
			case TextureDataType::RGB32I: internalFormat = (uint32)MTLPixelFormatRGBA32Sint; format = kMetalUploadPadRgbToRgba; break;
			case TextureDataType::DepthComponent:
			case TextureDataType::DepthComponent16:
			case TextureDataType::DepthComponent24:
			case TextureDataType::DepthComponent32:
				internalFormat = (uint32)MTLPixelFormatDepth32Float;
				break;
			default: internalFormat = (uint32)MTLPixelFormatRGBA8Unorm; break;
		}
	}

	// Same "distinguish targets, not translate to a real API token"
	// sentinel scheme as VulkanRenderDevice::TranslateTextureTarget() (see
	// its comment) - 1 for plain 2D, kCubemapFaceTargetBase+faceIndex for
	// a cube face, 0 for anything else (multisample - not implemented).
	void MetalRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode)
	{
		if (engineTextureType == TextureType::Texture)
			mode = subMode = 1;
		else if (engineTextureType <= TextureType::CubemapNegative_Z)
			mode = subMode = kCubemapFaceTargetBase + engineTextureType;
		else
			mode = subMode = 0;
	}

	DeviceHandle MetalRenderDevice::CreateTextureObject()
	{
		DeviceHandle handle = nextTextureHandle++;
		textures[handle] = TextureRecord();
		return handle;
	}

	void MetalRenderDevice::DestroyTextureObject(const DeviceHandle texture)
	{
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(texture);
		if (it == textures.end())
			return;
		@autoreleasepool
		{
			if (it->second.samplerState != NULL) CFBridgingRelease(it->second.samplerState);
			if (it->second.texture != NULL) CFBridgingRelease(it->second.texture);
		}
		textures.erase(it);
		if (currentlyConfiguringTexture == texture)
			currentlyConfiguringTexture = 0;
		// Also drop it from whatever unit(s) it was bound to render with -
		// same reasoning as VulkanRenderDevice would need if a destroyed
		// texture were left resolvable via SendUniformInt()'s unit lookup
		// (it isn't there today, but this backend's own textureUnitBindings
		// has the identical stale-handle hazard without this).
		for (std::map<uint32, DeviceHandle>::iterator uIt = textureUnitBindings.begin(); uIt != textureUnitBindings.end(); )
		{
			if (uIt->second == texture)
			{
				std::map<uint32, DeviceHandle>::iterator dead = uIt;
				++uIt;
				textureUnitBindings.erase(dead);
			}
			else ++uIt;
		}
	}

	// Dual-purpose - see the header comment on textureUnitBindings/
	// currentlyConfiguringTexture for the full mechanism (copied from
	// VulkanRenderDevice::BindTextureToTarget() verbatim).
	void MetalRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture)
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

	void MetalRenderDevice::ActivateTextureUnit(const uint32 unit)
	{
		currentTextureUnit = unit;
		unitJustActivated = true;
	}

	void MetalRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap)
	{
		(void)type;
		if (currentlyConfiguringTexture == 0 || device == NULL || width == 0 || height == 0)
			return;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end())
			return;
		TextureRecord &tex = it->second;

		bool needsRgbPad = (format == kMetalUploadPadRgbToRgba);
		MTLPixelFormat wantedFormat = (MTLPixelFormat)internalFormat;
		bool isCubemapTarget = target >= kCubemapFaceTargetBase;
		uint32 faceIndex = isCubemapTarget ? (target - kCubemapFaceTargetBase) : 0;
		// Same "only reserve mip room if there's real data to build them
		// from right now" reasoning as VulkanRenderDevice::UploadTexture2D()'s
		// identical guard - see its comment.
		uint32 wantedMipLevels = (willMipmap && data != NULL) ? ComputeMipLevelsMSL(width, height) : 1;

		bool needsCreate = (tex.texture == NULL) || tex.width != width || tex.height != height
			|| tex.isCubemap != isCubemapTarget;

		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;

			if (needsCreate)
			{
				if (tex.texture != NULL) { CFBridgingRelease(tex.texture); tex.texture = NULL; }
				MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
				texDesc.textureType = isCubemapTarget ? MTLTextureTypeCube : MTLTextureType2D;
				texDesc.pixelFormat = wantedFormat;
				texDesc.width = width;
				texDesc.height = height;
				texDesc.mipmapLevelCount = wantedMipLevels;
				// MTLTextureUsageRenderTarget is required for a texture to
				// ever be attached as a color/depth target (see
				// BeginRenderEncoderForTarget()) - every shadow-casting
				// light's ShadowMap (Texture::CreateEmptyTexture() with no
				// initial data, routed through here same as an uploaded
				// texture) needs it despite never going through
				// UploadTexture2D() with real pixel data. Harmless to set on
				// every texture on Apple's unified memory architecture, so
				// no separate "is this a render target" signal is threaded
				// through from the caller.
				texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
				texDesc.storageMode = MTLStorageModeShared;
				id<MTLTexture> newTex = [mtlDevice newTextureWithDescriptor:texDesc];
				if (newTex == nil)
				{
					fprintf(stderr, "MetalRenderDevice::UploadTexture2D: newTextureWithDescriptor failed\n");
					return;
				}
				tex.texture = (void*)CFBridgingRetain(newTex);
				tex.width = width;
				tex.height = height;
				tex.isCubemap = isCubemapTarget;
				tex.hasMipmap = willMipmap;
				tex.mipsGenerated = false;
			}

			if (data != NULL)
			{
				id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)tex.texture;
				uint32 bytesPerTexel = BytesPerTexelMSL(wantedFormat);
				const void* uploadData = data;
				std::vector<uint8_t> padded;
				if (needsRgbPad)
				{
					// Expand 3 bytes/texel source into bytesPerTexel/texel
					// (4, or 8 for RGB16F->RGBA16F etc) - alpha (or the
					// trailing 1-2 bytes for the wide float formats) left
					// zeroed, matching VulkanRenderDevice's identical
					// padding (opaque textures never read their own alpha
					// channel back).
					uint32 srcBytesPerTexel = bytesPerTexel * 3 / 4;
					padded.resize((size_t)width * height * bytesPerTexel, 0);
					const uint8_t* src = (const uint8_t*)data;
					for (uint32 p = 0; p < width * height; p++)
						memcpy(&padded[(size_t)p * bytesPerTexel], &src[(size_t)p * srcBytesPerTexel], srcBytesPerTexel);
					uploadData = padded.data();
				}
				MTLRegion region = MTLRegionMake2D(0, 0, width, height);
				// bytesPerImage only matters for a 3D texture (0 = "not
				// applicable", per Apple's docs) - the slice-taking overload
				// still requires the argument even for a 2D/cube texture.
				[mtlTex replaceRegion:region mipmapLevel:level slice:faceIndex withBytes:uploadData bytesPerRow:(NSUInteger)width * bytesPerTexel bytesPerImage:0];
			}
		}
	}

	void MetalRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) { (void)target; (void)samples; (void)internalFormat; (void)width; (void)height; LogStub("UploadTexture2DMultisample"); }

	void MetalRenderDevice::GenerateMipmap(const uint32 target)
	{
		(void)target;
		if (currentlyConfiguringTexture == 0 || device == NULL || commandQueue == NULL)
			return;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end() || it->second.texture == NULL)
			return;
		@autoreleasepool
		{
			id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)it->second.texture;
			id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
			id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
			id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
			[blit generateMipmapsForTexture:mtlTex];
			[blit endEncoding];
			[cmdBuf commit];
			[cmdBuf waitUntilCompleted];
		}
		it->second.mipsGenerated = true;
	}

	// Lazily (re)builds a texture's MTLSamplerState - see the header
	// comment. Called from SendUniformInt() right before a texture is
	// actually bound for a draw, mirroring
	// VulkanRenderDevice::RebuildSamplerIfDirty()'s identical call site
	// reasoning (only worth rebuilding once wrap/filter state has
	// actually settled, not on every individual setter call).
	bool MetalRenderDevice::RebuildSamplerIfDirty(TextureRecord &tex)
	{
		if (!tex.samplerDirty)
			return tex.samplerState != NULL;
		if (device == NULL)
			return false;
		@autoreleasepool
		{
			if (tex.samplerState != NULL) { CFBridgingRelease(tex.samplerState); tex.samplerState = NULL; }

			MTLSamplerMinMagFilter minFilter, magFilter;
			MTLSamplerMipFilter mipFilter;
			TranslateTextureFilterMSL(tex.minFilter, minFilter, mipFilter);
			MTLSamplerMipFilter unusedMipFilter;
			TranslateTextureFilterMSL(tex.magFilter, magFilter, unusedMipFilter);

			MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
			samplerDesc.minFilter = minFilter;
			samplerDesc.magFilter = magFilter;
			samplerDesc.mipFilter = mipFilter;
			samplerDesc.sAddressMode = TranslateTextureRepeatMSL(tex.wrapS);
			samplerDesc.tAddressMode = TranslateTextureRepeatMSL(tex.wrapT);
			samplerDesc.rAddressMode = samplerDesc.tAddressMode;
			// See VulkanRenderDevice::RebuildSamplerIfDirty()'s identical
			// guard - a render-target whose mips were requested but never
			// actually populated must stay clamped to LOD 0, or automatic
			// LOD selection can sample undefined memory.
			samplerDesc.lodMinClamp = 0.0f;
			samplerDesc.lodMaxClamp = (tex.hasMipmap && tex.mipsGenerated) ? FLT_MAX : 0.0f;
			// LEQUAL matches GL's default GL_TEXTURE_COMPARE_FUNC, same as
			// VulkanRenderDevice's identical choice - PyrosShader.glsl's PCF
			// functions were written against that convention.
			samplerDesc.compareFunction = tex.compareModeEnabled ? MTLCompareFunctionLessEqual : MTLCompareFunctionNever;

			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLSamplerState> sampler = [mtlDevice newSamplerStateWithDescriptor:samplerDesc];
			if (sampler == nil)
				return false;
			tex.samplerState = (void*)CFBridgingRetain(sampler);
		}
		tex.samplerDirty = false;
		return true;
	}

	void MetalRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.wrapS = engineRepeat;
		it->second.samplerDirty = true;
	}
	void MetalRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.wrapT = engineRepeat;
		it->second.samplerDirty = true;
	}
	// Metal's sAddressMode/tAddressMode/rAddressMode are three independent
	// fields, but this engine only ever tracks S/T (see TextureRecord) -
	// RebuildSamplerIfDirty() reuses wrapT for R too, same as
	// VulkanRenderDevice's identical addressModeW = addressModeV choice
	// (no cubemap/3D-texture material on either backend sets R differently
	// from T today).
	void MetalRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat) { (void)target; (void)engineRepeat; }
	void MetalRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.magFilter = engineFilter;
		it->second.samplerDirty = true;
	}
	void MetalRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap)
	{
		(void)target; (void)hasMipmap;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.minFilter = engineFilter;
		it->second.samplerDirty = true;
	}
	// GL_TEXTURE_BASE_LEVEL/MAX_LEVEL has no Metal equivalent the way
	// lodMinClamp/lodMaxClamp (set from hasMipmap/mipsGenerated - see
	// RebuildSamplerIfDirty()) already covers the same "don't sample
	// unpopulated levels" need - same no-op as VulkanRenderDevice's
	// identical method.
	void MetalRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) { (void)target; (void)baseLevel; (void)maxLevel; }
	// Not implemented - MTLSamplerDescriptor's borderColor is one of three
	// fixed enum values (transparent/opaque black, opaque white), not an
	// arbitrary Vec4 the way GL_TEXTURE_BORDER_COLOR is; nothing on this
	// backend uses ClampToBorder with a non-default color yet (matches
	// VulkanRenderDevice, which also never wired VkSamplerCreateInfo's
	// equivalent up).
	void MetalRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 &color) { (void)target; (void)color; }
	void MetalRenderDevice::SetTextureCompareMode(const uint32 target)
	{
		(void)target;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		it->second.compareModeEnabled = true;
		it->second.samplerDirty = true;
	}
	// GL_UNPACK_ALIGNMENT has no Metal equivalent - replaceRegion:'s source
	// is always tightly packed (see UploadTexture2D()), same as
	// VulkanRenderDevice's identical no-op.
	void MetalRenderDevice::SetPixelUnpackAlignment(const uint32 value) { (void)value; }

	void MetalRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) { (void)target; (void)level; (void)format; (void)type; (void)outBuffer; LogStub("ReadTexturePixels"); }
	uint32 MetalRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) { (void)nativeInternalFormat; (void)width; (void)height; LogStub("GetTextureDataSize"); return 0; }

	// =====================================================================
	// Framebuffers - real now. See the header comment on FBORecord: no
	// VkRenderPass/VkFramebuffer-equivalent persistent object exists at
	// all, so AttachFramebufferTexture2D() just records attachments and
	// BeginRenderEncoderForTarget() builds a fresh MTLRenderPassDescriptor
	// from them on every bind - the whole "defer building until enough
	// attachments are known" problem VulkanRenderDevice's wasAlreadyBound/
	// finalizePending machinery exists to solve doesn't apply here.
	// =====================================================================

	DeviceHandle MetalRenderDevice::GetCurrentRenderTarget() { return currentBoundFBO; }

	DeviceHandle MetalRenderDevice::CreateFramebuffer()
	{
		DeviceHandle handle = nextFBOHandle++;
		fboRecords[handle] = FBORecord();
		return handle;
	}
	void MetalRenderDevice::DestroyFramebuffer(const DeviceHandle fbo)
	{
		fboRecords.erase(fbo);
		if (currentBoundFBO == fbo) currentBoundFBO = 0;
		if (currentReadFBO == fbo) currentReadFBO = 0;
	}
	void MetalRenderDevice::SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve)
	{
		std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.find(fbo);
		if (it != fboRecords.end())
			it->second.preserveDepth = preserve;
	}
	uint32 MetalRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess) { return engineAccess; }

	// A Read-only bind (BlitFramebuffer()'s source - still a stub, see
	// below) mirrors GL's GL_READ_FRAMEBUFFER target: never touches
	// currentBoundFBO or the render encoder, same as
	// VulkanRenderDevice::BindFramebuffer()'s identical early return.
	void MetalRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending)
	{
		// No deferred-build step to finalize - see this block's header
		// comment - so finalizePending has nothing to do here.
		(void)finalizePending;
		if (nativeAccess == FBOAccess::Read)
		{
			currentReadFBO = fbo;
			return;
		}

		if (fbo != 0)
		{
			currentBoundFBO = fbo;
			BeginRenderEncoderForTarget(fbo);
			return;
		}

		currentBoundFBO = 0;
		EndCurrentRenderEncoderIfOpen();
		if (frameInProgress)
		{
			// Resume the swapchain target - Metal encoders, unlike Vulkan
			// render passes, can't be "resumed" once ended, so this opens
			// a *new* encoder against the same drawable/depth texture with
			// Load actions (preserve whatever the frame's own encoder -
			// or an earlier offscreen detour - already wrote), instead of
			// the original BeginFrame() encoder that just got ended above.
			// currentDrawable/depthTexture are still valid: both are held
			// for the whole frame regardless of how many offscreen detours
			// happen in between (see EndFrame()'s release timing).
			if (currentDrawable != NULL && currentCommandBuffer != NULL)
			{
				@autoreleasepool
				{
					id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)currentDrawable;
					MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
					rpd.colorAttachments[0].texture = drawable.texture;
					rpd.colorAttachments[0].loadAction = MTLLoadActionLoad;
					rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
					if (depthTexture != NULL)
					{
						id<MTLTexture> depthTex = (__bridge id<MTLTexture>)depthTexture;
						rpd.depthAttachment.texture = depthTex;
						rpd.depthAttachment.loadAction = MTLLoadActionLoad;
						rpd.depthAttachment.storeAction = MTLStoreActionStore;
					}
					id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
					id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpd];
					currentRenderEncoder = (void*)CFBridgingRetain(encoder);
					[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
					MTLViewport viewport = { 0.0, 0.0, (double)drawableWidth, (double)drawableHeight, 0.0, 1.0 };
					[encoder setViewport:viewport];
				}
			}
		}
		else if (currentCommandBuffer != NULL)
		{
			// Pre-frame offscreen work (shadow maps, rendered from
			// IRenderer::PreRender() before RenderScene()/BeginFrame()
			// ever runs) - submit and wait right here, one submit per
			// Bind()/UnBind() session. Deliberately not the batched-
			// across-sessions approach VulkanRenderDevice eventually grew
			// for its swapchain path (see git history around that
			// backend's BindFramebuffer() fix) - that batching is a
			// deferred-rendering-specific optimization, and taking it
			// here first, unproven, is exactly how that backend acquired
			// a real cross-session-corruption bug. Submitting synchronously
			// per light keeps this correct by construction: by the time a
			// real frame's BeginFrame() runs, every shadow map it needs is
			// already fully rendered.
			@autoreleasepool
			{
				id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
				[cmdBuf commit];
				[cmdBuf waitUntilCompleted];
			}
			CFBridgingRelease(currentCommandBuffer);
			currentCommandBuffer = NULL;
		}
	}

	uint32 MetalRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) { return engineAttachmentFormat; }

	void MetalRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound)
	{
		// wasAlreadyBound is VulkanRenderDevice's signal for whether it's
		// safe to defer building a render pass - meaningless here (see
		// this block's header comment), so unlike that backend this
		// applies immediately either way; a re-attach mid-session (a
		// point light's 2nd-6th cubemap face) just overwrites the same
		// map slot, picked up the next time BeginRenderEncoderForTarget()
		// runs.
		(void)wasAlreadyBound;
		if (currentBoundFBO == 0)
			return;
		std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.find(currentBoundFBO);
		if (it == fboRecords.end())
			return;
		FBOAttachmentRef ref;
		ref.texture = textureId;
		ref.target = nativeTextureTarget;
		if (nativeAttachmentFormat == FrameBufferAttachmentFormat::Depth_Attachment)
			it->second.depthAttachment = ref;
		else if (nativeAttachmentFormat <= FrameBufferAttachmentFormat::Color_Attachment15)
			it->second.colorAttachments[nativeAttachmentFormat] = ref;
		else
			fprintf(stderr, "MetalRenderDevice::AttachFramebufferTexture2D: Stencil_Attachment is not implemented - attachment format %u ignored\n", nativeAttachmentFormat);
	}
	void MetalRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) { (void)nativeAttachmentFormat; (void)renderbuffer; LogStub("AttachFramebufferRenderbuffer"); }
	// No-ops, same as VulkanRenderDevice's identical set - GL's separate
	// draw/read-buffer selection has no Metal (or Vulkan) equivalent;
	// every color attachment a render pass declares is always written,
	// and there's no glReadPixels-shaped "current read buffer" concept.
	void MetalRenderDevice::SetDrawBufferNone() {}
	void MetalRenderDevice::SetReadBufferNone() {}
	void MetalRenderDevice::SetDrawBufferBack() {}
	void MetalRenderDevice::SetReadBufferBack() {}
	void MetalRenderDevice::SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) { (void)colorAttachmentIndices; }
	// Real Vulkan/Metal render passes are validated at creation time
	// (vkCreateRenderPass / newRenderPipelineStateWithDescriptor:error:),
	// not queried after the fact the way glCheckFramebufferStatus() is -
	// same optimistic "always complete" answer as VulkanRenderDevice's
	// identical override; a real failure surfaces as a loud fprintf at
	// the actual creation call instead.
	uint32 MetalRenderDevice::CheckFramebufferStatus() { return FBOStatus::Complete; }
	uint32 MetalRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus) { return nativeStatus; }

	DeviceHandle MetalRenderDevice::CreateRenderbuffer() { LogStub("CreateRenderbuffer"); return 0; }
	void MetalRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo) { (void)rbo; LogStub("DestroyRenderbuffer"); }
	void MetalRenderDevice::BindRenderbuffer(const DeviceHandle rbo) { (void)rbo; LogStub("BindRenderbuffer"); }
	uint32 MetalRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType) { (void)engineDataType; LogStub("TranslateRenderbufferFormat"); return 0; }
	void MetalRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) { (void)nativeFormat; (void)width; (void)height; LogStub("RenderbufferStorage"); }
	void MetalRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) { (void)nativeFormat; (void)samples; (void)width; (void)height; LogStub("RenderbufferStorageMultisample"); }

	void MetalRenderDevice::SetMultisampleEnabled(const bool enabled) { (void)enabled; LogStub("SetMultisampleEnabled"); }
	void MetalRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) { (void)srcX0; (void)srcY0; (void)srcX1; (void)srcY1; (void)dstX0; (void)dstY0; (void)dstX1; (void)dstY1; (void)engineMask; (void)engineFilter; LogStub("BlitFramebuffer"); }
	// See IRenderDevice.h's comment on CopyDepthTexture for why this
	// exists - DeferredRenderer needs forwardDepthTexture (lastPassFBO's
	// real depth attachment) populated with the G-buffer's just-finished
	// depth values so its translucent sub-pass depth-tests correctly.
	// Left as a no-op stub, forwardDepthTexture stayed at its cleared/
	// allocated value (effectively all-zero, i.e. "nearest possible"),
	// failing every translucent draw's depth test against real scene
	// geometry - SkyboxTest (a single huge DoubleSided cube, nothing else
	// in the scene) went fully black because its *only* object never
	// passed depth test. Encoded onto the frame's existing
	// currentCommandBuffer, not a new one - CopyDepthTexture() runs
	// between FBO->UnBind() (which already ended the G-buffer's render
	// encoder - see EndCurrentRenderEncoderIfOpen()) and lastPassFBO->Bind(),
	// so no render encoder is open here and this stays correctly ordered
	// with the rest of the frame without a separate submit/wait. That
	// holds when this DeferredRenderer pass *is* the main swapchain pass,
	// but not when it's nested inside PostEffectsManager's capture:
	// FrameBuffer::UnBind() (called just above by DeferredRenderer, right
	// before this) unconditionally rebinds whatever FBO was underneath on
	// its BoundFBOs stack (ExternalFBO), reopening a render encoder for
	// it - a blit encoder can't coexist with an open render encoder on
	// the same command buffer (AGXG15GFamilyCommandBuffer's "already
	// encoding" assertion, hit running SkyboxTest with its tonemap
	// effect). End it here unconditionally instead of assuming it's
	// already closed - matches VulkanRenderDevice::CopyDepthTexture()'s
	// own EndOffscreenRenderPassIfOpen() call for the identical reason
	// (a transfer can't run inside a render pass there either). Whatever
	// needs a render encoder next just opens a fresh one with Load
	// actions - see BindFramebuffer()'s comment on why that's safe.
	void MetalRenderDevice::CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height)
	{
		if (currentCommandBuffer == NULL)
			return;
		std::map<DeviceHandle, TextureRecord>::iterator srcIt = textures.find(srcTexture);
		std::map<DeviceHandle, TextureRecord>::iterator dstIt = textures.find(dstTexture);
		if (srcIt == textures.end() || dstIt == textures.end() || srcIt->second.texture == NULL || dstIt->second.texture == NULL)
			return;
		EndCurrentRenderEncoderIfOpen();
		@autoreleasepool
		{
			id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
			id<MTLTexture> srcTex = (__bridge id<MTLTexture>)srcIt->second.texture;
			id<MTLTexture> dstTex = (__bridge id<MTLTexture>)dstIt->second.texture;
			id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
			[blit copyFromTexture:srcTex sourceSlice:0 sourceLevel:0
				sourceOrigin:MTLOriginMake(0, 0, 0)
				sourceSize:MTLSizeMake(width, height, 1)
				toTexture:dstTex destinationSlice:0 destinationLevel:0
				destinationOrigin:MTLOriginMake(0, 0, 0)];
			[blit endEncoding];
		}
	}

	void MetalRenderDevice::EndCurrentRenderEncoderIfOpen()
	{
		if (currentRenderEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder endEncoding];
		}
		CFBridgingRelease(currentRenderEncoder);
		currentRenderEncoder = NULL;
	}

	// Builds a fresh MTLRenderPassDescriptor from fboRecords[fbo] and opens
	// a new encoder on it - see the header comment on FBORecord for why
	// there's no persistent render-pass object to reuse across binds the
	// way VulkanRenderDevice's FBORecord::renderPass/framebuffersByTarget
	// cache needs. Each FBOAttachmentRef.target is whatever
	// TranslateTextureTarget() produced (1 for a plain 2D texture,
	// kCubemapFaceTargetBase+faceIndex for one cube face - see that
	// function's comment) - a cube face resolves to Metal's `slice`,
	// which happens to need no reordering since TextureType::CubemapPositive_X
	// .. CubemapNegative_Z (0..5) is already Metal's own +X,-X,+Y,-Y,+Z,-Z
	// cube-slice order.
	void MetalRenderDevice::BeginRenderEncoderForTarget(const DeviceHandle fbo)
	{
		std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.find(fbo);
		if (it == fboRecords.end())
			return;
		FBORecord &record = it->second;

		EndCurrentRenderEncoderIfOpen();

		@autoreleasepool
		{
			if (currentCommandBuffer == NULL)
			{
				// Pre-frame case: a shadow map rendered from
				// IRenderer::PreRender(), strictly before BeginFrame() ever
				// runs, so there's no swapchain command buffer yet to reuse.
				// BindFramebuffer(0,...)'s !frameInProgress branch commits +
				// waits on this one once this FBO's Bind()/UnBind() session
				// ends.
				id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
				id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
				currentCommandBuffer = (void*)CFBridgingRetain(cmdBuf);
			}

			MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
			uint32 targetWidth = 0, targetHeight = 0;

			for (std::map<uint32, FBOAttachmentRef>::iterator cIt = record.colorAttachments.begin(); cIt != record.colorAttachments.end(); cIt++)
			{
				std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(cIt->second.texture);
				if (texIt == textures.end() || texIt->second.texture == NULL)
					continue;
				id<MTLTexture> tex = (__bridge id<MTLTexture>)texIt->second.texture;
				uint32 slot = cIt->first;
				rpd.colorAttachments[slot].texture = tex;
				rpd.colorAttachments[slot].loadAction = MTLLoadActionClear;
				rpd.colorAttachments[slot].storeAction = MTLStoreActionStore;
				rpd.colorAttachments[slot].clearColor = MTLClearColorMake(pendingClearColor.x, pendingClearColor.y, pendingClearColor.z, pendingClearColor.w);
				if (cIt->second.target >= kCubemapFaceTargetBase)
					rpd.colorAttachments[slot].slice = cIt->second.target - kCubemapFaceTargetBase;
				targetWidth = texIt->second.width;
				targetHeight = texIt->second.height;
			}

			if (record.depthAttachment.texture != 0)
			{
				std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(record.depthAttachment.texture);
				if (texIt != textures.end() && texIt->second.texture != NULL)
				{
					id<MTLTexture> depthTex = (__bridge id<MTLTexture>)texIt->second.texture;
					rpd.depthAttachment.texture = depthTex;
					// preserveDepth (SetFramebufferPreserveDepth()) means a
					// prior pass already wrote depth this session and it must
					// survive - e.g. VelocityRenderer sampling the same depth
					// buffer the main pass just wrote - so Load instead of
					// Clear, matching VulkanRenderDevice's identical use of
					// this flag.
					rpd.depthAttachment.loadAction = record.preserveDepth ? MTLLoadActionLoad : MTLLoadActionClear;
					rpd.depthAttachment.storeAction = MTLStoreActionStore;
					rpd.depthAttachment.clearDepth = 1.0;
					if (record.depthAttachment.target >= kCubemapFaceTargetBase)
						rpd.depthAttachment.slice = record.depthAttachment.target - kCubemapFaceTargetBase;
					if (targetWidth == 0) targetWidth = texIt->second.width;
					if (targetHeight == 0) targetHeight = texIt->second.height;
				}
			}

			id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
			id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpd];
			currentRenderEncoder = (void*)CFBridgingRetain(encoder);
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];

			if (targetWidth > 0 && targetHeight > 0)
			{
				MTLViewport viewport = { 0.0, 0.0, (double)targetWidth, (double)targetHeight, 0.0, 1.0 };
				[encoder setViewport:viewport];
			}
		}

		currentVao = 0;
		currentPipeline = 0;
	}

}

#endif /* METAL_BACKEND */
