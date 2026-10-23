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
#if __has_include(<MetalFX/MetalFX.h>)
#import <MetalFX/MetalFX.h>
#import <AppKit/AppKit.h>
#define PYROS_HAVE_METALFX 1
#else
#define PYROS_HAVE_METALFX 0
#endif

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <execinfo.h>
#include <chrono>
#include <mach/mach_time.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <set>
#include <string>

#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>   // DrawingType
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#ifdef METAL_SHADER_TOOLING
#include <Pyros3D/Rendering/SPIRV/ShaderCompiler.h>
#include <spirv_cross/spirv_msl.hpp>
#include <iterator>
#include <mutex>
#include <filesystem>
#include <fstream>
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

	// Bytes per texel for the formats this backend's textures actually
	// use - readback sizing (GetTextureDataSize/ReadTexturePixels) needs it.
	p3d::uint32 BytesPerPixelMSL(MTLPixelFormat fmt)
	{
		switch (fmt)
		{
			case MTLPixelFormatR8Unorm:        return 1;
			case MTLPixelFormatRG8Unorm:       return 2;
			case MTLPixelFormatR16Float:       return 2;
			case MTLPixelFormatDepth16Unorm:   return 2;
			case MTLPixelFormatRGBA8Unorm:
			case MTLPixelFormatBGRA8Unorm:
			case MTLPixelFormatRGBA8Unorm_sRGB:
			case MTLPixelFormatR32Float:
			case MTLPixelFormatRG16Float:
			case MTLPixelFormatDepth32Float:   return 4;
			case MTLPixelFormatRGBA16Float:    return 8;
			case MTLPixelFormatRGBA32Float:    return 16;
			default:                           return 4;
		}
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
			// A Matrix attribute is ONE engine attribute spanning four
			// MSL vertex-descriptor attributes, one vec4 column each -
			// this returns the format of a single column, and
			// CreatePipeline() below is what emits the four consecutive
			// locations at offset + c*16. Same split as Vulkan's
			// TranslateAttributeFormatVk()/componentCount pair and GL's
			// SetVertexAttribute(location+1/+2/+3) in
			// IRenderer::BindMesh(). Until this case existed, Metal fell
			// through to the default below: RenderingInstancedComponent's
			// aInstancedTransform got a Float3 at its base location and
			// nothing at all at the other three, so
			// newRenderPipelineStateWithDescriptor: failed outright
			// ("Vertex attribute aInstancedTransform_1(10) is missing
			// from the vertex descriptor") and every instanced draw was
			// skipped - instanced rendering did not work on Metal at all,
			// main pass or shadow pass.
			case Buffer::Attribute::Type::Matrix: return MTLVertexFormatFloat4;
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
	// Every format TranslateTextureFormat() can produce must appear here.
	//
	// The `default` is a trap and has caught one already: RG32Float was
	// missing, fell through to 4, and UploadTexture2D then handed
	// replaceRegion a bytesPerRow half of what the texture needed.
	// Metal's complaint is "AGX: Texture read/write assertion failed:
	// bytes_per_row >= used_bytes_per_row", which names the symptom and
	// not the format - and it only fires for formats nothing had
	// uploaded before, so it stays hidden until someone adds one. It
	// went unnoticed until a DDGI visibility atlas became the first
	// RG32F texture in the engine.
	p3d::uint32 BytesPerTexelMSL(MTLPixelFormat format)
	{
		switch (format)
		{
			case MTLPixelFormatR8Unorm: return 1;
			case MTLPixelFormatRG8Unorm: return 2;
			case MTLPixelFormatR16Float: return 2;
			case MTLPixelFormatDepth16Unorm: return 2;
			case MTLPixelFormatR16Sint: return 2;
			case MTLPixelFormatStencil8: return 1;
			case MTLPixelFormatRGBA8Unorm:
			case MTLPixelFormatBGRA8Unorm:
			case MTLPixelFormatR32Float:
			case MTLPixelFormatR32Sint:
			case MTLPixelFormatRG16Float:
			case MTLPixelFormatRG16Sint:
			case MTLPixelFormatDepth32Float:
				return 4;
			case MTLPixelFormatRGBA16Float:
			case MTLPixelFormatRGBA16Sint:
			case MTLPixelFormatRG32Float:
			case MTLPixelFormatRG32Sint:
				return 8;
			case MTLPixelFormatRGBA32Float:
			case MTLPixelFormatRGBA32Sint:
				return 16;
			default:
				// Loud, because a silent wrong answer here corrupts an
				// upload rather than failing it.
				fprintf(stderr, "MetalRenderDevice: BytesPerTexelMSL has no case for MTLPixelFormat %u"
					" - assuming 4 bytes, which is probably wrong.\n", (unsigned)format);
				return 4;
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
		  swapchainPixelFormat((uint32)MTLPixelFormatBGRA8Unorm),
		  presentAsLinear(false),
		  depthTexture(NULL),
		  imguiMetalBackendActive(false), imguiDummyColorTexture(NULL),
		  frameBoundarySemaphore(NULL), currentCommandBuffer(NULL), currentRenderEncoder(NULL),
		  currentDrawable(NULL), lastSubmittedCommandBuffer(NULL),
		  nextVaoHandle(1), currentVao(0), currentPipeline(0),
		  nextShaderStageHandle(1), nextAutoUboBinding(kFirstAutoUboBinding), nextProgramHandle(1),
		  nextBufferHandle(1), nextTextureHandle(1),
		  currentTextureUnit(0), unitJustActivated(false), currentlyConfiguringTexture(0),
		  nextFBOHandle(1), currentBoundFBO(0), currentReadFBO(0),
		  nextComputePipelineHandle(1),
		  computeCommandBuffer(NULL), currentComputeEncoder(NULL), boundComputePipeline(0),
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
			GpuTimerInit();
		}
	}

	void MetalRenderDevice::GpuTimerInit()
	{
		@autoreleasepool
		{
			id<MTLDevice> dev = (__bridge id<MTLDevice>)device;
			if (![dev supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) { fprintf(stderr, "MetalRenderDevice: no stage-boundary counter sampling - no GPU pass timings\n"); return; }
			id<MTLCounterSet> ts = nil;
			for (id<MTLCounterSet> cs in dev.counterSets) if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp]) ts = cs;
			if (ts == nil) return;
			MTLCounterSampleBufferDescriptor* d = [MTLCounterSampleBufferDescriptor new];
			d.counterSet = ts;
			d.storageMode = MTLStorageModeShared;
			d.sampleCount = kGpuTimerSlots * kGpuTimerPairs * 2;
			NSError* err = nil;
			id<MTLCounterSampleBuffer> buf = [dev newCounterSampleBufferWithDescriptor:d error:&err];
			if (buf == nil) { fprintf(stderr, "MetalRenderDevice: no counter sample buffer (%s)\n", err ? [[err localizedDescription] UTF8String] : "?"); return; }
			gpuTimerBuffer = (void*)CFBridgingRetain(buf);
			MTLTimestamp c = 0, g = 0;
			[dev sampleTimestamps:&c gpuTimestamp:&g];
			gpuCalibCpu = c; gpuCalibGpu = g;
		}
	}

	void MetalRenderDevice::GpuTimerAttach(void* renderPassDescriptor)
	{
		if (gpuTimerBuffer == NULL || renderPassDescriptor == NULL) return;
		const uint32 slot = gpuTimerSlot;
		const uint32 i = gpuTimerCount[slot];
		if (i >= kGpuTimerPairs) return;
		MTLRenderPassDescriptor* rpd = (__bridge MTLRenderPassDescriptor*)renderPassDescriptor;
		MTLRenderPassSampleBufferAttachmentDescriptor* a = rpd.sampleBufferAttachments[0];
		a.sampleBuffer = (__bridge id<MTLCounterSampleBuffer>)gpuTimerBuffer;
		const NSUInteger base = (slot * kGpuTimerPairs + i) * 2;
		a.startOfVertexSampleIndex = base;
		a.endOfVertexSampleIndex = MTLCounterDontSample;
		a.startOfFragmentSampleIndex = MTLCounterDontSample;
		a.endOfFragmentSampleIndex = base + 1;
		const char* label = FrameProfiler::Instance().CurrentScopeName();
		std::snprintf(gpuTimerLabel[slot][i], sizeof(gpuTimerLabel[slot][i]), "%s", (label && label[0]) ? label : "(no scope)");
		gpuTimerCount[slot] = i + 1;
	}

	// The slot's frame is done (called once the frame semaphore has let the slot come round).
	void MetalRenderDevice::GpuTimerCollect(const uint32 slot)
	{
		const uint32 count = gpuTimerCount[slot];
		gpuTimerCount[slot] = 0;
		if (gpuTimerBuffer == NULL || count == 0) return;
		@autoreleasepool
		{
			id<MTLCounterSampleBuffer> buf = (__bridge id<MTLCounterSampleBuffer>)gpuTimerBuffer;
			NSData* data = [buf resolveCounterRange:NSMakeRange(slot * kGpuTimerPairs * 2, count * 2)];
			if (data == nil) return;
			const MTLCounterResultTimestamp* t = (const MTLCounterResultTimestamp*)data.bytes;
			// (GPU ticks to nanoseconds: the CPU's clock against the GPU's, measured over the run so far)
			{
				id<MTLDevice> dev = (__bridge id<MTLDevice>)device;
				MTLTimestamp c = 0, g = 0;
				[dev sampleTimestamps:&c gpuTimestamp:&g];
				// (both clocks are in nanoseconds on Apple GPUs - the ratio comes out 1; measured rather
				// than assumed for a GPU whose ticks are something else)
				if (g > gpuCalibGpu + 1000000 && c > gpuCalibCpu)
					gpuTicksToNs = (f64)(c - gpuCalibCpu) / (f64)(g - gpuCalibGpu);
			}
			FrameProfiler::ScopeRecord records[FrameProfiler::kMaxScopes];
			uint32 recordCount = 0;
			f64 total = 0.0;
			uint64 first = ~0ull, last = 0;
			for (uint32 i = 0; i < count; i++)
			{
				const uint64 a = t[i * 2].timestamp, b = t[i * 2 + 1].timestamp;
				if (a == MTLCounterErrorValue || b == MTLCounterErrorValue || a == 0 || b <= a) continue;
				// (a sample a pass did not write - no vertex stage ran - still holds its value from the
				// last frame this slot timed: older than this frame, and three frames long)
				if (a < gpuTimerSlotStart[slot] || b < gpuTimerSlotStart[slot]) continue;
				first = std::min(first, a); last = std::max(last, b);
				const f64 ms = (f64)(b - a) * gpuTicksToNs * 1e-6;
				total += ms;
				uint32 r = 0;
				while (r < recordCount && std::strncmp(records[r].name, gpuTimerLabel[slot][i], FrameProfiler::kMaxNameLen) != 0) r++;
				if (r == recordCount)
				{
					if (recordCount == FrameProfiler::kMaxScopes - 1) continue;
					std::snprintf(records[r].name, FrameProfiler::kMaxNameLen, "%s", gpuTimerLabel[slot][i]);
					records[r].ms = 0.0; records[r].depth = 0;
					recordCount++;
				}
				records[r].ms += ms;
			}
			// (Apple's GPU runs passes alongside each other, so their sum overcounts: the total is the
			// span from the frame's first pass starting to its last one ending)
			(void)total;
			// (the compute done since the last publish: its own command buffers, not in the span)
			{
				std::lock_guard<std::mutex> g(computeTimesMutex);
				for (std::map<std::string, f64>::iterator ct = computeTimesMs.begin(); ct != computeTimesMs.end() && recordCount < FrameProfiler::kMaxScopes - 1; ++ct)
				{
					std::snprintf(records[recordCount].name, FrameProfiler::kMaxNameLen, "%s", ct->first.c_str());
					records[recordCount].ms = ct->second; records[recordCount].depth = 0;
					recordCount++;
				}
				computeTimesMs.clear();
			}
			std::snprintf(records[recordCount].name, FrameProfiler::kMaxNameLen, "Total (sum of passes)");
			records[recordCount].ms = last > first ? (f64)(last - first) * gpuTicksToNs * 1e-6 : 0.0; records[recordCount].depth = 0;
			FrameProfiler::Instance().SetGpuTimings(records, recordCount + 1);
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
			pipelineByDescription.clear();
			for (uint32 k = 0; k < kFallbackKinds; k++)
				if (fallbackTextures[k] != NULL) { CFBridgingRelease(fallbackTextures[k]); fallbackTextures[k] = NULL; }
			if (fallbackSampler != NULL) { CFBridgingRelease(fallbackSampler); fallbackSampler = NULL; }
			if (fallbackCompareSampler != NULL) { CFBridgingRelease(fallbackCompareSampler); fallbackCompareSampler = NULL; }
			pipelineRefs.clear();
			pipelineDescriptionKey.clear();

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

			// Compute, before the command queue goes: an encoder still
			// open here would be leaked, and a committed-but-unwaited
			// command buffer can outlive the queue it came from.
			// FlushComputeEncoding is not used - it commits, and
			// committing work during teardown is exactly what the render
			// device lifetime notes warn about; these are dropped, not
			// submitted.
			if (currentComputeEncoder != NULL) { CFBridgingRelease(currentComputeEncoder); currentComputeEncoder = NULL; }
			if (computeCommandBuffer != NULL) { CFBridgingRelease(computeCommandBuffer); computeCommandBuffer = NULL; }
			for (std::map<DeviceHandle, ComputePipelineRecord>::iterator it = computePipelines.begin(); it != computePipelines.end(); ++it)
			{
				if (it->second.state != NULL) CFBridgingRelease(it->second.state);
			}
			computePipelines.clear();
			// The MTLBuffers themselves were released by the `buffers`
			// loop above - a storage buffer is an entry in that same map.
			storageBufferSizes.clear();

			if (depthTexture != NULL) { CFBridgingRelease(depthTexture); depthTexture = NULL; }
			if (currentRenderEncoder != NULL) { CFBridgingRelease(currentRenderEncoder); currentRenderEncoder = NULL; }
			if (currentCommandBuffer != NULL) { CFBridgingRelease(currentCommandBuffer); currentCommandBuffer = NULL; }
			if (currentDrawable != NULL) { CFBridgingRelease(currentDrawable); currentDrawable = NULL; }
			if (lastSubmittedCommandBuffer != NULL) { CFBridgingRelease(lastSubmittedCommandBuffer); lastSubmittedCommandBuffer = NULL; }
			if (frameBoundarySemaphore != NULL) { CFBridgingRelease(frameBoundarySemaphore); frameBoundarySemaphore = NULL; }
			if (gpuTimerBuffer != NULL) { CFBridgingRelease(gpuTimerBuffer); gpuTimerBuffer = NULL; }
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
			// BGRA8Unorm (not _sRGB): ImGui writes already-sRGB UI colours
			// into this drawable. An sRGB pixel format encoded them again
			// and washed the chrome out. Scene brightness for the editor
			// viewport is handled by gamma-encoding the offscreen capture
			// (PostEffectsManager::GetViewportColor), not the swapchain.
			layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
			swapchainPixelFormat = (uint32)MTLPixelFormatBGRA8Unorm;
			// This engine only ever samples an offscreen render target back
			// as a texture (see IRenderDevice::CopyDepthTexture()'s comment
			// on the same distinction on the Vulkan side) - the swapchain/
			// drawable itself is never read back that way, so
			// framebufferOnly's perf benefit (lets the driver skip a
			// present-compatible memory layout) is free to take.
			layer.framebufferOnly = YES;
			// PYROS_METAL_DISPLAY_SYNC=0|1 (see the note below; the default is
			// decided where it is measured)
			if (const char* sync = getenv("PYROS_METAL_DISPLAY_SYNC"))
				if (@available(macOS 10.13, *)) layer.displaySyncEnabled = (sync[0] != '0') ? YES : NO;
			// Deliberately not setting displaySyncEnabled = NO here, having
			// measured it. The backend is capped at exactly 60fps and that
			// property looks like the cause (it defaults to YES, and
			// MoltenVK sets it to NO for VK_PRESENT_MODE_IMMEDIATE_KHR),
			// but it is not: with it set to NO and reading back 0, timing
			// BeginFrame showed [layer nextDrawable] blocking 16.1ms per
			// frame while the frame-boundary semaphore cost 0.03ms. The
			// pacing is WindowServer recycling one drawable per refresh for
			// a composited window, which that property does not control -
			// it only changed frame times from a steady 16.67ms to a
			// jittery 16.7-18.5ms. GL is uncapped here only because its
			// presentation path does not go through a CAMetalLayer at all.
			//
			// This caps throughput measurement, not real work: per-scope
			// CPU timings are unaffected by present pacing, which is how
			// the Vulkan offscreen-stall fix was measured.
			//
			// colorspace left at the system sRGB default unless the editor
			// calls SetPresentAsLinear(true) - see that method's comment.
			layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
		}

		metalLayer = layerPtr;
		drawableWidth = width;
		drawableHeight = height;
		if (hdrWanted) ApplyLayerOutput();
		RebuildDepthTexture();
		return true;
	}

	// The layer's format and colour space: 8-bit sRGB, or - HDR output - half floats in extended sRGB
	// with EDR, where 1.0 is SDR white and the display's headroom is above it. Encoded the same either
	// way, so the UI (sRGB colours, at most 1.0) needs nothing; only the tonemap goes past 1.
	void MetalRenderDevice::ApplyLayerOutput()
	{
		if (metalLayer == NULL) return;
		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			const bool on = hdrWanted;
			layer.wantsExtendedDynamicRangeContent = on ? YES : NO;
			layer.pixelFormat = on ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
			swapchainPixelFormat = (uint32)layer.pixelFormat;
			CGColorSpaceRef cs = CGColorSpaceCreateWithName(on ? kCGColorSpaceExtendedSRGB : (presentAsLinear ? kCGColorSpaceLinearSRGB : kCGColorSpaceSRGB));
			layer.colorspace = cs;
			if (cs != NULL) CGColorSpaceRelease(cs);
			hdrOn = on;
		}
	}

	bool MetalRenderDevice::SetHDROutput(const bool on)
	{
		hdrWanted = on;
		// (the frame being drawn is in the old format: a pass on the window is ended first)
		if (metalLayer != NULL && hdrOn != on)
		{
			if (frameInProgress) EndCurrentRenderEncoderIfOpen();
			ApplyLayerOutput();
			// (ImGui's pipeline is keyed off a texture of the window's format)
			if (imguiDummyColorTexture != NULL)
			{
				@autoreleasepool
				{
					id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
					MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(MTLPixelFormat)swapchainPixelFormat width:1 height:1 mipmapped:NO];
					d.usage = MTLTextureUsageRenderTarget; d.storageMode = MTLStorageModePrivate;
					id<MTLTexture> t = [mtlDevice newTextureWithDescriptor:d];
					if (t != nil) { CFBridgingRelease(imguiDummyColorTexture); imguiDummyColorTexture = (void*)CFBridgingRetain(t); }
				}
			}
		}
		return on ? hdrOn : true;
	}

	// The window's display, as it is now: how many times SDR white it can show (its brightness setting
	// moves it). 1 when HDR output is off or the display has none.
	f32 MetalRenderDevice::GetHDRHeadroom() const
	{
		if (!hdrOn || metalLayer == NULL) return 1.f;
		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			NSScreen* screen = nil;
			id delegate = layer.delegate;
			if ([delegate isKindOfClass:[NSView class]]) screen = ((NSView*)delegate).window.screen;
			if (screen == nil) screen = [NSScreen mainScreen];
			const CGFloat h = screen != nil ? screen.maximumExtendedDynamicRangeColorComponentValue : 1.0;
			return (f32)(h > 1.0 ? h : 1.0);
		}
	}

	void MetalRenderDevice::SetPresentAsLinear(const bool enabled)
	{
		presentAsLinear = enabled;
		if (metalLayer == NULL)
			return;
		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			// Linear: UNORM bytes are linear light, ColorSync maps to the
			// display - same brightness model as typical OpenGL default FB
			// content. sRGB: UNORM bytes are display-encoded (TonemapEffect).
			CGColorSpaceRef cs = CGColorSpaceCreateWithName(
				enabled ? kCGColorSpaceLinearSRGB : kCGColorSpaceSRGB);
			layer.colorspace = cs;
			if (cs != NULL)
				CGColorSpaceRelease(cs);
		}
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
		WaitSubmittedCompute();
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

		// PYROS_FRAME_WAIT_TRACE=1: how long a frame waited here, and on what (a frame of the GPU's
		// still in flight, or the display not yet handing back a drawable), when it was over 4 ms.
		static const bool waitTrace = getenv("PYROS_FRAME_WAIT_TRACE") != NULL;
		const std::chrono::steady_clock::time_point w0 = std::chrono::steady_clock::now();
		dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
		const std::chrono::steady_clock::time_point w1 = std::chrono::steady_clock::now();
		// (past the semaphore, at most MAX_FRAMES_IN_FLIGHT - 1 frames are on the GPU: the frame that
		// last used the next slot, kGpuTimerSlots ago, is done)
		deviceFrameNumber++;
		gpuTimerSlot = (gpuTimerSlot + 1) % kGpuTimerSlots;
		GpuTimerCollect(gpuTimerSlot);
		if (gpuTimerBuffer != NULL)
		{
			@autoreleasepool
			{
				MTLTimestamp c = 0, g = 0;
				[(__bridge id<MTLDevice>)device sampleTimestamps:&c gpuTimestamp:&g];
				gpuTimerSlotStart[gpuTimerSlot] = g;
			}
		}

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			FrameProfiler::NoteDisplayWait(std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - w0).count());
			if (waitTrace)
			{
				const std::chrono::steady_clock::time_point w2 = std::chrono::steady_clock::now();
				const f64 inFlight = std::chrono::duration<f64, std::milli>(w1 - w0).count(), display = std::chrono::duration<f64, std::milli>(w2 - w1).count();
				if (inFlight + display > 4.0)
					fprintf(stderr, "[frame-wait] frame %llu: %.1f ms for the GPU's frame in flight, %.1f ms for a drawable (layer: %lu drawables, sync %d)\n",
						(unsigned long long)deviceFrameNumber, inFlight, display, (unsigned long)layer.maximumDrawableCount, (int)layer.displaySyncEnabled);
			}
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

			{
				// When each frame really reached the screen: the profiler shows the gaps between them,
				// which are what is seen (a frame that waited a second refresh for a drawable, with
				// frames already queued, may show as nothing at all). PYROS_PRESENT_TRACE=1 prints each.
				static const bool presentTrace = getenv("PYROS_PRESENT_TRACE") != NULL;
				[drawable addPresentedHandler:^(id<MTLDrawable> d) {
					static CFTimeInterval lastShown = 0.0;
					const CFTimeInterval at = d.presentedTime;
					if (at > 0.0 && lastShown > 0.0)
					{
						FrameProfiler::NotePresented((at - lastShown) * 1000.0);
						if (presentTrace) fprintf(stderr, "[present] %.2f ms\n", (at - lastShown) * 1000.0);
					}
					else if (at <= 0.0 && presentTrace) fprintf(stderr, "[present] dropped (never shown)\n");
					if (at > 0.0) lastShown = at;
				}];
				[cmdBuf presentDrawable:drawable];
			}

			[cmdBuf addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull finishedBuffer) {
				// A frame the GPU refused is a frame of nothing, and it does
				// not say so unless asked: said once, with Metal's own words.
				if (finishedBuffer.status == MTLCommandBufferStatusError)
				{
					static bool said = false;
					if (!said) { said = true; fprintf(stderr, "Metal: a frame's command buffer failed: %s\n", finishedBuffer.error ? [[finishedBuffer.error localizedDescription] UTF8String] : "(no reason given)"); }
				}
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
		// PYROS_FRAME_WAIT_TRACE=1: how long a frame waited here, and on what (a frame of the GPU's
		// still in flight, or the display not yet handing back a drawable), when it was over 4 ms.
		static const bool waitTrace = getenv("PYROS_FRAME_WAIT_TRACE") != NULL;
		const std::chrono::steady_clock::time_point w0 = std::chrono::steady_clock::now();
		dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
		const std::chrono::steady_clock::time_point w1 = std::chrono::steady_clock::now();
		// (past the semaphore, at most MAX_FRAMES_IN_FLIGHT - 1 frames are on the GPU: the frame that
		// last used the next slot, kGpuTimerSlots ago, is done)
		deviceFrameNumber++;
		gpuTimerSlot = (gpuTimerSlot + 1) % kGpuTimerSlots;
		GpuTimerCollect(gpuTimerSlot);
		if (gpuTimerBuffer != NULL)
		{
			@autoreleasepool
			{
				MTLTimestamp c = 0, g = 0;
				[(__bridge id<MTLDevice>)device sampleTimestamps:&c gpuTimestamp:&g];
				gpuTimerSlotStart[gpuTimerSlot] = g;
			}
		}

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			FrameProfiler::NoteDisplayWait(std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - w0).count());
			if (waitTrace)
			{
				const std::chrono::steady_clock::time_point w2 = std::chrono::steady_clock::now();
				const f64 inFlight = std::chrono::duration<f64, std::milli>(w1 - w0).count(), display = std::chrono::duration<f64, std::milli>(w2 - w1).count();
				if (inFlight + display > 4.0)
					fprintf(stderr, "[frame-wait] frame %llu: %.1f ms for the GPU's frame in flight, %.1f ms for a drawable (layer: %lu drawables, sync %d)\n",
						(unsigned long long)deviceFrameNumber, inFlight, display, (unsigned long)layer.maximumDrawableCount, (int)layer.displaySyncEnabled);
			}
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
				// Stored, not DontCare: an offscreen pass in the middle of the frame (a forward
				// renderer's shadow maps) ends this encoder, and the one that resumes the swapchain
				// LOADS the depth - left undefined, every fragment of the main pass failed its depth
				// test and the forward demos with shadows drew nothing on Metal.
				rpd.depthAttachment.storeAction = MTLStoreActionStore;
				rpd.depthAttachment.clearDepth = 1.0;
			}

			GpuTimerAttach((__bridge void*)rpd);
			id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpd];
			currentRenderEncoder = (void*)CFBridgingRetain(encoder);
			encoderBoundTextures.clear(); encoderBoundCubes.clear();

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

	std::string MetalRenderDevice::MemoryReport()
	{
		// the uniform rings only: what each holds against what a frame has asked of it
		std::string out = "{\"rings\":[";
		bool first = true;
		for (std::map<DeviceHandle, BufferRecord>::iterator it = buffers.begin(); it != buffers.end(); ++it)
		{
			const BufferRecord &b = it->second;
			if (!b.isDynamicUniform) continue;
			if (!first) out += ",";
			first = false;
			out += "{\"handle\":" + std::to_string((unsigned long long)it->first) + ",\"binding\":" + std::to_string(b.bindingPoint)
				+ ",\"slotBytes\":" + std::to_string(b.alignedSlotSize) + ",\"slots\":" + std::to_string(b.slotCount)
				+ ",\"bytes\":" + std::to_string((unsigned long long)b.alignedSlotSize * b.slotCount)
				+ ",\"peak\":" + std::to_string(b.peakWrites > b.demandThisFrame ? b.peakWrites : b.demandThisFrame)
				+ ",\"maxSlots\":" + std::to_string(b.maxSlotCount) + ",\"growths\":" + std::to_string(b.growths)
				+ ",\"exhausted\":false}";
		}
		out += "]}";
		return out;
	}

	void MetalRenderDevice::EndFrame()
	{
		if (!frameInProgress)
			return;

		// The rings' frame turns over: this frame's writes become the last one's.
		for (std::map<DeviceHandle, BufferRecord>::iterator it = buffers.begin(); it != buffers.end(); ++it)
		{
			BufferRecord &rec = it->second;
			if (!rec.isDynamicUniform) continue;
			if (rec.demandThisFrame > rec.peakWrites) rec.peakWrites = rec.demandThisFrame;
			rec.demandThisFrame = 0;
			for (uint32 i = BufferRecord::kFramesRead - 1; i > 0; i--) rec.writes[i] = rec.writes[i - 1];
			rec.writes[0] = 0;
		}

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
			{
				// When each frame really reached the screen: the profiler shows the gaps between them,
				// which are what is seen (a frame that waited a second refresh for a drawable, with
				// frames already queued, may show as nothing at all). PYROS_PRESENT_TRACE=1 prints each.
				static const bool presentTrace = getenv("PYROS_PRESENT_TRACE") != NULL;
				[drawable addPresentedHandler:^(id<MTLDrawable> d) {
					static CFTimeInterval lastShown = 0.0;
					const CFTimeInterval at = d.presentedTime;
					if (at > 0.0 && lastShown > 0.0)
					{
						FrameProfiler::NotePresented((at - lastShown) * 1000.0);
						if (presentTrace) fprintf(stderr, "[present] %.2f ms\n", (at - lastShown) * 1000.0);
					}
					else if (at <= 0.0 && presentTrace) fprintf(stderr, "[present] dropped (never shown)\n");
					if (at > 0.0) lastShown = at;
				}];
				[cmdBuf presentDrawable:drawable];
			}

			dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
			[cmdBuf addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull finishedBuffer) {
				// A frame the GPU refused is a frame of nothing, and it does
				// not say so unless asked: said once, with Metal's own words.
				if (finishedBuffer.status == MTLCommandBufferStatusError)
				{
					static bool said = false;
					if (!said) { said = true; fprintf(stderr, "Metal: a frame's command buffer failed: %s\n", finishedBuffer.error ? [[finishedBuffer.error localizedDescription] UTF8String] : "(no reason given)"); }
				}
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

	uint32 MetalRenderDevice::TranslateBufferBit(const uint32 bufferBits) { return bufferBits; }
	// The clear already happens via the encoder's load action
	// (pendingClearColor, set by SetClearColor() below) - there is no
	// separate "clear now" operation inside a render pass on this backend.
	//
	// Except for depth on an FBO whose depth is preserved
	// (SetFramebufferPreserveDepth()): its encoder *loaded* depth, so a
	// caller asking for a clear - ForwardRenderer at the top of every frame
	// rendered into such a capture - would otherwise draw over last frame's
	// depth. Vulkan clears there with vkCmdClearAttachments; here the
	// encoder is reopened with a depth Clear and every colour Load. Not for
	// a scissored clear (AxisHelper's corner): a load action covers the
	// whole target, and wiping all of it is what that scissor prevents.
	// A clear is the start of a pass on Metal (its load action). Right after a bind that cleared the same
	// buffers there is nothing to do; otherwise the pass begins again, clearing what is named and keeping
	// the rest - a resumed framebuffer, or depth cleared between two layers of drawing.
	void MetalRenderDevice::Clear(const uint32 nativeBufferBits)
	{
		const uint32 want = nativeBufferBits & (Buffer_Bit::Color | Buffer_Bit::Depth);
		if (want == 0 || currentRenderEncoder == NULL || currentBoundFBO == 0)
			return;
		if (!encoderDrew && (encoderClearedBits & want) == want)
			return;
		std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.find(currentBoundFBO);
		if (it == fboRecords.end())
			return;
		// (a scissor narrower than the target: a pass clears all of it - not done)
		const DeviceHandle sized = it->second.depthAttachment.texture != 0 ? it->second.depthAttachment.texture
			: (it->second.colorAttachments.empty() ? 0 : it->second.colorAttachments.begin()->second.texture);
		std::map<DeviceHandle, TextureRecord>::iterator depthIt = textures.find(sized);
		if (depthIt == textures.end())
			return;
		if (scissorEnabled && (lastScissor[0] > 0 || lastScissor[1] > 0
			|| lastScissor[2] < depthIt->second.width || lastScissor[3] < depthIt->second.height))
			return;
		const uint32 viewport[4] = { lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3] };
		const bool restoreScissor = scissorEnabled;
		const uint32 scissor[4] = { lastScissor[0], lastScissor[1], lastScissor[2], lastScissor[3] };
		BeginRenderEncoderForTarget(currentBoundFBO, (int)want);
		if (viewport[2] > 0 && viewport[3] > 0)
			SetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		if (restoreScissor)
			SetScissorRect((f32)scissor[0], (f32)scissor[1], (f32)scissor[2], (f32)scissor[3]);
	}
	void MetalRenderDevice::SetClearColor(const Vec4 &color) { lastClearColor = color; pendingClearColor = color; }

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
			// Metal validates a scissor against the render target and
			// aborts on one that leaves it, which a clip rect intersected
			// down to nothing otherwise would.
			rect.x = (NSUInteger)(x > 0.f ? x : 0.f);
			rect.y = (NSUInteger)(y > 0.f ? y : 0.f);
			rect.width = (NSUInteger)(width > 0.f ? width : 0.f);
			rect.height = (NSUInteger)(height > 0.f ? height : 0.f);
			lastScissor[0] = (uint32)rect.x; lastScissor[1] = (uint32)rect.y;
			lastScissor[2] = (uint32)rect.width; lastScissor[3] = (uint32)rect.height;
			[encoder setScissorRect:rect];
		}
	}

	// As on Vulkan: no test to switch off, only a rect, so "off" is the
	// whole viewport.
	void MetalRenderDevice::SetScissorTestEnabled(const bool enabled)
	{
		scissorEnabled = enabled;
		if (enabled || currentRenderEncoder == NULL) return;
		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			MTLScissorRect rect;
			rect.x = lastViewport[0];
			rect.y = lastViewport[1];
			rect.width = lastViewport[2];
			rect.height = lastViewport[3];
			[encoder setScissorRect:rect];
		}
	}

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
			fprintf(stderr, "MetalRenderDevice::CreatePipeline: program %u has no compiled vertex+fragment function pair (vs_handle=%u,fs_handle=%u,vs_found=%d,fs_found=%d,vs_fn=%p,fs_fn=%p)\n",
				desc.shaderProgram,
				progIt->second.vertexShader, progIt->second.fragmentShader,
				vs != shaderStages.end(), fs != shaderStages.end(),
				vs != shaderStages.end() ? vs->second.function : NULL,
				fs != shaderStages.end() ? fs->second.function : NULL);
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
			// Same reasoning as VulkanRenderDevice's boundFboHasOwnPass: a
			// bound FBO's real colour attachments win over the "shadow
			// passes are depth-only" shortcut. That shortcut was safe while
			// every shadow FBO was depth-only, but a point light's cube map
			// is an R32F colour attachment now, and declaring
			// MTLPixelFormatInvalid for it means Metal silently never
			// stores what the shadow shader wrote - exactly the failure
			// mode this block's own comment describes above.
			std::map<DeviceHandle, FBORecord>::iterator boundFboIt = fboRecords.find(currentBoundFBO);
			const bool boundFboHasColor = currentBoundFBO != 0 && boundFboIt != fboRecords.end()
				&& !boundFboIt->second.colorAttachments.empty();
			if (desc.isShadowPass && !boundFboHasColor)
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
					pipelineDesc.colorAttachments[0].pixelFormat = (MTLPixelFormat)swapchainPixelFormat;
				}
			}
			pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
			if (currentBoundFBO != 0 && boundFboIt != fboRecords.end() && boundFboIt->second.depthAttachment.texture != 0)
				pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			else if (currentBoundFBO == 0)
				pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			// A multisample target needs a pipeline that rasterises at its
			// sample count - Metal API validation rejects the draw otherwise.
			// Every attachment of one FBO shares a count, so the first found
			// is the answer.
			NSUInteger rasterSamples = 1;
			if (currentBoundFBO != 0 && boundFboIt != fboRecords.end())
			{
				DeviceHandle probe = boundFboIt->second.depthAttachment.texture;
				if (!boundFboIt->second.colorAttachments.empty())
					probe = boundFboIt->second.colorAttachments.begin()->second.texture;
				std::map<DeviceHandle, TextureRecord>::iterator probeIt = textures.find(probe);
				if (probeIt != textures.end() && probeIt->second.samples > 1)
					rasterSamples = probeIt->second.samples;
			}
			pipelineDesc.rasterSampleCount = rasterSamples;
			// Color-only offscreen targets (every IEffect FBO, including
			// PostEffectsManager::GetViewportColor()'s gamma blit) must keep
			// Invalid: a pipeline that declares Depth32Float against a
			// depth-less render pass fails newRenderPipelineState or draws
			// nothing - the editor viewport stayed black/dark because the
			// gamma encode pass never actually ran.

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
					bool bufferHasBoundAttribute = false;
					for (size_t a = 0; a < layout.attributes.size(); a++)
					{
						const IRenderDevice::VertexAttributeDesc &attr = layout.attributes[a];
						std::map<std::string, uint32>::iterator locIt = progIt->second.attributeLocations.find(attr.name);
						if (locIt == progIt->second.attributeLocations.end())
							continue; // shader doesn't use this attribute - matches GL's -1-location no-op, not an error
						bufferHasBoundAttribute = true;
						uint32 location = locIt->second;
						// A Matrix attribute occupies 4 consecutive
						// locations, one vec4 column each - mirrors
						// VulkanRenderDevice's componentCount loop and GL's
						// SetVertexAttribute(location+1/+2/+3) in
						// IRenderer::BindMesh(). SPIRV-Cross reflects the
						// extra three as aInstancedTransform_1/_2/_3, which
						// is what Metal's own pipeline validation names when
						// they are absent.
						uint32 componentCount = (attr.type == Buffer::Attribute::Type::Matrix) ? 4 : 1;
						MTLVertexFormat attrFormat = TranslateVertexFormatMSL(attr.type);
						for (uint32 c = 0; c < componentCount; c++)
						{
							vertexDesc.attributes[location + c].format = attrFormat;
							vertexDesc.attributes[location + c].offset = attr.offset + c * 16;
							// See the header comment on kFirstVertexBufferIndex -
							// vertex attribute buffers live above the UBO
							// binding range, not at 0..N, to avoid colliding
							// with PyrosShader.glsl's own UBO_BINDING numbers
							// in MSL's single shared buffer-index namespace.
							vertexDesc.attributes[location + c].bufferIndex = (NSUInteger)(kFirstVertexBufferIndex + bufferIdx);
						}
					}
					// Only describe the buffer's layout if the shader actually
					// reads something out of it. Metal validates the vertex
					// descriptor as a whole and asserts outright on a layout
					// that has a stride but no attribute pointing at it
					// ("None of the attributes set bufferIndex to N, but
					// MTLVertexDescriptor set buffer layout[N].stride(...)"),
					// where GL and Vulkan both just leave the buffer unread.
					// A mesh reaches here with a buffer no shader variant
					// wants whenever a component carries an attribute buffer
					// the material's shader doesn't declare - e.g. an
					// instanced component whose material was built without
					// ShaderUsage::InstancedRendering, so the vertex shader
					// has no aInstancedTransform to point at the per-instance
					// buffer (which is a material bug in its own right, but
					// one GL and Vulkan render wrongly rather than abort on).
					// The buffer still gets bound in DrawElements(); nothing
					// samples it.
					if (!bufferHasBoundAttribute)
						continue;
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

			// Everything the pipeline state below is built from - see
			// pipelineByDescription.
			std::string descriptionKey;
			{
				const auto put = [&descriptionKey](const auto &v) { descriptionKey.append(reinterpret_cast<const char*>(&v), sizeof(v)); };
				put(desc.shaderProgram); put(desc.depthTest); put(desc.depthTestMode); put(desc.depthWrite);
				put(desc.blendingEnabled); put(desc.blendSrcFactor); put(desc.blendDstFactor); put(desc.blendEquation);
				put(desc.cullFace); put(desc.wireframe); put(desc.drawingType); put(desc.isShadowPass); put(desc.noVertexInput);
				put((uint32)desc.vertexLayout.size());
				for (size_t b = 0; b < desc.vertexLayout.size(); b++)
				{
					const IRenderDevice::VertexBufferLayoutDesc &layout = desc.vertexLayout[b];
					put(layout.stride); put((uint32)layout.attributes.size());
					for (size_t a = 0; a < layout.attributes.size(); a++)
					{
						put((uint32)layout.attributes[a].name.size()); descriptionKey.append(layout.attributes[a].name);
						put(layout.attributes[a].type); put(layout.attributes[a].offset); put(layout.attributes[a].divisor);
					}
				}
				for (NSUInteger c = 0; c < 8; c++)
				{
					const NSUInteger format = (NSUInteger)pipelineDesc.colorAttachments[c].pixelFormat;
					put(format);
				}
				const NSUInteger depthFormat = (NSUInteger)pipelineDesc.depthAttachmentPixelFormat;
				put(depthFormat);
				put(rasterSamples);
			}
			{
				std::map<std::string, DeviceHandle>::iterator shared = pipelineByDescription.find(descriptionKey);
				if (shared != pipelineByDescription.end() && pipelines.find(shared->second) != pipelines.end())
				{
					pipelineRefs[shared->second]++;
					return shared->second;
				}
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
			record.desc = desc;
			record.forSwapchain = currentBoundFBO == 0 && !desc.isShadowPass;
			record.swapchainFormat = swapchainPixelFormat;

			DeviceHandle handle = nextPipelineHandle++;
			pipelines[handle] = record;
			pipelineRefs[handle] = 1;
			pipelineByDescription[descriptionKey] = handle;
			pipelineDescriptionKey[handle] = descriptionKey;
			return handle;
		}
	}

	void MetalRenderDevice::DestroyPipeline(const DeviceHandle pipeline)
	{
		std::map<DeviceHandle, PipelineRecord>::iterator it = pipelines.find(pipeline);
		if (it == pipelines.end())
			return;
		// Shared - see pipelineByDescription. Only the last user frees it.
		std::map<DeviceHandle, uint32>::iterator refIt = pipelineRefs.find(pipeline);
		if (refIt != pipelineRefs.end() && refIt->second > 1)
		{
			refIt->second--;
			return;
		}
		if (refIt != pipelineRefs.end()) pipelineRefs.erase(refIt);
		std::map<DeviceHandle, std::string>::iterator keyIt = pipelineDescriptionKey.find(pipeline);
		if (keyIt != pipelineDescriptionKey.end())
		{
			std::map<std::string, DeviceHandle>::iterator byDesc = pipelineByDescription.find(keyIt->second);
			if (byDesc != pipelineByDescription.end() && byDesc->second == pipeline)
				pipelineByDescription.erase(byDesc);
			pipelineDescriptionKey.erase(keyIt);
		}
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
		// (made for the window before its format changed - HDR output turned on or off: made again)
		if (it->second.forSwapchain && it->second.swapchainFormat != swapchainPixelFormat && currentBoundFBO == 0)
		{
			const PipelineDesc desc = it->second.desc;
			const DeviceHandle fresh = CreatePipeline(desc);
			std::map<DeviceHandle, PipelineRecord>::iterator f = pipelines.find(fresh);
			it = pipelines.find(pipeline);
			if (f != pipelines.end() && it != pipelines.end() && fresh != pipeline)
			{
				if (it->second.pipelineState != NULL) CFBridgingRelease(it->second.pipelineState);
				it->second.pipelineState = (void*)CFBridgingRetain((__bridge id<MTLRenderPipelineState>)f->second.pipelineState);
				it->second.swapchainFormat = swapchainPixelFormat;
				DestroyPipeline(fresh);
				it = pipelines.find(pipeline);
			}
			if (it == pipelines.end()) { currentPipeline = 0; return; }
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
			lastViewport[0] = x; lastViewport[1] = y; lastViewport[2] = width; lastViewport[3] = height;
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
	void MetalRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint, const DeviceHandle bufferHandle)
	{
		// Buffers are bound at draw time (BindProgramUniformBuffers), so all
		// there is to do here is remember which one this program named. No
		// handle means "whatever is registered at that binding point", which
		// is what the draw falls back to. Two haze-then-smoke effects in a
		// chain drew black without this: the haze read the smoke's block.
		(void)blockName;
		if (bufferHandle != 0)
			explicitBlockBuffers[program][bindingPoint] = bufferHandle;
	}

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
		// The cube-face case (skipYFlip, set by
		// IRenderer::RenderingPointShadowFace) needs the *opposite* of
		// Metal's usual convention, not "the same minus a flip". Metal's
		// NDC Y points up like GL's, so the correction above deliberately
		// doesn't flip - correct for anything whose result is presented or
		// sampled with normal UVs. A cube face is neither: it is stored in
		// a top-origin texture and then read back by direction, so it has
		// to land in the same orientation GL's bottom-origin FBO produces,
		// or every lookup is mirrored about the face's horizontal axis.
		//
		// Measured with the shadow cube-map viewer, caster off-centre so
		// the mirror is visible: GL puts it at 459,120 in -Y and Vulkan at
		// 459,121; Metal had it at 460,394, i.e. 512-120 flipped. The
		// visible symptom was a point-light shadow falling on the wrong
		// side of its caster.
		static const Matrix clipCorrectionYFlip(
			1.f, 0.f, 0.f, 0.f,
			0.f, -1.f, 0.f, 0.f,
			0.f, 0.f, 0.5f, 0.f,
			0.f, 0.f, 0.5f, 1.f
		);
		return (skipYFlip ? clipCorrectionYFlip : clipCorrection) * projectionMatrix;
	}
	// Maps a light-space clip position to a shadow-map texcoord. Z needs no
	// remap (TranslateProjectionMatrix() above already put it in [0,1],
	// which is what the depth texture stores), so only X/Y are biased -
	// that much matches VulkanRenderDevice's version this was copied from.
	// The Y *sign* does not: this is the same NDC-Y trap
	// TranslateProjectionMatrix() had (see its comment). Vulkan's NDC Y
	// points down, so its render targets' v=0 row is the one NDC -1 wrote
	// and a plain +0.5y+0.5 lands on the right texel. Metal's NDC Y points
	// up while its render targets still store v=0 at the top - the same
	// mismatch IEffect.cpp's full-screen-quad shader documents - so the
	// mapping has to be -0.5y+0.5 instead. With the Vulkan copy left in
	// place every shadow lookup sampled the shadow map vertically mirrored:
	// RotatingCubeWithLightingAndShadow's cube lost its shadow entirely
	// (the mirrored texel is unoccluded floor), and SimplePhysics' shadows
	// slid around as the camera moved, because the deferred path feeds this
	// matrix a position reconstructed from depth - so a mirrored lookup
	// moves with the view instead of staying put on the geometry.
	Matrix MetalRenderDevice::TranslateShadowBiasMatrix()
	{
		static const Matrix xyOnlyBias(
			0.5f, 0.f, 0.f, 0.f,
			0.f, -0.5f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.5f, 0.5f, 0.f, 1.f
		);
		return xyOnlyBias;
	}

	// Unused - Metal bakes primitive topology into the pipeline
	// (CreatePipeline() hardcodes MTLPrimitiveTypeTriangle in
	// DrawElements()/DrawElementsInstanced() directly), not the draw call,
	// same reasoning as VulkanRenderDevice's identical stub.
	// Metal takes the primitive type as a draw-call argument (unlike Vulkan,
	// which bakes it into the pipeline), so a real translation is all this
	// backend needs. Was a stub returning 0, which meant every mesh drew as
	// MTLPrimitiveTypeTriangle regardless - the editor's line grid came out
	// as filled triangles, as did every DebugRenderer bounding volume.
	// Line_Loop, Quads and Polygons have no Metal equivalent (Metal dropped
	// them along with the fixed-function pipeline); they fall back to the
	// nearest primitive that still shows something rather than drawing
	// nothing at all.
	uint32 MetalRenderDevice::TranslateDrawType(const uint32 engineDrawType)
	{
		switch (engineDrawType)
		{
		case DrawingType::Lines:          return MTLPrimitiveTypeLine;
		case DrawingType::Line_Loop:      return MTLPrimitiveTypeLineStrip;  // no loop primitive; last segment is missing
		case DrawingType::Line_Strip:     return MTLPrimitiveTypeLineStrip;
		case DrawingType::Points:         return MTLPrimitiveTypePoint;
		case DrawingType::Triangle_Strip: return MTLPrimitiveTypeTriangleStrip;
		case DrawingType::Triangle_Fan:   return MTLPrimitiveTypeTriangle;   // no fan primitive in Metal
		case DrawingType::Quads:
		case DrawingType::Polygons:
		case DrawingType::Triangles:
		default:                          return MTLPrimitiveTypeTriangle;
		}
	}

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
				// The buffer this program named for the binding, if it named one.
				DeviceHandle wanted = 0;
				std::map<DeviceHandle, std::map<uint32, DeviceHandle> >::iterator named = explicitBlockBuffers.find(programHandle);
				if (named != explicitBlockBuffers.end())
				{
					std::map<uint32, DeviceHandle>::iterator n = named->second.find(bindingPoint);
					if (n != named->second.end()) wanted = n->second;
				}
				if (wanted == 0)
				{
					std::map<uint32, DeviceHandle>::iterator bufHandleIt = uniformBufferByBindingPoint.find(bindingPoint);
					if (bufHandleIt == uniformBufferByBindingPoint.end())
						continue;
					wanted = bufHandleIt->second;
				}
				std::map<DeviceHandle, BufferRecord>::iterator bufIt = buffers.find(wanted);
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
		// PYROS_DRAW_SKIP_TRACE=1: say when a draw is dropped here, and why (it is silent otherwise).
		static const bool skipTrace = getenv("PYROS_DRAW_SKIP_TRACE") != NULL;
		if (currentRenderEncoder == NULL || currentPipeline == 0)
		{
			if (skipTrace) fprintf(stderr, "[draw-skip] DrawArrays(%u): %s (frame %llu)\n", count,
				currentRenderEncoder == NULL ? "no render encoder" : "no pipeline", (unsigned long long)deviceFrameNumber);
			return;
		}
		std::map<DeviceHandle, PipelineRecord>::iterator pipeIt = pipelines.find(currentPipeline);
		if (pipeIt == pipelines.end())
		{
			if (skipTrace) fprintf(stderr, "[draw-skip] DrawArrays(%u): pipeline %u not found\n", count, (unsigned)currentPipeline);
			return;
		}

		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;

			// DebugRenderer (and any other DrawArrays path with real vertex
			// input) records VBOs on the current VAO via BindArrayBuffer().
			// PostEffects uses noVertexInput and leaves vertexBuffers empty.
			if (currentVao != 0)
			{
				std::map<DeviceHandle, VaoRecord>::iterator vaoIt = vaos.find(currentVao);
				if (vaoIt != vaos.end())
				{
					for (size_t i = 0; i < vaoIt->second.vertexBuffers.size(); i++)
					{
						std::map<DeviceHandle, BufferRecord>::iterator vboIt = buffers.find(vaoIt->second.vertexBuffers[i]);
						if (vboIt == buffers.end() || vboIt->second.buffer == NULL)
							return;
						id<MTLBuffer> vbo = (__bridge id<MTLBuffer>)vboIt->second.buffer;
						[encoder setVertexBuffer:vbo offset:0 atIndex:(NSUInteger)(kFirstVertexBufferIndex + i)];
					}
				}
			}

			BindProgramUniformBuffers(pipeIt->second.programHandle);
			BindRaySceneIfRead(pipeIt->second.programHandle);
			FillUnboundSamplers(pipeIt->second.programHandle);
			[encoder drawPrimitives:(MTLPrimitiveType)nativeDrawType vertexStart:first vertexCount:count];
			encoderDrew = true;
		}
	}

	void MetalRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount)
	{
		(void)cmd;
		// (no triangles draws nothing - GL and Vulkan take it as a no-op; Metal's validation stops the
		// program. A mesh left empty - an emptied decal, a LOD with no faces - asks it.)
		if (indexCount == 0)
		{
			if (std::getenv("PYROS_EMPTY_DRAW_TRACE")) { static int n = 0; if (n++ < 3) { void* bt[24]; const int k = backtrace(bt, 24); fprintf(stderr, "[emptydraw]\n"); backtrace_symbols_fd(bt, k, 2); } }
			return;
		}
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
			BindRaySceneIfRead(pipeIt->second.programHandle);

			// __INDEX_C_TYPE__ (Global.h) is uint32 - matches
			// MTLIndexTypeUInt32, same as VulkanRenderDevice's identical
			// comment on its own DrawElements().
			FillUnboundSamplers(pipeIt->second.programHandle);
			id<MTLBuffer> ibo = (__bridge id<MTLBuffer>)iboIt->second.buffer;
			[encoder drawIndexedPrimitives:(MTLPrimitiveType)nativeDrawType
				indexCount:indexCount
				indexType:MTLIndexTypeUInt32
				indexBuffer:ibo
				indexBufferOffset:0];
			encoderDrew = true;
		}
	}

	void MetalRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount)
	{
		// Zero instances draws nothing; Vulkan and GL take it as a no-op, Metal
		// validation rejects it (an instanced component scaled down to none).
		if (instanceCount == 0 || indexCount == 0)
			return;
		(void)cmd;
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
			BindRaySceneIfRead(pipeIt->second.programHandle);

			FillUnboundSamplers(pipeIt->second.programHandle);
			id<MTLBuffer> ibo = (__bridge id<MTLBuffer>)iboIt->second.buffer;
			[encoder drawIndexedPrimitives:(MTLPrimitiveType)nativeDrawType
				indexCount:indexCount
				indexType:MTLIndexTypeUInt32
				indexBuffer:ibo
				indexBufferOffset:0
				instanceCount:instanceCount];
			encoderDrew = true;
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
		case 17: // BIND_VelocityFrameUniforms - see VulkanRenderDevice.h's case 17
		case 20: // BIND_VelocityObjectUniforms
		case 21: // BIND_AmbientLightUniforms - see VulkanRenderDevice.h's own
		         // case 21 for the full account. Every IRenderer instance
		         // drawing in one frame (main viewport, AxisHelper's gizmo,
		         // camera/thumbnail previews) shares this one buffer, so
		         // without a per-draw slot whichever instance rendered last
		         // silently won the binding for every draw already recorded
		         // that frame. Vulkan grew this case; this list was copied
		         // from that one before it did, and the omission is why the
		         // Metal editor's meshes dropped their light contribution
		         // for a frame at a time.
		case 22: // BIND_MaterialUniforms
		case 23: // BIND_ObjectLightCounts
		case 27: case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
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
		// How far it may grow, not where it starts - see
		// VulkanRenderDevice::CreateUniformBuffer() for the measurement
		// (1.1 GB reserved by 71 rings for 1.3 MB of writes a frame).
		const uint32 maxSlotCount = slotCount;
		if (dynamic)
		{
			uint32 start = (64u * 1024u) / alignedSlotSize;
			if (start < 16u) start = 16u;
			if (start < slotCount) slotCount = start;
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
			record.maxSlotCount = maxSlotCount;
			record.bindingPoint = bindingPoint;
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
		{
			BufferRecord &rec = it->second;
			rec.demandThisFrame++;
			uint64_t live = 1;
			for (uint32 i = 0; i < BufferRecord::kFramesRead; i++) live += rec.writes[i];
			if (live >= rec.slotCount && rec.slotCount < rec.maxSlotCount)
			{
				// Full: a larger buffer under the same handle. Draws read
				// the record when they are recorded, so the next one binds
				// the new buffer; those already recorded hold the old one.
				uint32 slots = rec.slotCount;
				while (slots < rec.maxSlotCount && slots < live * 2) slots *= 2;
				if (slots > rec.maxSlotCount) slots = rec.maxSlotCount;
				@autoreleasepool
				{
					id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
					id<MTLBuffer> larger = [mtlDevice newBufferWithLength:(NSUInteger)rec.alignedSlotSize * slots options:MTLResourceStorageModeShared];
					if (larger != nil)
					{
						memset(larger.contents, 0, (size_t)rec.alignedSlotSize * slots);
						if (rec.buffer != NULL) CFBridgingRelease(rec.buffer);
						rec.buffer = (void*)CFBridgingRetain(larger);
						rec.slotCount = slots;
						rec.currentSlot = slots - 1;
						for (uint32 i = 0; i < BufferRecord::kFramesRead; i++) rec.writes[i] = 0;
						rec.growths++;
					}
				}
			}
			rec.currentSlot = (rec.currentSlot + 1) % rec.slotCount;
			rec.writes[0]++;
		}
		UpdateUniformBuffer(buffer, 0, sizeBytes, data);
	}
	void MetalRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer)
	{
		for (std::map<DeviceHandle, std::map<uint32, DeviceHandle> >::iterator p = explicitBlockBuffers.begin(); p != explicitBlockBuffers.end(); ++p)
			for (std::map<uint32, DeviceHandle>::iterator b = p->second.begin(); b != p->second.end(); )
			{
				if (b->second == buffer) p->second.erase(b++);
				else ++b;
			}
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
		// METAL is additive to shaderc's predefined VULKAN (which every
		// shader's IO_LOCATION/UBO_BINDING/SAMPLER_BINDING macros key off,
		// and which Metal needs exactly as much as Vulkan does - explicit
		// bindings either way). It exists for the handful of places where
		// Metal's NDC/texture-origin combination genuinely differs from
		// both other backends - see IEffect's full-screen-quad vertex
		// shader, the only user today.
		return std::string("#version 450\n#define METAL 1\n") + definitions + std::string(" ") + shaderBody;
	}

	DeviceHandle MetalRenderDevice::CreateShaderStage(const uint32 engineShaderType)
	{
		ShaderStageRecord record;
		record.engineShaderType = engineShaderType;
		DeviceHandle handle = nextShaderStageHandle++;
		shaderStages[handle] = record;
		return handle;
	}

#ifdef METAL_SHADER_TOOLING
	// What SPIRV-Cross makes of a stage's SPIR-V - the MSL text and the index remaps it decided
	// on - kept by the SPIR-V and the stage's kind, in memory and beside the SPIR-V cache on disk.
	// It depends on nothing else, and it was most of a cached shader's cost: 158 ms of the 246 a
	// scene's programs took on Metal, every run (and a material first seen in play paid it then).
	namespace {
		const uint32 kMslCacheVersion = 1;    // (raised whenever the translation changes)
		struct MslTranslation
		{
			std::string msl;
			std::map<uint32, uint32> highBindingRemap, samplerIndexRemap, textureIndexRemap;
			int32 raySceneMslIndex = -1;
			uint32 workgroupSize[3] = { 1, 1, 1 };
		};
		uint64 MslKey(const std::vector<uint32> &spirv, const uint32 kind)
		{
			uint64 h = 14695981039346656037ull;
			const unsigned char* p = reinterpret_cast<const unsigned char*>(spirv.data());
			for (size_t i = 0; i < spirv.size() * 4; i++) { h ^= p[i]; h *= 1099511628211ull; }
			h ^= ((uint64)kind << 32) ^ kMslCacheVersion; h *= 1099511628211ull;
			return h;
		}
		std::mutex g_mslLock;
		std::map<uint64, MslTranslation> g_msl;
		std::string MslPath(const uint64 key)
		{
			char name[64];
			std::snprintf(name, sizeof(name), "%016llx.msl", (unsigned long long)key);
			return SpirvShaderCompiler::CacheDirectory() + "/" + name;
		}
		void PutMap(std::string &out, const std::map<uint32, uint32> &m)
		{
			const uint32 n = (uint32)m.size(); out.append((const char*)&n, 4);
			for (std::map<uint32, uint32>::const_iterator i = m.begin(); i != m.end(); ++i) { out.append((const char*)&i->first, 4); out.append((const char*)&i->second, 4); }
		}
		bool GetMap(const std::string &in, size_t &at, std::map<uint32, uint32> &m)
		{
			uint32 n = 0;
			if (at + 4 > in.size()) return false;
			memcpy(&n, in.data() + at, 4); at += 4;
			if (n > 4096 || at + (size_t)n * 8 > in.size()) return false;
			for (uint32 k = 0; k < n; k++) { uint32 a, b; memcpy(&a, in.data() + at, 4); memcpy(&b, in.data() + at + 4, 4); at += 8; m[a] = b; }
			return true;
		}
		// (headed by the SPIR-V's length and the whole SPIR-V, compared: a file that is not this
		// stage's - a hash that collides, a half-written one - is not used)
		bool LoadMsl(const uint64 key, const std::vector<uint32> &spirv, MslTranslation &t)
		{
			{
				std::lock_guard<std::mutex> g(g_mslLock);
				std::map<uint64, MslTranslation>::iterator it = g_msl.find(key);
				if (it != g_msl.end()) { t = it->second; return true; }
			}
			static const bool off = getenv("PYROS_SHADER_CACHE") && getenv("PYROS_SHADER_CACHE")[0] == '0';
			if (off) return false;
			std::ifstream f(MslPath(key).c_str(), std::ios::binary);
			// (and the game's own, shipped beside it: read, never written)
			if (!f && !SpirvShaderCompiler::ShippedCacheDirectory().empty())
			{
				char name[64];
				std::snprintf(name, sizeof(name), "%016llx.msl", (unsigned long long)key);
				f.open((SpirvShaderCompiler::ShippedCacheDirectory() + "/" + name).c_str(), std::ios::binary);
			}
			if (!f) return false;
			std::string in((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			size_t at = 0;
			uint32 words = 0;
			if (in.size() < 4) return false;
			memcpy(&words, in.data(), 4); at = 4;
			if (words != spirv.size() || at + (size_t)words * 4 > in.size() || memcmp(in.data() + at, spirv.data(), (size_t)words * 4) != 0) return false;
			at += (size_t)words * 4;
			MslTranslation r;
			if (!GetMap(in, at, r.highBindingRemap) || !GetMap(in, at, r.samplerIndexRemap) || !GetMap(in, at, r.textureIndexRemap)) return false;
			if (at + 16 + 4 > in.size()) return false;
			memcpy(&r.raySceneMslIndex, in.data() + at, 4); memcpy(r.workgroupSize, in.data() + at + 4, 12); at += 16;
			uint32 len = 0; memcpy(&len, in.data() + at, 4); at += 4;
			if (len == 0 || at + len != in.size()) return false;
			r.msl.assign(in.data() + at, len);
			t = r;
			std::lock_guard<std::mutex> g(g_mslLock);
			g_msl[key] = r;
			return true;
		}
		void StoreMsl(const uint64 key, const std::vector<uint32> &spirv, const MslTranslation &t)
		{
			{
				std::lock_guard<std::mutex> g(g_mslLock);
				g_msl[key] = t;
			}
			std::string out;
			const uint32 words = (uint32)spirv.size();
			out.append((const char*)&words, 4);
			out.append((const char*)spirv.data(), (size_t)words * 4);
			PutMap(out, t.highBindingRemap); PutMap(out, t.samplerIndexRemap); PutMap(out, t.textureIndexRemap);
			out.append((const char*)&t.raySceneMslIndex, 4); out.append((const char*)t.workgroupSize, 12);
			const uint32 len = (uint32)t.msl.size(); out.append((const char*)&len, 4); out.append(t.msl);
			std::error_code ec;
			std::filesystem::create_directories(SpirvShaderCompiler::CacheDirectory(), ec);
			const std::string path = MslPath(key), part = path + ".part";
			{
				std::ofstream f(part.c_str(), std::ios::binary | std::ios::trunc);
				if (!f) return;
				f.write(out.data(), (std::streamsize)out.size());
				if (!f) { f.close(); std::filesystem::remove(part, ec); return; }
			}
			std::filesystem::rename(part, path, ec);
			if (ec) std::filesystem::remove(part, ec);
		}
	}
#endif

	bool MetalRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog)
	{
		std::map<DeviceHandle, ShaderStageRecord>::iterator it = shaderStages.find(shader);
		if (it == shaderStages.end() || device == NULL)
			return false;

#ifdef METAL_SHADER_TOOLING
		// A switch, not a ternary on Fragment: `? Fragment : Vertex` sends
		// a compute stage down the vertex path, where it fails on
		// gl_GlobalInvocationID instead of on anything that names the
		// real problem. Same trap the shared ShaderCompiler.cpp had.
		uint32 spirvStage;
		switch (it->second.engineShaderType)
		{
		case ShaderType::FragmentShader: spirvStage = SpirvShaderStage::Fragment; break;
		case ShaderType::ComputeShader:  spirvStage = SpirvShaderStage::Compute;  break;
		default:                         spirvStage = SpirvShaderStage::Vertex;   break;
		}

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
			fprintf(stderr, "[METAL] SpirvShaderCompiler::Compile FAILED: %s\n", errorLog.c_str());
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
			const uint64 mslKey = MslKey(it->second.spirv, it->second.engineShaderType);
			MslTranslation cached;
			const bool mslCached = LoadMsl(mslKey, it->second.spirv, cached);
			if (mslCached)
			{
				mslSource = cached.msl;
				it->second.highBindingRemap = cached.highBindingRemap;
				it->second.samplerIndexRemap = cached.samplerIndexRemap;
				it->second.textureIndexRemap = cached.textureIndexRemap;
				it->second.raySceneMslIndex = cached.raySceneMslIndex;
				for (int k = 0; k < 3; k++) it->second.workgroupSize[k] = cached.workgroupSize[k];
			}
			else try
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
				//
				// The compacted indices are handed out from *below*
				// kFirstVertexBufferIndex, filling whichever of 0..23 this
				// stage doesn't already occupy with a passthrough binding.
				// They used to start AT kFirstVertexBufferIndex and count
				// up, which put the first remapped UBO at exactly the index
				// DrawElements()/DrawElementsInstanced() bind vertex
				// attribute buffer 0 to (kFirstVertexBufferIndex + 0) - one
				// shared MSL buffer-index namespace, so the two aliased.
				// BindProgramUniformBuffers() runs after the vertex-buffer
				// binds in both draw paths, so the UBO won, and every
				// [[stage_in]] attribute in that shader read the UBO's
				// matrices as vertex data. Only bit any shader declaring a
				// >=24 UBO binding in its VERTEX stage (fragment-stage
				// remaps land in the separate setFragmentBuffer: table and
				// were always harmless): secondpassPoint.glsl's/
				// secondpassSpot.glsl's PointVertParams/SpotVertParams
				// (33/34) and particleSystem.glsl's synthesized AutoUBO
				// (kFirstAutoUboBinding=43). Their light-volume/particle
				// meshes rasterized as garbage spikes fanning out of the
				// scene - a deferred point light lit thin slivers of
				// geometry instead of a sphere's worth, and the particle
				// quads never landed anywhere near their emitter.
				spirv_cross::ShaderResources resources = mslCompiler.get_shader_resources();
				std::set<uint32> passthroughBindings;
				// Every MSL buffer index handed out below, so the storage
				// buffer loop that follows can detect a collision rather
				// than quietly overwrite one binding with another.
				std::set<uint32> usedMslBufferIndices;
				for (size_t i = 0; i < resources.uniform_buffers.size(); i++)
				{
					uint32 b = mslCompiler.get_decoration(resources.uniform_buffers[i].id, spv::DecorationBinding);
					if (b < kFirstVertexBufferIndex)
						passthroughBindings.insert(b);
				}
				uint32 nextCompactHighBinding = 0;
				for (size_t i = 0; i < resources.uniform_buffers.size(); i++)
				{
					const spirv_cross::Resource &res = resources.uniform_buffers[i];
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
					if (binding.binding >= kFirstVertexBufferIndex)
					{
						while (nextCompactHighBinding < kFirstVertexBufferIndex &&
							passthroughBindings.find(nextCompactHighBinding) != passthroughBindings.end())
							nextCompactHighBinding++;
						if (nextCompactHighBinding >= kFirstVertexBufferIndex)
						{
							// 24 free slots against at most a handful of UBOs
							// per stage - unreachable in practice, but a wrong
							// index here is exactly the kind of silent
							// garbage-read this whole remap exists to prevent.
							errorLog = "Metal: no free MSL buffer index left to remap high UBO binding " + std::to_string(binding.binding);
							return false;
						}
						uint32 compact = nextCompactHighBinding++;
						it->second.highBindingRemap[binding.binding] = compact;
						binding.msl_buffer = compact;
					}
					else
					{
						binding.msl_buffer = binding.binding;
					}
					usedMslBufferIndices.insert(binding.msl_buffer);
					mslCompiler.add_msl_resource_binding(binding);
				}

				// Storage buffers (SSBOs) need the exact same treatment,
				// and for the exact same reason - measured, not assumed:
				// a compute kernel declaring `binding = 0` (Values) and
				// `binding = 1` (Params) came out of SPIRV-Cross as
				// buffer(1) and buffer(0) respectively, swapped, because
				// the default numbering follows declaration/id order and
				// not the binding decoration. The dispatch then read its
				// element count out of the data buffer and wrote its
				// results into the 4-byte params buffer. Nothing errored;
				// the output buffer simply came back untouched.
				//
				// MSL has ONE buffer index space shared by UBOs and
				// SSBOs, unlike GL (where GL_UNIFORM_BUFFER binding 0 and
				// GL_SHADER_STORAGE_BUFFER binding 0 are unrelated slots)
				// and unlike this engine's own GL backend. So a shader
				// using UBO binding 0 and SSBO binding 0 together is
				// portable to GL and NOT to Metal - which is also what
				// Vulkan says, since bindings must be unique within a
				// descriptor set there too. Rather than silently
				// overwriting one with the other, that is refused here
				// with a message that names both.
				for (size_t i = 0; i < resources.storage_buffers.size(); i++)
				{
					const spirv_cross::Resource &res = resources.storage_buffers[i];
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);

					// Metal allows [[buffer(0)]]..[[buffer(30)]]. A compute
					// stage has no vertex buffers, so the whole range is
					// available to it; a graphics stage must stay below
					// kFirstVertexBufferIndex or it would land on top of
					// one of the mesh's attribute buffers.
					const uint32 ceiling = (it->second.engineShaderType == ShaderType::ComputeShader)
						? kMaxMslBufferIndex + 1 : kFirstVertexBufferIndex;
					if (binding.binding >= ceiling)
					{
						errorLog = "Metal: storage buffer binding " + std::to_string(binding.binding)
							+ " is too high (max " + std::to_string(ceiling - 1) + " for this stage).";
						return false;
					}
					if (usedMslBufferIndices.find(binding.binding) != usedMslBufferIndices.end())
					{
						errorLog = "Metal: storage buffer binding " + std::to_string(binding.binding)
							+ " collides with a uniform buffer at the same binding. MSL has one buffer "
							"index space for both (GL does not), so they must be given distinct "
							"binding numbers - Vulkan requires this too.";
						return false;
					}
					binding.msl_buffer = binding.binding;
					usedMslBufferIndices.insert(binding.msl_buffer);
					mslCompiler.add_msl_resource_binding(binding);
				}

				// The ray scene (GL_EXT_ray_query's accelerationStructureEXT): Metal reads it
				// from a [[buffer(N)]] like any other buffer, so it takes the lowest index no
				// uniform or storage buffer of this stage holds - and MSL 3.0, the first whose
				// raytracing header SPIRV-Cross's output compiles against (2.3 and 2.4 reject
				// its intersection_params). One per stage: a second is refused.
				it->second.raySceneMslIndex = -1;
				if (!resources.acceleration_structures.empty())
				{
					if (resources.acceleration_structures.size() > 1)
					{
						errorLog = "Metal: more than one acceleration structure in a shader stage - the engine binds one ray scene.";
						return false;
					}
					mslOptions.set_msl_version(3, 0);
					mslCompiler.set_msl_options(mslOptions);
					uint32 free = 0;
					while (free < kFirstVertexBufferIndex && usedMslBufferIndices.find(free) != usedMslBufferIndices.end()) free++;
					if (free >= kFirstVertexBufferIndex)
					{
						errorLog = "Metal: no free MSL buffer index for the ray scene.";
						return false;
					}
					const spirv_cross::Resource &res = resources.acceleration_structures[0];
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
					binding.msl_buffer = free;
					usedMslBufferIndices.insert(free);
					mslCompiler.add_msl_resource_binding(binding);
					it->second.raySceneMslIndex = (int32)free;
				}

				// Compact both [[texture(N)]] and [[sampler(N)]] into
				// contiguous ranges, advancing by each resource's array
				// size. Texture passthrough used to work for single
				// samplers (N up to 127), but PyrosShader.glsl's
				// `uPointShadowMaps[4]` at engine binding 10 and
				// `uSpotShadowMaps[4]` at 11 make Metal reserve 10..13 and
				// 11..14 - overlapping ranges that make
				// newLibraryWithSource fail ("cannot reserve 'texture'
				// resource locations"). Sampler compaction already existed
				// for the 16-entry cap, but only advanced by 1 per
				// resource, so `array<sampler, 4> [[sampler(1)]]` and
				// `array<sampler, 4> [[sampler(2)]]` collided the same way.
				// Remember both remaps for SendUniformInt().
				uint32 nextSamplerIndex = 0;
				uint32 nextTextureIndex = 0;
				// Metal has 16 sampler states a stage (and 128 textures). A material that
				// lights by GI with shadows of every kind wants more - the point and spot
				// shadow arrays alone take eight. Over the limit, the textures always read
				// the same way give up their sampler state for one written into the
				// shader (a constexpr sampler): the shadow arrays first (compare
				// less-or-equal, linear, clamped - what SetTextureCompareMode makes),
				// then the GI's own pictures (linear, clamped). Their slot in the table
				// is kConstexprSamplerSlot: nothing is bound there.
				std::set<uint32> constexprSamplers;
				{
					uint32 wanted = 0;
					for (size_t i = 0; i < resources.sampled_images.size(); i++)
					{
						const spirv_cross::SPIRType &t = mslCompiler.get_type(resources.sampled_images[i].type_id);
						wanted += t.array.empty() ? 1 : (t.array[0] > 0 ? t.array[0] : 1);
					}
					for (int pass = 0; pass < 2 && wanted > kMaxMslSamplersPerStage; pass++)
						for (size_t i = 0; i < resources.sampled_images.size() && wanted > kMaxMslSamplersPerStage; i++)
						{
							const spirv_cross::Resource &res = resources.sampled_images[i];
							const spirv_cross::SPIRType &t = mslCompiler.get_type(res.type_id);
							const uint32 n = t.array.empty() ? 1 : (t.array[0] > 0 ? t.array[0] : 1);
							const std::string name = mslCompiler.get_name(res.id);
							const bool pick = (pass == 0) ? n > 1 : (name.compare(0, 5, "uDDGI") == 0 || name == "uBRDFLut");
							const uint32 b = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
							if (!pick || constexprSamplers.count(b)) continue;
							spirv_cross::MSLConstexprSampler cs;
							cs.min_filter = spirv_cross::MSL_SAMPLER_FILTER_LINEAR;
							cs.mag_filter = spirv_cross::MSL_SAMPLER_FILTER_LINEAR;
							cs.mip_filter = spirv_cross::MSL_SAMPLER_MIP_FILTER_NONE;
							cs.s_address = cs.t_address = cs.r_address = spirv_cross::MSL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
							if (t.image.depth)
							{
								cs.compare_enable = true;
								cs.compare_func = spirv_cross::MSL_SAMPLER_COMPARE_FUNC_LESS_EQUAL;
							}
							mslCompiler.remap_constexpr_sampler_by_binding(mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet), b, cs);
							constexprSamplers.insert(b);
							wanted -= n;
						}
				}
				for (size_t i = 0; i < resources.sampled_images.size(); i++)
				{
					const spirv_cross::Resource &res = resources.sampled_images[i];
					const spirv_cross::SPIRType &type = mslCompiler.get_type(res.type_id);
					uint32 arraySize = type.array.empty() ? 1 : (type.array[0] > 0 ? type.array[0] : 1);
					spirv_cross::MSLResourceBinding binding;
					binding.stage = mslCompiler.get_execution_model();
					binding.desc_set = mslCompiler.get_decoration(res.id, spv::DecorationDescriptorSet);
					binding.binding = mslCompiler.get_decoration(res.id, spv::DecorationBinding);
					const bool fixedSampler = constexprSamplers.count(binding.binding) != 0;
					if (nextTextureIndex + arraySize > kMaxMslTexturesPerStage)
					{
						errorLog = "Metal: more than " + std::to_string(kMaxMslTexturesPerStage) +
							" textures in one shader stage - no free MSL texture index for binding " +
							std::to_string(binding.binding);
						return false;
					}
					if (!fixedSampler && nextSamplerIndex + arraySize > kMaxMslSamplersPerStage)
					{
						errorLog = "Metal: more than " + std::to_string(kMaxMslSamplersPerStage) +
							" samplers in one shader stage - no free MSL sampler index for binding " +
							std::to_string(binding.binding);
						return false;
					}
					binding.msl_texture = nextTextureIndex;
					binding.msl_sampler = fixedSampler ? 0 : nextSamplerIndex;
					it->second.textureIndexRemap[binding.binding] = nextTextureIndex;
					it->second.samplerIndexRemap[binding.binding] = fixedSampler ? kConstexprSamplerSlot : nextSamplerIndex;
					nextTextureIndex += arraySize;
					if (!fixedSampler) nextSamplerIndex += arraySize;
					mslCompiler.add_msl_resource_binding(binding);
				}

				mslSource = mslCompiler.compile();
				if (mslSource.empty())
				{
					errorLog = "SPIRV-Cross produced empty MSL source";
					return false;
				}

				// Capture the kernel's own layout(local_size_x/y/z) while
				// the reflection object is still in scope. Metal's
				// dispatchThreadgroups:threadsPerThreadgroup: takes the
				// threadgroup size at the CALL site, unlike
				// glDispatchCompute which reads it from the compiled
				// shader - and Metal does not check the two against each
				// other, so a mismatch silently runs the wrong number of
				// threads per group. Reading it here means Dispatch()
				// never has to be told what the shader already declared.
				if (it->second.engineShaderType == ShaderType::ComputeShader)
				{
					const spirv_cross::SPIREntryPoint &entry =
						mslCompiler.get_entry_point("main", spv::ExecutionModelGLCompute);
					it->second.workgroupSize[0] = entry.workgroup_size.x > 0 ? entry.workgroup_size.x : 1;
					it->second.workgroupSize[1] = entry.workgroup_size.y > 0 ? entry.workgroup_size.y : 1;
					it->second.workgroupSize[2] = entry.workgroup_size.z > 0 ? entry.workgroup_size.z : 1;
				}
			}
			catch (const std::exception &e)
			{
				errorLog = std::string("SPIRV-Cross MSL compile failed: ") + e.what();
				return false;
			}
			if (!mslCached)
			{
				MslTranslation made;
				made.msl = mslSource;
				made.highBindingRemap = it->second.highBindingRemap;
				made.samplerIndexRemap = it->second.samplerIndexRemap;
				made.textureIndexRemap = it->second.textureIndexRemap;
				made.raySceneMslIndex = it->second.raySceneMslIndex;
				for (int k = 0; k < 3; k++) made.workgroupSize[k] = it->second.workgroupSize[k];
				StoreMsl(mslKey, it->second.spirv, made);
			}
			NSString* mslNSString = [NSString stringWithUTF8String:mslSource.c_str()];
			NSError* nsError = nil;
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLLibrary> library = [mtlDevice newLibraryWithSource:mslNSString options:nil error:&nsError];
			if (library == nil)
			{
				errorLog = nsError ? std::string([[nsError localizedDescription] UTF8String]) : "newLibraryWithSource failed";
				fprintf(stderr, "[METAL] newLibraryWithSource failed (shader=%u): %s\n", (uint32)shader, errorLog.c_str());
				return false;
			}
			// SPIRV-Cross names every stage's entry point "main0" by
			// default (MSL doesn't allow a function literally named
			// "main").
			id<MTLFunction> function = [library newFunctionWithName:@"main0"];
			if (function == nil)
			{
				NSArray<NSString*>* names = [library functionNames];
				errorLog = std::string("MSL library has no main0 entry point, available functions: ") + [[names description] UTF8String];
				fprintf(stderr, "[METAL] %s\n", errorLog.c_str());
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
		switch (shaderIt->second.engineShaderType)
		{
		case ShaderType::FragmentShader:
			progIt->second.fragmentShader = shader;
			break;
		case ShaderType::ComputeShader:
			// Marks the whole program as compute. Metal enforces the same
			// exclusivity GL does - a compute function cannot be part of a
			// render pipeline - so there is no valid program holding both.
			progIt->second.computeShader = shader;
			progIt->second.isCompute = true;
			break;
		default:
			progIt->second.vertexShader = shader;
			break;
		}
	}
	bool MetalRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator it = programs.find(program);
		if (it == programs.end())
			return false;

		// A compute program has one stage and no vertex input, so none of
		// the reflection below (attribute locations for the
		// MTLVertexDescriptor, per-stage sampler/texture index merging
		// across two stages) applies to it. Linking is really just
		// "is the kernel compiled?" - the MTLComputePipelineState that
		// would be the actual link step is built later, by
		// CreateComputePipeline, because it needs no state from here.
		if (it->second.isCompute)
		{
			std::map<DeviceHandle, ShaderStageRecord>::iterator cs = shaderStages.find(it->second.computeShader);
			if (cs == shaderStages.end() || cs->second.function == NULL)
			{
				errorLog = "Compute program has no compiled compute stage";
				return false;
			}
			return true;
		}

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
		it->second.raySceneFragmentIndex = fs->second.raySceneMslIndex;
		// Kept per-stage rather than merged - see ProgramRecord::samplerMslIndex.
		it->second.samplerMslIndex[0] = vs->second.samplerIndexRemap;
		it->second.samplerMslIndex[1] = fs->second.samplerIndexRemap;
		it->second.textureMslIndex[0] = vs->second.textureIndexRemap;
		it->second.textureMslIndex[1] = fs->second.textureIndexRemap;
		it->second.samplerArraySizes.clear();
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
					it->second.samplerArraySizes[res.binding] = res.arraySize;
					it->second.samplerKinds[res.binding] = (res.isCube ? kFallbackCube : 0) | (res.isDepthCompare ? kFallbackDepth : 0);
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
	void MetalRenderDevice::DeleteProgram(const DeviceHandle program) { programs.erase(program); explicitBlockBuffers.erase(program); }

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

		uint32 arraySize = 1;
		std::map<uint32, uint32>::iterator arrIt = progIt->second.samplerArraySizes.find((uint32)handle);
		if (arrIt != progIt->second.samplerArraySizes.end())
			arraySize = arrIt->second;
		// Bind every declared array element. IRenderer may send fewer units
		// than the shader array (or a single fallback unit when there are
		// no shadow casters) - pad the remaining slots with the last valid
		// texture so Metal/Vulkan both see a complete array, matching
		// VulkanRenderDevice::SendUniformInt()'s fill behaviour.
		// The program's own array, no more and no less: fewer units than it declares repeat the last
		// (below); more are ignored, as GL ignores them. (More were bound past the array's end - into
		// the texture slots of whatever the program declares next.)
		uint32 bindCount = arraySize;
		if (bindCount == 0)
			return;

		@autoreleasepool
		{
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			const uint32 stageMask = maskIt->second;
			for (uint32 elem = 0; elem < bindCount; elem++)
			{
				uint32 unitIndex = elem < count ? elem : (count - 1);
				std::map<uint32, DeviceHandle>::iterator unitIt = textureUnitBindings.find((uint32)data[unitIndex]);
				if (unitIt == textureUnitBindings.end())
					continue;
				std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(unitIt->second);
				if (texIt == textures.end() || texIt->second.texture == NULL)
					continue;

				id<MTLTexture> tex = (__bridge id<MTLTexture>)texIt->second.texture;
				RebuildSamplerIfDirty(texIt->second);
				id<MTLSamplerState> sampler = (__bridge id<MTLSamplerState>)texIt->second.samplerState;
				// A texture of another kind than the slot is declared (a unit left holding a point
				// light's cube map, read through a spot light's 2D shadow slot - IRenderer pads unused
				// shadow slots with a spare unit, whatever it holds): Metal draws it undefined, and its
				// validation stops the frame. The slot's own blank instead - it is never read there.
				{
					std::map<uint32, uint32>::iterator kIt = progIt->second.samplerKinds.find((uint32)handle);
					if (kIt != progIt->second.samplerKinds.end())
					{
						const bool wantCube = (kIt->second & kFallbackCube) != 0;
						const bool isCube = tex.textureType == MTLTextureTypeCube;
						if (wantCube != isCube)
						{
							id<MTLTexture> fb = (__bridge id<MTLTexture>)GetFallbackTexture(kIt->second);
							if (fb != nil)
							{
								tex = fb;
								sampler = (__bridge id<MTLSamplerState>)((kIt->second & kFallbackDepth) ? fallbackCompareSampler : fallbackSampler);
							}
						}
					}
				}
				for (int stage = 0; stage < 2; stage++)
				{
					if (!(stageMask & (1u << stage)))
						continue;
					std::map<uint32, uint32>::iterator tIt = progIt->second.textureMslIndex[stage].find((uint32)handle);
					std::map<uint32, uint32>::iterator sIt = progIt->second.samplerMslIndex[stage].find((uint32)handle);
					NSUInteger textureIndex = ((tIt != progIt->second.textureMslIndex[stage].end()) ? (NSUInteger)tIt->second : (NSUInteger)handle) + elem;
					const bool fixedSampler = sIt != progIt->second.samplerMslIndex[stage].end() && sIt->second == kConstexprSamplerSlot;
					NSUInteger samplerIndex = ((sIt != progIt->second.samplerMslIndex[stage].end()) ? (NSUInteger)sIt->second : (NSUInteger)handle) + elem;
					// (a sampler written into the shader takes no slot: see CompileShaderStage)
					id<MTLSamplerState> stageSampler = fixedSampler ? nil : sampler;
					if (stage == 0)
					{
						[encoder setVertexTexture:tex atIndex:textureIndex];
						if (stageSampler != nil) [encoder setVertexSamplerState:stageSampler atIndex:samplerIndex];
					}
					else
					{
						[encoder setFragmentTexture:tex atIndex:textureIndex];
						if (stageSampler != nil) [encoder setFragmentSamplerState:stageSampler atIndex:samplerIndex];
					}
					encoderBoundTextures.insert(((uint64)stage << 32) | (uint64)textureIndex);
					if (tex.textureType == MTLTextureTypeCube) encoderBoundCubes.insert(((uint64)stage << 32) | (uint64)textureIndex);
					else encoderBoundCubes.erase(((uint64)stage << 32) | (uint64)textureIndex);
				}
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

	void *MetalRenderDevice::GetImGuiTextureID(const DeviceHandle texture, const uint32 engineTextureType)
	{
		// ImGui's Metal backend takes an id<MTLTexture> as the ImTextureID
		// directly. Bridged without transferring ownership - the record
		// keeps the reference, and this pointer is only used to draw during
		// the frame it was fetched in.
		if (texture == 0 || engineTextureType != TextureType::Texture)
			return NULL;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(texture);
		if (it == textures.end() || it->second.texture == NULL)
			return NULL;
		// Multisample targets cannot be sampled by ImGui's shader.
		if (it->second.samples > 1 || it->second.isCubemap)
			return NULL;
		return it->second.texture;
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

	bool MetalRenderDevice::SupportsCompressedFormat(const uint32 format) const
	{
		if (device == NULL || format < 1 || format > 3) return false;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			if (@available(macOS 11.0, *)) return [mtlDevice supportsBCTextureCompression];
		}
		return false;
	}

	void MetalRenderDevice::UploadCompressedTexture2D(const uint32 format, const uint32 width, const uint32 height, const std::vector<std::vector<uchar> > &levels)
	{
		if (currentlyConfiguringTexture == 0 || device == NULL || width == 0 || height == 0 || levels.empty()) return;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end()) return;
		TextureRecord &tex = it->second;
		const MTLPixelFormat pf = format == 1 ? MTLPixelFormatBC1_RGBA : (format == 3 ? MTLPixelFormatBC5_RGUnorm : MTLPixelFormatBC3_RGBA);
		const uint32 blockBytes = format == 1 ? 8u : 16u;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			if (tex.texture != NULL) { CFBridgingRelease(tex.texture); tex.texture = NULL; }
			MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
			texDesc.textureType = MTLTextureType2D;
			texDesc.pixelFormat = pf;
			texDesc.width = width;
			texDesc.height = height;
			texDesc.mipmapLevelCount = levels.size();
			// (sampled only: blocks are not something to draw into)
			texDesc.usage = MTLTextureUsageShaderRead;
			texDesc.storageMode = MTLStorageModeShared;
			id<MTLTexture> newTex = [mtlDevice newTextureWithDescriptor:texDesc];
			if (newTex == nil) { fprintf(stderr, "MetalRenderDevice::UploadCompressedTexture2D: newTextureWithDescriptor failed\n"); return; }
			uint32 w = width, h = height;
			for (size_t l = 0; l < levels.size(); l++)
			{
				const uint32 bw = (w + 3) / 4, bh = (h + 3) / 4;
				if (levels[l].size() >= (size_t)bw * bh * blockBytes)
					[newTex replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:l slice:0 withBytes:levels[l].data() bytesPerRow:(NSUInteger)bw * blockBytes bytesPerImage:0];
				w = std::max(1u, w / 2); h = std::max(1u, h / 2);
			}
			tex.texture = (void*)CFBridgingRetain(newTex);
			tex.width = width;
			tex.height = height;
			tex.isCubemap = false;
			tex.hasMipmap = levels.size() > 1;
			tex.mipsGenerated = true;
		}
	}

	uint32 MetalRenderDevice::GetMaxSamples() const
	{
		if (device == NULL)
			return 1;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			for (uint32 n = 8; n > 1; n >>= 1)
				if ([mtlDevice supportsTextureSampleCount:n])
					return n;
		}
		return 1;
	}

	// Never uploaded into, like GL's glTexImage2DMultisample(): a
	// multisample texture is only ever filled by rendering into it, and
	// read back either by a resolve (BlitFramebuffer()) or by a shader's
	// texelFetch on a sampler2DMS.
	void MetalRenderDevice::UploadTexture2DMultisample(const uint32 target, const uint32 samples, const uint32 internalFormat, const uint32 width, const uint32 height)
	{
		(void)target;
		if (currentlyConfiguringTexture == 0 || device == NULL || width == 0 || height == 0)
			return;
		std::map<DeviceHandle, TextureRecord>::iterator it = textures.find(currentlyConfiguringTexture);
		if (it == textures.end())
			return;
		TextureRecord &tex = it->second;

		uint32 wantedSamples = 1;
		const uint32 maxSamples = GetMaxSamples();
		for (uint32 n = maxSamples; n > 1; n >>= 1)
			if (n <= samples) { wantedSamples = n; break; }

		const MTLPixelFormat wantedFormat = (MTLPixelFormat)internalFormat;
		if (tex.texture != NULL && tex.width == width && tex.height == height && tex.samples == wantedSamples
			&& ((__bridge id<MTLTexture>)tex.texture).pixelFormat == wantedFormat)
			return;

		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			if (tex.texture != NULL) { CFBridgingRelease(tex.texture); tex.texture = NULL; }
			MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
			texDesc.textureType = wantedSamples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
			texDesc.pixelFormat = wantedFormat;
			texDesc.width = width;
			texDesc.height = height;
			texDesc.sampleCount = wantedSamples;
			texDesc.mipmapLevelCount = 1;
			texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
			// Private: the CPU never touches it, and a shared multisample
			// texture is not allowed on Apple Silicon.
			texDesc.storageMode = MTLStorageModePrivate;
			id<MTLTexture> newTex = [mtlDevice newTextureWithDescriptor:texDesc];
			if (newTex == nil)
			{
				fprintf(stderr, "MetalRenderDevice::UploadTexture2DMultisample: newTextureWithDescriptor failed (%ux%u, samples=%u)\n", width, height, wantedSamples);
				return;
			}
			tex.texture = (void*)CFBridgingRetain(newTex);
			tex.width = width;
			tex.height = height;
			tex.samples = wantedSamples;
			tex.isCubemap = false;
			tex.hasMipmap = false;
			tex.mipsGenerated = false;
		}
	}

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
			// A Metal texture's level count is fixed at creation; with one
			// level there is nothing to generate. Asking anyway is invalid
			// (Metal API validation aborts) and cost a blocking GPU round trip.
			if (mtlTex.mipmapLevelCount <= 1)
				return;
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
			// Anisotropic filtering - VulkanRenderDevice's identical choice: only
			// what is mipmapped and read smoothly, never a shadow map.
			if (tex.hasMipmap && tex.mipsGenerated && minFilter == MTLSamplerMinMagFilterLinear && magFilter == MTLSamplerMinMagFilterLinear && !tex.compareModeEnabled)
				samplerDesc.maxAnisotropy = (NSUInteger)IRenderDevice::TextureAnisotropy();
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

	// Real readback, not a stub. Both of these returning nothing meant any
	// caller reading a texture back on Metal silently got an empty buffer -
	// which is not a neutral failure: the point-shadow cube map viewer
	// reported every face as a uniform 1.0 and looked like evidence that
	// Metal's shadow map was empty, when in fact nothing had been read at
	// all. `target` selects the cube face, same contract as GL's
	// glGetTexImage and VulkanRenderDevice::ReadTexturePixels().
	void MetalRenderDevice::ReadTexturePixels(const uint32 target, const uint32 level, const uint32 format, const uint32 type, void *outBuffer)
	{
		(void)format; (void)type;
		if (outBuffer == NULL || device == NULL)
			return;
		std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(currentlyConfiguringTexture);
		if (texIt == textures.end() || texIt->second.texture == NULL)
			return;

		@autoreleasepool
		{
			id<MTLDevice> dev = (__bridge id<MTLDevice>)device;
			id<MTLTexture> tex = (__bridge id<MTLTexture>)texIt->second.texture;
			const uint32 w = texIt->second.width, h = texIt->second.height;
			const uint32 bytesPerPixel = BytesPerPixelMSL((MTLPixelFormat)tex.pixelFormat);
			if (w == 0 || h == 0 || bytesPerPixel == 0)
				return;
			const NSUInteger bytesPerRow = (NSUInteger)w * bytesPerPixel;
			const NSUInteger total = bytesPerRow * h;

			id<MTLBuffer> staging = [dev newBufferWithLength:total options:MTLResourceStorageModeShared];
			if (staging == nil)
				return;

			const NSUInteger slice = (texIt->second.isCubemap && target >= kCubemapFaceTargetBase)
				? (NSUInteger)(target - kCubemapFaceTargetBase) : 0;

			id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
			id<MTLCommandBuffer> cmd = [queue commandBuffer];
			id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
			[blit copyFromTexture:tex
					  sourceSlice:slice
					  sourceLevel:(NSUInteger)level
					 sourceOrigin:MTLOriginMake(0, 0, 0)
					   sourceSize:MTLSizeMake(w, h, 1)
						 toBuffer:staging
				destinationOffset:0
		   destinationBytesPerRow:bytesPerRow
		 destinationBytesPerImage:total];
			[blit endEncoding];
			[cmd commit];
			[cmd waitUntilCompleted];

			memcpy(outBuffer, [staging contents], total);
		}
	}

	// Mirrors GLRenderDevice::GetTextureDataSize()'s contract - the byte
	// size the caller should size its buffer to - but switches on
	// MTLPixelFormat, since that is what TranslateTextureFormat() packs
	// into `internalFormat` on this backend.
	uint32 MetalRenderDevice::GetTextureDataSize(const uint32 nativeInternalFormat, const uint32 width, const uint32 height)
	{
		return BytesPerPixelMSL((MTLPixelFormat)nativeInternalFormat) * width * height;
	}

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
		boundFboAwaitingAttachment = false;
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
			encoderBoundTextures.clear(); encoderBoundCubes.clear();
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
		// Re-attaching a *different* target to a slot mid-session has to
		// restart the encoder. A Metal encoder is bound to the slice it was
		// created with, and recording this attach without ending it left
		// all six of a point light's cube faces drawing into whichever
		// slice happened to be current when the first face began: one face
		// held every face's geometry (the caster showed up in +X instead of
		// -Y) and the other five stayed at their clear value. The old
		// "picked up the next time BeginRenderEncoderForTarget() runs"
		// assumption only held while nothing re-attached mid-pass, which
		// was true until point-light shadows moved to a colour cube map.
		const FBOAttachmentRef *existing = NULL;
		if (nativeAttachmentFormat == FrameBufferAttachmentFormat::Depth_Attachment)
			existing = &it->second.depthAttachment;
		else
		{
			std::map<uint32, FBOAttachmentRef>::iterator cIt = it->second.colorAttachments.find(nativeAttachmentFormat);
			if (cIt != it->second.colorAttachments.end())
				existing = &cIt->second;
		}
		// Any attach while an encoder is already open for this FBO has to
		// restart it, not just one that changes the recorded target. The
		// first face of a point light's cube map re-attaches
		// CubemapPositive_X - exactly what FrameBuffer::Init() already
		// recorded - so a "did the target change" test says no and face 0
		// inherits whatever encoder was left open, which is the *previous*
		// frame's face 5. That is why +X came back full of zeroes after the
		// other five faces started rendering correctly.
		(void)existing;
		const bool retargeting = currentRenderEncoder != NULL || boundFboAwaitingAttachment;

		if (nativeAttachmentFormat == FrameBufferAttachmentFormat::Depth_Attachment)
			it->second.depthAttachment = ref;
		else if (nativeAttachmentFormat <= FrameBufferAttachmentFormat::Color_Attachment15)
			it->second.colorAttachments[nativeAttachmentFormat] = ref;
		else
			fprintf(stderr, "MetalRenderDevice::AttachFramebufferTexture2D: Stencil_Attachment is not implemented - attachment format %u ignored\n", nativeAttachmentFormat);

		if (retargeting)
		{
			EndCurrentRenderEncoderIfOpen();
			BeginRenderEncoderForTarget(currentBoundFBO);
		}
	}
	// A renderbuffer here is just a texture nobody samples (see
	// CreateRenderbuffer()), so this is the same attach as a real texture -
	// mirrors VulkanRenderDevice::AttachFramebufferRenderbuffer(), which
	// forwards to its own AttachFramebufferTexture2D() for the same reason.
	void MetalRenderDevice::AttachFramebufferRenderbuffer(const uint32 nativeAttachmentFormat, const DeviceHandle renderbuffer)
	{
		AttachFramebufferTexture2D(nativeAttachmentFormat, 1, renderbuffer, true);
	}
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

	// Metal has no renderbuffer object - a write-only attachment is just an
	// MTLTexture nobody binds to a sampler - so these map straight onto the
	// texture machinery above, exactly as VulkanRenderDevice's equivalents
	// map onto its own. Left as stubs, an FBO built with a renderbuffer
	// depth attachment (FrameBuffer::AddAttach()'s RenderBuffer path, e.g.
	// IslandDemo's water *reflection* target - its refraction target uses a
	// depth texture instead, because the water shader samples that one) got
	// no depth attachment at all on Metal: the reflection pass then had
	// nothing to depth-test against and drew in submission order.
	DeviceHandle MetalRenderDevice::CreateRenderbuffer() { return CreateTextureObject(); }
	void MetalRenderDevice::DestroyRenderbuffer(const DeviceHandle rbo) { DestroyTextureObject(rbo); }
	void MetalRenderDevice::BindRenderbuffer(const DeviceHandle rbo) { currentlyConfiguringTexture = rbo; }
	// Real MTLPixelFormat values, not GL tokens - matches every other
	// Translate*Format() on this backend. RenderbufferStorage() below feeds
	// the result straight into UploadTexture2D() as its internalFormat.
	uint32 MetalRenderDevice::TranslateRenderbufferFormat(const uint32 engineDataType)
	{
		switch (engineDataType)
		{
		case RenderBufferDataType::Depth:
		case RenderBufferDataType::Depth_Multisample:
			return (uint32)MTLPixelFormatDepth32Float;
		case RenderBufferDataType::Stencil:
		case RenderBufferDataType::Stencil_Multisample:
			return (uint32)MTLPixelFormatStencil8;
		case RenderBufferDataType::RGBA:
		case RenderBufferDataType::RGBA_Multisample:
		default:
			return (uint32)MTLPixelFormatRGBA8Unorm;
		}
	}
	void MetalRenderDevice::RenderbufferStorage(const uint32 nativeFormat, const uint32 width, const uint32 height)
	{
		UploadTexture2D(0, 0, nativeFormat, width, height, 0, 0, NULL, false);
	}
	void MetalRenderDevice::RenderbufferStorageMultisample(const uint32 nativeFormat, const uint32 samples, const uint32 width, const uint32 height)
	{
		UploadTexture2DMultisample(0, samples, nativeFormat, width, height);
	}

	// Nothing to toggle, as on Vulkan: the sample count belongs to the
	// attachments, and CreatePipeline() reads it from them.
	void MetalRenderDevice::SetMultisampleEnabled(const bool enabled) { (void)enabled; }

	// GL's glBlitFramebuffer(): source is the Read-bound FBO, destination
	// the Write-bound one. A multisample source resolves through a render
	// pass whose store action is the resolve, which is the only way Metal
	// resolves (there is no resolve command) - and it does depth as well
	// as colour, which Vulkan's vkCmdResolveImage does not. Anything else
	// is a straight copy on a blit encoder.
	//
	// Binding the destination for Write already opened an encoder that
	// clears it. That encoder is ended here and not reopened: reopening
	// would clear the result straight back out, and nothing draws into a
	// blit destination before it is unbound anyway.
	void MetalRenderDevice::BlitFramebuffer(const uint32 srcX0, const uint32 srcY0, const uint32 srcX1, const uint32 srcY1, const uint32 dstX0, const uint32 dstY0, const uint32 dstX1, const uint32 dstY1, const uint32 engineMask, const uint32 engineFilter)
	{
		(void)engineFilter;
		std::map<DeviceHandle, FBORecord>::iterator srcFbo = fboRecords.find(currentReadFBO);
		std::map<DeviceHandle, FBORecord>::iterator dstFbo = fboRecords.find(currentBoundFBO);
		if (srcFbo == fboRecords.end() || dstFbo == fboRecords.end())
		{
			fprintf(stderr, "MetalRenderDevice::BlitFramebuffer: needs a Read-bound source (%u) and a Write-bound destination (%u)\n", currentReadFBO, currentBoundFBO);
			return;
		}
		const bool depth = (engineMask == FBOBufferBit::Depth);
		if (engineMask == FBOBufferBit::Stencil)
		{
			fprintf(stderr, "MetalRenderDevice::BlitFramebuffer: stencil is not implemented on this backend\n");
			return;
		}
		DeviceHandle srcId = 0, dstId = 0;
		if (depth)
		{
			srcId = srcFbo->second.depthAttachment.texture;
			dstId = dstFbo->second.depthAttachment.texture;
		}
		else
		{
			if (!srcFbo->second.colorAttachments.empty()) srcId = srcFbo->second.colorAttachments.begin()->second.texture;
			if (!dstFbo->second.colorAttachments.empty()) dstId = dstFbo->second.colorAttachments.begin()->second.texture;
		}
		std::map<DeviceHandle, TextureRecord>::iterator srcTex = textures.find(srcId);
		std::map<DeviceHandle, TextureRecord>::iterator dstTex = textures.find(dstId);
		if (srcId == 0 || dstId == 0 || srcTex == textures.end() || dstTex == textures.end()
			|| srcTex->second.texture == NULL || dstTex->second.texture == NULL)
		{
			fprintf(stderr, "MetalRenderDevice::BlitFramebuffer: source or destination has no matching attachment\n");
			return;
		}
		const uint32 w = srcX1 - srcX0, h = srcY1 - srcY0;
		if (w != dstX1 - dstX0 || h != dstY1 - dstY0)
		{
			fprintf(stderr, "MetalRenderDevice::BlitFramebuffer: scaling blits are not implemented (%ux%u -> %ux%u)\n", w, h, dstX1 - dstX0, dstY1 - dstY0);
			return;
		}

		EndCurrentRenderEncoderIfOpen();
		bool ownCommandBuffer = false;
		@autoreleasepool
		{
			if (currentCommandBuffer == NULL)
			{
				id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
				currentCommandBuffer = (void*)CFBridgingRetain([queue commandBuffer]);
				ownCommandBuffer = true;
			}
			id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
			id<MTLTexture> src = (__bridge id<MTLTexture>)srcTex->second.texture;
			id<MTLTexture> dst = (__bridge id<MTLTexture>)dstTex->second.texture;

			if (srcTex->second.samples > 1 && dstTex->second.samples == 1)
			{
				MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
				if (depth)
				{
					rpd.depthAttachment.texture = src;
					rpd.depthAttachment.loadAction = MTLLoadActionLoad;
					rpd.depthAttachment.storeAction = MTLStoreActionMultisampleResolve;
					rpd.depthAttachment.resolveTexture = dst;
					// Sample 0, not an average: a blend of two depths is a
					// surface that is not there. Same answer GL gives.
					rpd.depthAttachment.depthResolveFilter = MTLMultisampleDepthResolveFilterSample0;
				}
				else
				{
					rpd.colorAttachments[0].texture = src;
					rpd.colorAttachments[0].loadAction = MTLLoadActionLoad;
					rpd.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;
					rpd.colorAttachments[0].resolveTexture = dst;
				}
				[[cmdBuf renderCommandEncoderWithDescriptor:rpd] endEncoding];
			}
			else
			{
				id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
				[blit copyFromTexture:src sourceSlice:0 sourceLevel:0
					sourceOrigin:MTLOriginMake(srcX0, srcY0, 0)
					sourceSize:MTLSizeMake(w, h, 1)
					toTexture:dst destinationSlice:0 destinationLevel:0
					destinationOrigin:MTLOriginMake(dstX0, dstY0, 0)];
				[blit endEncoding];
			}

			if (ownCommandBuffer)
			{
				[cmdBuf commit];
				[cmdBuf waitUntilCompleted];
				CFBridgingRelease(currentCommandBuffer);
				currentCommandBuffer = NULL;
			}
		}
		// (back into the swapchain's pass if that is what the blit ended - see CopyDepthTexture)
		ResumeSwapchainPass();
	}
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
	const char* MetalRenderDevice::TemporalUpscalerId() const
	{
#if PYROS_HAVE_METALFX
		if (@available(macOS 13.0, *))
		{
			// (PYROS_NO_METALFX=1 to be without it)
			static const bool on = getenv("PYROS_NO_METALFX") == NULL;
			if (on && device != NULL && [MTLFXTemporalScalerDescriptor supportsDevice:(__bridge id<MTLDevice>)device]) return "metalfx";
		}
#endif
		return "";
	}

	bool MetalRenderDevice::RunTemporalUpscale(const TemporalUpscale &frame)
	{
#if PYROS_HAVE_METALFX
		if (@available(macOS 13.0, *))
		{
			if (currentCommandBuffer == NULL || device == NULL) return false;
			std::map<DeviceHandle, TextureRecord>::iterator c = textures.find(frame.color), d = textures.find(frame.depth),
				m = textures.find(frame.motion), o = textures.find(frame.output);
			if (c == textures.end() || d == textures.end() || m == textures.end() || o == textures.end()
				|| c->second.texture == NULL || d->second.texture == NULL || m->second.texture == NULL || o->second.texture == NULL)
				return false;
			const bool reopen = currentRenderEncoder != NULL;
			const DeviceHandle reopenFbo = currentBoundFBO;
			EndCurrentRenderEncoderIfOpen();
			@autoreleasepool
			{
				id<MTLTexture> colorTex = (__bridge id<MTLTexture>)c->second.texture;
				id<MTLTexture> depthTex = (__bridge id<MTLTexture>)d->second.texture;
				id<MTLTexture> motionTex = (__bridge id<MTLTexture>)m->second.texture;
				id<MTLTexture> outputTex = (__bridge id<MTLTexture>)o->second.texture;
				const uint32 key[8] = { frame.renderWidth, frame.renderHeight, frame.outputWidth, frame.outputHeight,
					(uint32)colorTex.pixelFormat, (uint32)depthTex.pixelFormat, (uint32)motionTex.pixelFormat, (uint32)outputTex.pixelFormat };
				bool fresh = false;
				if (fxScaler == NULL || memcmp(key, fxKey, sizeof(key)) != 0)
				{
					if (fxScaler != NULL) { CFBridgingRelease(fxScaler); fxScaler = NULL; }
					MTLFXTemporalScalerDescriptor* desc = [[MTLFXTemporalScalerDescriptor alloc] init];
					desc.inputWidth = frame.renderWidth; desc.inputHeight = frame.renderHeight;
					desc.outputWidth = frame.outputWidth; desc.outputHeight = frame.outputHeight;
					desc.colorTextureFormat = colorTex.pixelFormat;
					desc.depthTextureFormat = depthTex.pixelFormat;
					desc.motionTextureFormat = motionTex.pixelFormat;
					desc.outputTextureFormat = outputTex.pixelFormat;
					// (what it is given has been through the whole post chain:
					// nothing to expose, nothing brighter than white)
					desc.autoExposureEnabled = NO;
					id<MTLFXTemporalScaler> made = [desc newTemporalScalerWithDevice:(__bridge id<MTLDevice>)device];
					memcpy(fxKey, key, sizeof(key));
					if (made == nil)
					{
						if (!fxSaidWhy) { fxSaidWhy = true; fprintf(stderr, "MetalFX: no temporal scaler for %ux%u -> %ux%u with these texture formats (%u %u %u -> %u)\n", key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]); }
						return false;
					}
					if ((outputTex.usage & made.outputTextureUsage) != made.outputTextureUsage)
					{
						// The scaler writes its output as a compute pass would, which
						// a texture made as a render target does not allow. The one
						// behind this handle is made again, the same but for that -
						// nothing has been drawn into it that matters (it is the
						// upscaler's to fill, every frame).
						MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:outputTex.pixelFormat
							width:outputTex.width height:outputTex.height mipmapped:NO];
						td.usage = outputTex.usage | made.outputTextureUsage;
						td.storageMode = MTLStorageModePrivate;
						id<MTLTexture> again = [(__bridge id<MTLDevice>)device newTextureWithDescriptor:td];
						if (again == nil)
						{
							if (!fxSaidWhy) { fxSaidWhy = true; fprintf(stderr, "MetalFX: could not make an output texture the scaler can write\n"); }
							return false;
						}
						CFBridgingRelease(o->second.texture);
						o->second.texture = (void*)CFBridgingRetain(again);
						outputTex = again;
					}
					fxScaler = (void*)CFBridgingRetain(made);
					fresh = true;
				}
				id<MTLFXTemporalScaler> scaler = (__bridge id<MTLFXTemporalScaler>)fxScaler;
				scaler.colorTexture = colorTex;
				scaler.depthTexture = depthTex;
				scaler.motionTexture = motionTex;
				scaler.outputTexture = outputTex;
				scaler.exposureTexture = nil;
				scaler.preExposure = 1.f;
				scaler.jitterOffsetX = frame.jitterX;
				scaler.jitterOffsetY = frame.jitterY;
				scaler.motionVectorScaleX = frame.motionScaleX;
				scaler.motionVectorScaleY = frame.motionScaleY;
				scaler.depthReversed = NO;
				scaler.reset = (frame.reset || fresh) ? YES : NO;
				@try { [scaler encodeToCommandBuffer:(__bridge id<MTLCommandBuffer>)currentCommandBuffer]; }
				@catch (NSException* e) { fprintf(stderr, "MetalFX: encode threw: %s\n", [[e reason] UTF8String]); return false; }
			}
			// The pass that was open when this was asked for is open again after
			// it (an encoder, once ended, is not resumed: whatever is drawn
			// next would otherwise be drawn into nothing).
			if (reopen && reopenFbo == 0) BindFramebuffer(FBOAccess::Read_Write, 0, true);
			return true;
		}
#endif
		(void)frame;
		return false;
	}

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
		// The copy had to end the pass it found open. When that was the swapchain's, the next draw
		// to the screen found no encoder and was dropped: a chain of one post effect, after the
		// capture's depth copy, drew nothing at all and the frame was the clear colour.
		ResumeSwapchainPass();
	}

	void* MetalRenderDevice::GetFallbackTexture(const uint32 kind)
	{
		if (kind >= kFallbackKinds || device == NULL)
			return NULL;
		if (fallbackTextures[kind] != NULL)
			return fallbackTextures[kind];
		const bool cube = (kind & kFallbackCube) != 0;
		const bool depth = (kind & kFallbackDepth) != 0;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
			desc.textureType = cube ? MTLTextureTypeCube : MTLTextureType2D;
			desc.pixelFormat = depth ? MTLPixelFormatDepth32Float : MTLPixelFormatRGBA8Unorm;
			desc.width = 1;
			desc.height = 1;
			desc.mipmapLevelCount = 1;
			desc.usage = depth ? (MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget) : MTLTextureUsageShaderRead;
			desc.storageMode = depth ? MTLStorageModePrivate : MTLStorageModeShared;
			id<MTLTexture> tex = [mtlDevice newTextureWithDescriptor:desc];
			if (tex == nil)
				return NULL;
			const uint32 slices = cube ? 6u : 1u;
			if (!depth)
			{
				const uint8 white[4] = { 255, 255, 255, 255 };
				for (uint32 slice = 0; slice < slices; slice++)
					[tex replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:slice withBytes:white bytesPerRow:4 bytesPerImage:0];
			}
			else
			{
				// Private depth: clear each slice to far with its own pass.
				// Same queue as every later draw, so they see it cleared.
				id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
				id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
				for (uint32 slice = 0; slice < slices; slice++)
				{
					MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
					rpd.depthAttachment.texture = tex;
					rpd.depthAttachment.slice = slice;
					rpd.depthAttachment.loadAction = MTLLoadActionClear;
					rpd.depthAttachment.storeAction = MTLStoreActionStore;
					rpd.depthAttachment.clearDepth = 1.0;
					[[cmdBuf renderCommandEncoderWithDescriptor:rpd] endEncoding];
				}
				[cmdBuf commit];
			}
			fallbackTextures[kind] = (void*)CFBridgingRetain(tex);
		}
		return fallbackTextures[kind];
	}

	void MetalRenderDevice::FillUnboundSamplers(const DeviceHandle program)
	{
		if (currentRenderEncoder == NULL)
			return;
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(program);
		if (progIt == programs.end() || progIt->second.samplerStageMask.empty())
			return;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			if (fallbackSampler == NULL)
			{
				MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
				sd.minFilter = MTLSamplerMinMagFilterNearest;
				sd.magFilter = MTLSamplerMinMagFilterNearest;
				sd.sAddressMode = sd.tAddressMode = sd.rAddressMode = MTLSamplerAddressModeClampToEdge;
				fallbackSampler = (void*)CFBridgingRetain([mtlDevice newSamplerStateWithDescriptor:sd]);
				sd.compareFunction = MTLCompareFunctionLessEqual;
				fallbackCompareSampler = (void*)CFBridgingRetain([mtlDevice newSamplerStateWithDescriptor:sd]);
			}
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			ProgramRecord &prog = progIt->second;
			for (std::map<uint32, uint32>::iterator maskIt = prog.samplerStageMask.begin(); maskIt != prog.samplerStageMask.end(); ++maskIt)
			{
				const uint32 binding = maskIt->first;
				std::map<uint32, uint32>::iterator arrIt = prog.samplerArraySizes.find(binding);
				const uint32 arraySize = (arrIt != prog.samplerArraySizes.end() && arrIt->second > 0) ? arrIt->second : 1;
				std::map<uint32, uint32>::iterator kindIt = prog.samplerKinds.find(binding);
				const uint32 kind = kindIt != prog.samplerKinds.end() ? kindIt->second : 0;
				for (int stage = 0; stage < 2; stage++)
				{
					if (!(maskIt->second & (1u << stage)))
						continue;
					std::map<uint32, uint32>::iterator tIt = prog.textureMslIndex[stage].find(binding);
					std::map<uint32, uint32>::iterator sIt = prog.samplerMslIndex[stage].find(binding);
					const NSUInteger textureBase = (tIt != prog.textureMslIndex[stage].end()) ? (NSUInteger)tIt->second : (NSUInteger)binding;
					const NSUInteger samplerBase = (sIt != prog.samplerMslIndex[stage].end()) ? (NSUInteger)sIt->second : (NSUInteger)binding;
					const bool fixedSampler = sIt != prog.samplerMslIndex[stage].end() && sIt->second == kConstexprSamplerSlot;
					for (uint32 elem = 0; elem < arraySize; elem++)
					{
						const uint64 key = ((uint64)stage << 32) | (uint64)(textureBase + elem);
						if (encoderBoundTextures.count(key) && (encoderBoundCubes.count(key) != 0) == ((kind & kFallbackCube) != 0))
							continue;
						id<MTLTexture> tex = (__bridge id<MTLTexture>)GetFallbackTexture(kind);
						if (tex == nil)
							continue;
						id<MTLSamplerState> sampler = (__bridge id<MTLSamplerState>)((kind & kFallbackDepth) ? fallbackCompareSampler : fallbackSampler);
						if (stage == 0)
						{
							[encoder setVertexTexture:tex atIndex:textureBase + elem];
							if (!fixedSampler) [encoder setVertexSamplerState:sampler atIndex:samplerBase + elem];
						}
						else
						{
							[encoder setFragmentTexture:tex atIndex:textureBase + elem];
							if (!fixedSampler) [encoder setFragmentSamplerState:sampler atIndex:samplerBase + elem];
						}
						encoderBoundTextures.insert(key);
						if (kind & kFallbackCube) encoderBoundCubes.insert(key); else encoderBoundCubes.erase(key);
					}
				}
			}
		}
	}

	// (as BindFramebuffer(0) does when an offscreen detour ends: a new encoder on the same drawable,
	// with Load actions, since a Metal encoder once ended cannot be resumed)
	void MetalRenderDevice::ResumeSwapchainPass()
	{
		if (!frameInProgress || currentBoundFBO != 0 || currentRenderEncoder != NULL || currentDrawable == NULL || currentCommandBuffer == NULL)
			return;
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
			encoderBoundTextures.clear(); encoderBoundCubes.clear();
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
			MTLViewport viewport = { 0.0, 0.0, (double)drawableWidth, (double)drawableHeight, 0.0, 1.0 };
			[encoder setViewport:viewport];
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
	void MetalRenderDevice::BeginRenderEncoderForTarget(const DeviceHandle fbo, const int clearBits)
	{
		std::map<DeviceHandle, FBORecord>::iterator it = fboRecords.find(fbo);
		if (it == fboRecords.end())
			return;
		FBORecord &record = it->second;

		EndCurrentRenderEncoderIfOpen();

		// Bound again in this frame on the same attachments: what was drawn into it stays (a framebuffer
		// unbound to draw elsewhere - a light's shadow face - and bound again had been cleared under the
		// renderer, which GL never does). The first bind of a frame clears as ever; an explicit Clear()
		// clears exactly what it names.
		uint64 layout = 1469598103934665603ull;
		for (std::map<uint32, FBOAttachmentRef>::iterator c = record.colorAttachments.begin(); c != record.colorAttachments.end(); ++c)
			layout = (layout ^ ((uint64)c->first * 1000003ull + (uint64)c->second.texture * 31ull + c->second.target)) * 1099511628211ull;
		layout = (layout ^ ((uint64)record.depthAttachment.texture * 31ull + record.depthAttachment.target)) * 1099511628211ull;
		const bool resume = clearBits < 0 && record.contentFrame == deviceFrameNumber && record.contentLayout == layout;
		const bool clearColor = clearBits < 0 ? !resume : (clearBits & Buffer_Bit::Color) != 0;
		const bool clearDepth = clearBits < 0 ? (!resume && !record.preserveDepth) : (clearBits & Buffer_Bit::Depth) != 0;
		record.contentFrame = deviceFrameNumber; record.contentLayout = layout;
		encoderDrew = false;
		encoderClearedBits = (clearColor ? Buffer_Bit::Color : 0) | (clearDepth ? Buffer_Bit::Depth : 0);

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
			bool anyAttachment = false;

			for (std::map<uint32, FBOAttachmentRef>::iterator cIt = record.colorAttachments.begin(); cIt != record.colorAttachments.end(); cIt++)
			{
				std::map<DeviceHandle, TextureRecord>::iterator texIt = textures.find(cIt->second.texture);
				if (texIt == textures.end() || texIt->second.texture == NULL)
					continue;
				id<MTLTexture> tex = (__bridge id<MTLTexture>)texIt->second.texture;
				uint32 slot = cIt->first;
				rpd.colorAttachments[slot].texture = tex;
				anyAttachment = true;
				rpd.colorAttachments[slot].loadAction = clearColor ? MTLLoadActionClear : MTLLoadActionLoad;
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
					anyAttachment = true;
					// preserveDepth (SetFramebufferPreserveDepth()) means a
					// prior pass already wrote depth this session and it must
					// survive - e.g. VelocityRenderer sampling the same depth
					// buffer the main pass just wrote - so Load instead of
					// Clear, matching VulkanRenderDevice's identical use of
					// this flag.
					rpd.depthAttachment.loadAction = clearDepth ? MTLLoadActionClear : MTLLoadActionLoad;
					rpd.depthAttachment.storeAction = MTLStoreActionStore;
					rpd.depthAttachment.clearDepth = 1.0;
					if (record.depthAttachment.target >= kCubemapFaceTargetBase)
						rpd.depthAttachment.slice = record.depthAttachment.target - kCubemapFaceTargetBase;
					if (targetWidth == 0) targetWidth = texIt->second.width;
					if (targetHeight == 0) targetHeight = texIt->second.height;
				}
			}

			// Nothing to render into yet: FrameBuffer::AddAttach binds the FBO
			// GL-style before its first attachment exists, and an encoder over
			// an attachment-less descriptor is invalid - Metal API validation
			// (MTL_DEBUG_LAYER=1) aborted editor startup on exactly this, in
			// PainterPick's constructor. Every draw/clear path already checks
			// for a NULL encoder; the attachment that follows opens it.
			boundFboAwaitingAttachment = !anyAttachment;
			if (!anyAttachment)
				return;

			id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)currentCommandBuffer;
			GpuTimerAttach((__bridge void*)rpd);
			id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpd];
			currentRenderEncoder = (void*)CFBridgingRetain(encoder);
			encoderBoundTextures.clear(); encoderBoundCubes.clear();
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

	// =====================================================================
	// Compute
	//
	// The structural difference from GL, and the reason this is not a
	// transliteration of GLRenderDevice's version: a dispatch cannot be
	// encoded into a render pass. GL has no such concept - glDispatchCompute
	// is just another call on the context - but Metal requires an
	// MTLComputeCommandEncoder, and only one encoder may be open on a
	// command buffer at a time. The frame's command buffer has an open
	// MTLRenderCommandEncoder for effectively all of its life (see
	// BeginFrame/EndFrame), so compute gets its own command buffer rather
	// than trying to interleave.
	//
	// What that buys: dispatching outside a frame - a bake, a smoke test,
	// any offline GPU work - is fully supported and needs no cooperation
	// from the renderer. What it costs: dispatching *during* a frame, with
	// the render encoder open, is refused rather than silently corrupting
	// the frame. Suspending and resuming a render pass correctly means
	// rebuilding the pass descriptor with Load actions and re-applying
	// viewport, winding, pipeline and every bound resource, and there is
	// no caller that needs it yet. When GPU particles arrive, that is the
	// work to do here - and doing it speculatively now would be untested
	// code on the frame path.
	// =====================================================================

	bool MetalRenderDevice::SupportsCompute() const
	{
		// Every Metal device supports compute - there is no feature bit to
		// check and no OS version that lacks it. The only real question is
		// whether this object has a device at all, and whether the shader
		// toolchain that turns GLSL into an MSL kernel was built in: with
		// METAL_SHADER_TOOLING off, CompileShaderStage cannot produce a
		// function for ANY stage, so claiming compute support would be a
		// lie that fails later and less clearly.
#ifdef METAL_SHADER_TOOLING
		return device != NULL;
#else
		return false;
#endif
	}

	uint32 MetalRenderDevice::GetMaxComputeWorkGroupInvocations() const
	{
		if (device == NULL)
			return 0;
		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			// maxThreadsPerThreadgroup is per-dimension; the scalar limit
			// on total threads in a group is the width component, which is
			// what a 1D local_size_x is measured against.
			return (uint32)mtlDevice.maxThreadsPerThreadgroup.width;
		}
	}

	uint32 MetalRenderDevice::GetMaxComputeWorkGroupCount(const uint32 dimension) const
	{
		// Metal publishes no limit on the NUMBER of threadgroups - only on
		// threads per group (above). Reporting a large value rather than 0
		// keeps a caller's "does my dispatch fit?" check meaningful instead
		// of making it fail on every Metal device; 0 here would read as
		// "compute unsupported", which is the wrong answer.
		if (device == NULL || dimension > 2)
			return 0;
		return 0x7FFFFFFF;
	}

	bool MetalRenderDevice::BeginComputeEncoding(const char *what)
	{
		if (!SupportsCompute())
		{
			ComputeUnsupported(what);
			return false;
		}
		if (currentComputeEncoder != NULL)
			return true;

		// The refusal described in the block comment above. Checked here
		// rather than in each entry point so there is exactly one place
		// that knows the rule.
		if (currentRenderEncoder != NULL)
		{
			echo(std::string("ERROR: ") + what + " called while a render pass is open. "
				"Metal cannot encode a dispatch inside a render pass - the render encoder must end first. "
				"Dispatch outside BeginFrame()/EndFrame(), or extend MetalRenderDevice to suspend and resume the pass.");
			return false;
		}

		@autoreleasepool
		{
			id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
			if (queue == nil)
			{
				echo(std::string("ERROR: ") + what + " has no Metal command queue.");
				return false;
			}
			id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
			id<MTLComputeCommandEncoder> encoder = [cmdBuf computeCommandEncoder];
			{
				const char* lab = FrameProfiler::Instance().CurrentScopeName();
				computeLabel = std::string("compute:") + ((lab && lab[0]) ? lab : "(no scope)");
				const std::string label = computeLabel;
				std::mutex* m = &computeTimesMutex;
				std::map<std::string, f64>* times = &computeTimesMs;
				[cmdBuf addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull done) {
					const f64 ms = (done.GPUEndTime - done.GPUStartTime) * 1000.0;
					if (ms > 0.0 && ms < 1000.0) { std::lock_guard<std::mutex> g(*m); (*times)[label] += ms; }
				}];
			}
			if (cmdBuf == nil || encoder == nil)
			{
				echo(std::string("ERROR: ") + what + " could not open a Metal compute encoder.");
				return false;
			}
			computeCommandBuffer = (void*)CFBridgingRetain(cmdBuf);
			currentComputeEncoder = (void*)CFBridgingRetain(encoder);
		}
		return true;
	}

	void MetalRenderDevice::WaitSubmittedCompute()
	{
		if (submittedCompute.empty()) return;
		PYROS_PROFILE_SCOPE("Metal.ComputeWait");
		@autoreleasepool
		{
			for (size_t k = 0; k < submittedCompute.size(); k++)
			{
				[(__bridge id<MTLCommandBuffer>)submittedCompute[k].commandBuffer waitUntilCompleted];
				CFBridgingRelease(submittedCompute[k].commandBuffer);
			}
		}
		submittedCompute.clear();
	}

	void MetalRenderDevice::WaitSubmittedComputeFor(const DeviceHandle buffer)
	{
		if (submittedCompute.empty()) return;
		// (in submission order: the last that uses it is the one that matters, the ones before it
		// finish first anyway - one queue)
		int last = -1;
		for (size_t k = 0; k < submittedCompute.size(); k++) if (submittedCompute[k].buffers.count(buffer)) last = (int)k;
		if (last < 0) return;
		PYROS_PROFILE_SCOPE("Metal.ComputeWait");
		@autoreleasepool
		{
			[(__bridge id<MTLCommandBuffer>)submittedCompute[last].commandBuffer waitUntilCompleted];
			for (int k = 0; k <= last; k++) CFBridgingRelease(submittedCompute[k].commandBuffer);
		}
		submittedCompute.erase(submittedCompute.begin(), submittedCompute.begin() + last + 1);
	}

	void MetalRenderDevice::PruneSubmittedCompute()
	{
		@autoreleasepool
		{
			size_t done = 0;
			while (done < submittedCompute.size())
			{
				const MTLCommandBufferStatus st = [(__bridge id<MTLCommandBuffer>)submittedCompute[done].commandBuffer status];
				if (st != MTLCommandBufferStatusCompleted && st != MTLCommandBufferStatusError) break;
				CFBridgingRelease(submittedCompute[done].commandBuffer);
				done++;
			}
			if (done) submittedCompute.erase(submittedCompute.begin(), submittedCompute.begin() + done);
		}
	}

	void MetalRenderDevice::FlushComputeEncoding(const bool wait)
	{
		if (currentComputeEncoder == NULL && computeCommandBuffer == NULL)
			return;
		@autoreleasepool
		{
			if (currentComputeEncoder != NULL)
			{
				id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)currentComputeEncoder;
				[encoder endEncoding];
				CFBridgingRelease(currentComputeEncoder);
				currentComputeEncoder = NULL;
			}
			if (computeCommandBuffer != NULL)
			{
				id<MTLCommandBuffer> cmdBuf = (__bridge id<MTLCommandBuffer>)computeCommandBuffer;
				[cmdBuf commit];
				// waitUntilCompleted, not addCompletedHandler: the caller
				// that asked for this is about to read the buffer's
				// contents on the CPU, and MTLResourceStorageModeShared
				// memory is only coherent once the GPU is actually done.
				// Reading without this returns pre-dispatch data, which
				// looks exactly like a broken shader.
				if (wait)
				{
					[cmdBuf waitUntilCompleted];
					CFBridgingRelease(computeCommandBuffer);
					computeEncodingBuffers.clear();
				}
				else
				{
					// (kept, to wait on before the CPU touches what it uses: ComputeBarrierBit::Submit)
					PruneSubmittedCompute();
					SubmittedCompute sc; sc.commandBuffer = computeCommandBuffer; sc.buffers.swap(computeEncodingBuffers);
					submittedCompute.push_back(sc);
					computeEncodingBuffers.clear();
				}
				computeCommandBuffer = NULL;
			}
		}
		boundComputePipeline = 0;
	}

	DeviceHandle MetalRenderDevice::CreateComputePipeline(const DeviceHandle program)
	{
		if (!SupportsCompute())
		{
			ComputeUnsupported("CreateComputePipeline");
			return 0;
		}
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(program);
		if (progIt == programs.end() || !progIt->second.isCompute)
		{
			echo("ERROR: CreateComputePipeline called with a handle that is not a linked compute program.");
			return 0;
		}
		std::map<DeviceHandle, ShaderStageRecord>::iterator csIt = shaderStages.find(progIt->second.computeShader);
		if (csIt == shaderStages.end() || csIt->second.function == NULL)
		{
			echo("ERROR: CreateComputePipeline: the program's compute stage has no compiled MTLFunction.");
			return 0;
		}

		@autoreleasepool
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			id<MTLFunction> function = (__bridge id<MTLFunction>)csIt->second.function;
			NSError* nsError = nil;
			id<MTLComputePipelineState> state = [mtlDevice newComputePipelineStateWithFunction:function error:&nsError];
			if (state == nil)
			{
				echo(std::string("ERROR: newComputePipelineStateWithFunction failed: ")
					+ (nsError ? [[nsError localizedDescription] UTF8String] : "unknown error"));
				return 0;
			}

			// The kernel may declare more threads per group than this
			// device can run. Metal reports that per pipeline (it depends
			// on the compiled kernel's register use, not just the device),
			// and dispatching past it is a hard API error, so it is worth
			// catching at creation with a message that names both numbers.
			const uint32 declared = csIt->second.workgroupSize[0] * csIt->second.workgroupSize[1] * csIt->second.workgroupSize[2];
			const uint32 allowed = (uint32)state.maxTotalThreadsPerThreadgroup;
			if (declared > allowed)
			{
				echo("ERROR: CreateComputePipeline: the kernel declares "
					+ std::to_string(declared) + " threads per threadgroup but this pipeline allows at most "
					+ std::to_string(allowed) + ". Lower its local_size.");
				return 0;
			}

			ComputePipelineRecord record;
			record.state = (void*)CFBridgingRetain(state);
			record.workgroupSize[0] = csIt->second.workgroupSize[0];
			record.workgroupSize[1] = csIt->second.workgroupSize[1];
			record.workgroupSize[2] = csIt->second.workgroupSize[2];
			DeviceHandle handle = nextComputePipelineHandle++;
			computePipelines[handle] = record;
			return handle;
		}
	}

	void MetalRenderDevice::DestroyComputePipeline(const DeviceHandle pipeline)
	{
		std::map<DeviceHandle, ComputePipelineRecord>::iterator it = computePipelines.find(pipeline);
		if (it == computePipelines.end())
			return;
		if (it->second.state != NULL)
			CFBridgingRelease(it->second.state);
		computePipelines.erase(it);
		if (boundComputePipeline == pipeline)
			boundComputePipeline = 0;
	}

	void MetalRenderDevice::BindComputePipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline)
	{
		(void)cmd;
		std::map<DeviceHandle, ComputePipelineRecord>::const_iterator it = computePipelines.find(pipeline);
		if (it == computePipelines.end())
		{
			echo("ERROR: BindComputePipeline called with an unknown pipeline handle.");
			return;
		}
		if (!BeginComputeEncoding("BindComputePipeline"))
			return;
		@autoreleasepool
		{
			id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)currentComputeEncoder;
			id<MTLComputePipelineState> state = (__bridge id<MTLComputePipelineState>)it->second.state;
			[encoder setComputePipelineState:state];
		}
		boundComputePipeline = pipeline;
	}

	DeviceHandle MetalRenderDevice::CreateStorageBuffer(const uint32 sizeBytes, const uint32 bindingPoint, const void *data)
	{
		if (!SupportsCompute())
		{
			ComputeUnsupported("CreateStorageBuffer");
			return 0;
		}
		// Metal has no distinct storage-buffer type: an SSBO and a UBO are
		// both just MTLBuffer, differing only in how the kernel declares
		// them. So this reuses CreateBuffer's allocation wholesale rather
		// than duplicating it - StorageModeShared is exactly what a
		// readback needs on Apple Silicon (no blit, no staging buffer).
		//
		// bindingPoint is deliberately NOT applied here, unlike the GL
		// version: Metal has no persistent indexed binding state outside
		// an encoder, so the slot is set per-encoder by BindStorageBuffer.
		(void)bindingPoint;
		const DeviceHandle handle = CreateBuffer(0, 0, data, sizeBytes);
		if (handle == 0)
			return 0;
		storageBufferSizes[handle] = sizeBytes;
		return handle;
	}

	void MetalRenderDevice::UpdateStorageBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data)
	{
		if (!StorageRangeIsValid(storageBufferSizes, buffer, offset, sizeBytes, "UpdateStorageBuffer") || data == NULL)
			return;
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL)
			return;
		@autoreleasepool
		{
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
			// (submitted work may still be reading it: ComputeBarrierBit::Submit)
			WaitSubmittedComputeFor(buffer);
			memcpy((uint8*)[buf contents] + offset, data, sizeBytes);
		}
	}

	void MetalRenderDevice::ReadStorageBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, void *outData)
	{
		if (outData == NULL)
			return;
		if (!StorageRangeIsValid(storageBufferSizes, buffer, offset, sizeBytes, "ReadStorageBuffer"))
		{
			memset(outData, 0, sizeBytes);
			return;
		}
		// Any dispatch still queued must finish before its writes are
		// visible in shared memory. Flushing here rather than requiring
		// the caller to have issued a HostRead barrier makes the readback
		// correct by construction - the barrier remains the documented
		// way to say it explicitly, and is a no-op if already flushed.
		FlushComputeEncoding(true);
		WaitSubmittedComputeFor(buffer);

		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL)
		{
			memset(outData, 0, sizeBytes);
			return;
		}
		@autoreleasepool
		{
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
			memcpy(outData, (const uint8*)[buf contents] + offset, sizeBytes);
		}
	}

	void MetalRenderDevice::BindStorageBuffer(const CommandBufferHandle cmd, const DeviceHandle buffer, const uint32 bindingPoint)
	{
		(void)cmd;
		std::map<DeviceHandle, BufferRecord>::iterator it = buffers.find(buffer);
		if (it == buffers.end() || it->second.buffer == NULL)
		{
			echo("ERROR: BindStorageBuffer called with an unknown buffer handle.");
			return;
		}
		if (!BeginComputeEncoding("BindStorageBuffer"))
			return;
		@autoreleasepool
		{
			id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)currentComputeEncoder;
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)it->second.buffer;
			// The MSL index equals the SPIR-V binding, the same convention
			// CompileShaderStage() forces for uniform buffers (see the
			// long comment there on msl_options and why the default
			// declaration-order numbering is wrong for this engine).
			[encoder setBuffer:buf offset:0 atIndex:bindingPoint];
			computeEncodingBuffers.insert(buffer);
		}
	}

	void MetalRenderDevice::DestroyStorageBuffer(const DeviceHandle buffer)
	{
		WaitSubmittedComputeFor(buffer);
		storageBufferSizes.erase(buffer);
		// The MTLBuffer was allocated by CreateBuffer, so it is freed by
		// the matching DestroyBuffer rather than by hand here.
		DestroyBuffer(buffer);
	}

	void MetalRenderDevice::Dispatch(const CommandBufferHandle cmd, const uint32 groupsX, const uint32 groupsY, const uint32 groupsZ)
	{
		(void)cmd;
		if (!SupportsCompute())
		{
			ComputeUnsupported("Dispatch");
			return;
		}
		if (groupsX == 0 || groupsY == 0 || groupsZ == 0)
		{
			echo("WARNING: Dispatch called with a zero work group count - nothing will run. "
				"Remember these are GROUP counts: use (items + localSize - 1) / localSize, not the item count.");
			return;
		}
		if (boundComputePipeline == 0 || currentComputeEncoder == NULL)
		{
			echo("ERROR: Dispatch called with no compute pipeline bound - call BindComputePipeline first.");
			return;
		}
		std::map<DeviceHandle, ComputePipelineRecord>::const_iterator it = computePipelines.find(boundComputePipeline);
		if (it == computePipelines.end())
			return;

		@autoreleasepool
		{
			id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)currentComputeEncoder;
			MTLSize groups = MTLSizeMake(groupsX, groupsY, groupsZ);
			// From the shader, never from the caller - see the capture in
			// CompileShaderStage(). This is the half of the dispatch GL
			// does not make you say, and the half Metal will not check.
			MTLSize threadsPerGroup = MTLSizeMake(
				it->second.workgroupSize[0],
				it->second.workgroupSize[1],
				it->second.workgroupSize[2]);
			[encoder dispatchThreadgroups:groups threadsPerThreadgroup:threadsPerGroup];
		}
	}

	bool MetalRenderDevice::HasPendingComputeWork() const
	{
		return currentComputeEncoder != NULL || computeCommandBuffer != NULL;
	}

	void MetalRenderDevice::ComputeBarrier(const CommandBufferHandle cmd, const uint32 barrierBits)
	{
		(void)cmd;
		if (!SupportsCompute())
		{
			ComputeUnsupported("ComputeBarrier");
			return;
		}
		// Two of these are not encoder barriers at all, because what they
		// hand the data to is not another dispatch in this encoder:
		//
		//   HostRead - CPU visibility of shared memory is a command-buffer
		//     COMPLETION property, so this must commit and wait.
		//   VertexBuffer - the reader is a draw call, which is recorded
		//     into the frame's own command buffer, not this one. Leaving
		//     the compute buffer open just accumulates dispatches that
		//     never execute; the draw then samples whatever the buffer
		//     held before, which for a freshly allocated particle pool is
		//     zeroes. That is a black screen with no error anywhere, and
		//     it is exactly what happened the first time GPU particles
		//     were pointed at a real scene.
		//
		// The wait on VertexBuffer is heavier than Metal strictly needs -
		// command buffers on one queue execute in submission order, so
		// committing without waiting would already order the dispatch
		// before the frame. It is kept because Vulkan's equivalent is NOT
		// ordered without a semaphore, and one behaviour across backends
		// is worth more here than one frame of latency. Revisit with a
		// real semaphore chain if this shows up in a profile.
		if (barrierBits & ComputeBarrierBit::HostRead)
		{
			FlushComputeEncoding(true);
			return;
		}
		// VertexBuffer: committed, not waited for. The draw that reads it is in the frame's command
		// buffer, on the same queue and committed after this - Metal runs a queue's command buffers in
		// the order they are committed, and tracks the buffer across them. The wait cost the frame the
		// GPU's whole particle simulation every frame (GPU particles).
		if (barrierBits & ComputeBarrierBit::VertexBuffer)
		{
			FlushComputeEncoding(false);
			return;
		}
		// Submit: committed now and not waited for - the CPU reads it a frame later, and the
		// storage-buffer calls wait on it first (WaitSubmittedCompute)
		if (barrierBits & ComputeBarrierBit::Submit)
		{
			FlushComputeEncoding(false);
			return;
		}
		if (currentComputeEncoder == NULL)
			return;
		@autoreleasepool
		{
			id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)currentComputeEncoder;
			// Metal hazard-tracks resources automatically by default
			// (MTLHazardTrackingModeTracked), so a dispatch reading what a
			// previous dispatch in the SAME encoder wrote is already
			// ordered. This barrier is what makes that explicit and is
			// required for untracked resources; issuing it costs an
			// ordering point and nothing else.
			MTLBarrierScope scope = 0;
			if (barrierBits & (ComputeBarrierBit::StorageBuffer | ComputeBarrierBit::VertexBuffer))
				scope |= MTLBarrierScopeBuffers;
			if (barrierBits & ComputeBarrierBit::ImageAccess)
				scope |= MTLBarrierScopeTextures;
			if (barrierBits == ComputeBarrierBit::All)
				scope = MTLBarrierScopeBuffers | MTLBarrierScopeTextures;
			if (scope != 0)
				[encoder memoryBarrierWithScope:scope];
		}
	}


	// ---- Ray queries -----------------------------------------------------
	// See IRenderDevice::SupportsRayQuery(). Metal's acceleration structures:
	// a primitive one per geometry, built once; an instance one per frame
	// over them. PYROS_NO_RAYTRACING=1 reports none, for A/B tests.
	bool MetalRenderDevice::SupportsRayQuery() const
	{
		if (device == NULL || std::getenv("PYROS_NO_RAYTRACING") != NULL) return false;
		if (@available(macOS 13.0, *))
		{
			id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
			return [mtlDevice supportsRaytracing];
		}
		return false;
	}

	DeviceHandle MetalRenderDevice::CreateRayGeometry(const f32 *positions, const uint32 vertexCount, const uint32 strideFloats,
		const uint32 *indices, const uint32 indexCount)
	{
		if (!SupportsRayQuery() || positions == NULL || indices == NULL || vertexCount == 0 || indexCount < 3) return 0;
		if (@available(macOS 13.0, *))
		{
			@autoreleasepool
			{
				id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
				// (packed tight: three floats a vertex, whatever the caller's stride)
				std::vector<f32> packed((size_t)vertexCount * 3);
				for (uint32 v = 0; v < vertexCount; v++)
					for (uint32 k = 0; k < 3; k++) packed[(size_t)v * 3 + k] = positions[(size_t)v * strideFloats + k];
				id<MTLBuffer> vb = [mtlDevice newBufferWithBytes:packed.data() length:packed.size() * sizeof(f32) options:MTLResourceStorageModeShared];
				id<MTLBuffer> ib = [mtlDevice newBufferWithBytes:indices length:(NSUInteger)(indexCount / 3) * 3 * sizeof(uint32) options:MTLResourceStorageModeShared];
				MTLAccelerationStructureTriangleGeometryDescriptor* tri = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
				tri.vertexBuffer = vb;
				tri.vertexStride = 3 * sizeof(f32);
				tri.vertexFormat = MTLAttributeFormatFloat3;
				tri.indexBuffer = ib;
				tri.indexType = MTLIndexTypeUInt32;
				tri.triangleCount = indexCount / 3;
				tri.opaque = YES;
				MTLPrimitiveAccelerationStructureDescriptor* desc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
				desc.geometryDescriptors = @[ tri ];
				MTLAccelerationStructureSizes sizes = [mtlDevice accelerationStructureSizesWithDescriptor:desc];
				id<MTLAccelerationStructure> accel = [mtlDevice newAccelerationStructureWithSize:sizes.accelerationStructureSize];
				id<MTLBuffer> scratch = [mtlDevice newBufferWithLength:std::max<NSUInteger>(sizes.buildScratchBufferSize, 16) options:MTLResourceStorageModePrivate];
				if (accel == nil || scratch == nil) return 0;
				id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
				id<MTLCommandBuffer> cb = [queue commandBuffer];
				id<MTLAccelerationStructureCommandEncoder> enc = [cb accelerationStructureCommandEncoder];
				[enc buildAccelerationStructure:accel descriptor:desc scratchBuffer:scratch scratchBufferOffset:0];
				[enc endEncoding];
				// (committed now: queued ahead of any frame that will trace against it, and Metal
				// runs one queue's command buffers in the order they were committed)
				[cb commit];
				const DeviceHandle h = nextRayGeometryHandle++;
				rayGeometries[h] = (void*)CFBridgingRetain(accel);
				return h;
			}
		}
		return 0;
	}

	void MetalRenderDevice::DestroyRayGeometry(const DeviceHandle geometry)
	{
		std::map<DeviceHandle, void*>::iterator it = rayGeometries.find(geometry);
		if (it == rayGeometries.end()) return;
		// (a scene built over it still holds it, through raySceneUsed, until it is rebuilt)
		CFBridgingRelease(it->second);
		rayGeometries.erase(it);
	}

	void MetalRenderDevice::SetRayScene(const std::vector<RayInstance> &instances)
	{
		if (!SupportsRayQuery()) return;
		if (@available(macOS 13.0, *))
		{
			@autoreleasepool
			{
				id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)device;
				// The geometries it reaches, each once, and every instance pointing at its own.
				NSMutableArray<id<MTLAccelerationStructure>>* used = [NSMutableArray array];
				std::map<DeviceHandle, uint32> indexOf;
				std::vector<MTLAccelerationStructureInstanceDescriptor> descs;
				descs.reserve(instances.size());
				for (size_t i = 0; i < instances.size(); i++)
				{
					std::map<DeviceHandle, void*>::iterator g = rayGeometries.find(instances[i].geometry);
					if (g == rayGeometries.end()) continue;
					std::map<DeviceHandle, uint32>::iterator at = indexOf.find(instances[i].geometry);
					uint32 index;
					if (at == indexOf.end())
					{
						index = (uint32)used.count;
						indexOf[instances[i].geometry] = index;
						[used addObject:(__bridge id<MTLAccelerationStructure>)g->second];
					}
					else index = at->second;
					MTLAccelerationStructureInstanceDescriptor d;
					memset(&d, 0, sizeof(d));
					const f32 *m = instances[i].world.m;     // (column-major: column c is m[4c..4c+2], the translation m[12..14])
					for (uint32 c = 0; c < 4; c++)
					{
						d.transformationMatrix.columns[c].x = m[c * 4 + 0];
						d.transformationMatrix.columns[c].y = m[c * 4 + 1];
						d.transformationMatrix.columns[c].z = m[c * 4 + 2];
					}
					d.options = MTLAccelerationStructureInstanceOptionOpaque | MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
					d.mask = 0xFF;
					d.intersectionFunctionTableOffset = 0;
					d.accelerationStructureIndex = index;
					descs.push_back(d);
				}
				// (no instances: one triangle far below everything, so a scene to bind always exists)
				if (descs.empty())
				{
					static DeviceHandle nothing = 0;
					if (nothing == 0)
					{
						const f32 p[9] = { 0.f, -1e6f, 0.f, 1.f, -1e6f, 0.f, 0.f, -1e6f, 1.f };
						const uint32 ix[3] = { 0, 1, 2 };
						nothing = CreateRayGeometry(p, 3, 3, ix, 3);
					}
					std::map<DeviceHandle, void*>::iterator g = rayGeometries.find(nothing);
					if (g == rayGeometries.end()) return;
					[used addObject:(__bridge id<MTLAccelerationStructure>)g->second];
					MTLAccelerationStructureInstanceDescriptor d;
					memset(&d, 0, sizeof(d));
					d.transformationMatrix.columns[0].x = 1.f; d.transformationMatrix.columns[1].y = 1.f; d.transformationMatrix.columns[2].z = 1.f;
					d.options = MTLAccelerationStructureInstanceOptionOpaque;
					d.mask = 0xFF;
					descs.push_back(d);
				}

				// (the oldest slot whose frame is done; failing that the oldest, said once)
				uint32 slot = (raySceneSlot + 1) % kRaySceneRing;
				{
					uint32 oldest = slot; bool found = false;
					for (uint32 k = 1; k <= kRaySceneRing; k++)
					{
						const uint32 c = (raySceneSlot + k) % kRaySceneRing;
						if (raySceneBuiltFrame[c] < raySceneBuiltFrame[oldest]) oldest = c;
						if (raySceneBuiltFrame[c] + MAX_FRAMES_IN_FLIGHT + 1 <= deviceFrameNumber) { slot = c; found = true; break; }
					}
					if (!found)
					{
						slot = oldest;
						static bool said = false;
						if (!said) { said = true; fprintf(stderr, "MetalRenderDevice: more ray scenes in flight than %u slots - one is rebuilt while still traced\n", kRaySceneRing); }
					}
				}
				raySceneBuiltFrame[slot] = deviceFrameNumber;
				const uint32 n = (uint32)descs.size();
				if (raySceneInstanceCapacity[slot] < n)
				{
					if (raySceneInstances[slot] != NULL) CFBridgingRelease(raySceneInstances[slot]);
					const uint32 cap = n + n / 4 + 16;
					id<MTLBuffer> buf = [mtlDevice newBufferWithLength:(NSUInteger)cap * sizeof(MTLAccelerationStructureInstanceDescriptor) options:MTLResourceStorageModeShared];
					raySceneInstances[slot] = (void*)CFBridgingRetain(buf);
					raySceneInstanceCapacity[slot] = cap;
				}
				id<MTLBuffer> instBuf = (__bridge id<MTLBuffer>)raySceneInstances[slot];
				memcpy([instBuf contents], descs.data(), (size_t)n * sizeof(MTLAccelerationStructureInstanceDescriptor));

				MTLInstanceAccelerationStructureDescriptor* desc = [MTLInstanceAccelerationStructureDescriptor descriptor];
				desc.instancedAccelerationStructures = used;
				desc.instanceCount = n;
				desc.instanceDescriptorBuffer = instBuf;
				desc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;
				MTLAccelerationStructureSizes sizes = [mtlDevice accelerationStructureSizesWithDescriptor:desc];
				id<MTLAccelerationStructure> accel = nil;
				if (raySceneAccel[slot] != NULL)
				{
					id<MTLAccelerationStructure> old = (__bridge id<MTLAccelerationStructure>)raySceneAccel[slot];
					if (old.size >= sizes.accelerationStructureSize) accel = old;
					else { CFBridgingRelease(raySceneAccel[slot]); raySceneAccel[slot] = NULL; }
				}
				if (accel == nil)
				{
					accel = [mtlDevice newAccelerationStructureWithSize:sizes.accelerationStructureSize + sizes.accelerationStructureSize / 4];
					if (accel == nil) return;
					raySceneAccel[slot] = (void*)CFBridgingRetain(accel);
				}
				id<MTLBuffer> scratch = [mtlDevice newBufferWithLength:std::max<NSUInteger>(sizes.buildScratchBufferSize, 16) options:MTLResourceStorageModePrivate];
				id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
				id<MTLCommandBuffer> cb = [queue commandBuffer];
				id<MTLAccelerationStructureCommandEncoder> enc = [cb accelerationStructureCommandEncoder];
				[enc buildAccelerationStructure:accel descriptor:desc scratchBuffer:scratch scratchBufferOffset:0];
				[enc endEncoding];
				[cb commit];

				raySceneSlot = slot;
				raySceneReady = true;
				for (size_t i = 0; i < raySceneUsed.size(); i++) CFBridgingRelease(raySceneUsed[i]);
				raySceneUsed.clear();
				for (id<MTLAccelerationStructure> a in used) raySceneUsed.push_back((void*)CFBridgingRetain(a));
			}
		}
	}

	void MetalRenderDevice::BindRaySceneIfRead(const DeviceHandle programHandle)
	{
		std::map<DeviceHandle, ProgramRecord>::iterator progIt = programs.find(programHandle);
		if (progIt == programs.end() || progIt->second.raySceneFragmentIndex < 0 || currentRenderEncoder == NULL) return;
		if (@available(macOS 13.0, *))
		{
			// (nothing built yet: an empty scene, so the shader never reads an unbound buffer)
			if (!raySceneReady) SetRayScene(std::vector<RayInstance>());
			if (!raySceneReady || raySceneAccel[raySceneSlot] == NULL) return;
			id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)currentRenderEncoder;
			[encoder setFragmentAccelerationStructure:(__bridge id<MTLAccelerationStructure>)raySceneAccel[raySceneSlot]
				atBufferIndex:(NSUInteger)progIt->second.raySceneFragmentIndex];
			// (an instance structure only points at the geometries' own: they must be resident in the pass too)
			if (!raySceneUsed.empty())
				[encoder useResources:(const id<MTLResource> __unsafe_unretained *)(void*)raySceneUsed.data()
					count:raySceneUsed.size() usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
		}
	}

}

#endif /* METAL_BACKEND */
