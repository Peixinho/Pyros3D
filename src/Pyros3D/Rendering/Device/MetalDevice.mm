//============================================================================
// Name        : MetalDevice.mm
// Author      : Duarte Peixinho
// Description : The device layer on Metal - the native backend on the Mac (and later iOS). The same frame as
//               the WebGPU and Vulkan backends' (docs/device-design.md): a draw's data is a slot in
//               this frame's ring - shared memory, written in place - bound at an offset; a material's
//               resources are one bind group made at load; pipelines compile on Metal's own threads
//               (the completion-handler form) and are never waited for by a draw.
//
// Shaders are the same SPIR-V the Vulkan backend takes, turned into the Metal shading language by
// SPIRV-Cross when they are made. Where things land, the same for every shader:
//   group 0 binding 0 (the draw's data)        buffer 1, vertex and fragment
//   group 1 binding b (the material)           this stage's next texture, sampler or buffer (from 2), in binding
//                                              order - Metal has 31 buffer slots a stage and the engine's bindings
//                                              run past 40; the pipeline keeps each stage's numbering
//   vertex buffer binding n                    buffer 16 + n
// Conventions as the other backends': clip space y up (Metal's own), depth 0..1, reversed Z as the
// renderer asks for it. Barriers are Metal's to work out: every resource is hazard-tracked.
//============================================================================

#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <cstdlib>
// (the frame a test's capture is of: PYROS_SHOT_FRAME, else the 120th)
static inline uint64_t ShotFrame() { static const uint64_t f = getenv("PYROS_SHOT_FRAME") ? (uint64_t)atoll(getenv("PYROS_SHOT_FRAME")) : 120; return f; }
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <ImageIO/ImageIO.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif
#import <CoreServices/CoreServices.h>
#include <cstdlib>
#include <spirv_cross/spirv_msl.hpp>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace p3d { namespace gfx {

	namespace {

		MTLPixelFormat ToMTL(const Format f)
		{
			switch (f)
			{
			case Format::R8: return MTLPixelFormatR8Unorm;
			case Format::RG8: return MTLPixelFormatRG8Unorm;
			case Format::RGBA8: return MTLPixelFormatRGBA8Unorm;
			case Format::RGBA8_sRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
			case Format::BGRA8: return MTLPixelFormatBGRA8Unorm;
			case Format::BGRA8_sRGB: return MTLPixelFormatBGRA8Unorm_sRGB;
			case Format::R16F: return MTLPixelFormatR16Float;
			case Format::RG16F: return MTLPixelFormatRG16Float;
			case Format::RGBA16F: return MTLPixelFormatRGBA16Float;
			case Format::R32F: return MTLPixelFormatR32Float;
			case Format::RG32F: return MTLPixelFormatRG32Float;
			case Format::RGBA32F: return MTLPixelFormatRGBA32Float;
			case Format::R11G11B10F: return MTLPixelFormatRG11B10Float;
			case Format::RGB10A2: return MTLPixelFormatRGB10A2Unorm;
			case Format::R32U: return MTLPixelFormatR32Uint;
			case Format::RG32U: return MTLPixelFormatRG32Uint;
			case Format::BC1: return MTLPixelFormatBC1_RGBA;
			case Format::BC1_sRGB: return MTLPixelFormatBC1_RGBA_sRGB;
			case Format::BC3: return MTLPixelFormatBC3_RGBA;
			case Format::BC3_sRGB: return MTLPixelFormatBC3_RGBA_sRGB;
			case Format::BC4: return MTLPixelFormatBC4_RUnorm;
			case Format::BC5: return MTLPixelFormatBC5_RGUnorm;
			case Format::BC6H: return MTLPixelFormatBC6H_RGBUfloat;
			case Format::BC7: return MTLPixelFormatBC7_RGBAUnorm;
			case Format::BC7_sRGB: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
			case Format::D32F: return MTLPixelFormatDepth32Float;
			case Format::D16: return MTLPixelFormatDepth16Unorm;
			case Format::D24S8: return MTLPixelFormatDepth32Float_Stencil8;      // (no 24-bit depth on Apple silicon)
			case Format::D32FS8: return MTLPixelFormatDepth32Float_Stencil8;
			default: return MTLPixelFormatInvalid;
			}
		}
		uint32_t BlockBytes(const Format f, uint32_t &blockDim)
		{
			blockDim = 1;
			switch (f)
			{
			case Format::R8: return 1;
			case Format::RG8: case Format::R16F: return 2;
			case Format::RGBA8: case Format::RGBA8_sRGB: case Format::BGRA8: case Format::BGRA8_sRGB: case Format::RG16F: case Format::R32F:
			case Format::R11G11B10F: case Format::RGB10A2: case Format::R32U: return 4;
			case Format::RGBA16F: case Format::RG32F: case Format::RG32U: return 8;
			case Format::RGBA32F: return 16;
			case Format::BC1: case Format::BC1_sRGB: case Format::BC4: blockDim = 4; return 8;
			case Format::BC3: case Format::BC3_sRGB: case Format::BC5: case Format::BC6H: case Format::BC7: case Format::BC7_sRGB: blockDim = 4; return 16;
			default: return 4;
			}
		}
		bool IsDepth(const Format f) { return f == Format::D32F || f == Format::D16 || f == Format::D24S8 || f == Format::D32FS8; }
		MTLCompareFunction ToMTL(const Compare c)
		{
			switch (c)
			{
			case Compare::Never: return MTLCompareFunctionNever;
			case Compare::Less: return MTLCompareFunctionLess;
			case Compare::LessEqual: return MTLCompareFunctionLessEqual;
			case Compare::Equal: return MTLCompareFunctionEqual;
			case Compare::Greater: return MTLCompareFunctionGreater;
			case Compare::GreaterEqual: return MTLCompareFunctionGreaterEqual;
			default: return MTLCompareFunctionAlways;
			}
		}
		MTLSamplerAddressMode ToMTL(const Address a)
		{
			switch (a)
			{
			case Address::Clamp: return MTLSamplerAddressModeClampToEdge;
			case Address::Border: return MTLSamplerAddressModeClampToZero;
			case Address::Mirror: return MTLSamplerAddressModeMirrorRepeat;
			default: return MTLSamplerAddressModeRepeat;
			}
		}
		MTLVertexFormat VertexFormat(const Format f)
		{
			switch (f)
			{
			case Format::R32F: return MTLVertexFormatFloat;
			case Format::RG32F: return MTLVertexFormatFloat2;
			case Format::RGB32F: return MTLVertexFormatFloat3;
			case Format::RGBA32F: return MTLVertexFormatFloat4;
			case Format::RGBA8: return MTLVertexFormatUChar4Normalized;
			case Format::RGBA16F: return MTLVertexFormatHalf4;
			case Format::R32U: return MTLVertexFormatUInt;
			default: return MTLVertexFormatFloat3;      // (Unknown: three floats - as the other backends)
			}
		}
		void BlendState(const Blend b, MTLRenderPipelineColorAttachmentDescriptor* c)
		{
			c.blendingEnabled = b != Blend::Opaque;
			c.rgbBlendOperation = c.alphaBlendOperation = MTLBlendOperationAdd;
			switch (b)
			{
			case Blend::Alpha: c.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha; c.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				c.sourceAlphaBlendFactor = MTLBlendFactorOne; c.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha; break;
			case Blend::PremultipliedAlpha: c.sourceRGBBlendFactor = c.sourceAlphaBlendFactor = MTLBlendFactorOne;
				c.destinationRGBBlendFactor = c.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha; break;
			case Blend::Additive: c.sourceRGBBlendFactor = c.sourceAlphaBlendFactor = MTLBlendFactorOne; c.destinationRGBBlendFactor = c.destinationAlphaBlendFactor = MTLBlendFactorOne; break;
			case Blend::Multiply: c.sourceRGBBlendFactor = c.sourceAlphaBlendFactor = MTLBlendFactorDestinationColor; c.destinationRGBBlendFactor = c.destinationAlphaBlendFactor = MTLBlendFactorZero; break;
			default: break;
			}
		}

		template <typename T> struct Pool {
			// (a deque: what Get gave stays where it is when more are added - as the Vulkan backend's)
			std::deque<T> items; std::vector<uint32_t> free; std::mutex m;
			uint32_t Add(const T &t) { std::lock_guard<std::mutex> l(m); if (!free.empty()) { uint32_t i = free.back(); free.pop_back(); items[i] = t; return i + 1; } items.push_back(t); return (uint32_t)items.size(); }
			T* Get(uint32_t id) { return (id && id <= items.size()) ? &items[id - 1] : nullptr; }
			void Remove(uint32_t id) { std::lock_guard<std::mutex> l(m); if (id && id <= items.size()) { items[id - 1] = T(); free.push_back(id - 1); } }
		};

		struct BufferRec { id<MTLBuffer> buffer = nil; uint64_t size = 0; Memory memory = Memory::GpuOnly; };
		struct TextureRec { id<MTLTexture> texture = nil; TextureDesc desc; bool backbuffer = false; };
		struct SamplerRec { id<MTLSamplerState> sampler = nil; };
		struct Slots { std::map<uint32_t, uint32_t> buffer, texture, sampler; int rayScene = -1; };     // (group 1 binding -> this stage's Metal index; rayScene: where its accelerationStructureEXT is, -1 none)
		struct RayGeometryRec { id<MTLAccelerationStructure> accel = nil; };
		struct ShaderRec { id<MTLFunction> function = nil; Stage stage = Stage::Vertex; MTLSize threads = MTLSizeMake(64, 1, 1); bool usesDrawData = false; Slots slots; std::string error; };
		struct PipelineRec {
			std::shared_ptr<std::atomic<int>> state = std::make_shared<std::atomic<int>>(0);    // 0 compiling, 1 ready, 2 failed
			id<MTLRenderPipelineState> render = nil; id<MTLComputePipelineState> compute = nil;
			id<MTLDepthStencilState> depth = nil;
			MTLCullMode cull = MTLCullModeBack; MTLWinding winding = MTLWindingCounterClockwise; MTLPrimitiveType primitive = MTLPrimitiveTypeTriangle;
			float biasConstant = 0.f, biasSlope = 0.f;
			MTLSize threads = MTLSizeMake(64, 1, 1);
			std::vector<BindingDesc> material;
			bool isCompute = false;
			bool vertexDrawData = true, fragmentDrawData = true;     // (which stages read the draw's data: only those are given its offset)
			Slots vertexSlots, fragmentSlots;                        // (each stage's Metal index for each of group 1's bindings)
		};
		struct BindGroupRec {
			struct Entry { uint32_t binding; BindingKind kind; id<MTLTexture> texture; id<MTLSamplerState> sampler; id<MTLBuffer> buffer; uint64_t offset; bool vertex, fragment, dynamic; };
			std::vector<Entry> entries;
		};

		const uint32_t kFrames = 3;
		const uint32_t kDrawDataSlot = 256;
		const uint64_t kDrawDataBytes = 8ull << 20;
		const uint32_t kSlotChunk = 64;
		const uint32_t kDrawDataIndex = 1, kMaterialBufferBase = 2, kVertexBufferBase = 16;

		class MetalDevice;

		class MetalCommandList : public CommandList {
		public:
			MetalDevice* dev = nullptr;
			id<MTLCommandBuffer> cmd = nil;
			id<MTLRenderCommandEncoder> render = nil;
			id<MTLComputeCommandEncoder> compute = nil;
			id<MTLBlitCommandEncoder> blit = nil;
			PipelineRec* current = nullptr;
			bool skipping = true;
			id<MTLBuffer> indexBuffer = nil; uint64_t indexOffset = 0; MTLIndexType indexType = MTLIndexTypeUInt16;
			uint64_t slotNext = 0, slotEnd = 0, drawOffset = 0; bool haveDrawData = false;
			uint32_t lDraws = 0, lSkipped = 0, lPipelines = 0, lBindGroups = 0, lDispatches = 0; uint64_t lDrawDataBytes = 0;
			std::string name;

			void EndEncoders() { if (render) { [render endEncoding]; render = nil; } if (compute) { [compute endEncoding]; compute = nil; } if (blit) { [blit endEncoding]; blit = nil; } }
			id<MTLBlitCommandEncoder> Blit() { if (!blit) { EndEncoders(); blit = [cmd blitCommandEncoder]; } return blit; }
			id<MTLComputeCommandEncoder> Compute() { if (!compute) { EndEncoders(); compute = [cmd computeCommandEncoder]; } return compute; }
			void Barriers(const Barrier*, uint32_t) override {}      // (hazard-tracked resources: Metal's own)
			void BeginPass(const PassDesc &p) override;
			void EndPass() override { if (render) { [render endEncoding]; render = nil; } }
			void SetViewport(float x, float y, float w, float h, float mn, float mx) override { if (render) [render setViewport:(MTLViewport){ x, y, w, h, mn, mx }]; }
			void SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override { if (render) [render setScissorRect:(MTLScissorRect){ (NSUInteger)std::max(x, 0), (NSUInteger)std::max(y, 0), w, h }]; }
			void SetPipeline(Pipeline p) override;
			void DrawData(const void* data, uint32_t size) override;
			void SetBindGroup(BindGroup g, const uint32_t* dynamicOffsets, uint32_t dynamicCount) override;
			void SetVertexBuffer(uint32_t binding, Buffer b, uint64_t offset) override;
			void SetIndexBuffer(Buffer b, uint64_t offset, bool index32) override;
			void Draw(uint32_t v, uint32_t i, uint32_t fv, uint32_t fi) override;
			void DrawIndexed(uint32_t n, uint32_t i, uint32_t fi, int32_t vo, uint32_t fin) override;
			void DrawIndexedIndirect(Buffer args, uint64_t offset, uint32_t drawCount, Buffer count, uint64_t countOffset) override;
			void Dispatch(uint32_t x, uint32_t y, uint32_t z) override;
			void DispatchIndirect(Buffer args, uint64_t offset) override;
			void CopyBuffer(Buffer from, uint64_t fromOffset, Buffer to, uint64_t toOffset, uint64_t size) override;
			void CopyTexture(Texture from, Texture to) override;
			void Timestamp(QueryPool, uint32_t) override {}          // (GPU times are each list's command buffer's own)
			void BeginMarker(const char* n) override { if (render) [render pushDebugGroup:@(n ? n : "")]; else if (compute) [compute pushDebugGroup:@(n ? n : "")]; }
			void EndMarker() override { if (render) [render popDebugGroup]; else if (compute) [compute popDebugGroup]; }
			bool Ready();
			void BindDrawData();
		};

		class MetalDevice : public Device {
		public:
			id<MTLDevice> device = nil;
			id<MTLCommandQueue> queue = nil;
			CAMetalLayer* layer = nil;
			id<CAMetalDrawable> drawable = nil;
			dispatch_semaphore_t inFlight;
			Capabilities caps;
			Pool<BufferRec> buffers; Pool<TextureRec> textures; Pool<SamplerRec> samplers; Pool<ShaderRec> shaders; Pool<PipelineRec> pipelines; Pool<BindGroupRec> groups;
			// Ray queries: each geometry's own structure, and the frame's scene over them (a slot a frame in flight - with
			// the geometries it reaches, kept alive and made resident while it is traced)
			Pool<RayGeometryRec> rayGeometries;
			id<MTLAccelerationStructure> rayScenes[kFrames] = {}; id<MTLBuffer> rayInstances[kFrames] = {}; uint32_t rayInstanceCapacity[kFrames] = {};
			NSMutableArray<id<MTLAccelerationStructure>>* rayUsed[kFrames] = {};
			int raySlot = -1; RayGeometry rayNothing;
			id<MTLBuffer> drawRing = nil;
			std::atomic<uint64_t> frameCursor{ 0 };
			uint64_t frameBase = 0;
			uint32_t frame = 0; uint64_t frameNumber = 0;
			uint32_t backbufferId = 0;
			bool frameOpen = false;     // (a frame's slot taken and not yet given back by Present)
			std::mutex listsMutex; std::vector<MetalCommandList*> lists, retired[kFrames];
			std::vector<id> doomed[kFrames];     // (destroyed in that frame: let go when its slot is taken again)
			// (the GPU's own times: each list's command buffer reports its start and end when it finishes)
			struct Timed { std::string name; double gpuMs; };
			std::mutex statsMutex; std::vector<Timed> finished[kFrames]; std::vector<Timed> pending; uint64_t finishedFrame = 0;
			// (and each render pass's: a timestamp at the start of its vertex stage and the end of its fragment stage,
			// where the GPU can sample at stage boundaries - Apple's GPUs)
			static const uint32_t kTimedPasses = 64;
			id<MTLCounterSampleBuffer> passSamples[kFrames] = {};
			std::atomic<uint32_t> passCount[kFrames];
			std::vector<std::string> passNames[kFrames];
			bool passTiming = false;
			MTLTimestamp calibCpu = 0, calibGpu = 0;
			double frameGpuStart[kFrames] = {};     // (seconds: when the GPU began the frame's first command buffer - a first pass is timed from it)
			FrameStats stats;
			std::atomic<uint32_t> pipelinesPending{ 0 }, pipelinesFinished{ 0 };
			std::atomic<uint32_t> aDraws{ 0 }, aSkipped{ 0 }, aPipelines{ 0 }, aGroups{ 0 }, aDispatches{ 0 }; std::atomic<uint64_t> aDrawData{ 0 };

			bool Init(const SurfaceSource &s)
			{
				if (s.kind != SurfaceSource::MetalLayer || !s.a) { fprintf(stderr, "Metal device: needs a CAMetalLayer\n"); return false; }
				layer = (__bridge CAMetalLayer*)s.a;
				device = layer.device ? layer.device : MTLCreateSystemDefaultDevice();
				if (!device) return false;
				layer.device = device;
				// (sRGB, as the other backends' swapchains: shaders write linear values and the store encodes them)
				layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
				// (PYROS_HDR=1: half floats in extended linear sRGB - 1 is SDR white, more is brighter, as far as the screen's
				// EDR headroom goes; shaders still write linear values, nothing to encode)
				const char* hdrEnv = getenv("PYROS_HDR");
				if (hdrEnv && atoi(hdrEnv) != 0)
				{
					hdrOutput = true;
					layer.pixelFormat = MTLPixelFormatRGBA16Float;
					layer.wantsExtendedDynamicRangeContent = YES;
					CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
					layer.colorspace = cs;
					CGColorSpaceRelease(cs);
					fprintf(stderr, "Metal device: HDR output (extended linear sRGB), headroom now %.2f, this screen up to %.2f\n", OutputHeadroom(), NSScreen.mainScreen ? (float)NSScreen.mainScreen.maximumPotentialExtendedDynamicRangeColorComponentValue : 1.f);
				}
				layer.framebufferOnly = NO;
				layer.drawableSize = CGSizeMake(s.width, s.height);
				layer.displaySyncEnabled = s.vsync;
				layer.maximumDrawableCount = 3;
				queue = [device newCommandQueueWithMaxCommandBufferCount:256];
				inFlight = dispatch_semaphore_create(kFrames);
				drawRing = [device newBufferWithLength:kDrawDataBytes * kFrames options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
				drawRing.label = @"Device.DrawData";
				caps.name = std::string("Metal (") + [device.name UTF8String] + ")";
				caps.bc = device.supportsBCTextureCompression;
				caps.timestamps = true;
				// (ray queries where the GPU traces rays itself: Apple silicon's, macOS 13 on - PYROS_RAY_QUERY=0 to try without)
				if (@available(macOS 13.0, *)) caps.rayQuery = device.supportsRaytracing && !(getenv("PYROS_RAY_QUERY") && getenv("PYROS_RAY_QUERY")[0] == '0');
				for (uint32_t f = 0; f < kFrames; f++) passCount[f].store(0);
				if (@available(macOS 11.0, iOS 14.0, *))
				{
					id<MTLCounterSet> timestamps = nil;
					for (id<MTLCounterSet> cs in device.counterSets) if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp]) timestamps = cs;
					if (timestamps && [device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
					{
						MTLCounterSampleBufferDescriptor* d = [MTLCounterSampleBufferDescriptor new];
						d.counterSet = timestamps; d.storageMode = MTLStorageModeShared; d.sampleCount = kTimedPasses * 2;
						passTiming = true;
						for (uint32_t f = 0; f < kFrames && passTiming; f++)
						{
							passSamples[f] = [device newCounterSampleBufferWithDescriptor:d error:nil];
							if (!passSamples[f]) passTiming = false;
						}
						[device sampleTimestamps:&calibCpu gpuTimestamp:&calibGpu];
					}
				}
				caps.multiDrawIndirect = false; caps.indirectCount = false; caps.bindless = false;
				caps.maxPushConstantBytes = 4096;          // (setBytes: small data inline in the encoder)
				TextureRec back; back.backbuffer = true; back.desc.format = hdrOutput ? Format::RGBA16F : Format::BGRA8; back.desc.usage = TexColorTarget;
				backbufferId = textures.Add(back);
				return true;
			}

			const Capabilities &Caps() const override { return caps; }

			RayGeometry CreateRayGeometry(const float* positions, uint32_t vertexCount, uint32_t strideFloats, const uint32_t* indices, uint32_t indexCount) override
			{
				if (!caps.rayQuery || !positions || !indices || vertexCount == 0 || indexCount < 3) return RayGeometry();
				if (@available(macOS 13.0, *))
				{
					@autoreleasepool
					{
						// (packed tight: three floats a vertex, whatever the caller's stride)
						std::vector<float> packed((size_t)vertexCount * 3);
						for (uint32_t v = 0; v < vertexCount; v++) for (uint32_t k = 0; k < 3; k++) packed[(size_t)v * 3 + k] = positions[(size_t)v * strideFloats + k];
						id<MTLBuffer> vb = [device newBufferWithBytes:packed.data() length:packed.size() * sizeof(float) options:MTLResourceStorageModeShared];
						id<MTLBuffer> ib = [device newBufferWithBytes:indices length:(NSUInteger)(indexCount / 3) * 3 * sizeof(uint32_t) options:MTLResourceStorageModeShared];
						MTLAccelerationStructureTriangleGeometryDescriptor* tri = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
						tri.vertexBuffer = vb; tri.vertexStride = 3 * sizeof(float); tri.vertexFormat = MTLAttributeFormatFloat3;
						tri.indexBuffer = ib; tri.indexType = MTLIndexTypeUInt32; tri.triangleCount = indexCount / 3; tri.opaque = YES;
						MTLPrimitiveAccelerationStructureDescriptor* desc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
						desc.geometryDescriptors = @[ tri ];
						MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:desc];
						id<MTLAccelerationStructure> accel = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
						id<MTLBuffer> scratch = [device newBufferWithLength:std::max<NSUInteger>(sizes.buildScratchBufferSize, 16) options:MTLResourceStorageModePrivate];
						if (!accel || !scratch) return RayGeometry();
						id<MTLCommandBuffer> cb = [queue commandBuffer];
						cb.label = @"Device.RayGeometry";
						id<MTLAccelerationStructureCommandEncoder> enc = [cb accelerationStructureCommandEncoder];
						[enc buildAccelerationStructure:accel descriptor:desc scratchBuffer:scratch scratchBufferOffset:0];
						[enc endEncoding];
						// (committed now: ahead, on the one queue, of every frame that traces it)
						[cb commit];
						RayGeometryRec r; r.accel = accel;
						RayGeometry g; g.id = rayGeometries.Add(r); return g;
					}
				}
				return RayGeometry();
			}
			void Destroy(RayGeometry g) override { rayGeometries.Remove(g.id); }     // (a scene built over it holds it until its slot is built again)
			void SetRayScene(const RayInstance* instances, uint32_t count) override
			{
				if (!caps.rayQuery) return;
				if (@available(macOS 13.0, *))
				{
					@autoreleasepool
					{
						if (!rayNothing)
						{
							// (one triangle far below everything: a scene to trace always exists, and an instance whose geometry
							// is gone keeps its place - the shaders find an instance's own data by its index)
							const float p[9] = { 0.f, -1e7f, 0.f, 1.f, -1e7f, 0.f, 0.f, -1e7f, 1.f };
							const uint32_t ix[3] = { 0, 1, 2 };
							rayNothing = CreateRayGeometry(p, 3, 3, ix, 3);
						}
						NSMutableArray<id<MTLAccelerationStructure>>* used = [NSMutableArray array];
						std::map<uint32_t, uint32_t> indexOf;
						std::vector<MTLAccelerationStructureInstanceDescriptor> descs;
						descs.reserve(std::max(count, 1u));
						auto add = [&](RayGeometry geometry, const float* t)
						{
							RayGeometryRec* g = rayGeometries.Get(geometry.id);
							if (!g || !g->accel) { g = rayGeometries.Get(rayNothing.id); geometry = rayNothing; }
							if (!g || !g->accel) return;
							std::map<uint32_t, uint32_t>::iterator at = indexOf.find(geometry.id);
							uint32_t index;
							if (at == indexOf.end()) { index = (uint32_t)used.count; indexOf[geometry.id] = index; [used addObject:g->accel]; }
							else index = at->second;
							MTLAccelerationStructureInstanceDescriptor d;
							memset(&d, 0, sizeof(d));
							// (the 3x4 rows into Metal's four columns of three)
							for (uint32_t c = 0; c < 4; c++) { d.transformationMatrix.columns[c].x = t[c]; d.transformationMatrix.columns[c].y = t[4 + c]; d.transformationMatrix.columns[c].z = t[8 + c]; }
							d.options = MTLAccelerationStructureInstanceOptionOpaque | MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
							d.mask = 0xFF; d.accelerationStructureIndex = index;
							descs.push_back(d);
						};
						for (uint32_t i = 0; i < count; i++) add(instances[i].geometry, instances[i].transform);
						if (descs.empty()) { const float id3x4[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }; add(rayNothing, id3x4); }
						if (descs.empty()) return;
						const uint32_t slot = frame, n = (uint32_t)descs.size();
						if (rayInstanceCapacity[slot] < n)
						{
							const uint32_t cap = n + n / 4 + 16;
							rayInstances[slot] = [device newBufferWithLength:(NSUInteger)cap * sizeof(MTLAccelerationStructureInstanceDescriptor) options:MTLResourceStorageModeShared];
							rayInstanceCapacity[slot] = cap;
						}
						memcpy(rayInstances[slot].contents, descs.data(), (size_t)n * sizeof(MTLAccelerationStructureInstanceDescriptor));
						MTLInstanceAccelerationStructureDescriptor* desc = [MTLInstanceAccelerationStructureDescriptor descriptor];
						desc.instancedAccelerationStructures = used;
						desc.instanceCount = n;
						desc.instanceDescriptorBuffer = rayInstances[slot];
						desc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;
						MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:desc];
						if (!rayScenes[slot] || rayScenes[slot].size < sizes.accelerationStructureSize)
							rayScenes[slot] = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize + sizes.accelerationStructureSize / 4];
						if (!rayScenes[slot]) return;
						id<MTLBuffer> scratch = [device newBufferWithLength:std::max<NSUInteger>(sizes.buildScratchBufferSize, 16) options:MTLResourceStorageModePrivate];
						id<MTLCommandBuffer> cb = [queue commandBuffer];
						cb.label = @"Device.RayScene";
						id<MTLAccelerationStructureCommandEncoder> enc = [cb accelerationStructureCommandEncoder];
						[enc buildAccelerationStructure:rayScenes[slot] descriptor:desc scratchBuffer:scratch scratchBufferOffset:0];
						[enc endEncoding];
						[cb commit];
						rayUsed[slot] = used;
						raySlot = (int)slot;
					}
				}
			}
			// (the frame's scene where a pipeline's stage traces it - with the geometries it reaches made resident)
			void BindRayScene(id<MTLRenderCommandEncoder> render, id<MTLComputeCommandEncoder> compute, int index)
			{
				if (index < 0 || !caps.rayQuery) return;
				if (@available(macOS 13.0, *))
				{
					if (raySlot < 0) SetRayScene(nullptr, 0);
					if (raySlot < 0 || !rayScenes[raySlot]) return;
					NSArray<id<MTLAccelerationStructure>>* used = rayUsed[raySlot];
					if (render)
					{
						[render setFragmentAccelerationStructure:rayScenes[raySlot] atBufferIndex:(NSUInteger)index];
						for (id<MTLAccelerationStructure> a in used) [render useResource:a usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
					}
					if (compute)
					{
						[compute setAccelerationStructure:rayScenes[raySlot] atBufferIndex:(NSUInteger)index];
						for (id<MTLAccelerationStructure> a in used) [compute useResource:a usage:MTLResourceUsageRead];
					}
				}
			}
			Format BackbufferFormat() const override
			{
				switch (layer.pixelFormat)
				{
				case MTLPixelFormatBGRA8Unorm: return Format::BGRA8;
				case MTLPixelFormatRGBA8Unorm: return Format::RGBA8;
				case MTLPixelFormatRGBA8Unorm_sRGB: return Format::RGBA8_sRGB;
				case MTLPixelFormatRGBA16Float: return Format::RGBA16F;
				default: return Format::BGRA8_sRGB;
				}
			}
			bool hdrOutput = false;
			float OutputHeadroom() const override
			{
				if (!hdrOutput) return 1.f;
#if TARGET_OS_OSX
				// (it changes with the screen's brightness: asked each frame)
				NSScreen* screen = NSScreen.mainScreen;
				// (EDR's headroom opens once EDR content is on the screen - 1 at first, more a frame or two later)
				const float h = screen ? std::max(1.f, (float)screen.maximumExtendedDynamicRangeColorComponentValue) : 1.f;
				static float said = 1.f;
				if (fabsf(h - said) > 0.25f) { said = h; fprintf(stderr, "Metal device: HDR headroom %.2f (potential %.2f)\n", h, screen ? (float)screen.maximumPotentialExtendedDynamicRangeColorComponentValue : 1.f); }
				return h;
#else
				return 1.f;
#endif
			}
			bool ReadTexture(Texture t, std::vector<uint8_t> &out) override
			{
				TextureRec* r = textures.Get(t.id);
				if (!r || !r->texture) return false;
				const uint32_t bpt = BytesPerTexel(r->desc.format);
				if (!bpt || r->texture.sampleCount > 1) return false;
				const uint32_t w = (uint32_t)r->texture.width, h = (uint32_t)r->texture.height;
				@autoreleasepool
				{
					// (one queue: what was committed before is done before this blit)
					id<MTLBuffer> buf = [device newBufferWithLength:(NSUInteger)w * h * bpt options:MTLResourceStorageModeShared];
					id<MTLCommandBuffer> cb = [queue commandBuffer];
					id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
					[be copyFromTexture:r->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(w, h, 1)
						toBuffer:buf destinationOffset:0 destinationBytesPerRow:w * bpt destinationBytesPerImage:w * h * bpt];
					[be endEncoding];
					[cb commit];
					[cb waitUntilCompleted];
					const uint8_t* p = (const uint8_t*)buf.contents;
					out.assign(p, p + (size_t)w * h * bpt);
				}
				return true;
			}

			Buffer CreateBuffer(const BufferDesc &d, const void* initial) override
			{
				BufferRec r; r.size = d.size; r.memory = d.memory;
				// (Apple silicon's memory is the CPU's: every buffer shared - an upload is a memcpy, no staging)
				MTLResourceOptions o = MTLResourceStorageModeShared | (d.memory == Memory::Upload ? MTLResourceCPUCacheModeWriteCombined : 0);
				r.buffer = initial ? [device newBufferWithBytes:initial length:std::max<uint64_t>(d.size, 4) options:o] : [device newBufferWithLength:std::max<uint64_t>(d.size, 4) options:o];
				if (d.debugName) r.buffer.label = @(d.debugName);
				Buffer b; b.id = buffers.Add(r); return b;
			}
			Texture CreateTexture(const TextureDesc &d, const TextureData* data, uint32_t count) override
			{
				MTLTextureDescriptor* td = [MTLTextureDescriptor new];
				td.pixelFormat = ToMTL(d.format);
				td.width = d.width; td.height = d.height; td.mipmapLevelCount = std::max(1u, d.mips);
				td.sampleCount = std::max(1u, d.samples);
				switch (d.kind)
				{
				case TextureKind::Tex2DArray: td.textureType = MTLTextureType2DArray; td.arrayLength = d.depthOrLayers; break;
				case TextureKind::Cube: td.textureType = MTLTextureTypeCube; break;
				case TextureKind::Tex3D: td.textureType = MTLTextureType3D; td.depth = d.depthOrLayers; break;
				default: td.textureType = d.samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D; break;
				}
				MTLTextureUsage u = 0;
				if (d.usage & TexSampled) u |= MTLTextureUsageShaderRead;
				if (d.usage & TexStorage) u |= MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
				if (d.usage & (TexColorTarget | TexDepthTarget)) u |= MTLTextureUsageRenderTarget;
				td.usage = u ? u : MTLTextureUsageShaderRead;
				// (a target lives on the GPU; transient ones in tile memory where the frame graph says so)
				td.storageMode = (d.usage & (TexColorTarget | TexDepthTarget)) || IsDepth(d.format) ? (d.transient ? MTLStorageModeMemoryless : MTLStorageModePrivate) : MTLStorageModeShared;
				TextureRec r; r.desc = d; r.texture = [device newTextureWithDescriptor:td];
				if (!r.texture) { fprintf(stderr, "Metal device: texture %ux%u could not be made\n", d.width, d.height); return Texture(); }
				if (d.debugName) r.texture.label = @(d.debugName);
				Texture t; t.id = textures.Add(r);
				if (data && count) Upload(t, data, count);
				return t;
			}
			Sampler CreateSampler(const SamplerDesc &d) override
			{
				MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
				sd.minFilter = d.min == Filter::Linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
				sd.magFilter = d.mag == Filter::Linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
				sd.mipFilter = d.mip == Filter::Linear ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
				sd.sAddressMode = ToMTL(d.u); sd.tAddressMode = ToMTL(d.v); sd.rAddressMode = ToMTL(d.w);
				sd.maxAnisotropy = (NSUInteger)std::max(1.f, std::min(16.f, d.anisotropy));
				if (d.compare) sd.compareFunction = MTLCompareFunctionGreaterEqual;   // (reversed Z)
				sd.supportArgumentBuffers = YES;
				SamplerRec r; r.sampler = [device newSamplerStateWithDescriptor:sd];
				Sampler s; s.id = samplers.Add(r); return s;
			}
			// (the lists' command buffers don't retain what they use: what is destroyed is kept until its frame's slot comes back,
			// by when the GPU is done with it - freed under it, it is an address fault, and then Metal ignores the process's work)
			void Destroy(Buffer b) override { if (BufferRec* r = buffers.Get(b.id)) Doom(r->buffer); buffers.Remove(b.id); }
			void Destroy(Texture t) override { if (TextureRec* r = textures.Get(t.id)) if (!r->backbuffer) Doom(r->texture); textures.Remove(t.id); }
			void Doom(id o) { if (!o) return; std::lock_guard<std::mutex> l(listsMutex); doomed[frame].push_back(o); }
			BindGroup CreateBindGroup(Pipeline layoutOf, const BindGroupEntry* e, uint32_t count) override
			{
				PipelineRec* p = pipelines.Get(layoutOf.id);
				BindGroupRec g;
				for (uint32_t i = 0; i < count; i++)
				{
					BindGroupRec::Entry en = { e[i].binding, BindingKind::Texture2D, nil, nil, nil, e[i].offset, true, true, false };
					if (p) for (const BindingDesc &bd : p->material) if (bd.binding == e[i].binding) { en.kind = bd.kind; en.vertex = bd.vertex || bd.compute; en.fragment = bd.fragment; en.dynamic = bd.dynamic; }
					if (TextureRec* t = textures.Get(e[i].texture.id)) en.texture = t->texture;
					if (SamplerRec* s = samplers.Get(e[i].sampler.id)) { en.sampler = s->sampler; if (!en.texture && en.kind != BindingKind::ComparisonSampler) en.kind = BindingKind::Sampler; }
					if (BufferRec* b = buffers.Get(e[i].buffer.id)) { en.buffer = b->buffer; if (en.kind != BindingKind::StorageRead && en.kind != BindingKind::StorageReadWrite) en.kind = BindingKind::Uniform; }
					g.entries.push_back(en);
				}
				// (in binding order: a SetBindGroup's dynamic offsets go to its dynamic entries in that order)
				std::sort(g.entries.begin(), g.entries.end(), [](const BindGroupRec::Entry &a, const BindGroupRec::Entry &b) { return a.binding < b.binding; });
				BindGroup bg; bg.id = groups.Add(g); return bg;
			}
			void Destroy(BindGroup g) override
			{
				if (BindGroupRec* r = groups.Get(g.id)) for (const BindGroupRec::Entry &e : r->entries) { Doom(e.texture); Doom(e.buffer); Doom(e.sampler); }
				groups.Remove(g.id);
			}
			BindlessIndex Index(Texture, Sampler) override { return kNoIndex; }
			BindlessIndex Index(Buffer) override { return kNoIndex; }
			void* Map(Buffer upload, uint64_t &offsetOut, uint64_t) override { offsetOut = 0; BufferRec* b = buffers.Get(upload.id); return b ? b->buffer.contents : nullptr; }
			void Upload(Buffer to, uint64_t offset, const void* data, uint64_t size) override
			{
				BufferRec* b = buffers.Get(to.id);
				if (b && offset + size <= b->size) memcpy((uint8_t*)b->buffer.contents + offset, data, size);
			}
			void Upload(Texture to, const TextureData* data, uint32_t count) override
			{
				TextureRec* t = textures.Get(to.id);
				if (!t || !t->texture) return;
				uint32_t dim; const uint32_t bb = BlockBytes(t->desc.format, dim);
				for (uint32_t i = 0; i < count; i++)
				{
					const uint32_t w = std::max(1u, t->desc.width >> data[i].mip), h = std::max(1u, t->desc.height >> data[i].mip);
					const uint32_t rowBytes = ((w + dim - 1) / dim) * bb;
					// (a 3D texture's `layer` is its depth slice - at a z, in slice 0; an array's is its slice)
					const bool volume = t->desc.kind == TextureKind::Tex3D;
					const uint32_t z = volume ? data[i].layer : 0, slice = volume ? 0 : data[i].layer;
					MTLRegion region = MTLRegionMake3D(0, 0, z, w, h, 1);
					if (t->texture.storageMode == MTLStorageModeShared)
						[t->texture replaceRegion:region mipmapLevel:data[i].mip slice:slice withBytes:data[i].bytes bytesPerRow:rowBytes bytesPerImage:rowBytes * ((h + dim - 1) / dim)];
					else
					{
						// (a private texture: through a blit, on the queue ahead of the frame's lists)
						id<MTLBuffer> staging = [device newBufferWithBytes:data[i].bytes length:data[i].size options:MTLResourceStorageModeShared];
						id<MTLCommandBuffer> cb = [queue commandBuffer];
						id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
						[be copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:rowBytes sourceBytesPerImage:rowBytes * ((h + dim - 1) / dim) sourceSize:MTLSizeMake(w, h, 1)
							toTexture:t->texture destinationSlice:slice destinationLevel:data[i].mip destinationOrigin:MTLOriginMake(0, 0, z)];
						[be endEncoding]; [cb commit];
					}
				}
			}

			Shader CreateShader(const ShaderDesc &d) override
			{
				ShaderRec r; r.stage = d.stage;
				if (!d.spirv || !d.spirvWords) { fprintf(stderr, "Metal device: shader %s has no SPIR-V\n", d.debugName ? d.debugName : ""); Shader s; s.id = shaders.Add(r); return s; }
				try
				{
					spirv_cross::CompilerMSL msl(d.spirv, (size_t)d.spirvWords);
					spirv_cross::CompilerMSL::Options o; o.set_msl_version(3, 0); o.platform = spirv_cross::CompilerMSL::Options::macOS;
					msl.set_msl_options(o);
					spv::ExecutionModel model = d.stage == Stage::Fragment ? spv::ExecutionModelFragment : (d.stage == Stage::Compute ? spv::ExecutionModelGLCompute : spv::ExecutionModelVertex);
					// (where each resource lands - the same for every shader; see the file's head)
					spirv_cross::ShaderResources res = msl.get_shader_resources();
					// (group 1's bindings, by kind, numbered in binding order - kind 0 buffer, 1 texture, 2 sampler, 3 both)
					std::map<uint32_t, int> wanted;
					auto collect = [&](const spirv_cross::SmallVector<spirv_cross::Resource> &list, int kind) {
						for (const spirv_cross::Resource &rs : list)
							if (msl.get_decoration(rs.id, spv::DecorationDescriptorSet) != 0) wanted[msl.get_decoration(rs.id, spv::DecorationBinding)] = kind;
					};
					collect(res.uniform_buffers, 0); collect(res.storage_buffers, 0); collect(res.separate_images, 1); collect(res.storage_images, 1);
					collect(res.separate_samplers, 2); collect(res.sampled_images, 3);
					uint32_t nextBuffer = kMaterialBufferBase, nextTexture = 0, nextSampler = 0;
					for (const auto &w : wanted)
					{
						if (w.second == 0) r.slots.buffer[w.first] = nextBuffer++;
						if (w.second == 1 || w.second == 3) r.slots.texture[w.first] = nextTexture++;
						if (w.second == 2 || w.second == 3) r.slots.sampler[w.first] = nextSampler++;
					}
					if (nextBuffer > kVertexBufferBase) fprintf(stderr, "Metal device: shader %s reads %u buffers - more than fit under the vertex buffers\n", d.debugName ? d.debugName : "", nextBuffer - kMaterialBufferBase);
					auto bindAll = [&](const spirv_cross::SmallVector<spirv_cross::Resource> &list) {
						for (const spirv_cross::Resource &rs : list)
						{
							const uint32_t set = msl.get_decoration(rs.id, spv::DecorationDescriptorSet), binding = msl.get_decoration(rs.id, spv::DecorationBinding);
							spirv_cross::MSLResourceBinding rb; rb.stage = model; rb.desc_set = set; rb.binding = binding;
							if (set == 0) { rb.msl_buffer = kDrawDataIndex; r.usesDrawData = true; }
							else
							{
								if (r.slots.buffer.count(binding)) rb.msl_buffer = r.slots.buffer[binding];
								if (r.slots.texture.count(binding)) rb.msl_texture = r.slots.texture[binding];
								if (r.slots.sampler.count(binding)) rb.msl_sampler = r.slots.sampler[binding];
							}
							msl.add_msl_resource_binding(rb);
						}
					};
					bindAll(res.uniform_buffers); bindAll(res.storage_buffers); bindAll(res.separate_images); bindAll(res.storage_images);
					bindAll(res.separate_samplers); bindAll(res.sampled_images);
					// (the frame's scene for ray queries - its own set, 2: a buffer index after the material's)
					for (const spirv_cross::Resource &rs : res.acceleration_structures)
					{
						spirv_cross::MSLResourceBinding rb; rb.stage = model;
						rb.desc_set = msl.get_decoration(rs.id, spv::DecorationDescriptorSet); rb.binding = msl.get_decoration(rs.id, spv::DecorationBinding);
						rb.msl_buffer = nextBuffer++; r.slots.rayScene = (int)rb.msl_buffer;
						msl.add_msl_resource_binding(rb);
					}
					// (vertex inputs are [[attribute(location)]]: the pipeline's vertex descriptor puts their buffers at 16 + binding)
					const std::string source = msl.compile();
					if (d.stage == Stage::Compute)
					{
						const spirv_cross::SPIREntryPoint &ep = msl.get_entry_point(msl.get_entry_points_and_stages()[0].name, model);
						r.threads = MTLSizeMake(std::max(1u, ep.workgroup_size.x), std::max(1u, ep.workgroup_size.y), std::max(1u, ep.workgroup_size.z));
					}
					NSError* err = nil;
					MTLCompileOptions* co = [MTLCompileOptions new];
					co.mathMode = MTLMathModeFast;
					id<MTLLibrary> lib = [device newLibraryWithSource:@(source.c_str()) options:co error:&err];
					if (!lib) { r.error = err ? [[err localizedDescription] UTF8String] : "?"; fprintf(stderr, "Metal device: shader %s: %s\n", d.debugName ? d.debugName : "", r.error.c_str()); }
					else
					{
						const std::string entry = msl.get_cleansed_entry_point_name(msl.get_entry_points_and_stages()[0].name, model);
						r.function = [lib newFunctionWithName:@(entry.c_str())];
					}
				}
				catch (const std::exception &e) { r.error = e.what(); fprintf(stderr, "Metal device: shader %s: SPIRV-Cross: %s\n", d.debugName ? d.debugName : "", e.what()); }
				Shader s; s.id = shaders.Add(r); return s;
			}
			Pipeline CreatePipeline(const GraphicsPipelineDesc &d) override
			{
				PipelineRec r; r.material = d.material;
				r.cull = d.cull == Cull::None ? MTLCullModeNone : (d.cull == Cull::Front ? MTLCullModeFront : MTLCullModeBack);
				r.winding = d.frontCounterClockwise ? MTLWindingCounterClockwise : MTLWindingClockwise;
				r.primitive = d.topology == Topology::TriangleStrip ? MTLPrimitiveTypeTriangleStrip : (d.topology == Topology::Lines ? MTLPrimitiveTypeLine : (d.topology == Topology::Points ? MTLPrimitiveTypePoint : MTLPrimitiveTypeTriangle));
				r.biasConstant = d.depthBiasConstant; r.biasSlope = d.depthBiasSlope;
				MTLDepthStencilDescriptor* ds = [MTLDepthStencilDescriptor new];
				ds.depthCompareFunction = d.depthTest ? ToMTL(d.depthCompare) : MTLCompareFunctionAlways;
				ds.depthWriteEnabled = d.depthTest && d.depthWrite;
				r.depth = [device newDepthStencilStateWithDescriptor:ds];
				ShaderRec* vs = shaders.Get(d.vertex.id); ShaderRec* fs = shaders.Get(d.fragment.id);
				r.vertexDrawData = vs && vs->usesDrawData; r.fragmentDrawData = fs && fs->usesDrawData;
				if (vs) r.vertexSlots = vs->slots;
				if (fs) r.fragmentSlots = fs->slots;
				Pipeline p; p.id = pipelines.Add(r);
				PipelineRec* pr = pipelines.Get(p.id);
				if (!vs || !vs->function) { pr->state->store(2); return p; }
				MTLRenderPipelineDescriptor* pd = [MTLRenderPipelineDescriptor new];
				pd.vertexFunction = vs->function;
				pd.fragmentFunction = fs ? fs->function : nil;
				pd.rasterSampleCount = std::max<uint8_t>(1, d.samples);
				pd.alphaToCoverageEnabled = d.alphaToCoverage;
				for (uint8_t c = 0; c < d.colorCount; c++)
				{
					// (Unknown: the backbuffer's)
					pd.colorAttachments[c].pixelFormat = d.colorFormats[c] == Format::Unknown ? layer.pixelFormat : ToMTL(d.colorFormats[c]);
					BlendState(d.blend[c], pd.colorAttachments[c]);
					if (!d.colorWrite) pd.colorAttachments[c].writeMask = MTLColorWriteMaskNone;
				}
				if (d.depthFormat != Format::Unknown) pd.depthAttachmentPixelFormat = ToMTL(d.depthFormat);
				if (d.depthFormat == Format::D24S8 || d.depthFormat == Format::D32FS8) pd.stencilAttachmentPixelFormat = ToMTL(d.depthFormat);
				if (!d.bindings.empty())
				{
					MTLVertexDescriptor* vd = [MTLVertexDescriptor new];
					for (const VertexAttribute &a : d.attributes)
					{
						vd.attributes[a.location].format = VertexFormat(a.format);
						vd.attributes[a.location].offset = a.offset;
						vd.attributes[a.location].bufferIndex = kVertexBufferBase + a.binding;
					}
					for (const VertexBinding &b : d.bindings)
					{
						vd.layouts[kVertexBufferBase + b.binding].stride = b.stride;
						vd.layouts[kVertexBufferBase + b.binding].stepFunction = b.perInstance ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
					}
					pd.vertexDescriptor = vd;
				}
				if (d.debugName) pd.label = @(d.debugName);
				pipelinesPending++;
				std::shared_ptr<std::atomic<int>> state = pr->state;
				MetalDevice* self = this; const uint32_t pid = p.id;
				const std::string dbg = d.debugName ? d.debugName : "";
				// (on Metal's own compiler threads: the draw asks PipelineReady and skips until then)
				[device newRenderPipelineStateWithDescriptor:pd completionHandler:^(id<MTLRenderPipelineState> ps, NSError* err) {
					if (PipelineRec* q = self->pipelines.Get(pid)) q->render = ps;
					if (!ps) fprintf(stderr, "Metal device: pipeline %s: %s\n", dbg.c_str(), err ? [[err localizedDescription] UTF8String] : "?");
					state->store(ps ? 1 : 2);
					self->pipelinesPending--; self->pipelinesFinished++;
				}];
				return p;
			}
			Pipeline CreatePipeline(const ComputePipelineDesc &d) override
			{
				PipelineRec r; r.material = d.material; r.isCompute = true;
				ShaderRec* cs = shaders.Get(d.compute.id);
				if (cs) { r.threads = cs->threads; r.vertexSlots = cs->slots; }     // (a compute shader's numbering kept where the vertex stage's is)
				Pipeline p; p.id = pipelines.Add(r);
				PipelineRec* pr = pipelines.Get(p.id);
				if (!cs || !cs->function) { pr->state->store(2); return p; }
				pipelinesPending++;
				std::shared_ptr<std::atomic<int>> state = pr->state;
				MetalDevice* self = this; const uint32_t pid = p.id;
				[device newComputePipelineStateWithFunction:cs->function completionHandler:^(id<MTLComputePipelineState> ps, NSError* err) {
					if (PipelineRec* q = self->pipelines.Get(pid)) q->compute = ps;
					if (!ps) fprintf(stderr, "Metal device: compute pipeline: %s\n", err ? [[err localizedDescription] UTF8String] : "?");
					state->store(ps ? 1 : 2);
					self->pipelinesPending--; self->pipelinesFinished++;
				}];
				return p;
			}
			bool PipelineReady(Pipeline p) const override { PipelineRec* r = const_cast<MetalDevice*>(this)->pipelines.Get(p.id); return r && r->state->load() == 1; }
			void SavePipelineCache(const std::string &) override {}      // (Metal keeps its own compiled shader cache)

			void BeginFrame() override
			{
				// (begun again before it was presented - waiting for a pipeline, say: the same frame, its slot kept)
				if (frameOpen) return;
				dispatch_semaphore_wait(inFlight, DISPATCH_TIME_FOREVER);
				frameOpen = true;
				frame = (frame + 1) % kFrames; frameNumber++;
				frameBase = (uint64_t)frame * kDrawDataBytes;
				frameCursor.store(0);
				{
					std::lock_guard<std::mutex> l(listsMutex);
					for (MetalCommandList* c : retired[frame]) delete c;
					retired[frame].clear();
					doomed[frame].clear();
				}
				stats.frame = frameNumber;
				stats.draws = aDraws.exchange(0); stats.skippedDraws = aSkipped.exchange(0); stats.pipelineChanges = aPipelines.exchange(0);
				stats.bindGroupChanges = aGroups.exchange(0); stats.dispatches = aDispatches.exchange(0); stats.drawDataBytes = aDrawData.exchange(0);
				stats.pipelinesPending = pipelinesPending.load(); stats.pipelinesFinished = pipelinesFinished.exchange(0);
				{
					std::lock_guard<std::mutex> l(statsMutex);
					stats.passes.clear(); stats.gpuMs = 0.0;
					for (const Timed &t : finished[frame]) { FrameStats::Pass p; p.name = t.name; p.gpuMs = t.gpuMs; stats.passes.push_back(p); stats.gpuMs += t.gpuMs; }
				}
			}
			CommandList* NewCommandList(const char* debugName) override
			{
				MetalCommandList* c = new MetalCommandList();
				c->dev = this; c->name = debugName ? debugName : "list";
				c->cmd = [queue commandBufferWithUnretainedReferences];
				c->cmd.label = @(c->name.c_str());
				std::lock_guard<std::mutex> l(listsMutex);
				lists.push_back(c);
				return c;
			}
			void Submit(CommandList* const* in, uint32_t count) override
			{
				const uint32_t f = frame;
				for (uint32_t i = 0; i < count; i++)
				{
					MetalCommandList* c = static_cast<MetalCommandList*>(in[i]);
					c->EndEncoders();
					aDraws += c->lDraws; aSkipped += c->lSkipped; aPipelines += c->lPipelines; aGroups += c->lBindGroups; aDispatches += c->lDispatches; aDrawData += c->lDrawDataBytes;
					const std::string name = c->name;
					MetalDevice* self = this;
					[c->cmd addCompletedHandler:^(id<MTLCommandBuffer> cb) {
						const double ms = (cb.GPUEndTime - cb.GPUStartTime) * 1000.0;
						std::lock_guard<std::mutex> l(self->statsMutex);
						self->pending.push_back({ name, ms });
						if (self->frameGpuStart[f] == 0.0 || cb.GPUStartTime < self->frameGpuStart[f]) self->frameGpuStart[f] = cb.GPUStartTime;
					}];
					[c->cmd enqueue];
					[c->cmd commit];
					std::lock_guard<std::mutex> l(listsMutex);
					lists.erase(std::remove(lists.begin(), lists.end(), c), lists.end());
					retired[f].push_back(c);
				}
			}
			Texture AcquireBackbuffer() override
			{
				// (one a frame: a second pass over it - an editor's panels over its views - draws on the same)
				if (drawable) { Texture t; t.id = backbufferId; return t; }
				@autoreleasepool { drawable = [layer nextDrawable]; }
				if (!drawable) return Texture();
				if (TextureRec* t = textures.Get(backbufferId))
				{
					t->texture = drawable.texture;
					t->desc.width = (uint32_t)drawable.texture.width; t->desc.height = (uint32_t)drawable.texture.height;
				}
				Texture t; t.id = backbufferId; return t;
			}
			void ResizeSurface(uint32_t w, uint32_t h) override { layer.drawableSize = CGSizeMake(w, h); }
			void Present() override
			{
				// (the frame's last command buffer: shows the drawable once everything before it on the queue is done,
				// and gives the frame's slot back)
				id<MTLCommandBuffer> cb = [queue commandBuffer];
				cb.label = @"Device.Present";
				// (PYROS_SHOT=<file.png>: the 120th frame as it is shown, read back from the GPU - a test's way to see
				// what was drawn without a screen capture)
				id<MTLBuffer> shot = nil; uint32_t sw = 0, sh = 0;
				const char* shotPath = getenv("PYROS_SHOT");
				if (drawable && frameNumber == ShotFrame() && shotPath)
				{
					sw = (uint32_t)drawable.texture.width; sh = (uint32_t)drawable.texture.height;
					const uint32_t bpp = hdrOutput ? 8 : 4;
					shot = [device newBufferWithLength:(NSUInteger)sw * sh * bpp options:MTLResourceStorageModeShared];
					id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
					[be copyFromTexture:drawable.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(sw, sh, 1)
						toBuffer:shot destinationOffset:0 destinationBytesPerRow:sw * bpp destinationBytesPerImage:sw * sh * bpp];
					[be endEncoding];
				}
				if (drawable) [cb presentDrawable:drawable];
				dispatch_semaphore_t sem = inFlight;
				MetalDevice* self = this; const uint32_t f = frame;
				[cb addCompletedHandler:^(id<MTLCommandBuffer>) {
					{
						std::lock_guard<std::mutex> l(self->statsMutex);
						self->finished[f] = self->pending; self->pending.clear();
						// Each render pass's own time, in place of the lists' where the GPU gave them
						const uint32_t n = std::min(self->passCount[f].load(), MetalDevice::kTimedPasses);
						if (self->passTiming && n > 0)
						{
							MTLTimestamp cpu = 0, gpu = 0;
							[self->device sampleTimestamps:&cpu gpuTimestamp:&gpu];
							const double nsPerTick = gpu > self->calibGpu ? (double)(cpu - self->calibCpu) / (double)(gpu - self->calibGpu) : 1.0;
							NSData* data = [self->passSamples[f] resolveCounterRange:NSMakeRange(0, 2 * n)];
							const MTLCounterResultTimestamp* ts = data ? (const MTLCounterResultTimestamp*)data.bytes : nullptr;
							// (a pass's vertex stage starts early and then waits on the pass before it: its cost is from the later of its
							// own start and the previous pass's end, to its own end)
							std::vector<Timed> passes;
							// (the first pass from its own start: from the frame's - its first command buffer's - it took in everything
							// before it, another view's lists, uploads, the wait between: a sky table of 192 x 108 timed at 3 to 15 ms.
							// What the GPU did of the frame before the first pass is said apart, "(before)")
							uint64_t prevEnd = 0, frameStart = 0;
							if (self->frameGpuStart[f] > 0.0 && nsPerTick > 0.0)
							{
								const double startNs = self->frameGpuStart[f] * 1e9;
								const double ticks = (double)gpu - ((double)cpu - startNs) / nsPerTick;
								if (ticks > 0.0) frameStart = (uint64_t)ticks;
							}
							self->frameGpuStart[f] = 0.0;
							for (uint32_t i = 0; ts && i < n; i++)
							{
								const uint64_t a = ts[2 * i].timestamp, b = ts[2 * i + 1].timestamp;
								if (a == MTLCounterErrorValue || b == MTLCounterErrorValue || a == 0 || b <= a) continue;
								if (passes.empty() && frameStart > 0 && frameStart < a)
								{
									const double before = (double)(a - frameStart) * nsPerTick * 1e-6;
									if (before > 0.05) passes.push_back({ "(before)", before });
									prevEnd = a;
								}
								const uint64_t from = std::max(a, prevEnd);
								prevEnd = std::max(prevEnd, b);
								passes.push_back({ i < self->passNames[f].size() ? self->passNames[f][i] : "pass", b > from ? (double)(b - from) * nsPerTick * 1e-6 : 0.0 });
							}
							if (!passes.empty()) self->finished[f] = passes;
						}
						self->passCount[f].store(0);
					}
					dispatch_semaphore_signal(sem);
				}];
				[cb commit];
				if (shot)
				{
					[cb waitUntilCompleted];
					uint8_t* px = (uint8_t*)shot.contents;
					if (hdrOutput)
					{
						// (half floats, linear, past 1 where it is HDR: written as SDR - clamped and sRGB-encoded - in place)
						const __fp16* hf = (const __fp16*)shot.contents;
						for (uint32_t i = 0; i < sw * sh; i++)
							for (uint32_t c = 0; c < 4; c++)
							{
								float v = std::min(std::max((float)hf[i * 4 + c], 0.f), 1.f);
								if (c < 3) v = v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.f / 2.4f) - 0.055f;
								px[i * 4 + c] = (uint8_t)(v * 255.f + 0.5f);
							}
					}
					else for (uint32_t i = 0; i < sw * sh; i++) std::swap(px[i * 4 + 0], px[i * 4 + 2]);      // (BGRA to RGBA)
					CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
					CGContextRef ctx = CGBitmapContextCreate(px, sw, sh, 8, sw * 4, cs, kCGImageAlphaNoneSkipLast);
					CGImageRef img = CGBitmapContextCreateImage(ctx);
					CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*)shotPath, strlen(shotPath), false);
					CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, nullptr);
					if (dst) { CGImageDestinationAddImage(dst, img, nullptr); CGImageDestinationFinalize(dst); CFRelease(dst); }
					CFRelease(url); CGImageRelease(img); CGContextRelease(ctx); CGColorSpaceRelease(cs);
				}
				frameOpen = false;
				drawable = nil;
				if (TextureRec* t = textures.Get(backbufferId)) t->texture = nil;
			}
			void EndFrame() override {}
			double GpuMilliseconds(QueryPool, uint32_t, uint32_t) override { return stats.gpuMs; }
			const FrameStats &LastFrameStats() override { return stats; }

			~MetalDevice() override
			{
				// (every frame's slot back before the queue goes - one taken and never presented given back first)
				if (frameOpen) { dispatch_semaphore_signal(inFlight); frameOpen = false; }
				for (uint32_t i = 0; i < kFrames; i++) dispatch_semaphore_wait(inFlight, DISPATCH_TIME_FOREVER);
				for (uint32_t i = 0; i < kFrames; i++) { for (MetalCommandList* c : retired[i]) delete c; retired[i].clear(); doomed[i].clear(); }
				for (uint32_t i = 0; i < kFrames; i++) dispatch_semaphore_signal(inFlight);
			}
		};

		void MetalCommandList::BeginPass(const PassDesc &p)
		{
			EndEncoders();
			MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
			for (uint8_t c = 0; c < p.colorCount; c++)
			{
				TextureRec* t = dev->textures.Get(p.colors[c].texture.id);
				rp.colorAttachments[c].texture = t ? t->texture : nil;
				rp.colorAttachments[c].level = p.colors[c].mip; rp.colorAttachments[c].slice = p.colors[c].layer;
				rp.colorAttachments[c].loadAction = p.colors[c].load == Load::Clear ? MTLLoadActionClear : (p.colors[c].load == Load::Keep ? MTLLoadActionLoad : MTLLoadActionDontCare);
				rp.colorAttachments[c].clearColor = MTLClearColorMake(p.colors[c].clear[0], p.colors[c].clear[1], p.colors[c].clear[2], p.colors[c].clear[3]);
				if (p.colors[c].store == StoreOp::Resolve)
				{
					TextureRec* r = dev->textures.Get(p.colors[c].resolve.id);
					rp.colorAttachments[c].resolveTexture = r ? r->texture : nil;
					rp.colorAttachments[c].storeAction = MTLStoreActionMultisampleResolve;
				}
				else rp.colorAttachments[c].storeAction = p.colors[c].store == StoreOp::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
			}
			if (TextureRec* d = dev->textures.Get(p.depth.texture.id))
			{
				rp.depthAttachment.texture = d->texture; rp.depthAttachment.slice = p.depth.layer;
				rp.depthAttachment.loadAction = p.depth.load == Load::Clear ? MTLLoadActionClear : (p.depth.load == Load::Keep ? MTLLoadActionLoad : MTLLoadActionDontCare);
				rp.depthAttachment.storeAction = p.depth.store == StoreOp::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
				rp.depthAttachment.clearDepth = p.depth.clear;
			}
			if (dev->passTiming)
			{
				const uint32_t f = dev->frame, i = dev->passCount[f].fetch_add(1);
				if (i < MetalDevice::kTimedPasses)
				{
					rp.sampleBufferAttachments[0].sampleBuffer = dev->passSamples[f];
					rp.sampleBufferAttachments[0].startOfVertexSampleIndex = 2 * i;
					rp.sampleBufferAttachments[0].endOfVertexSampleIndex = MTLCounterDontSample;
					rp.sampleBufferAttachments[0].startOfFragmentSampleIndex = MTLCounterDontSample;
					rp.sampleBufferAttachments[0].endOfFragmentSampleIndex = 2 * i + 1;
					std::lock_guard<std::mutex> l(dev->statsMutex);
					if (dev->passNames[f].size() <= i) dev->passNames[f].resize(i + 1);
					dev->passNames[f][i] = p.debugName ? p.debugName : "pass";
				}
			}
			render = [cmd renderCommandEncoderWithDescriptor:rp];
			if (p.debugName) render.label = @(p.debugName);
			if (p.width && p.height) [render setViewport:(MTLViewport){ 0, 0, (double)p.width, (double)p.height, 0, 1 }];
			// (the frame's ring of draw data, bound once; each draw moves its offset)
			[render setVertexBuffer:dev->drawRing offset:dev->frameBase atIndex:kDrawDataIndex];
			[render setFragmentBuffer:dev->drawRing offset:dev->frameBase atIndex:kDrawDataIndex];
			current = nullptr; skipping = true; haveDrawData = false;
		}
		bool MetalCommandList::Ready() { if (skipping || !current) { lSkipped++; return false; } return true; }
		void MetalCommandList::SetPipeline(Pipeline p)
		{
			PipelineRec* r = dev->pipelines.Get(p.id);
			current = r; lPipelines++;
			skipping = !r || r->state->load() != 1;
			if (skipping) return;
			if (r->isCompute) { [Compute() setComputePipelineState:r->compute]; dev->BindRayScene(nil, Compute(), r->vertexSlots.rayScene); return; }
			if (!render) { skipping = true; return; }
			[render setRenderPipelineState:r->render];
			dev->BindRayScene(render, nil, r->fragmentSlots.rayScene);
			[render setDepthStencilState:r->depth];
			[render setCullMode:r->cull];
			[render setFrontFacingWinding:r->winding];
			if (r->biasConstant != 0.f || r->biasSlope != 0.f) [render setDepthBias:r->biasConstant slopeScale:r->biasSlope clamp:0.f];
		}
		void MetalCommandList::DrawData(const void* data, uint32_t size)
		{
			if (slotNext == slotEnd) { slotNext = dev->frameCursor.fetch_add(kDrawDataSlot * kSlotChunk); slotEnd = slotNext + kDrawDataSlot * kSlotChunk; }
			const uint64_t at = slotNext; slotNext += kDrawDataSlot;      // (64 slots taken at once: threads do not meet on the cursor every draw)
			if (at + kDrawDataSlot > kDrawDataBytes) { haveDrawData = false; return; }
			memcpy((uint8_t*)dev->drawRing.contents + dev->frameBase + at, data, size < kDrawDataSlot ? size : kDrawDataSlot);
			drawOffset = dev->frameBase + at; haveDrawData = true; lDrawDataBytes += size;
			BindDrawData();
		}
		void MetalCommandList::BindDrawData()
		{
			if (render)
			{
				if (!current || current->vertexDrawData) [render setVertexBufferOffset:drawOffset atIndex:kDrawDataIndex];
				if (!current || current->fragmentDrawData) [render setFragmentBufferOffset:drawOffset atIndex:kDrawDataIndex];
			}
			else if (compute) [compute setBuffer:dev->drawRing offset:drawOffset atIndex:kDrawDataIndex];
		}
		void MetalCommandList::SetBindGroup(BindGroup g, const uint32_t* dynamicOffsets, uint32_t dynamicCount)
		{
			BindGroupRec* r = dev->groups.Get(g.id);
			if (!r || !current) return;
			lBindGroups++;
			// (each entry to the Metal index each stage of the bound pipeline gave its binding - a stage that does not
			// read it is not given it)
			auto at = [](const std::map<uint32_t, uint32_t> &m, uint32_t b) -> int { std::map<uint32_t, uint32_t>::const_iterator i = m.find(b); return i == m.end() ? -1 : (int)i->second; };
			uint32_t nextDynamic = 0;
			for (BindGroupRec::Entry e : r->entries)
			{
				if (e.dynamic && dynamicOffsets && nextDynamic < dynamicCount) e.offset += dynamicOffsets[nextDynamic++];
				const Slots &vs = current->vertexSlots, &fs = current->fragmentSlots;
				if (render)
				{
					switch (e.kind)
					{
					case BindingKind::Sampler: case BindingKind::ComparisonSampler:
						if (at(vs.sampler, e.binding) >= 0) [render setVertexSamplerState:e.sampler atIndex:at(vs.sampler, e.binding)];
						if (at(fs.sampler, e.binding) >= 0) [render setFragmentSamplerState:e.sampler atIndex:at(fs.sampler, e.binding)];
						break;
					case BindingKind::Uniform: case BindingKind::StorageRead: case BindingKind::StorageReadWrite:
						if (at(vs.buffer, e.binding) >= 0) [render setVertexBuffer:e.buffer offset:e.offset atIndex:at(vs.buffer, e.binding)];
						if (at(fs.buffer, e.binding) >= 0) [render setFragmentBuffer:e.buffer offset:e.offset atIndex:at(fs.buffer, e.binding)];
						break;
					default:
						if (at(vs.texture, e.binding) >= 0) [render setVertexTexture:e.texture atIndex:at(vs.texture, e.binding)];
						if (at(fs.texture, e.binding) >= 0) [render setFragmentTexture:e.texture atIndex:at(fs.texture, e.binding)];
						// (a combined image sampler's sampler goes with its texture, at the sampler index)
						if (e.sampler && at(vs.sampler, e.binding) >= 0) [render setVertexSamplerState:e.sampler atIndex:at(vs.sampler, e.binding)];
						if (e.sampler && at(fs.sampler, e.binding) >= 0) [render setFragmentSamplerState:e.sampler atIndex:at(fs.sampler, e.binding)];
						break;
					}
				}
				else
				{
					id<MTLComputeCommandEncoder> ce = Compute();
					switch (e.kind)
					{
					case BindingKind::Sampler: case BindingKind::ComparisonSampler: if (at(vs.sampler, e.binding) >= 0) [ce setSamplerState:e.sampler atIndex:at(vs.sampler, e.binding)]; break;
					case BindingKind::Uniform: case BindingKind::StorageRead: case BindingKind::StorageReadWrite: if (at(vs.buffer, e.binding) >= 0) [ce setBuffer:e.buffer offset:e.offset atIndex:at(vs.buffer, e.binding)]; break;
					default:
						if (at(vs.texture, e.binding) >= 0) [ce setTexture:e.texture atIndex:at(vs.texture, e.binding)];
						if (e.sampler && at(vs.sampler, e.binding) >= 0) [ce setSamplerState:e.sampler atIndex:at(vs.sampler, e.binding)];
						break;
					}
				}
			}
		}
		void MetalCommandList::SetVertexBuffer(uint32_t binding, Buffer b, uint64_t offset)
		{
			BufferRec* r = dev->buffers.Get(b.id);
			if (render && r) [render setVertexBuffer:r->buffer offset:offset atIndex:kVertexBufferBase + binding];
		}
		void MetalCommandList::SetIndexBuffer(Buffer b, uint64_t offset, bool index32)
		{
			BufferRec* r = dev->buffers.Get(b.id);
			indexBuffer = r ? r->buffer : nil; indexOffset = offset; indexType = index32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
		}
		void MetalCommandList::Draw(uint32_t v, uint32_t i, uint32_t fv, uint32_t fi)
		{
			if (!Ready() || !render) return;
			[render drawPrimitives:current->primitive vertexStart:fv vertexCount:v instanceCount:i baseInstance:fi];
			lDraws++;
		}
		void MetalCommandList::DrawIndexed(uint32_t n, uint32_t i, uint32_t fi, int32_t vo, uint32_t fin)
		{
			if (!Ready() || !render || !indexBuffer) return;
			const uint64_t stride = indexType == MTLIndexTypeUInt32 ? 4 : 2;
			[render drawIndexedPrimitives:current->primitive indexCount:n indexType:indexType indexBuffer:indexBuffer indexBufferOffset:indexOffset + fi * stride
				instanceCount:i baseVertex:vo baseInstance:fin];
			lDraws++;
		}
		void MetalCommandList::DrawIndexedIndirect(Buffer args, uint64_t offset, uint32_t drawCount, Buffer, uint64_t)
		{
			if (!Ready() || !render || !indexBuffer) return;
			BufferRec* a = dev->buffers.Get(args.id);
			if (!a) return;
			// (no count buffer here - Caps().indirectCount is off: culled draws are written as zero instances, as on WebGPU)
			for (uint32_t d = 0; d < drawCount; d++)
				[render drawIndexedPrimitives:current->primitive indexType:indexType indexBuffer:indexBuffer indexBufferOffset:indexOffset indirectBuffer:a->buffer indirectBufferOffset:offset + d * 20];
			lDraws += drawCount;
		}
		void MetalCommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
		{
			if (skipping || !current || !current->isCompute) { lSkipped++; return; }
			id<MTLComputeCommandEncoder> ce = Compute();
			[ce setComputePipelineState:current->compute];
			if (haveDrawData) [ce setBuffer:dev->drawRing offset:drawOffset atIndex:kDrawDataIndex];
			[ce dispatchThreadgroups:MTLSizeMake(x, y, z) threadsPerThreadgroup:current->threads];
			lDispatches++;
		}
		void MetalCommandList::DispatchIndirect(Buffer args, uint64_t offset)
		{
			if (skipping || !current || !current->isCompute) { lSkipped++; return; }
			BufferRec* a = dev->buffers.Get(args.id);
			if (!a) return;
			id<MTLComputeCommandEncoder> ce = Compute();
			[ce setComputePipelineState:current->compute];
			if (haveDrawData) [ce setBuffer:dev->drawRing offset:drawOffset atIndex:kDrawDataIndex];
			[ce dispatchThreadgroupsWithIndirectBuffer:a->buffer indirectBufferOffset:offset threadsPerThreadgroup:current->threads];
			lDispatches++;
		}
		void MetalCommandList::CopyBuffer(Buffer from, uint64_t fromOffset, Buffer to, uint64_t toOffset, uint64_t size)
		{
			BufferRec* a = dev->buffers.Get(from.id); BufferRec* b = dev->buffers.Get(to.id);
			if (a && b) [Blit() copyFromBuffer:a->buffer sourceOffset:fromOffset toBuffer:b->buffer destinationOffset:toOffset size:size];
		}
		void MetalCommandList::CopyTexture(Texture from, Texture to)
		{
			TextureRec* a = dev->textures.Get(from.id); TextureRec* b = dev->textures.Get(to.id);
			if (a && b && a->texture && b->texture) [Blit() copyFromTexture:a->texture toTexture:b->texture];
		}

	}

	Device* CreateMetalDevice(const SurfaceSource &surface)
	{
		MetalDevice* d = new MetalDevice();
		if (!d->Init(surface)) { delete d; return nullptr; }
		return d;
	}

} }
