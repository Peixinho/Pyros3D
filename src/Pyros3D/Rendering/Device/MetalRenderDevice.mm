//============================================================================
// Name        : MetalRenderDevice.mm
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See MetalRenderDevice.h for scope. Real so far: device/
//               queue/layer setup (BindToLayer) and the ClearAndPresent()
//               milestone. Every other IRenderDevice override below is a
//               stub - logs once via LogStub() and returns a safe default -
//               not yet exercised by anything (nothing constructs a real
//               IRenderer against this device yet). Compiled with ARC
//               (-fobjc-arc, set in PyrosBackend.cmake for this file only)
//               so id<MTLXxx> locals are memory-managed normally; every
//               Metal-typed *member* is still a plain void* (see the header
//               comment on why) and crosses that boundary via
//               CFBridgingRetain()/CFBridgingRelease()/(__bridge Type) -
//               retain on store, release on destroy, plain non-owning cast
//               to read.
//============================================================================

#include "Pyros3D/Rendering/Device/MetalRenderDevice.h"

#ifdef METAL_BACKEND

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <cstdio>
#include <set>
#include <string>

namespace {

	// Logs each not-yet-implemented method exactly once (not every call) -
	// most of these are on hot paths that would otherwise flood stderr the
	// moment anything real drives this device.
	void LogStub(const char* fn)
	{
		static std::set<std::string> warned;
		if (warned.insert(fn).second)
			fprintf(stderr, "MetalRenderDevice::%s: not implemented yet (Metal backend bring-up milestone covers BindToLayer/ClearAndPresent only)\n", fn);
	}

}

namespace p3d {

	MetalRenderDevice::MetalRenderDevice()
		: device(NULL), commandQueue(NULL), metalLayer(NULL), drawableWidth(0), drawableHeight(0),
		  frameBoundarySemaphore(NULL), currentCommandBuffer(NULL), currentRenderEncoder(NULL),
		  currentDrawable(NULL), lastSubmittedCommandBuffer(NULL),
		  nextVaoHandle(1), currentVao(0), currentPipeline(0),
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

		// Frame throttling - see the header comment on
		// frameBoundarySemaphore. Proves the completion-handler pattern
		// works on its own, before any real BeginFrame()/EndFrame() path
		// depends on it.
		dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)frameBoundarySemaphore;
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

		@autoreleasepool
		{
			CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			if (drawable == nil)
			{
				// Nothing was submitted to eventually signal the semaphore
				// back - must do it ourselves or every subsequent call
				// blocks forever after MAX_FRAMES_IN_FLIGHT misses in a row.
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

			// Must be added before commit - Metal rejects a completion
			// handler registered on an already-committed command buffer.
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
	// Everything below is a stub - not yet exercised by anything (see the
	// file header comment). Grouped in IRenderDevice.h's own declaration
	// order for easy side-by-side diffing against that file.
	// =====================================================================

	CommandBufferHandle MetalRenderDevice::BeginCommandBuffer() { LogStub("BeginCommandBuffer"); return 0; }
	void MetalRenderDevice::EndCommandBuffer(const CommandBufferHandle cmd) { (void)cmd; LogStub("EndCommandBuffer"); }
	void MetalRenderDevice::BeginFrame() { LogStub("BeginFrame"); }
	void MetalRenderDevice::EndFrame() { LogStub("EndFrame"); }

	uint32 MetalRenderDevice::TranslateBufferBit(const uint32 bufferBits) { (void)bufferBits; LogStub("TranslateBufferBit"); return 0; }
	void MetalRenderDevice::Clear(const uint32 nativeBufferBits) { (void)nativeBufferBits; LogStub("Clear"); }
	void MetalRenderDevice::SetClearColor(const Vec4 &color) { pendingClearColor = color; }

	void MetalRenderDevice::SetDepthTest(const bool enabled, const uint32 mode) { (void)enabled; (void)mode; LogStub("SetDepthTest"); }
	void MetalRenderDevice::SetDepthMask(const bool enabled) { (void)enabled; LogStub("SetDepthMask"); }
	void MetalRenderDevice::PrepareDepthClear() { LogStub("PrepareDepthClear"); }

	void MetalRenderDevice::SetStencilTestEnabled(const bool enabled) { (void)enabled; LogStub("SetStencilTestEnabled"); }
	void MetalRenderDevice::SetClearStencilValue() { LogStub("SetClearStencilValue"); }
	void MetalRenderDevice::SetStencilFunction(const uint32 func, const uint32 ref, const uint32 mask) { (void)func; (void)ref; (void)mask; LogStub("SetStencilFunction"); }
	void MetalRenderDevice::SetStencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass) { (void)sfail; (void)dpfail; (void)dppass; LogStub("SetStencilOperation"); }

	void MetalRenderDevice::SetScissorRect(const f32 x, const f32 y, const f32 width, const f32 height) { (void)x; (void)y; (void)width; (void)height; LogStub("SetScissorRect"); }
	void MetalRenderDevice::SetScissorTestEnabled(const bool enabled) { (void)enabled; LogStub("SetScissorTestEnabled"); }

	void MetalRenderDevice::SetWireFrame(const bool enabled) { (void)enabled; LogStub("SetWireFrame"); }

	void MetalRenderDevice::SetColorMask(const bool r, const bool g, const bool b, const bool a) { (void)r; (void)g; (void)b; (void)a; LogStub("SetColorMask"); }

	void MetalRenderDevice::SetPolygonOffsetEnabled(const bool enabled) { (void)enabled; LogStub("SetPolygonOffsetEnabled"); }
	void MetalRenderDevice::SetPolygonOffset(const f32 factor, const f32 units) { (void)factor; (void)units; LogStub("SetPolygonOffset"); }

	void MetalRenderDevice::SetBlendingEnabled(const bool enabled) { (void)enabled; LogStub("SetBlendingEnabled"); }
	void MetalRenderDevice::SetBlendFunction(const uint32 sfactor, const uint32 dfactor) { (void)sfactor; (void)dfactor; LogStub("SetBlendFunction"); }
	void MetalRenderDevice::SetBlendEquation(const uint32 mode) { (void)mode; LogStub("SetBlendEquation"); }

	void MetalRenderDevice::SetCullFaceMode(const uint32 cullFace) { (void)cullFace; LogStub("SetCullFaceMode"); }
	void MetalRenderDevice::DisableCullFace() { LogStub("DisableCullFace"); }

	DeviceHandle MetalRenderDevice::CreatePipeline(const PipelineDesc &desc) { (void)desc; LogStub("CreatePipeline"); return 0; }
	void MetalRenderDevice::DestroyPipeline(const DeviceHandle pipeline) { (void)pipeline; LogStub("DestroyPipeline"); }
	void MetalRenderDevice::BindPipeline(const CommandBufferHandle cmd, const DeviceHandle pipeline) { (void)cmd; (void)pipeline; LogStub("BindPipeline"); }

	void MetalRenderDevice::EnableClipDistance(const uint32 index) { (void)index; LogStub("EnableClipDistance"); }
	void MetalRenderDevice::DisableClipDistance(const uint32 index) { (void)index; LogStub("DisableClipDistance"); }

	void MetalRenderDevice::SetViewport(const uint32 x, const uint32 y, const uint32 width, const uint32 height) { (void)x; (void)y; (void)width; (void)height; LogStub("SetViewport"); }

	void MetalRenderDevice::UseProgram(const uint32 program) { (void)program; LogStub("UseProgram"); }
	DeviceHandle MetalRenderDevice::CreateVertexArray() { LogStub("CreateVertexArray"); return 0; }
	void MetalRenderDevice::DeleteVertexArray(const DeviceHandle vao) { (void)vao; LogStub("DeleteVertexArray"); }
	void MetalRenderDevice::BindVertexArray(const CommandBufferHandle cmd, const DeviceHandle vao) { (void)cmd; (void)vao; LogStub("BindVertexArray"); }
	void MetalRenderDevice::BindArrayBuffer(const uint32 buffer) { (void)buffer; LogStub("BindArrayBuffer"); }
	void MetalRenderDevice::BindElementBuffer(const uint32 buffer) { (void)buffer; LogStub("BindElementBuffer"); }
	void MetalRenderDevice::SetVertexAttribute(const int32 location, const uint32 typeCount, const uint32 nativeType, const uint32 stride, const uint32 offset) { (void)location; (void)typeCount; (void)nativeType; (void)stride; (void)offset; LogStub("SetVertexAttribute"); }
	void MetalRenderDevice::SetFloatVertexAttribute(const int32 location, const uint32 componentCount, const uint32 stride, const uint32 offset) { (void)location; (void)componentCount; (void)stride; (void)offset; LogStub("SetFloatVertexAttribute"); }
	void MetalRenderDevice::DisableVertexAttribute(const int32 location) { (void)location; LogStub("DisableVertexAttribute"); }
	void MetalRenderDevice::SetVertexAttributeDivisor(const int32 location, const uint32 divisor) { (void)location; (void)divisor; LogStub("SetVertexAttributeDivisor"); }
	void MetalRenderDevice::BindUniformBlockIfPresent(const uint32 program, const std::string &blockName, const uint32 bindingPoint) { (void)program; (void)blockName; (void)bindingPoint; LogStub("BindUniformBlockIfPresent"); }

	Matrix MetalRenderDevice::TranslateProjectionMatrix(const Matrix &projectionMatrix, const bool skipYFlip) { (void)skipYFlip; LogStub("TranslateProjectionMatrix"); return projectionMatrix; }
	Matrix MetalRenderDevice::TranslateShadowBiasMatrix() { LogStub("TranslateShadowBiasMatrix"); return Matrix(); }

	uint32 MetalRenderDevice::TranslateDrawType(const uint32 engineDrawType) { (void)engineDrawType; LogStub("TranslateDrawType"); return 0; }
	void MetalRenderDevice::DrawArrays(const uint32 nativeDrawType, const uint32 first, const uint32 count) { (void)nativeDrawType; (void)first; (void)count; LogStub("DrawArrays"); }
	void MetalRenderDevice::DrawElements(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount) { (void)cmd; (void)nativeDrawType; (void)indexCount; LogStub("DrawElements"); }
	void MetalRenderDevice::DrawElementsInstanced(const CommandBufferHandle cmd, const uint32 nativeDrawType, const uint32 indexCount, const uint32 instanceCount) { (void)cmd; (void)nativeDrawType; (void)indexCount; (void)instanceCount; LogStub("DrawElementsInstanced"); }

	DeviceHandle MetalRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint) { (void)sizeBytes; (void)bindingPoint; LogStub("CreateUniformBuffer"); return 0; }
	void MetalRenderDevice::UpdateUniformBuffer(const DeviceHandle buffer, const uint32 offset, const uint32 sizeBytes, const void *data) { (void)buffer; (void)offset; (void)sizeBytes; (void)data; LogStub("UpdateUniformBuffer"); }
	void MetalRenderDevice::ReplaceUniformBuffer(const DeviceHandle buffer, const uint32 sizeBytes, const void *data) { (void)buffer; (void)sizeBytes; (void)data; LogStub("ReplaceUniformBuffer"); }
	void MetalRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer) { (void)buffer; LogStub("DestroyUniformBuffer"); }

	DeviceHandle MetalRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) { (void)bufferType; (void)bufferDraw; (void)data; (void)length; LogStub("CreateBuffer"); return 0; }
	void MetalRenderDevice::ReallocateBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length) { (void)buffer; (void)bufferType; (void)bufferDraw; (void)data; (void)length; LogStub("ReallocateBuffer"); }
	void MetalRenderDevice::UpdateBufferSubData(const DeviceHandle buffer, const uint32 bufferType, const void *data, const uint32 length) { (void)buffer; (void)bufferType; (void)data; (void)length; LogStub("UpdateBufferSubData"); }
	void MetalRenderDevice::DestroyBuffer(const DeviceHandle buffer) { (void)buffer; LogStub("DestroyBuffer"); }
	void *MetalRenderDevice::MapBuffer(const DeviceHandle buffer, const uint32 bufferType, const uint32 mappingType) { (void)buffer; (void)bufferType; (void)mappingType; LogStub("MapBuffer"); return NULL; }
	void MetalRenderDevice::UnmapBuffer(const DeviceHandle buffer, const uint32 bufferType) { (void)buffer; (void)bufferType; LogStub("UnmapBuffer"); }

	uint32 MetalRenderDevice::TranslateAttributeType(const uint32 engineType) { (void)engineType; LogStub("TranslateAttributeType"); return 0; }

	std::string MetalRenderDevice::BuildShaderSource(const std::string &definitions, const std::string &shaderBody) { (void)definitions; (void)shaderBody; LogStub("BuildShaderSource"); return std::string(); }
	DeviceHandle MetalRenderDevice::CreateShaderStage(const uint32 engineShaderType) { (void)engineShaderType; LogStub("CreateShaderStage"); return 0; }
	bool MetalRenderDevice::CompileShaderStage(const DeviceHandle shader, const std::string &source, std::string &errorLog) { (void)shader; (void)source; (void)errorLog; LogStub("CompileShaderStage"); return false; }
	DeviceHandle MetalRenderDevice::CreateProgram() { LogStub("CreateProgram"); return 0; }
	void MetalRenderDevice::AttachShaderStage(const DeviceHandle program, const DeviceHandle shader) { (void)program; (void)shader; LogStub("AttachShaderStage"); }
	bool MetalRenderDevice::LinkProgram(const DeviceHandle program, std::string &errorLog) { (void)program; (void)errorLog; LogStub("LinkProgram"); return false; }
	bool MetalRenderDevice::IsProgram(const DeviceHandle handle) { (void)handle; LogStub("IsProgram"); return false; }
	bool MetalRenderDevice::IsShaderStage(const DeviceHandle handle) { (void)handle; LogStub("IsShaderStage"); return false; }
	void MetalRenderDevice::DetachShaderStage(const DeviceHandle program, const DeviceHandle shader) { (void)program; (void)shader; LogStub("DetachShaderStage"); }
	void MetalRenderDevice::DeleteShaderStage(const DeviceHandle shader) { (void)shader; LogStub("DeleteShaderStage"); }
	void MetalRenderDevice::DeleteProgram(const DeviceHandle program) { (void)program; LogStub("DeleteProgram"); }

	int32 MetalRenderDevice::GetUniformLocation(const uint32 program, const std::string &name) { (void)program; (void)name; LogStub("GetUniformLocation"); return -1; }
	int32 MetalRenderDevice::GetAttributeLocation(const uint32 program, const std::string &name) { (void)program; (void)name; LogStub("GetAttributeLocation"); return -1; }
	bool MetalRenderDevice::GetAutoUniformBlockLayout(const uint32 program, const uint32 engineShaderType, uint32 &outBinding, std::string &outBlockName, uint32 &outSize, std::map<std::string, uint32> &outOffsets) { (void)program; (void)engineShaderType; (void)outBinding; (void)outBlockName; (void)outSize; (void)outOffsets; return false; }

	void MetalRenderDevice::SendUniformInt(const int32 handle, const int32 *data, const uint32 count) { (void)handle; (void)data; (void)count; LogStub("SendUniformInt"); }
	void MetalRenderDevice::SendUniformFloat(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; LogStub("SendUniformFloat"); }
	void MetalRenderDevice::SendUniformVec2(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; LogStub("SendUniformVec2"); }
	void MetalRenderDevice::SendUniformVec3(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; LogStub("SendUniformVec3"); }
	void MetalRenderDevice::SendUniformVec4(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; LogStub("SendUniformVec4"); }
	void MetalRenderDevice::SendUniformMatrix(const int32 handle, const f32 *data, const uint32 count) { (void)handle; (void)data; (void)count; LogStub("SendUniformMatrix"); }

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

	DeviceHandle MetalRenderDevice::GetCurrentRenderTarget() { LogStub("GetCurrentRenderTarget"); return currentBoundFBO; }
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

	// Private helpers - not wired into any stub above yet (the framebuffer/
	// render-target path is out of scope for this milestone, see
	// BindFramebuffer()'s stub) - declared for the design sketch in the
	// header, defined here as no-ops so the class stays linkable if
	// something calls them ahead of a real implementation.
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
