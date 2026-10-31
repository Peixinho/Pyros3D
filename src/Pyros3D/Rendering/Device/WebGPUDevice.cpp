//============================================================================
// Name        : WebGPUDevice.cpp
// Author      : Duarte Peixinho
// Description : The device layer on WebGPU (Dawn natively, the browser's through Emscripten).
//               See docs/device-design.md: WebGPU's core is the baseline.
//
// What a frame is here: the per-draw data (DrawData) is gathered on the CPU as the lists are
// recorded and written in one go into this frame's slice of a uniform ring before the lists are
// submitted; a draw binds group 0 at its own dynamic offset into it. Pipelines are made with
// CreateRenderPipelineAsync - a draw whose pipeline is not ready is skipped, never waited for.
//============================================================================

#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <cstdlib>
// (the frame a test's capture is of: PYROS_SHOT_FRAME, else the 120th)
static inline uint64_t ShotFrame() { static const uint64_t f = getenv("PYROS_SHOT_FRAME") ? (uint64_t)atoll(getenv("PYROS_SHOT_FRAME")) : 120; return f; }
#include <Pyros3D/Rendering/Device/WritePng.h>
#if defined(PYROS_GFX_SPIRV_TO_WGSL)
#include <Pyros3D/Rendering/Device/SpirvToWgsl.h>
#endif
#include <webgpu/webgpu.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace p3d { namespace gfx {

	namespace {

		WGPUStringView Str(const char* s) { WGPUStringView v; v.data = s; v.length = s ? strlen(s) : 0; return v; }

		WGPUTextureFormat ToWGPU(const Format f)
		{
			switch (f)
			{
			case Format::R8: return WGPUTextureFormat_R8Unorm;
			case Format::RG8: return WGPUTextureFormat_RG8Unorm;
			case Format::RGBA8: return WGPUTextureFormat_RGBA8Unorm;
			case Format::RGBA8_sRGB: return WGPUTextureFormat_RGBA8UnormSrgb;
			case Format::BGRA8: return WGPUTextureFormat_BGRA8Unorm;
			case Format::BGRA8_sRGB: return WGPUTextureFormat_BGRA8UnormSrgb;
			case Format::R16F: return WGPUTextureFormat_R16Float;
			case Format::RG16F: return WGPUTextureFormat_RG16Float;
			case Format::RGBA16F: return WGPUTextureFormat_RGBA16Float;
			case Format::R32F: return WGPUTextureFormat_R32Float;
			case Format::RG32F: return WGPUTextureFormat_RG32Float;
			case Format::RGBA32F: return WGPUTextureFormat_RGBA32Float;
			case Format::R11G11B10F: return WGPUTextureFormat_RG11B10Ufloat;
			case Format::RGB10A2: return WGPUTextureFormat_RGB10A2Unorm;
			case Format::R32U: return WGPUTextureFormat_R32Uint;
			case Format::RG32U: return WGPUTextureFormat_RG32Uint;
			case Format::BC1: return WGPUTextureFormat_BC1RGBAUnorm;
			case Format::BC1_sRGB: return WGPUTextureFormat_BC1RGBAUnormSrgb;
			case Format::BC3: return WGPUTextureFormat_BC3RGBAUnorm;
			case Format::BC3_sRGB: return WGPUTextureFormat_BC3RGBAUnormSrgb;
			case Format::BC4: return WGPUTextureFormat_BC4RUnorm;
			case Format::BC5: return WGPUTextureFormat_BC5RGUnorm;
			case Format::BC6H: return WGPUTextureFormat_BC6HRGBUfloat;
			case Format::BC7: return WGPUTextureFormat_BC7RGBAUnorm;
			case Format::BC7_sRGB: return WGPUTextureFormat_BC7RGBAUnormSrgb;
			case Format::D32F: return WGPUTextureFormat_Depth32Float;
			case Format::D16: return WGPUTextureFormat_Depth16Unorm;
			case Format::D24S8: return WGPUTextureFormat_Depth24PlusStencil8;
			case Format::D32FS8: return WGPUTextureFormat_Depth32FloatStencil8;
			default: return WGPUTextureFormat_Undefined;
			}
		}
		Format FromWGPU(const WGPUTextureFormat f)
		{
			switch (f)
			{
			case WGPUTextureFormat_BGRA8Unorm: return Format::BGRA8;
			case WGPUTextureFormat_BGRA8UnormSrgb: return Format::BGRA8_sRGB;
			case WGPUTextureFormat_RGBA8Unorm: return Format::RGBA8;
			case WGPUTextureFormat_RGBA8UnormSrgb: return Format::RGBA8_sRGB;
			case WGPUTextureFormat_RGBA16Float: return Format::RGBA16F;
			default: return Format::Unknown;
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
		WGPUCompareFunction ToWGPU(const Compare c)
		{
			switch (c)
			{
			case Compare::Never: return WGPUCompareFunction_Never;
			case Compare::Less: return WGPUCompareFunction_Less;
			case Compare::LessEqual: return WGPUCompareFunction_LessEqual;
			case Compare::Equal: return WGPUCompareFunction_Equal;
			case Compare::Greater: return WGPUCompareFunction_Greater;
			case Compare::GreaterEqual: return WGPUCompareFunction_GreaterEqual;
			default: return WGPUCompareFunction_Always;
			}
		}
		WGPUAddressMode ToWGPU(const Address a)
		{
			switch (a)
			{
			case Address::Clamp: case Address::Border: return WGPUAddressMode_ClampToEdge;   // (no border in core WebGPU)
			case Address::Mirror: return WGPUAddressMode_MirrorRepeat;
			default: return WGPUAddressMode_Repeat;
			}
		}
		bool BlendState(const Blend b, WGPUBlendState &s)
		{
			s.color.operation = s.alpha.operation = WGPUBlendOperation_Add;
			switch (b)
			{
			case Blend::Alpha: s.color.srcFactor = WGPUBlendFactor_SrcAlpha; s.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; s.alpha.srcFactor = WGPUBlendFactor_One; s.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; return true;
			case Blend::PremultipliedAlpha: s.color.srcFactor = WGPUBlendFactor_One; s.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; s.alpha = s.color; return true;
			case Blend::Additive: s.color.srcFactor = WGPUBlendFactor_One; s.color.dstFactor = WGPUBlendFactor_One; s.alpha = s.color; return true;
			case Blend::Multiply: s.color.srcFactor = WGPUBlendFactor_Dst; s.color.dstFactor = WGPUBlendFactor_Zero; s.alpha = s.color; return true;
			default: return false;
			}
		}
		WGPUVertexFormat VertexFormat(const Format f)
		{
			switch (f)
			{
			case Format::R32F: return WGPUVertexFormat_Float32;
			case Format::RG32F: return WGPUVertexFormat_Float32x2;
			case Format::RGB32F: return WGPUVertexFormat_Float32x3;
			case Format::RGBA32F: return WGPUVertexFormat_Float32x4;
			case Format::RGBA8: return WGPUVertexFormat_Unorm8x4;
			case Format::RGBA16F: return WGPUVertexFormat_Float16x4;
			case Format::R32U: return WGPUVertexFormat_Uint32;
			default: return WGPUVertexFormat_Float32x3;     // (Unknown: three floats, a position or a normal)
			}
		}

		// A pool of things by handle id (0 none)
		template <typename T> struct Pool {
			// (a deque: what Get gave stays where it is when more are added - a vector moved them all, and an
			// upload that made its staging buffer wrote through a pointer to where the buffer had been)
			std::deque<T> items; std::vector<uint32_t> free; std::mutex m;
			uint32_t Add(const T &t) { std::lock_guard<std::mutex> l(m); if (!free.empty()) { uint32_t i = free.back(); free.pop_back(); items[i] = t; return i + 1; } items.push_back(t); return (uint32_t)items.size(); }
			T* Get(uint32_t id) { return (id && id <= items.size()) ? &items[id - 1] : nullptr; }
			void Remove(uint32_t id) { std::lock_guard<std::mutex> l(m); if (id && id <= items.size()) { items[id - 1] = T(); free.push_back(id - 1); } }
		};

		struct TextureRec { WGPUTexture texture = nullptr; WGPUTextureView view = nullptr; TextureDesc desc; bool backbuffer = false; };
		struct ShaderRec { WGPUShaderModule module = nullptr; std::string entry; };
		struct PipelineRec {
			std::atomic<int> state{ 0 };        // 0 compiling, 1 ready, 2 failed
			WGPURenderPipeline render = nullptr; WGPUComputePipeline compute = nullptr;
			WGPUBindGroupLayout material = nullptr; WGPUPipelineLayout layout = nullptr;
			uint32_t drawDataBytes = 0;
			class WebGPUDevice* owner = nullptr;
			PipelineRec() {}
			PipelineRec(const PipelineRec &o) : render(o.render), compute(o.compute), material(o.material), layout(o.layout), drawDataBytes(o.drawDataBytes) { state.store(o.state.load()); }
			PipelineRec &operator=(const PipelineRec &o) { render = o.render; compute = o.compute; material = o.material; layout = o.layout; drawDataBytes = o.drawDataBytes; state.store(o.state.load()); return *this; }
		};

		const uint32_t kFrames = 2;
		const uint32_t kDrawDataAlign = 256;            // (WebGPU's minUniformBufferOffsetAlignment)
		const uint32_t kDrawDataSlot = 256;             // one draw's share, at most
		const uint64_t kDrawDataBytes = 8ull << 20;     // a frame's: 32 768 draws
		const uint32_t kSlotChunk = 64;
		const uint32_t kTimingSlots = 4;                // frames of timestamps in flight
		const uint32_t kTimedPasses = 64;               // a frame's passes that are timed

		class WebGPUDevice;

		class WebGPUCommandList : public CommandList {
		public:
			WebGPUDevice* dev = nullptr;
			WGPUCommandEncoder encoder = nullptr;
			WGPURenderPassEncoder pass = nullptr;
			WGPUComputePassEncoder cpass = nullptr;
			PipelineRec* current = nullptr;
			bool skipping = false;              // the pipeline set is not ready: its draws are left out
			bool skipDraw = false;              // its data had no room this frame
			// (its own counters and its own run of draw-data slots: no atomics between threads a draw)
			uint32_t lDraws = 0, lSkipped = 0, lPipelines = 0, lBindGroups = 0, lDispatches = 0;
			uint64_t slotNext = 0, slotEnd = 0;
			const char* name = nullptr;

			void Barriers(const Barrier*, uint32_t) override {}        // (WebGPU tracks hazards itself)
			void BeginPass(const PassDesc &p) override;
			void EndPass() override { if (pass) { wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass); pass = nullptr; } if (cpass) { wgpuComputePassEncoderEnd(cpass); wgpuComputePassEncoderRelease(cpass); cpass = nullptr; } }
			void SetViewport(float x, float y, float w, float h, float mn, float mx) override { if (pass) wgpuRenderPassEncoderSetViewport(pass, x, y, w, h, mn, mx); }
			void SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override { if (pass) wgpuRenderPassEncoderSetScissorRect(pass, (uint32_t)x, (uint32_t)y, w, h); }
			void SetPipeline(Pipeline p) override;
			void DrawData(const void* data, uint32_t size) override;
			void SetBindGroup(BindGroup g, const uint32_t* dynamicOffsets, uint32_t dynamicCount) override;
			void SetVertexBuffer(uint32_t binding, Buffer b, uint64_t offset) override;
			void SetIndexBuffer(Buffer b, uint64_t offset, bool index32) override;
			void Draw(uint32_t v, uint32_t i, uint32_t fv, uint32_t fi) override { if (Ready()) { wgpuRenderPassEncoderDraw(pass, v, i, fv, fi); Count(true); } else Count(false); }
			void DrawIndexed(uint32_t n, uint32_t i, uint32_t fi, int32_t vo, uint32_t fin) override { if (Ready()) { wgpuRenderPassEncoderDrawIndexed(pass, n, i, fi, vo, fin); Count(true); } else Count(false); }
			void Count(bool drawn);
			void DrawIndexedIndirect(Buffer args, uint64_t offset, uint32_t drawCount, Buffer, uint64_t) override;
			void Dispatch(uint32_t x, uint32_t y, uint32_t z) override;
			void DispatchIndirect(Buffer args, uint64_t offset) override;
			void CopyBuffer(Buffer from, uint64_t fo, Buffer to, uint64_t too, uint64_t size) override;
			void CopyTexture(Texture from, Texture to) override;
			void Timestamp(QueryPool, uint32_t) override {}
			void BeginMarker(const char* n) override { if (pass) wgpuRenderPassEncoderPushDebugGroup(pass, Str(n)); }
			void EndMarker() override { if (pass) wgpuRenderPassEncoderPopDebugGroup(pass); }
			bool Ready();
		};

		class WebGPUDevice : public Device {
		public:
			WGPUInstance instance = nullptr; WGPUAdapter adapter = nullptr; WGPUDevice device = nullptr; WGPUQueue queue = nullptr;
			WGPUSurface surface = nullptr; WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8Unorm, viewFormat = WGPUTextureFormat_BGRA8UnormSrgb;
			uint32_t width = 0, height = 0; bool vsync = true, threadSafe = false; WGPUPresentMode presentMode = WGPUPresentMode_Fifo;
			Capabilities caps;
			Pool<WGPUBuffer> buffers; Pool<TextureRec> textures; Pool<WGPUSampler> samplers; Pool<ShaderRec> shaders;
			Pool<PipelineRec*> pipelines; Pool<WGPUBindGroup> groups;
			WGPUBindGroupLayout drawLayout = nullptr;       // group 0: the per-draw data, dynamic offset
			WGPUBuffer drawRing = nullptr; WGPUBindGroup drawGroup = nullptr;
			uint32_t frame = 0; uint64_t frameBase = 0, flushed = 0;
			std::vector<uint8_t> frameData = std::vector<uint8_t>(kDrawDataBytes);
			std::atomic<uint64_t> frameCursor{ 0 }; std::atomic<bool> warnedFull{ false };
			// ---- profiling: counters of the frame, and the passes' GPU times read back a few frames on
			std::atomic<uint32_t> cDraws{ 0 }, cSkipped{ 0 }, cPipelines{ 0 }, cBindGroups{ 0 }, cDispatches{ 0 }, cFinished{ 0 }, cPending{ 0 };
			uint64_t frameNumber = 0;
			struct Timing {
				WGPUQuerySet queries = nullptr; WGPUBuffer resolve = nullptr, read = nullptr;
				std::vector<std::string> names; uint64_t frame = 0;
				std::atomic<int> state{ 0 };        // 0 free, 1 written and being read back, 2 read
				std::vector<double> ms;
			};
			Timing timing[kTimingSlots];
			std::mutex statsMutex, timingMutex;
			FrameStats stats, gpuStats;
			int TimedPass(const char* name);
			void ResolveTimings();
			void ReadTimings();
			const FrameStats &LastFrameStats() override;
			std::vector<WebGPUCommandList*> lists, spare;
			uint32_t backbuffer = 0;                        // the texture handle the current surface texture is
			WGPUTexture surfaceTexture = nullptr;
			std::mutex recordMutex;

			bool Init(const SurfaceSource &s);
			~WebGPUDevice() override;
			const Capabilities &Caps() const override { return caps; }
			Format BackbufferFormat() const override { return FromWGPU(viewFormat); }
			bool ReadTexture(Texture t, std::vector<uint8_t> &out) override;

			Buffer CreateBuffer(const BufferDesc &d, const void* initial) override;
			Texture CreateTexture(const TextureDesc &d, const TextureData* data, uint32_t count) override;
			Sampler CreateSampler(const SamplerDesc &d) override;
			void Destroy(Buffer b) override { if (WGPUBuffer* p = buffers.Get(b.id)) { if (*p) wgpuBufferRelease(*p); buffers.Remove(b.id); } }
			void Destroy(Texture t) override { if (TextureRec* r = textures.Get(t.id)) { if (r->view) wgpuTextureViewRelease(r->view); if (r->texture && !r->backbuffer) wgpuTextureRelease(r->texture); textures.Remove(t.id); } }
			BindGroup CreateBindGroup(Pipeline layoutOf, const BindGroupEntry* e, uint32_t count) override;
			void Destroy(BindGroup g) override { if (WGPUBindGroup* p = groups.Get(g.id)) { if (*p) wgpuBindGroupRelease(*p); groups.Remove(g.id); } }
			BindlessIndex Index(Texture, Sampler) override { return kNoIndex; }    // (core WebGPU: none)
			BindlessIndex Index(Buffer) override { return kNoIndex; }
			void* Map(Buffer, uint64_t &, uint64_t) override { return nullptr; }   // (WebGPU: Upload instead)
			void Upload(Buffer to, uint64_t offset, const void* data, uint64_t size) override { if (WGPUBuffer* p = buffers.Get(to.id)) Write(*p, offset, data, size); }
			// (WebGPU writes whole words: a size that is not one - 16-bit indices, an odd count of them - padded with zeros;
			// the buffer itself was made a whole number of words)
			void Write(WGPUBuffer b, const uint64_t offset, const void* data, const uint64_t size)
			{
				if ((size & 3) == 0) { wgpuQueueWriteBuffer(queue, b, offset, data, (size_t)size); return; }
				std::vector<uint8_t> padded((size_t)((size + 3) & ~3ull), 0);
				memcpy(padded.data(), data, (size_t)size);
				wgpuQueueWriteBuffer(queue, b, offset, padded.data(), padded.size());
			}
			void Upload(Texture to, const TextureData* data, uint32_t count) override;
			Shader CreateShader(const ShaderDesc &d) override;
			Pipeline CreatePipeline(const GraphicsPipelineDesc &d) override;
			Pipeline CreatePipeline(const ComputePipelineDesc &d) override;
			bool PipelineReady(Pipeline p) const override;
			void SavePipelineCache(const std::string &) override {}       // (Dawn keeps its own blob cache)
			bool frameOpen = false;     // (begun and not yet presented)
			void BeginFrame() override;
			CommandList* NewCommandList(const char* name) override;
			void Submit(CommandList* const* l, uint32_t count) override;
			Texture AcquireBackbuffer() override;
			void Present() override;
			void ResizeSurface(uint32_t w, uint32_t h) override { if (w == width && h == height) return; width = w; height = h; Configure(); }
			void EndFrame() override {}
			double GpuMilliseconds(QueryPool, uint32_t, uint32_t) override { return 0.0; }

			WGPUBindGroupLayout MaterialLayout(const std::vector<BindingDesc> &m);
			uint64_t PlaceDrawData(const std::vector<uint8_t> &bytes);
			void Configure();
		};

		// ---------------------------------------------------------------- the list
		void WebGPUCommandList::BeginPass(const PassDesc &p)
		{
			EndPass();
			WGPURenderPassColorAttachment colors[8] = {};
			for (uint32_t i = 0; i < p.colorCount; i++)
			{
				TextureRec* t = dev->textures.Get(p.colors[i].texture.id);
				colors[i].view = t ? t->view : nullptr;
				colors[i].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
				colors[i].loadOp = p.colors[i].load == Load::Keep ? WGPULoadOp_Load : WGPULoadOp_Clear;
				colors[i].storeOp = p.colors[i].store == StoreOp::DontCare ? WGPUStoreOp_Discard : WGPUStoreOp_Store;
				colors[i].clearValue = { p.colors[i].clear[0], p.colors[i].clear[1], p.colors[i].clear[2], p.colors[i].clear[3] };
				if (TextureRec* r = dev->textures.Get(p.colors[i].resolve.id)) colors[i].resolveTarget = r->view;
			}
			WGPURenderPassDepthStencilAttachment depth = {};
			TextureRec* d = dev->textures.Get(p.depth.texture.id);
			if (d)
			{
				depth.view = d->view;
				depth.depthLoadOp = p.depth.load == Load::Keep ? WGPULoadOp_Load : WGPULoadOp_Clear;
				depth.depthStoreOp = p.depth.store == StoreOp::DontCare ? WGPUStoreOp_Discard : WGPUStoreOp_Store;
				depth.depthClearValue = p.depth.clear;
			}
			WGPURenderPassDescriptor rp = {};
			rp.label = Str(p.debugName);
			rp.colorAttachmentCount = p.colorCount; rp.colorAttachments = colors;
			rp.depthStencilAttachment = d ? &depth : nullptr;
			// (its GPU time: two timestamps of this frame's query set)
			WGPUPassTimestampWrites tw = {};
			const int slot = dev->TimedPass(p.debugName);
			if (slot >= 0) { tw.querySet = dev->timing[dev->frameNumber % kTimingSlots].queries; tw.beginningOfPassWriteIndex = slot * 2; tw.endOfPassWriteIndex = slot * 2 + 1; rp.timestampWrites = &tw; }
			pass = wgpuCommandEncoderBeginRenderPass(encoder, &rp);
			current = nullptr; skipping = true;
		}
		void WebGPUCommandList::SetPipeline(Pipeline p)
		{
			PipelineRec** r = dev->pipelines.Get(p.id);
			current = r ? *r : nullptr;
			skipping = !current || current->state.load() != 1;
			if (skipping) return;
			// (a compute pipeline: into a compute pass now - what is bound next, its groups and data, is that pass's;
			// left for the dispatch to begin, they went to no pass and the dispatch had none)
			if (current->compute && !cpass) { EndPass(); WGPUComputePassDescriptor cd = {}; cpass = wgpuCommandEncoderBeginComputePass(encoder, &cd); }
			if (pass && current->render) wgpuRenderPassEncoderSetPipeline(pass, current->render);
			lPipelines++;
			if (cpass && current->compute) wgpuComputePassEncoderSetPipeline(cpass, current->compute);
			// (its layout has group 0 whether or not its draws give data: one that gives none still has it bound - at the
			// frame's start, never read)
			if (current->drawDataBytes == 0)
			{
				const uint32_t offset = (uint32_t)dev->frameBase;
				if (pass) wgpuRenderPassEncoderSetBindGroup(pass, 0, dev->drawGroup, 1, &offset);
				if (cpass) wgpuComputePassEncoderSetBindGroup(cpass, 0, dev->drawGroup, 1, &offset);
			}
		}
		void WebGPUCommandList::DrawData(const void* data, uint32_t size)
		{
			if (skipping || size == 0) return;
			// (a slot of this frame's slice, taken by any thread at once; the slice is written to the GPU
			// in one go when the frame's lists are submitted)
			if (slotNext == slotEnd) { slotNext = dev->frameCursor.fetch_add(kDrawDataSlot * kSlotChunk); slotEnd = slotNext + kDrawDataSlot * kSlotChunk; }
			const uint64_t at = slotNext; slotNext += kDrawDataSlot;      // (64 slots taken at once: threads do not meet on the cursor every draw)
			if (at + kDrawDataSlot > kDrawDataBytes) { if (!dev->warnedFull.exchange(true)) fprintf(stderr, "WebGPU: a frame's draw data is past %llu bytes - draws dropped\n", (unsigned long long)kDrawDataBytes); skipDraw = true; return; }
			memcpy(&dev->frameData[at], data, size < kDrawDataSlot ? size : kDrawDataSlot);
			const uint32_t offset = (uint32_t)(dev->frameBase + at);
			skipDraw = false;
			if (pass) wgpuRenderPassEncoderSetBindGroup(pass, 0, dev->drawGroup, 1, &offset);
			if (cpass) wgpuComputePassEncoderSetBindGroup(cpass, 0, dev->drawGroup, 1, &offset);
		}
		void WebGPUCommandList::SetBindGroup(BindGroup g, const uint32_t* dynamicOffsets, uint32_t dynamicCount)
		{
			WGPUBindGroup* b = dev->groups.Get(g.id);
			if (!b || skipping) return;
			if (pass) wgpuRenderPassEncoderSetBindGroup(pass, 1, *b, dynamicCount, dynamicOffsets);
			lBindGroups++;
			if (cpass) wgpuComputePassEncoderSetBindGroup(cpass, 1, *b, dynamicCount, dynamicOffsets);
		}
		void WebGPUCommandList::SetVertexBuffer(uint32_t binding, Buffer b, uint64_t offset)
		{
			WGPUBuffer* p = dev->buffers.Get(b.id);
			if (pass && p) wgpuRenderPassEncoderSetVertexBuffer(pass, binding, *p, offset, WGPU_WHOLE_SIZE);
		}
		void WebGPUCommandList::SetIndexBuffer(Buffer b, uint64_t offset, bool index32)
		{
			WGPUBuffer* p = dev->buffers.Get(b.id);
			if (pass && p) wgpuRenderPassEncoderSetIndexBuffer(pass, *p, index32 ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16, offset, WGPU_WHOLE_SIZE);
		}
		bool WebGPUCommandList::Ready() { return pass && !skipping && !skipDraw; }
		void WebGPUCommandList::Count(bool drawn) { if (drawn) lDraws++; else lSkipped++; }
		void WebGPUCommandList::DrawIndexedIndirect(Buffer args, uint64_t offset, uint32_t drawCount, Buffer, uint64_t)
		{
			WGPUBuffer* p = dev->buffers.Get(args.id);
			if (!Ready() || !p) return;
			// (core WebGPU: one indirect draw a call; culled ones carry no instances)
			for (uint32_t i = 0; i < drawCount; i++) wgpuRenderPassEncoderDrawIndexedIndirect(pass, *p, offset + (uint64_t)i * 20);
		}
		void WebGPUCommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
		{
			if (!cpass) { EndPass(); WGPUComputePassDescriptor cd = {}; cpass = wgpuCommandEncoderBeginComputePass(encoder, &cd); if (current && current->compute && !skipping) wgpuComputePassEncoderSetPipeline(cpass, current->compute); }
			if (!skipping) { wgpuComputePassEncoderDispatchWorkgroups(cpass, x, y, z); lDispatches++; }
		}
		void WebGPUCommandList::DispatchIndirect(Buffer args, uint64_t offset)
		{
			WGPUBuffer* p = dev->buffers.Get(args.id);
			if (!cpass) { EndPass(); WGPUComputePassDescriptor cd = {}; cpass = wgpuCommandEncoderBeginComputePass(encoder, &cd); }
			if (p && !skipping) wgpuComputePassEncoderDispatchWorkgroupsIndirect(cpass, *p, offset);
		}
		void WebGPUCommandList::CopyBuffer(Buffer from, uint64_t fo, Buffer to, uint64_t too, uint64_t size)
		{
			WGPUBuffer* a = dev->buffers.Get(from.id); WGPUBuffer* b = dev->buffers.Get(to.id);
			EndPass();
			if (a && b) wgpuCommandEncoderCopyBufferToBuffer(encoder, *a, fo, *b, too, size);
		}
		void WebGPUCommandList::CopyTexture(Texture from, Texture to)
		{
			TextureRec* a = dev->textures.Get(from.id); TextureRec* b = dev->textures.Get(to.id);
			EndPass();
			if (!a || !b) return;
			WGPUTexelCopyTextureInfo s = {}; s.texture = a->texture; WGPUTexelCopyTextureInfo d = {}; d.texture = b->texture;
			WGPUExtent3D e = { a->desc.width, a->desc.height, a->desc.depthOrLayers };
			wgpuCommandEncoderCopyTextureToTexture(encoder, &s, &d, &e);
		}

		// ---------------------------------------------------------------- the device
		void OnAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView msg, void* u1, void*)
		{
			if (status == WGPURequestAdapterStatus_Success) *(WGPUAdapter*)u1 = adapter;
			else fprintf(stderr, "WebGPU: no adapter: %.*s\n", (int)msg.length, msg.data ? msg.data : "");
		}
		void OnDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView msg, void* u1, void*)
		{
			if (status == WGPURequestDeviceStatus_Success) *(WGPUDevice*)u1 = device;
			else fprintf(stderr, "WebGPU: no device: %.*s\n", (int)msg.length, msg.data ? msg.data : "");
		}
		void OnError(WGPUDevice const*, WGPUErrorType type, WGPUStringView msg, void*, void*)
		{
			fprintf(stderr, "WebGPU error %d: %.*s\n", (int)type, (int)msg.length, msg.data ? msg.data : "");
		}

		bool WebGPUDevice::Init(const SurfaceSource &s)
		{
			WGPUInstanceDescriptor id = {};
			static const WGPUInstanceFeatureName wanted[] = { WGPUInstanceFeatureName_TimedWaitAny };
			id.requiredFeatureCount = 1; id.requiredFeatures = wanted;
			instance = wgpuCreateInstance(&id);
			if (!instance) return false;

			WGPUSurfaceDescriptor sd = {};
#if defined(__EMSCRIPTEN__)
			// (the browser's: a canvas, by its selector - "#canvas" unless the host names another)
			WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas = {};
			canvas.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
			canvas.selector = Str(s.kind == SurfaceSource::Canvas && s.a ? (const char*)s.a : "#canvas");
			sd.nextInChain = &canvas.chain;
#else
			WGPUSurfaceSourceMetalLayer metal = {};
			WGPUSurfaceSourceWindowsHWND hwnd = {};
			WGPUSurfaceSourceXlibWindow xlib = {};
			WGPUSurfaceSourceWaylandSurface wayland = {};
			if (s.kind == SurfaceSource::MetalLayer) { metal.chain.sType = WGPUSType_SurfaceSourceMetalLayer; metal.layer = s.a; sd.nextInChain = &metal.chain; }
			else if (s.kind == SurfaceSource::Win32) { hwnd.chain.sType = WGPUSType_SurfaceSourceWindowsHWND; hwnd.hwnd = s.a; hwnd.hinstance = s.b; sd.nextInChain = &hwnd.chain; }
			else if (s.kind == SurfaceSource::Xlib) { xlib.chain.sType = WGPUSType_SurfaceSourceXlibWindow; xlib.display = s.a; xlib.window = (uint64_t)(uintptr_t)s.b; sd.nextInChain = &xlib.chain; }
			else if (s.kind == SurfaceSource::Wayland) { wayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface; wayland.display = s.a; wayland.surface = s.b; sd.nextInChain = &wayland.chain; }
#endif
			surface = wgpuInstanceCreateSurface(instance, &sd);

			WGPURequestAdapterOptions ao = {};
			ao.compatibleSurface = surface;
			ao.powerPreference = WGPUPowerPreference_HighPerformance;
			WGPURequestAdapterCallbackInfo ac = {};
			ac.mode = WGPUCallbackMode_WaitAnyOnly; ac.callback = OnAdapter; ac.userdata1 = &adapter;
			WGPUFutureWaitInfo wa = {}; wa.future = wgpuInstanceRequestAdapter(instance, &ao, ac);
			wgpuInstanceWaitAny(instance, 1, &wa, UINT64_MAX);
			if (!adapter) return false;

			WGPUAdapterInfo info = {};
			wgpuAdapterGetInfo(adapter, &info);
			caps.name = std::string(info.device.data ? info.device.data : "", info.device.length) + " (WebGPU / Dawn)";
			wgpuAdapterInfoFreeMembers(info);
			caps.bc = wgpuAdapterHasFeature(adapter, WGPUFeatureName_TextureCompressionBC);
			caps.timestamps = wgpuAdapterHasFeature(adapter, WGPUFeatureName_TimestampQuery);
			caps.maxPushConstantBytes = kDrawDataSlot;

			std::vector<WGPUFeatureName> features;
			if (caps.bc) features.push_back(WGPUFeatureName_TextureCompressionBC);
			if (caps.timestamps) features.push_back(WGPUFeatureName_TimestampQuery);
			// (32-bit float textures filtered - the sky's tables; R11G11B10F drawn into - bloom: features beyond WebGPU's
			// core, on nearly every adapter)
			if (wgpuAdapterHasFeature(adapter, WGPUFeatureName_Float32Filterable)) features.push_back(WGPUFeatureName_Float32Filterable);
			caps.rg11b10Target = wgpuAdapterHasFeature(adapter, WGPUFeatureName_RG11B10UfloatRenderable);
			if (caps.rg11b10Target) features.push_back(WGPUFeatureName_RG11B10UfloatRenderable);
#if !defined(__EMSCRIPTEN__)
			// (lists recorded on several threads: Dawn's device made thread-safe)
			if (wgpuAdapterHasFeature(adapter, WGPUFeatureName_ImplicitDeviceSynchronization)) { features.push_back(WGPUFeatureName_ImplicitDeviceSynchronization); caps.asyncCompute = false; threadSafe = true; }
#endif
			WGPUDeviceDescriptor dd = {};
			dd.label = Str("Pyros3D");
			dd.requiredFeatureCount = features.size(); dd.requiredFeatures = features.data();
			// (the adapter's own limits, not the spec's floor: a terrain's surface samples more than 16 textures, and every
			// adapter that runs the renderer has more)
			WGPULimits limits = {};
			if (wgpuAdapterGetLimits(adapter, &limits) == WGPUStatus_Success) dd.requiredLimits = &limits;
			dd.uncapturedErrorCallbackInfo.callback = OnError;
			// (Dawn checks every call against the spec; a shipped game does not pay for that - on with
			// PYROS_WEBGPU_VALIDATION=1. The browser validates in any case.)
#if !defined(__EMSCRIPTEN__)
			static const char* skip[] = { "skip_validation" };
			WGPUDawnTogglesDescriptor toggles = {};
			toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
			toggles.enabledToggleCount = 1; toggles.enabledToggles = skip;
			const char* v = getenv("PYROS_WEBGPU_VALIDATION");
			if (!(v && v[0] == '1')) dd.nextInChain = &toggles.chain;
#endif
			WGPURequestDeviceCallbackInfo dc = {};
			dc.mode = WGPUCallbackMode_WaitAnyOnly; dc.callback = OnDevice; dc.userdata1 = &device;
			WGPUFutureWaitInfo wd = {}; wd.future = wgpuAdapterRequestDevice(adapter, &dd, dc);
			wgpuInstanceWaitAny(instance, 1, &wd, UINT64_MAX);
			if (!device) return false;
			queue = wgpuDeviceGetQueue(device);

			WGPUSurfaceCapabilities sc = {};
			wgpuSurfaceGetCapabilities(surface, adapter, &sc);
			surfaceFormat = sc.formatCount ? sc.formats[0] : WGPUTextureFormat_BGRA8Unorm;
			for (size_t i = 0; i < sc.formatCount; i++) if (sc.formats[i] == WGPUTextureFormat_BGRA8UnormSrgb) surfaceFormat = sc.formats[i];
			// (a surface without an sRGB format - Metal's, here - is drawn into through an sRGB view of it, so the
			// picture is encoded as on every other backend: plain, it came out too dark)
			viewFormat = surfaceFormat == WGPUTextureFormat_BGRA8Unorm ? WGPUTextureFormat_BGRA8UnormSrgb
				: surfaceFormat == WGPUTextureFormat_RGBA8Unorm ? WGPUTextureFormat_RGBA8UnormSrgb : surfaceFormat;
			// (unsynchronised when asked, where the surface has it: immediate, else mailbox)
			presentMode = WGPUPresentMode_Fifo;
			if (!s.vsync)
				for (size_t i = 0; i < sc.presentModeCount; i++)
				{
					if (sc.presentModes[i] == WGPUPresentMode_Immediate) { presentMode = WGPUPresentMode_Immediate; break; }
					if (sc.presentModes[i] == WGPUPresentMode_Mailbox) presentMode = WGPUPresentMode_Mailbox;
				}
			caps.name += presentMode == WGPUPresentMode_Immediate ? ", present immediate" : presentMode == WGPUPresentMode_Mailbox ? ", present mailbox" : ", present fifo (vsync)";
			wgpuSurfaceCapabilitiesFreeMembers(sc);
			width = s.width; height = s.height; vsync = s.vsync;
			Configure();

			// group 0: a draw's data, at a dynamic offset into this frame's slice of the ring
			WGPUBindGroupLayoutEntry e = {};
			e.binding = 0; e.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
			e.buffer.type = WGPUBufferBindingType_Uniform; e.buffer.hasDynamicOffset = true; e.buffer.minBindingSize = kDrawDataSlot;
			WGPUBindGroupLayoutDescriptor bl = {}; bl.entryCount = 1; bl.entries = &e; bl.label = Str("draw data");
			drawLayout = wgpuDeviceCreateBindGroupLayout(device, &bl);
			WGPUBufferDescriptor rb = {}; rb.size = kDrawDataBytes * kFrames; rb.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst; rb.label = Str("draw data ring");
			drawRing = wgpuDeviceCreateBuffer(device, &rb);
			WGPUBindGroupEntry be = {}; be.binding = 0; be.buffer = drawRing; be.offset = 0; be.size = kDrawDataSlot;
			WGPUBindGroupDescriptor bd = {}; bd.layout = drawLayout; bd.entryCount = 1; bd.entries = &be;
			drawGroup = wgpuDeviceCreateBindGroup(device, &bd);
			if (caps.timestamps)
				for (uint32_t i = 0; i < kTimingSlots; i++)
				{
					WGPUQuerySetDescriptor qd = {}; qd.type = WGPUQueryType_Timestamp; qd.count = kTimedPasses * 2;
					timing[i].queries = wgpuDeviceCreateQuerySet(device, &qd);
					WGPUBufferDescriptor b1 = {}; b1.size = kTimedPasses * 16; b1.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
					timing[i].resolve = wgpuDeviceCreateBuffer(device, &b1);
					WGPUBufferDescriptor b2 = {}; b2.size = kTimedPasses * 16; b2.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
					timing[i].read = wgpuDeviceCreateBuffer(device, &b2);
				}
			return true;
		}

		void WebGPUDevice::Configure()
		{
			WGPUSurfaceConfiguration c = {};
			c.device = device; c.format = surfaceFormat; c.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;     // (copied from: a test's capture)
			if (viewFormat != surfaceFormat) { c.viewFormatCount = 1; c.viewFormats = &viewFormat; }
			c.width = width; c.height = height;
			c.presentMode = presentMode;
			c.alphaMode = WGPUCompositeAlphaMode_Auto;
			wgpuSurfaceConfigure(surface, &c);
		}

		WebGPUDevice::~WebGPUDevice()
		{
			if (device) wgpuDeviceRelease(device);
			if (surface) wgpuSurfaceRelease(surface);
			if (adapter) wgpuAdapterRelease(adapter);
			if (instance) wgpuInstanceRelease(instance);
		}

		Buffer WebGPUDevice::CreateBuffer(const BufferDesc &d, const void* initial)
		{
			WGPUBufferDescriptor bd = {};
			bd.label = Str(d.debugName);
			bd.size = (d.size + 3) & ~3ull;
			if (d.usage & BufVertex) bd.usage |= WGPUBufferUsage_Vertex;
			if (d.usage & BufIndex) bd.usage |= WGPUBufferUsage_Index;
			if (d.usage & BufUniform) bd.usage |= WGPUBufferUsage_Uniform;
			if (d.usage & BufStorage) bd.usage |= WGPUBufferUsage_Storage;
			if (d.usage & BufIndirect) bd.usage |= WGPUBufferUsage_Indirect;
			if (d.usage & BufCopySrc) bd.usage |= WGPUBufferUsage_CopySrc;
			bd.usage |= WGPUBufferUsage_CopyDst;
			if (d.memory == Memory::Readback) bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
			WGPUBuffer b = wgpuDeviceCreateBuffer(device, &bd);
			if (!b) return Buffer();
			if (initial) Write(b, 0, initial, d.size);
			Buffer h; h.id = buffers.Add(b); return h;
		}

		Texture WebGPUDevice::CreateTexture(const TextureDesc &d, const TextureData* data, uint32_t count)
		{
			WGPUTextureDescriptor td = {};
			td.label = Str(d.debugName);
			td.dimension = d.kind == TextureKind::Tex3D ? WGPUTextureDimension_3D : WGPUTextureDimension_2D;
			td.size = { d.width, d.height, d.kind == TextureKind::Cube ? 6u : d.depthOrLayers };
			td.format = ToWGPU(d.format); td.mipLevelCount = d.mips; td.sampleCount = d.samples;
			if (d.usage & TexSampled) td.usage |= WGPUTextureUsage_TextureBinding;
			if (d.usage & TexStorage) td.usage |= WGPUTextureUsage_StorageBinding;
			if (d.usage & (TexColorTarget | TexDepthTarget)) td.usage |= WGPUTextureUsage_RenderAttachment;
			if (d.usage & TexCopySrc) td.usage |= WGPUTextureUsage_CopySrc;
			// (written to by uploads - not a multisampled target, nor a depth one, which nothing copies into)
			const bool depth = d.format == Format::D32F || d.format == Format::D16 || d.format == Format::D24S8 || d.format == Format::D32FS8;
			if (d.samples <= 1 && !depth) td.usage |= WGPUTextureUsage_CopyDst;
			TextureRec r; r.desc = d;
			r.texture = wgpuDeviceCreateTexture(device, &td);
			if (!r.texture) return Texture();
			WGPUTextureViewDescriptor vd = {};
			vd.format = td.format; vd.mipLevelCount = d.mips; vd.baseArrayLayer = 0;
			vd.dimension = d.kind == TextureKind::Cube ? WGPUTextureViewDimension_Cube : d.kind == TextureKind::Tex2DArray ? WGPUTextureViewDimension_2DArray
				: d.kind == TextureKind::Tex3D ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
			vd.arrayLayerCount = d.kind == TextureKind::Tex3D ? 1 : td.size.depthOrArrayLayers;
			vd.aspect = WGPUTextureAspect_All;
			r.view = wgpuTextureCreateView(r.texture, &vd);
			Texture h; h.id = textures.Add(r);
			if (data && count) Upload(h, data, count);
			return h;
		}

		void WebGPUDevice::Upload(Texture to, const TextureData* data, uint32_t count)
		{
			TextureRec* r = textures.Get(to.id);
			if (!r) return;
			for (uint32_t i = 0; i < count; i++)
			{
				const uint32_t w = std::max(1u, r->desc.width >> data[i].mip), h = std::max(1u, r->desc.height >> data[i].mip);
				uint32_t dim = 1; const uint32_t bytes = BlockBytes(r->desc.format, dim);
				WGPUTexelCopyTextureInfo dst = {}; dst.texture = r->texture; dst.mipLevel = data[i].mip; dst.origin = { 0, 0, data[i].layer };
				WGPUTexelCopyBufferLayout layout = {}; layout.bytesPerRow = (w + dim - 1) / dim * bytes; layout.rowsPerImage = (h + dim - 1) / dim;
				WGPUExtent3D e = { w, h, 1 };
				wgpuQueueWriteTexture(queue, &dst, data[i].bytes, (size_t)data[i].size, &layout, &e);
			}
		}

		Sampler WebGPUDevice::CreateSampler(const SamplerDesc &d)
		{
			WGPUSamplerDescriptor sd = {};
			sd.addressModeU = ToWGPU(d.u); sd.addressModeV = ToWGPU(d.v); sd.addressModeW = ToWGPU(d.w);
			sd.magFilter = d.mag == Filter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
			sd.minFilter = d.min == Filter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
			sd.mipmapFilter = d.mip == Filter::Linear ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest;
			sd.lodMinClamp = 0.f; sd.lodMaxClamp = 32.f;
			sd.maxAnisotropy = (d.min == Filter::Linear && d.mag == Filter::Linear && d.mip == Filter::Linear) ? (uint16_t)std::max(1.f, std::min(16.f, d.anisotropy)) : 1;
			if (d.compare) sd.compare = WGPUCompareFunction_GreaterEqual;       // (reversed Z)
			Sampler h; h.id = samplers.Add(wgpuDeviceCreateSampler(device, &sd)); return h;
		}

		Shader WebGPUDevice::CreateShader(const ShaderDesc &d)
		{
			// (WGSL as given; else the SPIR-V - the renderer's, built at build time, or a material's surface compiled at run
			// time - turned into WGSL here, through Tint)
			std::string converted;
			const char* wgsl = d.wgsl;
			ShaderRec rec;
			rec.entry = d.entry ? d.entry : "main";
			if (!wgsl)
			{
#if defined(PYROS_GFX_SPIRV_TO_WGSL)
				std::string error;
				if (!d.spirv || !SpirvToWgsl(d.spirv, d.spirvWords, converted, error)) { fprintf(stderr, "WebGPU: shader '%s' could not be made WGSL: %s\n", d.debugName ? d.debugName : "?", error.c_str()); return Shader(); }
				wgsl = converted.c_str();
#else
				fprintf(stderr, "WebGPU: shader '%s' has no WGSL (this build turns no SPIR-V into WGSL)\n", d.debugName ? d.debugName : "?"); return Shader();
#endif
			}
			WGPUShaderSourceWGSL src = {}; src.chain.sType = WGPUSType_ShaderSourceWGSL; src.code = Str(wgsl);
			WGPUShaderModuleDescriptor md = {}; md.nextInChain = &src.chain; md.label = Str(d.debugName);
			rec.module = wgpuDeviceCreateShaderModule(device, &md);
			if (!rec.module) return Shader();
			Shader h; h.id = shaders.Add(rec); return h;
		}

		WGPUBindGroupLayout WebGPUDevice::MaterialLayout(const std::vector<BindingDesc> &m)
		{
			std::vector<WGPUBindGroupLayoutEntry> es(m.size());
			for (size_t i = 0; i < m.size(); i++)
			{
				WGPUBindGroupLayoutEntry &e = es[i]; e = WGPUBindGroupLayoutEntry();
				e.binding = m[i].binding;
				e.visibility = (m[i].vertex ? WGPUShaderStage_Vertex : 0) | (m[i].fragment ? WGPUShaderStage_Fragment : 0) | (m[i].compute ? WGPUShaderStage_Compute : 0);
				switch (m[i].kind)
				{
				case BindingKind::Texture2D: e.texture.sampleType = WGPUTextureSampleType_Float; e.texture.viewDimension = WGPUTextureViewDimension_2D; break;
				case BindingKind::TextureCube: e.texture.sampleType = WGPUTextureSampleType_Float; e.texture.viewDimension = WGPUTextureViewDimension_Cube; break;
				case BindingKind::Texture2DArray: e.texture.sampleType = WGPUTextureSampleType_Float; e.texture.viewDimension = WGPUTextureViewDimension_2DArray; break;
				case BindingKind::Texture3D: e.texture.sampleType = WGPUTextureSampleType_Float; e.texture.viewDimension = WGPUTextureViewDimension_3D; break;
				case BindingKind::DepthTexture: e.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat; e.texture.viewDimension = WGPUTextureViewDimension_2D; break;
				case BindingKind::UnfilteredTexture2D: e.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat; e.texture.viewDimension = WGPUTextureViewDimension_2D; break;
				case BindingKind::ShadowTexture: e.texture.sampleType = WGPUTextureSampleType_Depth; e.texture.viewDimension = WGPUTextureViewDimension_2D; break;
				case BindingKind::Sampler: e.sampler.type = WGPUSamplerBindingType_Filtering; break;
				case BindingKind::ComparisonSampler: e.sampler.type = WGPUSamplerBindingType_Comparison; break;
				case BindingKind::Uniform: e.buffer.type = WGPUBufferBindingType_Uniform; e.buffer.hasDynamicOffset = m[i].dynamic; break;
				case BindingKind::StorageRead: e.buffer.type = WGPUBufferBindingType_ReadOnlyStorage; e.buffer.hasDynamicOffset = m[i].dynamic; break;
				case BindingKind::StorageReadWrite: e.buffer.type = WGPUBufferBindingType_Storage; e.buffer.hasDynamicOffset = m[i].dynamic; break;
				}
			}
			WGPUBindGroupLayoutDescriptor d = {}; d.entryCount = es.size(); d.entries = es.data(); d.label = Str("material");
			return wgpuDeviceCreateBindGroupLayout(device, &d);
		}

		void OnRenderPipeline(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline p, WGPUStringView msg, void* u1, void*)
		{
			PipelineRec* r = (PipelineRec*)u1;
			if (status == WGPUCreatePipelineAsyncStatus_Success) { r->render = p; r->state.store(1); }
			if (r->owner) { r->owner->cFinished.fetch_add(1); r->owner->cPending.fetch_sub(1); }
			else { fprintf(stderr, "WebGPU: pipeline failed: %.*s\n", (int)msg.length, msg.data ? msg.data : ""); r->state.store(2); }
		}
		void OnComputePipeline(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline p, WGPUStringView msg, void* u1, void*)
		{
			PipelineRec* r = (PipelineRec*)u1;
			if (status == WGPUCreatePipelineAsyncStatus_Success) { r->compute = p; r->state.store(1); }
			if (r->owner) { r->owner->cFinished.fetch_add(1); r->owner->cPending.fetch_sub(1); }
			else { fprintf(stderr, "WebGPU: compute pipeline failed: %.*s\n", (int)msg.length, msg.data ? msg.data : ""); r->state.store(2); }
		}

		Pipeline WebGPUDevice::CreatePipeline(const GraphicsPipelineDesc &d)
		{
			ShaderRec* vs = shaders.Get(d.vertex.id); ShaderRec* fs = shaders.Get(d.fragment.id);
			if (!vs || !vs->module) return Pipeline();
			PipelineRec* r = new PipelineRec();
			r->owner = this; cPending.fetch_add(1);
			r->drawDataBytes = d.drawDataBytes;
			r->material = MaterialLayout(d.material);
			WGPUBindGroupLayout layouts[2] = { drawLayout, r->material };
			WGPUPipelineLayoutDescriptor pl = {}; pl.bindGroupLayoutCount = 2; pl.bindGroupLayouts = layouts;
			r->layout = wgpuDeviceCreatePipelineLayout(device, &pl);

			// (by binding number - WebGPU's slot is the layout's place in this list: a skinned pipeline's bones at 2 with
			// nothing at 1 leave 1 unused - its step mode undefined and no attributes)
			size_t slots = 0;
			for (const VertexBinding &vb : d.bindings) slots = std::max<size_t>(slots, vb.binding + 1);
			std::vector<WGPUVertexBufferLayout> vbl(slots);
			std::vector<std::vector<WGPUVertexAttribute> > attrs(slots);
			for (size_t i = 0; i < slots; i++) { vbl[i] = {}; vbl[i].stepMode = WGPUVertexStepMode_Undefined; }
			for (const VertexBinding &vb : d.bindings)
			{
				const uint32_t b = vb.binding;
				for (const VertexAttribute &a : d.attributes)
					if (a.binding == b)
					{
						WGPUVertexAttribute va = {}; va.format = VertexFormat(a.format); va.offset = a.offset; va.shaderLocation = a.location;
						attrs[b].push_back(va);
					}
				vbl[b].arrayStride = vb.stride;
				vbl[b].stepMode = vb.perInstance ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex;
				vbl[b].attributeCount = attrs[b].size(); vbl[b].attributes = attrs[b].data();
			}
			WGPURenderPipelineDescriptor pd = {};
			pd.label = Str(d.debugName);
			pd.layout = r->layout;
			pd.vertex.module = vs->module; pd.vertex.entryPoint = Str(vs->entry.c_str());
			pd.vertex.bufferCount = vbl.size(); pd.vertex.buffers = vbl.data();
			pd.primitive.topology = d.topology == Topology::Lines ? WGPUPrimitiveTopology_LineList : d.topology == Topology::Points ? WGPUPrimitiveTopology_PointList
				: d.topology == Topology::TriangleStrip ? WGPUPrimitiveTopology_TriangleStrip : WGPUPrimitiveTopology_TriangleList;
			pd.primitive.frontFace = d.frontCounterClockwise ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
			pd.primitive.cullMode = d.cull == Cull::Back ? WGPUCullMode_Back : d.cull == Cull::Front ? WGPUCullMode_Front : WGPUCullMode_None;
			WGPUDepthStencilState ds = {};
			if (d.depthFormat != Format::Unknown)
			{
				ds.format = ToWGPU(d.depthFormat);
				ds.depthWriteEnabled = d.depthWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
				ds.depthCompare = d.depthTest ? ToWGPU(d.depthCompare) : WGPUCompareFunction_Always;
				ds.stencilFront.compare = ds.stencilBack.compare = WGPUCompareFunction_Always;
				ds.stencilFront.passOp = ds.stencilBack.passOp = WGPUStencilOperation_Keep;
				ds.stencilFront.failOp = ds.stencilBack.failOp = WGPUStencilOperation_Keep;
				ds.stencilFront.depthFailOp = ds.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
				ds.depthBias = (int32_t)d.depthBiasConstant; ds.depthBiasSlopeScale = d.depthBiasSlope;
				pd.depthStencil = &ds;
			}
			pd.multisample.count = d.samples; pd.multisample.mask = 0xFFFFFFFFu; pd.multisample.alphaToCoverageEnabled = d.alphaToCoverage;
			WGPUColorTargetState targets[8] = {}; WGPUBlendState blends[8] = {};
			for (uint32_t i = 0; i < d.colorCount; i++)
			{
				targets[i].format = d.colorFormats[i] == Format::Unknown ? viewFormat : ToWGPU(d.colorFormats[i]);
				targets[i].writeMask = d.colorWrite ? WGPUColorWriteMask_All : WGPUColorWriteMask_None;
				if (BlendState(d.blend[i], blends[i])) targets[i].blend = &blends[i];
			}
			WGPUFragmentState fsd = {};
			if (fs && fs->module)
			{
				fsd.module = fs->module; fsd.entryPoint = Str(fs->entry.c_str()); fsd.targetCount = d.colorCount; fsd.targets = targets;
				pd.fragment = &fsd;
			}
			WGPUCreateRenderPipelineAsyncCallbackInfo cb = {};
			cb.mode = WGPUCallbackMode_AllowProcessEvents; cb.callback = OnRenderPipeline; cb.userdata1 = r;
			wgpuDeviceCreateRenderPipelineAsync(device, &pd, cb);
			Pipeline h; h.id = pipelines.Add(r); return h;
		}

		Pipeline WebGPUDevice::CreatePipeline(const ComputePipelineDesc &d)
		{
			ShaderRec* cs = shaders.Get(d.compute.id);
			if (!cs || !cs->module) return Pipeline();
			PipelineRec* r = new PipelineRec();
			r->owner = this; cPending.fetch_add(1);
			r->drawDataBytes = d.drawDataBytes;
			r->material = MaterialLayout(d.material);
			WGPUBindGroupLayout layouts[2] = { drawLayout, r->material };
			WGPUPipelineLayoutDescriptor pl = {}; pl.bindGroupLayoutCount = 2; pl.bindGroupLayouts = layouts;
			r->layout = wgpuDeviceCreatePipelineLayout(device, &pl);
			WGPUComputePipelineDescriptor pd = {};
			pd.label = Str(d.debugName); pd.layout = r->layout;
			pd.compute.module = cs->module; pd.compute.entryPoint = Str(cs->entry.c_str());
			WGPUCreateComputePipelineAsyncCallbackInfo cb = {};
			cb.mode = WGPUCallbackMode_AllowProcessEvents; cb.callback = OnComputePipeline; cb.userdata1 = r;
			wgpuDeviceCreateComputePipelineAsync(device, &pd, cb);
			Pipeline h; h.id = pipelines.Add(r); return h;
		}

		bool WebGPUDevice::PipelineReady(Pipeline p) const
		{
			PipelineRec* const* r = const_cast<WebGPUDevice*>(this)->pipelines.Get(p.id);
			return r && *r && (*r)->state.load() == 1;
		}

		BindGroup WebGPUDevice::CreateBindGroup(Pipeline layoutOf, const BindGroupEntry* e, uint32_t count)
		{
			PipelineRec** r = pipelines.Get(layoutOf.id);
			if (!r || !*r) return BindGroup();
			std::vector<WGPUBindGroupEntry> es(count);
			for (uint32_t i = 0; i < count; i++)
			{
				es[i] = WGPUBindGroupEntry(); es[i].binding = e[i].binding;
				if (TextureRec* t = textures.Get(e[i].texture.id)) es[i].textureView = t->view;
				if (WGPUSampler* s = samplers.Get(e[i].sampler.id)) es[i].sampler = *s;
				if (WGPUBuffer* b = buffers.Get(e[i].buffer.id)) { es[i].buffer = *b; es[i].offset = e[i].offset; es[i].size = e[i].size ? e[i].size : WGPU_WHOLE_SIZE; }
			}
			WGPUBindGroupDescriptor d = {}; d.layout = (*r)->material; d.entryCount = count; d.entries = es.data();
			BindGroup h; h.id = groups.Add(wgpuDeviceCreateBindGroup(device, &d)); return h;
		}

		void WebGPUDevice::BeginFrame()
		{
			// (begun again before it was presented - a second view drawn ahead of the frame's own, a scope's: the same frame)
			if (frameOpen) return;
			frameOpen = true;
			wgpuInstanceProcessEvents(instance);        // (pipelines finished on Dawn's workers, and timings read back)
			{
				std::lock_guard<std::mutex> l(statsMutex);
				stats.draws = cDraws.exchange(0); stats.skippedDraws = cSkipped.exchange(0); stats.pipelineChanges = cPipelines.exchange(0);
				stats.bindGroupChanges = cBindGroups.exchange(0); stats.dispatches = cDispatches.exchange(0);
				stats.pipelinesFinished = cFinished.exchange(0); stats.pipelinesPending = cPending.load();
				stats.drawDataBytes = std::min<uint64_t>(frameCursor.load(), kDrawDataBytes);
			}
			ReadTimings();
			frameNumber++;
			frame = (frame + 1) % kFrames;
			frameBase = (uint64_t)frame * kDrawDataBytes; frameCursor.store(0); flushed = 0; warnedFull.store(false);
		}

		CommandList* WebGPUDevice::NewCommandList(const char* name)
		{
			std::lock_guard<std::mutex> l(recordMutex);
			WebGPUCommandList* c = new WebGPUCommandList();
			c->dev = this; c->name = name;
			WGPUCommandEncoderDescriptor ed = {}; ed.label = Str(name);
			c->encoder = wgpuDeviceCreateCommandEncoder(device, &ed);
			lists.push_back(c);
			return c;
		}

		void WebGPUDevice::Submit(CommandList* const* l, uint32_t count)
		{
			// What the frame's draws have given since the last submit, written before the lists that read it
			const uint64_t upTo = std::min<uint64_t>(frameCursor.load(), kDrawDataBytes);
			if (upTo > flushed) { wgpuQueueWriteBuffer(queue, drawRing, frameBase + flushed, &frameData[flushed], (size_t)(upTo - flushed)); flushed = upTo; }
			std::vector<WGPUCommandBuffer> cbs;
			for (uint32_t i = 0; i < count; i++)
			{
				WebGPUCommandList* c = static_cast<WebGPUCommandList*>(l[i]);
				c->EndPass();
				cDraws += c->lDraws; cSkipped += c->lSkipped; cPipelines += c->lPipelines; cBindGroups += c->lBindGroups; cDispatches += c->lDispatches;
				WGPUCommandBufferDescriptor cd = {};
				cbs.push_back(wgpuCommandEncoderFinish(c->encoder, &cd));
				wgpuCommandEncoderRelease(c->encoder);
				delete c;
			}
			lists.clear();
			wgpuQueueSubmit(queue, cbs.size(), cbs.data());
			for (WGPUCommandBuffer cb : cbs) wgpuCommandBufferRelease(cb);
		}

		bool WebGPUDevice::ReadTexture(Texture t, std::vector<uint8_t> &out)
		{
			TextureRec* r = textures.Get(t.id);
			if (!r || !r->texture || r->backbuffer) return false;
			const uint32_t bpt = BytesPerTexel(r->desc.format);
			if (!bpt || !(r->desc.usage & TexCopySrc) || r->desc.samples > 1) return false;     // (copied out: made with TexCopySrc)
			const uint32_t w = wgpuTextureGetWidth(r->texture), h = wgpuTextureGetHeight(r->texture), row = (w * bpt + 255) & ~255u;
			WGPUBufferDescriptor bd = {}; bd.size = (uint64_t)row * h; bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
			WGPUBuffer read = wgpuDeviceCreateBuffer(device, &bd);
			WGPUCommandEncoderDescriptor ed = {}; WGPUCommandEncoder e = wgpuDeviceCreateCommandEncoder(device, &ed);
			WGPUTexelCopyTextureInfo src = {}; src.texture = r->texture;
			WGPUTexelCopyBufferInfo dst = {}; dst.buffer = read; dst.layout.bytesPerRow = row; dst.layout.rowsPerImage = h;
			WGPUExtent3D ext = { w, h, 1 };
			wgpuCommandEncoderCopyTextureToBuffer(e, &src, &dst, &ext);
			WGPUCommandBufferDescriptor cd = {}; WGPUCommandBuffer cb = wgpuCommandEncoderFinish(e, &cd);
			wgpuQueueSubmit(queue, 1, &cb); wgpuCommandBufferRelease(cb); wgpuCommandEncoderRelease(e);
			bool mapped = false;
			WGPUBufferMapCallbackInfo mc = {}; mc.mode = WGPUCallbackMode_WaitAnyOnly; mc.userdata1 = &mapped;
			mc.callback = [](WGPUMapAsyncStatus s, WGPUStringView, void* u1, void*) { *(bool*)u1 = s == WGPUMapAsyncStatus_Success; };
			WGPUFutureWaitInfo wf = {}; wf.future = wgpuBufferMapAsync(read, WGPUMapMode_Read, 0, bd.size, mc);
			wgpuInstanceWaitAny(instance, 1, &wf, UINT64_MAX);
			if (mapped)
			{
				const uint8_t* p = (const uint8_t*)wgpuBufferGetConstMappedRange(read, 0, bd.size);
				out.resize((size_t)w * h * bpt);
				for (uint32_t y = 0; y < h; y++) memcpy(&out[(size_t)y * w * bpt], p + (size_t)y * row, (size_t)w * bpt);
				wgpuBufferUnmap(read);
			}
			wgpuBufferRelease(read);
			return mapped;
		}

		Texture WebGPUDevice::AcquireBackbuffer()
		{
			// (one a frame: a second pass over it - an editor's panels over its views - draws on the same)
			if (surfaceTexture && backbuffer) { Texture h; h.id = backbuffer; return h; }
			WGPUSurfaceTexture st = {};
			wgpuSurfaceGetCurrentTexture(surface, &st);
			if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
			{
				Configure(); return Texture();
			}
			surfaceTexture = st.texture;
			if (backbuffer) { TextureRec* old = textures.Get(backbuffer); if (old && old->view) wgpuTextureViewRelease(old->view); textures.Remove(backbuffer); backbuffer = 0; }
			TextureRec r; r.texture = st.texture; r.backbuffer = true;
			r.desc.width = wgpuTextureGetWidth(st.texture); r.desc.height = wgpuTextureGetHeight(st.texture); r.desc.format = FromWGPU(viewFormat);
			WGPUTextureViewDescriptor vd = {}; vd.format = viewFormat; vd.dimension = WGPUTextureViewDimension_2D; vd.mipLevelCount = 1; vd.arrayLayerCount = 1; vd.aspect = WGPUTextureAspect_All;
			r.view = wgpuTextureCreateView(st.texture, &vd);
			backbuffer = textures.Add(r);
			Texture h; h.id = backbuffer; return h;
		}

		int WebGPUDevice::TimedPass(const char* name)
		{
			if (!caps.timestamps) return -1;
			std::lock_guard<std::mutex> l(timingMutex);
			Timing &t = timing[frameNumber % kTimingSlots];
			if (t.state.load() != 0 || t.names.size() >= kTimedPasses) return -1;     // (still being read: this frame untimed)
			t.names.push_back(name ? name : "pass");
			return (int)t.names.size() - 1;
		}

		void WebGPUDevice::ResolveTimings()
		{
			Timing &t = timing[frameNumber % kTimingSlots];
			if (!caps.timestamps || t.names.empty() || t.state.load() != 0) return;
			WGPUCommandEncoderDescriptor ed = {}; WGPUCommandEncoder e = wgpuDeviceCreateCommandEncoder(device, &ed);
			const uint32_t n = (uint32_t)t.names.size() * 2;
			wgpuCommandEncoderResolveQuerySet(e, t.queries, 0, n, t.resolve, 0);
			wgpuCommandEncoderCopyBufferToBuffer(e, t.resolve, 0, t.read, 0, n * 8);
			WGPUCommandBufferDescriptor cd = {}; WGPUCommandBuffer cb = wgpuCommandEncoderFinish(e, &cd);
			wgpuQueueSubmit(queue, 1, &cb); wgpuCommandBufferRelease(cb); wgpuCommandEncoderRelease(e);
			t.frame = frameNumber; t.state.store(1);
			WGPUBufferMapCallbackInfo mc = {};
			mc.mode = WGPUCallbackMode_AllowProcessEvents; mc.userdata1 = &t;
			mc.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* u1, void*) {
				Timing* t = (Timing*)u1;
				t->ms.clear();
				if (status == WGPUMapAsyncStatus_Success)
				{
					const uint64_t* ts = (const uint64_t*)wgpuBufferGetConstMappedRange(t->read, 0, t->names.size() * 16);
					// (a pass's start is stamped when its first stage begins, which then waits on the pass before it - Dawn on
					// Metal, as Metal's own counters: its cost is from the later of its start and the previous pass's end)
					uint64_t prevEnd = 0;
					for (size_t i = 0; ts && i < t->names.size(); i++)
					{
						const uint64_t a = ts[i * 2], b = ts[i * 2 + 1];
						if (b <= a) { t->ms.push_back(0.0); continue; }
						const uint64_t from = std::max(a, prevEnd);
						prevEnd = std::max(prevEnd, b);
						t->ms.push_back(b > from ? (double)(b - from) * 1e-6 : 0.0);     // (nanoseconds)
					}
					wgpuBufferUnmap(t->read);
				}
				t->state.store(2);
			};
			wgpuBufferMapAsync(t.read, WGPUMapMode_Read, 0, n * 8, mc);
		}

		void WebGPUDevice::ReadTimings()
		{
			for (uint32_t i = 0; i < kTimingSlots; i++)
			{
				Timing &t = timing[i];
				if (t.state.load() != 2) continue;
				if (t.frame >= gpuStats.frame || gpuStats.passes.empty())
				{
					std::lock_guard<std::mutex> l(statsMutex);
					gpuStats.frame = t.frame; gpuStats.passes.clear(); gpuStats.gpuMs = 0.0;
					for (size_t k = 0; k < t.ms.size(); k++) { FrameStats::Pass p; p.name = t.names[k]; p.gpuMs = t.ms[k]; gpuStats.passes.push_back(p); gpuStats.gpuMs += t.ms[k]; }
				}
				t.names.clear(); t.state.store(0);
			}
		}

		const FrameStats &WebGPUDevice::LastFrameStats()
		{
			std::lock_guard<std::mutex> l(statsMutex);
			const uint64_t f = gpuStats.frame; std::vector<FrameStats::Pass> p = gpuStats.passes; const double g = gpuStats.gpuMs;
			stats.frame = f; stats.passes = p; stats.gpuMs = g;
			return stats;
		}

		void WebGPUDevice::Present()
		{
			frameOpen = false;
			ResolveTimings();
			// (PYROS_SHOT=<file.png>: the 120th frame as it is shown, read back
			// from the GPU - a test's way to see what was drawn without a screen capture)
#if !defined(__EMSCRIPTEN__)
			const char* shotPath = getenv("PYROS_SHOT");
			if (shotPath && frameNumber == ShotFrame() && surfaceTexture)
			{
				const uint32_t w = wgpuTextureGetWidth(surfaceTexture), h = wgpuTextureGetHeight(surfaceTexture), row = (w * 4 + 255) & ~255u;
				WGPUBufferDescriptor bd = {}; bd.size = (uint64_t)row * h; bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
				WGPUBuffer read = wgpuDeviceCreateBuffer(device, &bd);
				WGPUCommandEncoderDescriptor ed = {}; WGPUCommandEncoder e = wgpuDeviceCreateCommandEncoder(device, &ed);
				WGPUTexelCopyTextureInfo src = {}; src.texture = surfaceTexture;
				WGPUTexelCopyBufferInfo dst = {}; dst.buffer = read; dst.layout.bytesPerRow = row; dst.layout.rowsPerImage = h;
				WGPUExtent3D ext = { w, h, 1 };
				wgpuCommandEncoderCopyTextureToBuffer(e, &src, &dst, &ext);
				WGPUCommandBufferDescriptor cd = {}; WGPUCommandBuffer cb = wgpuCommandEncoderFinish(e, &cd);
				wgpuQueueSubmit(queue, 1, &cb); wgpuCommandBufferRelease(cb); wgpuCommandEncoderRelease(e);
				bool mapped = false;
				WGPUBufferMapCallbackInfo mc = {}; mc.mode = WGPUCallbackMode_WaitAnyOnly; mc.userdata1 = &mapped;
				mc.callback = [](WGPUMapAsyncStatus s, WGPUStringView, void* u1, void*) { *(bool*)u1 = s == WGPUMapAsyncStatus_Success; };
				WGPUFutureWaitInfo wf = {}; wf.future = wgpuBufferMapAsync(read, WGPUMapMode_Read, 0, bd.size, mc);
				wgpuInstanceWaitAny(instance, 1, &wf, UINT64_MAX);
				if (mapped)
				{
					const uint8_t* p = (const uint8_t*)wgpuBufferGetConstMappedRange(read, 0, bd.size);
					std::vector<uint8_t> px((size_t)w * h * 4);
					const bool bgra = surfaceFormat == WGPUTextureFormat_BGRA8Unorm || surfaceFormat == WGPUTextureFormat_BGRA8UnormSrgb;
					for (uint32_t y = 0; y < h; y++)
						for (uint32_t x = 0; x < w; x++)
						{
							const uint8_t* s = p + (size_t)y * row + x * 4; uint8_t* o = &px[((size_t)y * w + x) * 4];
							o[0] = bgra ? s[2] : s[0]; o[1] = s[1]; o[2] = bgra ? s[0] : s[2]; o[3] = 255;
						}
					wgpuBufferUnmap(read);
					WritePng(shotPath, px.data(), w, h);
				}
				wgpuBufferRelease(read);
			}
			wgpuSurfacePresent(surface);
#endif
			// (in a browser the canvas is shown when the frame's task returns to it: there is no present)
			if (surfaceTexture) { wgpuTextureRelease(surfaceTexture); surfaceTexture = nullptr; }
		}
	}

	Device* CreateWebGPUDevice(const SurfaceSource &surface)
	{
		WebGPUDevice* d = new WebGPUDevice();
		if (!d->Init(surface)) { delete d; return nullptr; }
		return d;
	}

} }
