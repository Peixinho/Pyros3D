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
#include <set>
#include <string>

#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
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

}

namespace p3d {

	MetalRenderDevice::MetalRenderDevice()
		: device(NULL), commandQueue(NULL), metalLayer(NULL), drawableWidth(0), drawableHeight(0),
		  depthTexture(NULL),
		  frameBoundarySemaphore(NULL), currentCommandBuffer(NULL), currentRenderEncoder(NULL),
		  currentDrawable(NULL), lastSubmittedCommandBuffer(NULL),
		  nextVaoHandle(1), currentVao(0), currentPipeline(0),
		  nextShaderStageHandle(1), nextProgramHandle(1),
		  nextBufferHandle(1), nextTextureHandle(1), currentActiveTextureUnit(0),
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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
			// Both hardcoded to what BeginFrame()'s swapchain render pass
			// actually uses - see GetCurrentRenderTarget()'s header
			// comment: real per-target pixel formats (an offscreen FBO's
			// own texture format) are out of scope until framebuffers are
			// (BindFramebuffer() etc are still stubs).
			pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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
		if (!frameInProgress || currentRenderEncoder == NULL)
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

	// Identical NDC correction to VulkanRenderDevice's (Metal's clip space
	// matches Vulkan's exactly: Z in [0,1], Y+ down) - see
	// IRenderDevice::TranslateProjectionMatrix()'s comment for the full
	// derivation. Copied rather than shared: VulkanRenderDevice's version
	// is a private member function, and duplicating four constants is
	// cheaper than introducing a shared base for exactly one method pair.
	Matrix MetalRenderDevice::TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip)
	{
		// Matrix's constructor takes arguments column-by-column, not
		// row-by-row (see VulkanRenderDevice::TranslateProjectionMatrix()'s
		// identical comment) - this is the column-major encoding of:
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
		static const Matrix clipCorrectionNoYFlip(
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 0.5f, 0.f,
			0.f, 0.f, 0.5f, 1.f
		);
		return (skipYFlip ? clipCorrectionNoYFlip : clipCorrection) * projectionMatrix;
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
				if (stageMask & 1u)
					[encoder setVertexBuffer:buf offset:0 atIndex:(NSUInteger)bindingPoint];
				if (stageMask & 2u)
					[encoder setFragmentBuffer:buf offset:0 atIndex:(NSUInteger)bindingPoint];
			}
		}
	}

	void MetalRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count)
	{
		(void)nativeDrawType;
		if (!frameInProgress || currentRenderEncoder == NULL || currentPipeline == 0)
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
		if (!frameInProgress || currentRenderEncoder == NULL || currentVao == 0 || currentPipeline == 0)
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
		if (!frameInProgress || currentRenderEncoder == NULL || currentVao == 0 || currentPipeline == 0)
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

	DeviceHandle MetalRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint)
	{
		DeviceHandle handle = CreateBuffer(Buffer::Type::Attribute, Buffer::Draw::Dynamic, NULL, sizeBytes);
		if (handle == 0)
			return 0;
		buffers[handle].isDynamicUniform = true;
		uniformBufferByBindingPoint[bindingPoint] = handle;
		return handle;
	}
	void MetalRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL || data == NULL)
			return;
		@autoreleasepool
		{
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
			memcpy((char*)buf.contents + offset, data, sizeBytes);
		}
	}
	void MetalRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data)
	{
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

		if (SpirvShaderCompiler::NeedsAutoFixForVulkan(source))
		{
			// No AutoFix-equivalent wired up on this backend yet (see
			// BindUniformBlockIfPresent()'s comment on why the descriptor-
			// free binding model still needs *some* explicit binding
			// number per resource, same as Vulkan) - every shader this
			// milestone compiles is hand-authored with explicit
			// layout(location=)/layout(binding=) qualifiers already, so
			// this should never actually trigger. Fail loudly instead of
			// silently producing a shader with unreflectable bindings if
			// it ever does.
			errorLog = "MetalRenderDevice::CompileShaderStage: shader needs AutoFixForVulkan (loose uniforms/no explicit layout) - not implemented on this backend yet; author with explicit layout(location=)/layout(binding=) qualifiers";
			return false;
		}

		if (!SpirvShaderCompiler::Compile(source, spirvStage, it->second.spirv, errorLog))
			return false;

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
				// index to equal its original SPIR-V binding explicitly.
				spirv_cross::ShaderResources resources = mslCompiler.get_shader_resources();
				for (size_t i = 0; i < resources.uniform_buffers.size(); i++)
				{
					const spirv_cross::Resource &res = resources.uniform_buffers[i];
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
					binding.msl_buffer = binding.binding;
					mslCompiler.add_msl_resource_binding(binding);
				}
				// Same reasoning for sampled images (textures aren't
				// exercised by this milestone's shader, but the next
				// texture-sampling material this backend compiles would
				// hit the exact same silent-garbage-read bug without this).
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
		std::vector<SpirvResourceBinding> vsResources = SpirvShaderCompiler::Reflect(vs->second.spirv);
		std::vector<SpirvResourceBinding> fsResources = SpirvShaderCompiler::Reflect(fs->second.spirv);
		for (size_t i = 0; i < vsResources.size(); i++)
			it->second.bindingStageMask[vsResources[i].binding] |= 1u;
		for (size_t i = 0; i < fsResources.size(); i++)
			it->second.bindingStageMask[fsResources[i].binding] |= 2u;
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

	int32 MetalRenderDevice::GetUniformLocation(const uint32 program, const std::string &name) { (void)program; (void)name; return -1; }
	int32 MetalRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return -1;
		std::map<std::string, uint32>::iterator locIt = it->second.attributeLocations.find(name);
		return locIt == it->second.attributeLocations.end() ? -1 : (int32)locIt->second;
	}
	bool MetalRenderDevice::GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 &outBinding, std::string &outBlockName, uint32 &outSize, std::map<std::string, uint32> &outOffsets) { (void)program; (void)engineShaderType; (void)outBinding; (void)outBlockName; (void)outSize; (void)outOffsets; return false; }

	// Unreachable - loose uniforms have no Metal equivalent any more than
	// they have a Vulkan one (everything goes through a buffer, reflected
	// at LinkProgram() time - see its comment). Kept as stubs, not
	// removed, only because they're part of IRenderDevice's shared
	// interface.
	void MetalRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }
	void MetalRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; }

	// =====================================================================
	// Everything below is still a stub - framebuffers/textures/samplers,
	// none of which this milestone's swapchain-only draw path needs (see
	// the file header comment).
	// =====================================================================

	void MetalRenderDevice::TranslateTextureFormat(const uint32 engineDataType, uint32 &internalFormat, uint32 &format, uint32 &type) { (void)engineDataType; internalFormat = format = type = 0; LogStub("TranslateTextureFormat"); }
	void MetalRenderDevice::TranslateTextureTarget(const uint32 engineTextureType, uint32 &mode, uint32 &subMode) { (void)engineTextureType; mode = subMode = 0; LogStub("TranslateTextureTarget"); }

	DeviceHandle MetalRenderDevice::CreateTextureObject() { LogStub("CreateTextureObject"); return 0; }
	void MetalRenderDevice::DestroyTextureObject(const DeviceHandle texture) { (void)texture; LogStub("DestroyTextureObject"); }
	void MetalRenderDevice::BindTextureToTarget(const uint32 target, const DeviceHandle texture) { (void)target; (void)texture; LogStub("BindTextureToTarget"); }

	void MetalRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap) { (void)target; (void)level; (void)internalFormat; (void)width; (void)height; (void)format; (void)type; (void)data; (void)willMipmap; LogStub("UploadTexture2D"); }
	void MetalRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height) { (void)target; (void)samples; (void)internalFormat; (void)width; (void)height; LogStub("UploadTexture2DMultisample"); }
	void MetalRenderDevice::GenerateMipmap(const uint32 target) { (void)target; LogStub("GenerateMipmap"); }

	void MetalRenderDevice::SetTextureWrapS(const uint32 target, const uint32 engineRepeat) { (void)target; (void)engineRepeat; LogStub("SetTextureWrapS"); }
	void MetalRenderDevice::SetTextureWrapT(const uint32 target, const uint32 engineRepeat) { (void)target; (void)engineRepeat; LogStub("SetTextureWrapT"); }
	void MetalRenderDevice::SetTextureWrapR(const uint32 target, const uint32 engineRepeat) { (void)target; (void)engineRepeat; LogStub("SetTextureWrapR"); }
	void MetalRenderDevice::SetTextureMagFilter(const uint32 target, const uint32 engineFilter) { (void)target; (void)engineFilter; LogStub("SetTextureMagFilter"); }
	void MetalRenderDevice::SetTextureMinFilter(const uint32 target, const uint32 engineFilter, const bool hasMipmap) { (void)target; (void)engineFilter; (void)hasMipmap; LogStub("SetTextureMinFilter"); }
	void MetalRenderDevice::SetTextureBaseMaxLevel(const uint32 target, const uint32 baseLevel, const uint32 maxLevel) { (void)target; (void)baseLevel; (void)maxLevel; LogStub("SetTextureBaseMaxLevel"); }
	void MetalRenderDevice::SetTextureBorderColor(const uint32 target, const Vec4 &color) { (void)target; (void)color; LogStub("SetTextureBorderColor"); }
	void MetalRenderDevice::SetTextureCompareMode(const uint32 target) { (void)target; LogStub("SetTextureCompareMode"); }
	void MetalRenderDevice::SetPixelUnpackAlignment(const uint32 value) { (void)value; LogStub("SetPixelUnpackAlignment"); }

	void MetalRenderDevice::ActivateTextureUnit(const uint32 unit) { (void)unit; LogStub("ActivateTextureUnit"); }

	void MetalRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer) { (void)target; (void)level; (void)format; (void)type; (void)outBuffer; LogStub("ReadTexturePixels"); }
	uint32 MetalRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height) { (void)nativeInternalFormat; (void)width; (void)height; LogStub("GetTextureDataSize"); return 0; }

	DeviceHandle MetalRenderDevice::GetCurrentRenderTarget() { return currentBoundFBO; }
	DeviceHandle MetalRenderDevice::CreateFramebuffer() { LogStub("CreateFramebuffer"); return 0; }
	void MetalRenderDevice::DestroyFramebuffer(const DeviceHandle fbo) { (void)fbo; LogStub("DestroyFramebuffer"); }
	void MetalRenderDevice::SetFramebufferPreserveDepth(const DeviceHandle fbo, const bool preserve) { (void)fbo; (void)preserve; LogStub("SetFramebufferPreserveDepth"); }
	uint32 MetalRenderDevice::TranslateFramebufferAccess(const uint32 engineAccess) { (void)engineAccess; LogStub("TranslateFramebufferAccess"); return 0; }
	void MetalRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending) { (void)nativeAccess; (void)fbo; (void)finalizePending; LogStub("BindFramebuffer"); }
	uint32 MetalRenderDevice::TranslateFramebufferAttachment(const uint32 engineAttachmentFormat) { (void)engineAttachmentFormat; LogStub("TranslateFramebufferAttachment"); return 0; }
	void MetalRenderDevice::AttachFramebufferTexture2D(const uint32 nativeAttachmentFormat, const uint32 nativeTextureTarget, const uint32 textureId, const bool wasAlreadyBound) { (void)nativeAttachmentFormat; (void)nativeTextureTarget; (void)textureId; (void)wasAlreadyBound; LogStub("AttachFramebufferTexture2D"); }
	void MetalRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer) { (void)nativeAttachmentFormat; (void)renderbuffer; LogStub("AttachFramebufferRenderbuffer"); }
	void MetalRenderDevice::SetDrawBufferNone() { LogStub("SetDrawBufferNone"); }
	void MetalRenderDevice::SetReadBufferNone() { LogStub("SetReadBufferNone"); }
	void MetalRenderDevice::SetDrawBufferBack() { LogStub("SetDrawBufferBack"); }
	void MetalRenderDevice::SetReadBufferBack() { LogStub("SetReadBufferBack"); }
	void MetalRenderDevice::SetDrawBuffers(const std::vector<uint32> &colorAttachmentIndices) { (void)colorAttachmentIndices; LogStub("SetDrawBuffers"); }
	uint32 MetalRenderDevice::CheckFramebufferStatus() { LogStub("CheckFramebufferStatus"); return 0; }
	uint32 MetalRenderDevice::TranslateFramebufferStatus(const uint32 nativeStatus) { (void)nativeStatus; LogStub("TranslateFramebufferStatus"); return 0; }

	DeviceHandle MetalRenderDevice::CreateRenderbuffer() { LogStub("CreateRenderbuffer"); return 0; }
	void MetalRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo) { (void)rbo; LogStub("DestroyRenderbuffer"); }
	void MetalRenderDevice::BindRenderbuffer(const DeviceHandle rbo) { (void)rbo; LogStub("BindRenderbuffer"); }
	uint32 MetalRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType) { (void)engineDataType; LogStub("TranslateRenderbufferFormat"); return 0; }
	void MetalRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height) { (void)nativeFormat; (void)width; (void)height; LogStub("RenderbufferStorage"); }
	void MetalRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height) { (void)nativeFormat; (void)samples; (void)width; (void)height; LogStub("RenderbufferStorageMultisample"); }

	void MetalRenderDevice::SetMultisampleEnabled(const bool enabled) { (void)enabled; LogStub("SetMultisampleEnabled"); }
	void MetalRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter) { (void)srcX0; (void)srcY0; (void)srcX1; (void)srcY1; (void)dstX0; (void)dstY0; (void)dstX1; (void)dstY1; (void)engineMask; (void)engineFilter; LogStub("BlitFramebuffer"); }
	void MetalRenderDevice::CopyDepthTexture(const DeviceHandle srcTexture, const DeviceHandle dstTexture, const uint32 width, const uint32 height) { (void)srcTexture; (void)dstTexture; (void)width; (void)height; LogStub("CopyDepthTexture"); }

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

	void MetalRenderDevice::BeginRenderEncoderForTarget(const DeviceHandle fbo)
	{
		(void)fbo;
		LogStub("BeginRenderEncoderForTarget");
	}

}

#endif /* METAL_BACKEND */
