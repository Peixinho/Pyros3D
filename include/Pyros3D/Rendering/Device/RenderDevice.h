//============================================================================
// Name        : RenderDevice.h
// Author      : Duarte Peixinho
// Description : The explicit device API the renderer is rebuilt on - DRAFT for
//               review (docs/device-design.md). Not built yet: nothing includes it.
//
// What it replaces: IRenderDevice is OpenGL's state machine (set the blend, bind a
// texture to a target, draw), and Vulkan and Metal emulate it - building pipelines,
// descriptor sets and render passes as the state is seen, at draw time. Here the
// renderer says up front what it will draw with, and a draw names those objects:
//   - a pipeline is made from a full description, on a worker, ahead of use - never
//     on the draw path; a draw whose pipeline is not ready is the renderer's to skip
//   - a material's textures and buffers are one bind group, made when the material is -
//     never per draw; where the backend has bindless they are slots in one table instead
//   - a draw's own data (its object, its material) is a slot in a per-frame buffer: an
//     offset, not a descriptor - the same on WebGPU (no push constants) as everywhere
//   - command lists are recorded on any thread and submitted in order
//   - a pass says what it renders into; its barriers come from the frame graph
//============================================================================

#ifndef PYROS_RENDERDEVICE2_H
#define PYROS_RENDERDEVICE2_H

#include <cstdint>
#include <string>
#include <vector>

namespace p3d { namespace gfx {

	// ---------------------------------------------------------------- handles
	// Typed, so a texture cannot be passed where a buffer is wanted. 0 is none.
	// (generation in the top bits: a handle to something freed and its slot reused is
	// caught, not drawn with)
	template <typename Tag> struct Handle { uint32_t id = 0; explicit operator bool() const { return id != 0; } bool operator==(Handle o) const { return id == o.id; } };
	struct BufferTag; struct TextureTag; struct SamplerTag; struct ShaderTag; struct PipelineTag; struct QueryTag; struct BindGroupTag; struct RayGeometryTag;
	typedef Handle<BufferTag> Buffer;
	typedef Handle<TextureTag> Texture;
	typedef Handle<SamplerTag> Sampler;
	typedef Handle<ShaderTag> Shader;
	typedef Handle<PipelineTag> Pipeline;
	typedef Handle<QueryTag> QueryPool;
	typedef Handle<BindGroupTag> BindGroup;
	typedef Handle<RayGeometryTag> RayGeometry;     // (a mesh's triangles built for the ray-tracing hardware: Caps().rayQuery)

	// Where a texture or buffer sits in the bindless table: what a shader indexes with.
	// Stable for the resource's life.
	typedef uint32_t BindlessIndex;
	static const BindlessIndex kNoIndex = 0xFFFFFFFFu;

	// ---------------------------------------------------------------- formats
	enum class Format : uint16_t {
		Unknown,
		R8, RG8, RGBA8, RGBA8_sRGB, BGRA8, BGRA8_sRGB,
		R16F, RG16F, RGBA16F, R32F, RG32F, RGBA32F, R11G11B10F, RGB10A2,
		R32U, RG32U,
		BC1, BC1_sRGB, BC3, BC3_sRGB, BC4, BC5, BC6H, BC7, BC7_sRGB,
		D32F, D24S8, D32FS8,
		RGB32F,         // a vertex attribute's three floats only (no texture has it)
		D16,            // 16-bit depth: a shadow map's, half the bandwidth of D32F
	};

	// ---------------------------------------------------------------- resources
	enum BufferUsage : uint32_t {
		BufVertex = 1, BufIndex = 2, BufUniform = 4, BufStorage = 8, BufIndirect = 16,
		BufCopySrc = 32, BufCopyDst = 64,
	};
	enum class Memory : uint8_t {
		GpuOnly,        // device local; filled by copies
		Upload,         // written by the CPU every frame (ring-allocated by the device)
		Readback,       // read by the CPU after a fence
	};
	struct BufferDesc {
		uint64_t size = 0;
		uint32_t usage = 0;
		Memory memory = Memory::GpuOnly;
		bool bindless = false;          // a slot in the table, for shaders that index buffers
		const char* debugName = nullptr;
	};

	enum TextureUsage : uint32_t {
		TexSampled = 1, TexStorage = 2, TexColorTarget = 4, TexDepthTarget = 8, TexCopySrc = 16, TexCopyDst = 32,
	};
	enum class TextureKind : uint8_t { Tex2D, Tex2DArray, Cube, Tex3D };
	struct TextureDesc {
		TextureKind kind = TextureKind::Tex2D;
		Format format = Format::RGBA8;
		uint32_t width = 1, height = 1, depthOrLayers = 1, mips = 1, samples = 1;
		uint32_t usage = TexSampled;
		bool transient = false;         // the frame graph's: lives inside one frame, memory aliased
		const char* debugName = nullptr;
	};
	struct TextureData {                // one mip of one layer, tightly packed
		uint32_t mip = 0, layer = 0;
		const void* bytes = nullptr;
		uint64_t size = 0;
	};

	enum class Filter : uint8_t { Nearest, Linear };
	enum class Address : uint8_t { Repeat, Clamp, Mirror, Border };
	struct SamplerDesc {
		Filter min = Filter::Linear, mag = Filter::Linear, mip = Filter::Linear;
		Address u = Address::Repeat, v = Address::Repeat, w = Address::Repeat;
		float anisotropy = 8.f, lodBias = 0.f;
		bool compare = false;           // shadow maps
	};

	// ---------------------------------------------------------------- shaders, pipelines
	enum class Stage : uint8_t { Vertex, Fragment, Compute, Mesh, Task };
	struct ShaderDesc {
		Stage stage = Stage::Vertex;
		const uint32_t* spirv = nullptr; uint64_t spirvWords = 0;   // Vulkan, and Metal through SPIRV-Cross - built at build time
		const char* wgsl = nullptr;                                // WebGPU: the same shader as WGSL, built at build time (Tint)
		const char* entry = "main";
		const char* debugName = nullptr;
	};

	enum class Topology : uint8_t { Triangles, TriangleStrip, Lines, Points };
	enum class Cull : uint8_t { None, Back, Front };
	enum class Compare : uint8_t { Never, Less, LessEqual, Equal, Greater, GreaterEqual, Always };
	enum class Blend : uint8_t { Opaque, Alpha, PremultipliedAlpha, Additive, Multiply };
	struct VertexAttribute { uint8_t location, binding; Format format; uint16_t offset; };
	struct VertexBinding { uint8_t binding; uint16_t stride; bool perInstance; };

	// What a material's bind group holds (group 1; group 0 is the device's: the frame's per-draw data, at
	// a dynamic offset - DrawData). Said in the pipeline, so the layout is the same on every backend: an
	// automatic layout cannot take a dynamic offset.
	// DepthTexture: a depth texture read as values (texelFetch: a depth prepass, a scene's depth); ShadowTexture: one
	// compared against (a shadow map, with a ComparisonSampler) - WebGPU's layouts tell them apart
	// (UnfilteredTexture2D: a float texture read by texelFetch alone - RG32F, RGBA32F: WebGPU filters 32-bit floats only
	// with a feature not every device has)
	enum class BindingKind : uint8_t { Texture2D, TextureCube, Texture2DArray, Texture3D, DepthTexture, Sampler, ComparisonSampler, Uniform, StorageRead, StorageReadWrite, ShadowTexture, UnfilteredTexture2D,
		RayScene };     // (RayScene: the frame's scene for ray queries - SetRayScene's; no entry in a bind group, the device binds it)
	// `dynamic` (a uniform or storage buffer): its offset given by each SetBindGroup - an object's slot in a ring the
	// frame writes, without a bind group a draw (Vulkan's dynamic descriptors, WebGPU's hasDynamicOffset, Metal's offsets)
	struct BindingDesc { uint32_t binding; BindingKind kind; bool vertex = true, fragment = true, compute = false, dynamic = false; };

	// Everything a draw's state is, at once. The render-target formats are part of it
	// (Vulkan's dynamic rendering, Metal's pipeline descriptor): no render-pass object to match.
	struct GraphicsPipelineDesc {
		Shader vertex, fragment;
		std::vector<BindingDesc> material;          // its group 1 (see BindingDesc)
		uint32_t drawDataBytes = 0;                 // how much of group 0 a draw gives (DrawData), 0 none
		std::vector<VertexBinding> bindings;        // empty: vertices pulled from storage buffers by index
		std::vector<VertexAttribute> attributes;
		Topology topology = Topology::Triangles;
		Cull cull = Cull::Back;
		bool frontCounterClockwise = true;
		bool depthTest = true, depthWrite = true;
		bool colorWrite = true;      // (false: its colour targets left as they are - a pass that lays down depth alone)
		Compare depthCompare = Compare::GreaterEqual;    // reversed Z
		float depthBiasConstant = 0.f, depthBiasSlope = 0.f;
		bool alphaToCoverage = false;
		Format colorFormats[8] = {}; uint8_t colorCount = 0;
		Blend blend[8] = {};
		Format depthFormat = Format::Unknown;
		uint8_t samples = 1;
		const char* debugName = nullptr;
	};
	struct ComputePipelineDesc { Shader compute; std::vector<BindingDesc> material; uint32_t drawDataBytes = 0; const char* debugName = nullptr; };

	// ---------------------------------------------------------------- recording
	// What a pass renders into, and whether it keeps or clears what was there.
	enum class Load : uint8_t { Keep, Clear, DontCare };
	enum class StoreOp : uint8_t { Store, DontCare, Resolve };
	struct ColorTarget { Texture texture; uint32_t mip = 0, layer = 0; Load load = Load::Clear; StoreOp store = StoreOp::Store; float clear[4] = { 0.f, 0.f, 0.f, 1.f }; Texture resolve; };
	struct DepthTarget { Texture texture; uint32_t layer = 0; Load load = Load::Clear; StoreOp store = StoreOp::Store; float clear = 0.f; };   // reversed Z: 0 is far
	struct PassDesc {
		ColorTarget colors[8]; uint8_t colorCount = 0;
		DepthTarget depth;
		uint32_t width = 0, height = 0;
		const char* debugName = nullptr;
	};

	// A resource's state: what the next use needs it to be. The frame graph says these;
	// the device turns them into Vulkan barriers / Metal fences and hazard tracking.
	enum class Access : uint8_t {
		Undefined, ColorTarget, DepthTarget, DepthRead, ShaderRead, ShaderWrite, Indirect,
		CopySrc, CopyDst, Present, VertexIndex,
	};
	struct Barrier { Texture texture; Buffer buffer; Access from = Access::Undefined, to = Access::Undefined; };

	// A plain format's bytes a texel (0: a depth, compressed or packed one)
	inline uint32_t BytesPerTexel(const Format f)
	{
		switch (f)
		{
		case Format::R8: return 1;
		case Format::RG8: case Format::R16F: return 2;
		case Format::RGBA8: case Format::RGBA8_sRGB: case Format::BGRA8: case Format::BGRA8_sRGB: case Format::RG16F: case Format::R32F: case Format::R32U:
		case Format::R11G11B10F: case Format::RGB10A2: return 4;
		case Format::RGBA16F: case Format::RG32F: case Format::RG32U: return 8;
		case Format::RGBA32F: return 16;
		default: return 0;
		}
	}

	// Recorded on any thread; one list a thread a pass. Submitted in the order given.
	class CommandList {
	public:
		virtual ~CommandList() {}
		virtual void Barriers(const Barrier* barriers, uint32_t count) = 0;
		virtual void BeginPass(const PassDesc &pass) = 0;
		virtual void EndPass() = 0;
		virtual void SetViewport(float x, float y, float w, float h, float minDepth = 0.f, float maxDepth = 1.f) = 0;
		virtual void SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
		virtual void SetPipeline(Pipeline pipeline) = 0;
		// The draw's own data, up to 256 bytes, into this frame's per-draw buffer: its object's slot,
		// its material's, its indices. A dynamic offset into one buffer - WebGPU has no push constants,
		// and the native backends take it the same way (or as push constants, where cheaper).
		virtual void DrawData(const void* data, uint32_t size) = 0;
		// A material's resources, made once (CreateBindGroup) - set when the material changes. dynamicOffsets: one for
		// each of its `dynamic` bindings, in the order of their binding numbers, added to the entry's own offset
		virtual void SetBindGroup(BindGroup group, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicCount = 0) = 0;
		virtual void SetVertexBuffer(uint32_t binding, Buffer buffer, uint64_t offset) = 0;
		virtual void SetIndexBuffer(Buffer buffer, uint64_t offset, bool index32) = 0;
		virtual void Draw(uint32_t vertices, uint32_t instances, uint32_t firstVertex, uint32_t firstInstance) = 0;
		virtual void DrawIndexed(uint32_t indices, uint32_t instances, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;
		// GPU-driven: arguments written by a culling pass. drawCount draws read from args; a count buffer
		// only where Caps().indirectCount (not core WebGPU: there culled draws are written as zero-instance
		// draws, and drawCount is the bucket's size)
		virtual void DrawIndexedIndirect(Buffer args, uint64_t offset, uint32_t drawCount, Buffer count = Buffer(), uint64_t countOffset = 0) = 0;
		virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
		virtual void DispatchIndirect(Buffer args, uint64_t offset) = 0;
		virtual void CopyBuffer(Buffer from, uint64_t fromOffset, Buffer to, uint64_t toOffset, uint64_t size) = 0;
		virtual void CopyTexture(Texture from, Texture to) = 0;
		virtual void Timestamp(QueryPool pool, uint32_t index) = 0;
		virtual void BeginMarker(const char* name) = 0;
		virtual void EndMarker() = 0;
	};

	// ---------------------------------------------------------------- the device
	struct Capabilities {
		// What the backend has beyond WebGPU's core - the baseline every feature has to work on
		bool bindless = false, indirectCount = false, multiDrawIndirect = false, meshShaders = false, rayQuery = false,
			asyncCompute = false, bc = true, timestamps = false,
			rg11b10Target = true;       // (R11G11B10F as a render target: WebGPU's core has it only as a feature)
		uint32_t maxBindlessTextures = 0, maxPushConstantBytes = 128;
		std::string name;
	};

	// What a frame cost: measured in every backend, always on (a few queries a pass), read back without
	// waiting - the GPU's numbers are of the newest frame it has finished, two or three behind.
	struct FrameStats {
		struct Pass { std::string name; double gpuMs = 0.0; };
		uint64_t frame = 0;                 // the frame these GPU times are of
		std::vector<Pass> passes;
		double gpuMs = 0.0;                 // the passes', end to end
		uint32_t draws = 0, dispatches = 0, pipelineChanges = 0, bindGroupChanges = 0, skippedDraws = 0;
		uint64_t drawDataBytes = 0;
		uint32_t pipelinesPending = 0;      // still compiling (their draws skipped)
		uint32_t pipelinesFinished = 0;     // became ready during the frame
	};

	class Device {
	public:
		virtual ~Device() {}
		virtual const Capabilities &Caps() const = 0;
		// The window's backbuffer's format, as its views see it (a pipeline's Format::Unknown target is this): a texture
		// drawn into in its place - an editor's viewport - is made with it, the same pipelines then draw either
		virtual Format BackbufferFormat() const = 0;
		// How far past SDR white the screen shows now (1: an SDR picture). More than 1 only with an HDR backbuffer
		// (PYROS_HDR=1, on a display with headroom - Metal's EDR): the tonemap then lets highlights reach it
		virtual float OutputHeadroom() const { return 1.f; }
		// A texture's pixels - its first mip and layer - into `out`, rows tightly packed, as its format stores them (BGRA
		// for a BGRA one). Waits: the work submitted before it, this frame's too, is done first. For an editor's picking,
		// thumbnails, bakes and screenshots - not for every frame. False for a format of no plain texel size (a depth
		// or compressed one).
		virtual bool ReadTexture(Texture texture, std::vector<uint8_t> &out) = 0;

		// ---- resources (any thread)
		virtual Buffer CreateBuffer(const BufferDesc &desc, const void* initial = nullptr) = 0;
		virtual Texture CreateTexture(const TextureDesc &desc, const TextureData* data = nullptr, uint32_t dataCount = 0) = 0;
		virtual Sampler CreateSampler(const SamplerDesc &desc) = 0;
		virtual void Destroy(Buffer b) = 0;                 // freed once the frames using it are done
		virtual void Destroy(Texture t) = 0;
		// A material's set of resources, laid out as its shaders declare them (made once, at load)
		struct BindGroupEntry { uint32_t binding; Texture texture; Sampler sampler; Buffer buffer; uint64_t offset = 0, size = 0; };
		virtual BindGroup CreateBindGroup(Pipeline layoutOf, const BindGroupEntry* entries, uint32_t count) = 0;   // its group 1
		virtual void Destroy(BindGroup g) = 0;
		// Where Caps().bindless: its slot in the one table (kNoIndex otherwise)
		virtual BindlessIndex Index(Texture t, Sampler s) = 0;
		virtual BindlessIndex Index(Buffer b) = 0;
		// An upload buffer's memory for this frame (ring: valid until the frame is submitted)
		virtual void* Map(Buffer upload, uint64_t &offsetOut, uint64_t size) = 0;
		// Data into a GPU-only resource, through the device's staging ring, before the next submit
		virtual void Upload(Buffer to, uint64_t offset, const void* data, uint64_t size) = 0;
		virtual void Upload(Texture to, const TextureData* data, uint32_t count) = 0;

		// ---- ray queries (Caps().rayQuery: the GPU's ray-tracing hardware - Metal's acceleration structures)
		// A mesh's triangles built once, in its own space: positions xyz `strideFloats` apart, three 32-bit indices a
		// triangle. Each frame the scene's instances - a geometry, the 3x4 matrix that puts it in the world (rows), and
		// an id the shader reads (rayQueryGetIntersectionInstanceCustomIndexEXT) - make the frame's scene, which a
		// shader's `accelerationStructureEXT` (BindingKind::RayScene) traces. Without the hardware: no-ops.
		struct RayInstance { RayGeometry geometry; float transform[12]; uint32_t id = 0; };
		virtual RayGeometry CreateRayGeometry(const float* positions, uint32_t vertexCount, uint32_t strideFloats, const uint32_t* indices, uint32_t indexCount)
		{ (void)positions; (void)vertexCount; (void)strideFloats; (void)indices; (void)indexCount; return RayGeometry(); }
		virtual void Destroy(RayGeometry g) { (void)g; }
		virtual void SetRayScene(const RayInstance* instances, uint32_t count) { (void)instances; (void)count; }     // (once a frame, before its lists are submitted)

		// ---- shaders and pipelines (any thread; pipelines compile on the device's workers)
		virtual Shader CreateShader(const ShaderDesc &desc) = 0;
		virtual Pipeline CreatePipeline(const GraphicsPipelineDesc &desc) = 0;     // returns at once
		virtual Pipeline CreatePipeline(const ComputePipelineDesc &desc) = 0;
		virtual bool PipelineReady(Pipeline p) const = 0;                          // the draw's to ask
		virtual void SavePipelineCache(const std::string &path) = 0;

		// ---- frames
		virtual void BeginFrame() = 0;                        // waits for the frame N-2 slot
		virtual CommandList* NewCommandList(const char* debugName) = 0;   // any thread, during the frame
		virtual void Submit(CommandList* const* lists, uint32_t count) = 0;   // in this order
		virtual Texture AcquireBackbuffer() = 0;
		virtual void Present() = 0;
		// The window's new size in pixels: the backbuffer is that size from the next frame on (between frames)
		virtual void ResizeSurface(uint32_t width, uint32_t height) = 0;
		virtual void EndFrame() = 0;
		virtual double GpuMilliseconds(QueryPool pool, uint32_t from, uint32_t to) = 0;   // last finished frame
		virtual const FrameStats &LastFrameStats() = 0;    // counters of the last frame begun, GPU times of the newest finished
	};

	// WebGPU is a first backend, not a port: its core is the baseline every feature works on, and the
	// native backends add what they have (Caps). Vulkan 1.3 (dynamic rendering, descriptor indexing,
	// synchronization2 - MoltenVK has them), Metal 3 (argument buffers tier 2), WebGPU (the browser through
	// Emscripten, and native through Dawn). OpenGL: proposed to drop - not decided.
	// What a device presents to: a window's native surface
	struct SurfaceSource {
		enum Kind : uint8_t { MetalLayer, Win32, Xlib, Wayland, Canvas } kind = MetalLayer;
		void* a = nullptr;              // CAMetalLayer*, HWND, Display*, wl_display*, or a canvas selector (const char*)
		void* b = nullptr;              // -, HINSTANCE, Window (as uintptr), wl_surface*
		uint32_t width = 0, height = 0;
		bool vsync = true;
	};
	Device* CreateVulkanDevice(const SurfaceSource &surface);
	Device* CreateMetalDevice(const SurfaceSource &surface);
	Device* CreateWebGPUDevice(const SurfaceSource &surface);

} }

#endif
