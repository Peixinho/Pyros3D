//============================================================================
// Name        : VulkanDevice.cpp
// Author      : Duarte Peixinho
// Description : The device layer on Vulkan 1.3 - dynamic rendering, synchronization2, VMA; MoltenVK on the Mac.
//               The same frame as the WebGPU backend's (docs/device-design.md): a draw's data is a
//               slot in this frame's ring - persistently mapped here, written in place - bound at a
//               dynamic offset; a material's resources are one descriptor set made at load; pipelines
//               compile on worker threads into a pipeline cache and are never waited for by a draw.
//
// Conventions kept the same as WebGPU's, so one shader source means the same everywhere: clip space y
// up (a negative viewport height), depth 0..1, reversed Z as the renderer asks for it.
//============================================================================

#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <cstdlib>
// (the frame a test's capture is of: PYROS_SHOT_FRAME, else the 120th)
static inline uint64_t ShotFrame() { static const uint64_t f = getenv("PYROS_SHOT_FRAME") ? (uint64_t)atoll(getenv("PYROS_SHOT_FRAME")) : 120; return f; }
#include <Pyros3D/Rendering/Device/WritePng.h>
#include <volk.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace p3d { namespace gfx {

	namespace {

		VkFormat ToVk(const Format f)
		{
			switch (f)
			{
			case Format::R8: return VK_FORMAT_R8_UNORM;
			case Format::RG8: return VK_FORMAT_R8G8_UNORM;
			case Format::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
			case Format::RGBA8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
			case Format::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
			case Format::BGRA8_sRGB: return VK_FORMAT_B8G8R8A8_SRGB;
			case Format::R16F: return VK_FORMAT_R16_SFLOAT;
			case Format::RG16F: return VK_FORMAT_R16G16_SFLOAT;
			case Format::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
			case Format::R32F: return VK_FORMAT_R32_SFLOAT;
			case Format::RG32F: return VK_FORMAT_R32G32_SFLOAT;
			case Format::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case Format::R11G11B10F: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
			case Format::RGB10A2: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
			case Format::R32U: return VK_FORMAT_R32_UINT;
			case Format::RG32U: return VK_FORMAT_R32G32_UINT;
			case Format::BC1: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
			case Format::BC1_sRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
			case Format::BC3: return VK_FORMAT_BC3_UNORM_BLOCK;
			case Format::BC3_sRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
			case Format::BC4: return VK_FORMAT_BC4_UNORM_BLOCK;
			case Format::BC5: return VK_FORMAT_BC5_UNORM_BLOCK;
			case Format::BC6H: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
			case Format::BC7: return VK_FORMAT_BC7_UNORM_BLOCK;
			case Format::BC7_sRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
			case Format::D32F: return VK_FORMAT_D32_SFLOAT;
			case Format::D16: return VK_FORMAT_D16_UNORM;
			case Format::D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
			case Format::D32FS8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
			default: return VK_FORMAT_UNDEFINED;
			}
		}
		bool IsDepth(const Format f) { return f == Format::D32F || f == Format::D16 || f == Format::D24S8 || f == Format::D32FS8; }
		uint32_t BlockBytes(const Format f, uint32_t &dim)
		{
			dim = 1;
			switch (f)
			{
			case Format::R8: return 1;
			case Format::RG8: case Format::R16F: return 2;
			case Format::RGBA16F: case Format::RG32F: case Format::RG32U: return 8;
			case Format::RGBA32F: return 16;
			case Format::BC1: case Format::BC1_sRGB: case Format::BC4: dim = 4; return 8;
			case Format::BC3: case Format::BC3_sRGB: case Format::BC5: case Format::BC6H: case Format::BC7: case Format::BC7_sRGB: dim = 4; return 16;
			default: return 4;
			}
		}
		VkCompareOp ToVk(const Compare c)
		{
			switch (c)
			{
			case Compare::Never: return VK_COMPARE_OP_NEVER;
			case Compare::Less: return VK_COMPARE_OP_LESS;
			case Compare::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
			case Compare::Equal: return VK_COMPARE_OP_EQUAL;
			case Compare::Greater: return VK_COMPARE_OP_GREATER;
			case Compare::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
			default: return VK_COMPARE_OP_ALWAYS;
			}
		}
		VkSamplerAddressMode ToVk(const Address a)
		{
			switch (a)
			{
			case Address::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case Address::Border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			case Address::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			}
		}
		VkFormat VertexFormat(const Format f)
		{
			switch (f)
			{
			case Format::R32F: return VK_FORMAT_R32_SFLOAT;
			case Format::RG32F: return VK_FORMAT_R32G32_SFLOAT;
			case Format::RGB32F: return VK_FORMAT_R32G32B32_SFLOAT;
			case Format::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case Format::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
			case Format::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
			case Format::R32U: return VK_FORMAT_R32_UINT;
			default: return VK_FORMAT_R32G32B32_SFLOAT;     // (Unknown: three floats - as the WebGPU backend)
			}
		}

		template <typename T> struct Pool {
			// (a deque: what Get gave stays where it is when more are added - a vector moved them all, and an
			// upload that made its staging buffer wrote through a pointer to where the buffer had been)
			// (alive: an id removed is not there - Get gives nothing for it, and a second Remove of it is ignored: it was put on
			// the free list twice, and two new resources were then given the one id)
			std::deque<T> items; std::deque<bool> alive; std::vector<uint32_t> free; std::mutex m;
			uint32_t Add(const T &t)
			{
				std::lock_guard<std::mutex> l(m);
				if (!free.empty()) { uint32_t i = free.back(); free.pop_back(); items[i] = t; alive[i] = true; return i + 1; }
				items.push_back(t); alive.push_back(true); return (uint32_t)items.size();
			}
			T* Get(uint32_t id) { return (id && id <= items.size() && alive[id - 1]) ? &items[id - 1] : nullptr; }
			void Remove(uint32_t id)
			{
				std::lock_guard<std::mutex> l(m);
				if (id && id <= items.size() && alive[id - 1]) { items[id - 1] = T(); alive[id - 1] = false; free.push_back(id - 1); }
			}
		};

		struct BufferRec { VkBuffer buffer = VK_NULL_HANDLE; VmaAllocation alloc = nullptr; void* mapped = nullptr; uint64_t size = 0; };
		struct TextureRec {
			VkImage image = VK_NULL_HANDLE; VmaAllocation alloc = nullptr; VkImageView view = VK_NULL_HANDLE;
			TextureDesc desc; VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; bool swapchain = false;
		};
		struct PipelineRec {
			std::atomic<int> state{ 0 };            // 0 compiling, 1 ready, 2 failed
			VkPipeline pipeline = VK_NULL_HANDLE; VkPipelineLayout layout = VK_NULL_HANDLE; VkDescriptorSetLayout material = VK_NULL_HANDLE;
			bool compute = false;
			bool rayScene = false;                 // (its shaders trace the frame's scene: set 2, the device's - BindingKind::RayScene)
			std::vector<BindingDesc> bindings;     // (what its group 1 holds - a bind group's buffers written as the kind said)
			std::string name;
		};

		const uint32_t kFrames = 2;
		const uint32_t kDrawDataSlot = 256;
		const uint64_t kDrawDataBytes = 8ull << 20;
		const uint32_t kTimedPasses = 64;
		const uint32_t kSlotChunk = 64;

		class VulkanDevice;

		class VulkanCommandList : public CommandList {
		public:
			VulkanDevice* dev = nullptr;
			VkCommandBuffer cmd = VK_NULL_HANDLE;
			bool inPass = false, inCompute = false;
			PipelineRec* current = nullptr;
			bool skipping = true, skipDraw = false;
			uint32_t lDraws = 0, lSkipped = 0, lPipelines = 0, lBindGroups = 0, lDispatches = 0;
			uint64_t slotNext = 0, slotEnd = 0;
			int timedSlot = -1;
			std::vector<uint32_t> attachments;      // textures this pass renders into (kept: their layout after)
			void Barriers(const Barrier* b, uint32_t count) override;
			void BeginPass(const PassDesc &p) override;
			void EndPass() override;
			void SetViewport(float x, float y, float w, float h, float mn, float mx) override;
			void SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override { VkRect2D r = { { x, y }, { w, h } }; vkCmdSetScissor(cmd, 0, 1, &r); }
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
			void CopyBuffer(Buffer from, uint64_t fo, Buffer to, uint64_t too, uint64_t size) override;
			void CopyTexture(Texture from, Texture to) override;
			void Timestamp(QueryPool, uint32_t) override {}
			void BeginMarker(const char*) override {}
			void EndMarker() override {}
			bool Ready() const { return inPass && !skipping && !skipDraw; }
		};

		class VulkanDevice : public Device {
		public:
			VkInstance instance = VK_NULL_HANDLE; VkPhysicalDevice phys = VK_NULL_HANDLE; VkDevice device = VK_NULL_HANDLE;
			VkQueue queue = VK_NULL_HANDLE; uint32_t family = 0; VkSurfaceKHR surface = VK_NULL_HANDLE;
			VmaAllocator vma = nullptr;
			BufferRec shotBuffer; uint32_t shotW = 0, shotH = 0; bool shotWritten = false;     // (PYROS_SHOT)
			VkSwapchainKHR swapchain = VK_NULL_HANDLE; VkFormat swapFormat = VK_FORMAT_B8G8R8A8_SRGB; VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
			std::vector<uint32_t> swapTextures; std::vector<VkSemaphore> renderDone;
			uint32_t width = 0, height = 0, imageIndex = 0; bool vsync = true, haveImage = false;
			Capabilities caps; float timestampPeriod = 1.f;
			Pool<BufferRec> buffers; Pool<TextureRec> textures; Pool<VkSampler> samplers; Pool<VkShaderModule> shaders;
			Pool<PipelineRec*> pipelines; Pool<VkDescriptorSet> groups;
			VkDescriptorSetLayout drawLayout = VK_NULL_HANDLE; VkDescriptorPool descPool = VK_NULL_HANDLE; VkDescriptorSet drawSet = VK_NULL_HANDLE;
			uint32_t drawRing = 0;
			VkPipelineCache pipelineCache = VK_NULL_HANDLE;
			struct Frame {
				// (a pool a list: lists are recorded on several threads at once, and a pool is one thread's)
				std::vector<VkCommandPool> pools; std::vector<VkCommandBuffer> cmds; uint32_t used = 0;
				VkFence fence = VK_NULL_HANDLE; VkSemaphore acquired = VK_NULL_HANDLE;
				VkQueryPool queries = VK_NULL_HANDLE; std::vector<std::string> names; bool timed = false;
				// (what was destroyed while this frame was recorded: freed once its fence says the GPU is done with it -
				// Vulkan, unlike Metal and WebGPU, keeps nothing alive for work in flight)
				std::vector<std::function<void()> > retire;
				// (its lists, submitted and closed, in order: given to the queue at once when the frame is presented or
				// ended - several submits a frame (views, then an editor's panels over them) wait on its image once,
				// signal its end once, and hold one fence)
				std::vector<VkCommandBuffer> pending;
				bool acquireWaited = false;     // (its image's acquire waited on already - by a submit before its end: a readback)
			} frames[kFrames];
			uint32_t frame = 0; uint64_t frameNumber = 0, frameBase = 0;
			std::atomic<uint64_t> frameCursor{ 0 };
			std::mutex recordMutex;
			std::vector<VulkanCommandList*> lists;
			// pipelines compile here, never on a frame
			std::vector<std::thread> workers; std::deque<std::function<void()> > jobs; std::mutex jobMutex; std::condition_variable jobCv; bool stopping = false;
			// profiling
			std::atomic<uint32_t> cDraws{ 0 }, cSkipped{ 0 }, cPipelines{ 0 }, cBindGroups{ 0 }, cDispatches{ 0 }, cFinished{ 0 }, cPending{ 0 };
			FrameStats stats, gpuStats; std::mutex statsMutex, timingMutex;

			bool Init(const SurfaceSource &s);
			~VulkanDevice() override;
			const Capabilities &Caps() const override { return caps; }
			Format BackbufferFormat() const override
			{
				switch (swapFormat)
				{
				case VK_FORMAT_B8G8R8A8_UNORM: return Format::BGRA8;
				case VK_FORMAT_R8G8B8A8_UNORM: return Format::RGBA8;
				case VK_FORMAT_R8G8B8A8_SRGB: return Format::RGBA8_sRGB;
				default: return Format::BGRA8_sRGB;
				}
			}
			Buffer CreateBuffer(const BufferDesc &d, const void* initial) override;
			Texture CreateTexture(const TextureDesc &d, const TextureData* data, uint32_t count) override;
			Sampler CreateSampler(const SamplerDesc &d) override;
			void Destroy(Buffer b) override;
			void Destroy(Texture t) override;
			BindGroup CreateBindGroup(Pipeline layoutOf, const BindGroupEntry* e, uint32_t count) override;
			// Ray queries (VK_KHR_acceleration_structure + VK_KHR_ray_query, where the device has them): each geometry's own
			// structure (built at once, waited on), and each frame's scene over them, built by a command buffer that goes
			// ahead of the frame's lists in its submit - in set 2 of a pipeline that traces it (a set a frame in flight)
			struct RayGeometryRec { VkAccelerationStructureKHR as = VK_NULL_HANDLE; BufferRec storage; VkDeviceAddress address = 0; };
			Pool<RayGeometryRec> rayGeometries;
			VkDescriptorSetLayout rtLayout = VK_NULL_HANDLE; VkDescriptorPool rtPool = VK_NULL_HANDLE; VkCommandPool rtCmdPool = VK_NULL_HANDLE;
			VkDescriptorSet rtSets[kFrames] = {}; VkCommandBuffer rtCmds[kFrames] = {};
			VkAccelerationStructureKHR rtScenes[kFrames] = {}; BufferRec rtSceneStorage[kFrames], rtInstances[kFrames], rtScratch[kFrames];
			uint64_t rtBuiltOn[kFrames] = {}; bool rtQueued[kFrames] = {};
			uint64_t rtScratchAlign = 256;
			RayGeometry rayNothing;
			bool RtBuffer(BufferRec &out, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible, VkDeviceAddress* address);
			void RtFree(BufferRec &b) { if (b.buffer) vmaDestroyBuffer(vma, b.buffer, b.alloc); b = BufferRec(); }
			RayGeometry CreateRayGeometry(const float* positions, uint32_t vertexCount, uint32_t strideFloats, const uint32_t* indices, uint32_t indexCount) override;
			void Destroy(RayGeometry g) override;
			void SetRayScene(const RayInstance* instances, uint32_t count) override;
			// (freed once the frames recorded with it are done, as a buffer is: MoltenVK encodes a list when it is submitted -
			// at the frame's end, after a group let go mid-frame - and the GPU may still be reading it)
			void Destroy(BindGroup g) override
			{
				if (VkDescriptorSet* s = groups.Get(g.id))
				{
					const VkDescriptorSet set = *s;
					frames[frame].retire.push_back([this, set] { VkDescriptorSet d = set; vkFreeDescriptorSets(device, descPool, 1, &d); });
					groups.Remove(g.id);
				}
			}
			BindlessIndex Index(Texture, Sampler) override { return kNoIndex; }
			BindlessIndex Index(Buffer) override { return kNoIndex; }
			void* Map(Buffer b, uint64_t &offsetOut, uint64_t) override { BufferRec* r = buffers.Get(b.id); offsetOut = 0; return r ? r->mapped : nullptr; }
			void Upload(Buffer to, uint64_t offset, const void* data, uint64_t size) override;
			void Upload(Texture to, const TextureData* data, uint32_t count) override;
			Shader CreateShader(const ShaderDesc &d) override;
			Pipeline CreatePipeline(const GraphicsPipelineDesc &d) override;
			Pipeline CreatePipeline(const ComputePipelineDesc &d) override;
			bool PipelineReady(Pipeline p) const override;
			void SavePipelineCache(const std::string &path) override;
			bool frameOpen = false;     // (begun and not yet presented)
			void BeginFrame() override;
			CommandList* NewCommandList(const char* name) override;
			void Submit(CommandList* const* l, uint32_t count) override;
			Texture AcquireBackbuffer() override;
			void Present() override;
			void ResizeSurface(uint32_t w, uint32_t h) override { if (w == width && h == height) return; width = w; height = h; vkDeviceWaitIdle(device); CreateSwapchain(); }
			void EndFrame() override { Flush(); }
			double GpuMilliseconds(QueryPool, uint32_t, uint32_t) override { return 0.0; }
			const FrameStats &LastFrameStats() override;
			VkCommandBuffer FrameCommandBuffer();     // (a primary from this frame's pools, begun; recordMutex held)
			void Flush();
			void SubmitPending();     // (the frame's lists so far to the queue - not its end: a readback reads what they drew)
			bool ReadTexture(Texture t, std::vector<uint8_t> &out) override;

			bool CreateSwapchain();
			VkDescriptorSetLayout MaterialLayout(const std::vector<BindingDesc> &m);
			void Transition(VkCommandBuffer cmd, TextureRec &t, VkImageLayout to);
			void OneShot(const std::function<void(VkCommandBuffer)> &f);
			void Job(std::function<void()> f) { { std::lock_guard<std::mutex> l(jobMutex); jobs.push_back(std::move(f)); } jobCv.notify_one(); }
			int TimedPass(const char* name);
		};

		// ---------------------------------------------------------------- layouts
		void LayoutStage(VkImageLayout l, VkPipelineStageFlags2 &stage, VkAccessFlags2 &access)
		{
			switch (l)
			{
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT; break;
			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT; break;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT; break;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT; access = VK_ACCESS_2_TRANSFER_WRITE_BIT; break;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT; access = VK_ACCESS_2_TRANSFER_READ_BIT; break;
			case VK_IMAGE_LAYOUT_GENERAL: stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT; break;
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT; access = 0; break;
			default: stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; access = 0; break;
			}
		}
		void VulkanDevice::Transition(VkCommandBuffer cmd, TextureRec &t, VkImageLayout to)
		{
			if (t.layout == to) return;
			VkImageMemoryBarrier2 b = {}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			LayoutStage(t.layout, b.srcStageMask, b.srcAccessMask);
			LayoutStage(to, b.dstStageMask, b.dstAccessMask);
			b.oldLayout = t.layout; b.newLayout = to; b.image = t.image;
			b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.subresourceRange.aspectMask = IsDepth(t.desc.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS; b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			VkDependencyInfo d = {}; d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO; d.imageMemoryBarrierCount = 1; d.pImageMemoryBarriers = &b;
			vkCmdPipelineBarrier2(cmd, &d);
			t.layout = to;
		}

		// ---------------------------------------------------------------- the list
		void VulkanCommandList::Barriers(const Barrier* b, uint32_t count)
		{
			for (uint32_t i = 0; i < count; i++)
				if (TextureRec* t = dev->textures.Get(b[i].texture.id))
				{
					VkImageLayout to = b[i].to == Access::ShaderRead ? (IsDepth(t->desc.format) ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
						: b[i].to == Access::ShaderWrite ? VK_IMAGE_LAYOUT_GENERAL : b[i].to == Access::CopySrc ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
						: b[i].to == Access::CopyDst ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : b[i].to == Access::Present ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : t->layout;
					dev->Transition(cmd, *t, to);
				}
		}
		void VulkanCommandList::BeginPass(const PassDesc &p)
		{
			EndPass();
			VkRenderingAttachmentInfo colors[8] = {};
			uint32_t w = p.width, h = p.height;
			attachments.clear();
			for (uint32_t i = 0; i < p.colorCount; i++)
			{
				TextureRec* t = dev->textures.Get(p.colors[i].texture.id);
				if (!t) continue;
				dev->Transition(cmd, *t, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				attachments.push_back(p.colors[i].texture.id);
				colors[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				colors[i].imageView = t->view; colors[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colors[i].loadOp = p.colors[i].load == Load::Keep ? VK_ATTACHMENT_LOAD_OP_LOAD : p.colors[i].load == Load::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colors[i].storeOp = p.colors[i].store == StoreOp::DontCare ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
				memcpy(colors[i].clearValue.color.float32, p.colors[i].clear, 16);
				// (a multisampled target resolved into its single-sampled texture at the pass's end - the average)
				if (TextureRec* r = dev->textures.Get(p.colors[i].resolve.id))
				{
					dev->Transition(cmd, *r, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					colors[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
					colors[i].resolveImageView = r->view; colors[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				if (!w) { w = t->desc.width; h = t->desc.height; }
			}
			VkRenderingAttachmentInfo depth = {};
			TextureRec* d = dev->textures.Get(p.depth.texture.id);
			if (d)
			{
				dev->Transition(cmd, *d, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
				depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				depth.imageView = d->view; depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
				depth.loadOp = p.depth.load == Load::Keep ? VK_ATTACHMENT_LOAD_OP_LOAD : p.depth.load == Load::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				depth.storeOp = p.depth.store == StoreOp::DontCare ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
				depth.clearValue.depthStencil.depth = p.depth.clear;
				if (!w) { w = d->desc.width; h = d->desc.height; }
			}
			timedSlot = dev->TimedPass(p.debugName);
			VulkanDevice::Frame &f = dev->frames[dev->frame];
			if (timedSlot >= 0) vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, f.queries, timedSlot * 2);
			VkRenderingInfo ri = {}; ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			ri.renderArea = { { 0, 0 }, { w, h } }; ri.layerCount = 1;
			ri.colorAttachmentCount = p.colorCount; ri.pColorAttachments = colors;
			ri.pDepthAttachment = d ? &depth : nullptr;
			vkCmdBeginRendering(cmd, &ri);
			inPass = true; skipping = true; current = nullptr;
			SetViewport(0.f, 0.f, (float)w, (float)h, 0.f, 1.f);
			SetScissor(0, 0, w, h);
		}
		void VulkanCommandList::EndPass()
		{
			if (inPass)
			{
				vkCmdEndRendering(cmd);
				if (timedSlot >= 0) vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, dev->frames[dev->frame].queries, timedSlot * 2 + 1);
				timedSlot = -1; inPass = false;
			}
			inCompute = false;
		}
		void VulkanCommandList::SetViewport(float x, float y, float w, float h, float mn, float mx)
		{
			// (clip space y up, as WebGPU's: the viewport turned over)
			VkViewport v = { x, y + h, w, -h, mn, mx };
			vkCmdSetViewport(cmd, 0, 1, &v);
		}
		void VulkanCommandList::SetPipeline(Pipeline p)
		{
			PipelineRec** r = dev->pipelines.Get(p.id);
			current = r ? *r : nullptr;
			skipping = !current || current->state.load() != 1;
			if (skipping) return;
			vkCmdBindPipeline(cmd, current->compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, current->pipeline);
			lPipelines++;
			// (the frame's scene, where it traces it - one built this frame, else an empty one: never an unwritten set)
			if (current->rayScene)
			{
				if (dev->rtBuiltOn[dev->frame] != dev->frameNumber || !dev->rtScenes[dev->frame]) dev->SetRayScene(nullptr, 0);
				if (dev->rtScenes[dev->frame])
					vkCmdBindDescriptorSets(cmd, current->compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, current->layout, 2, 1, &dev->rtSets[dev->frame], 0, nullptr);
			}
		}
		void VulkanCommandList::DrawData(const void* data, uint32_t size)
		{
			if (skipping || !size) return;
			if (slotNext == slotEnd) { slotNext = dev->frameCursor.fetch_add(kDrawDataSlot * kSlotChunk); slotEnd = slotNext + kDrawDataSlot * kSlotChunk; }
			const uint64_t at = slotNext; slotNext += kDrawDataSlot;      // (64 slots taken at once: threads do not meet on the cursor every draw)
			if (at + kDrawDataSlot > kDrawDataBytes) { skipDraw = true; return; }
			BufferRec* ring = dev->buffers.Get(dev->drawRing);
			memcpy((uint8_t*)ring->mapped + dev->frameBase + at, data, size < kDrawDataSlot ? size : kDrawDataSlot);      // (written in place: the ring is mapped)
			const uint32_t offset = (uint32_t)(dev->frameBase + at);
			vkCmdBindDescriptorSets(cmd, current->compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, current->layout, 0, 1, &dev->drawSet, 1, &offset);
			skipDraw = false;
		}
		void VulkanCommandList::SetBindGroup(BindGroup g, const uint32_t* dynamicOffsets, uint32_t dynamicCount)
		{
			VkDescriptorSet* s = dev->groups.Get(g.id);
			// (a group let go and bound still is the caller's bug: said - once - and the draw goes without it)
			if (!s && g.id) { static bool said = false; if (!said) { said = true; fprintf(stderr, "Vulkan: bind group %u bound after it was let go (pipeline '%s')\n", g.id, current ? current->name.c_str() : "?"); } }
			if (!s || skipping) return;
			vkCmdBindDescriptorSets(cmd, current->compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, current->layout, 1, 1, s, dynamicCount, dynamicOffsets);
			lBindGroups++;
		}
		void VulkanCommandList::SetVertexBuffer(uint32_t binding, Buffer b, uint64_t offset)
		{
			BufferRec* r = dev->buffers.Get(b.id);
			if (r) { VkDeviceSize o = offset; vkCmdBindVertexBuffers(cmd, binding, 1, &r->buffer, &o); }
		}
		void VulkanCommandList::SetIndexBuffer(Buffer b, uint64_t offset, bool index32)
		{
			BufferRec* r = dev->buffers.Get(b.id);
			if (r) vkCmdBindIndexBuffer(cmd, r->buffer, offset, index32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
		}
		void VulkanCommandList::Draw(uint32_t v, uint32_t i, uint32_t fv, uint32_t fi)
		{
			if (!Ready()) { lSkipped++; return; }
			vkCmdDraw(cmd, v, i, fv, fi); lDraws++;
		}
		void VulkanCommandList::DrawIndexed(uint32_t n, uint32_t i, uint32_t fi, int32_t vo, uint32_t fin)
		{
			if (!Ready()) { lSkipped++; return; }
			vkCmdDrawIndexed(cmd, n, i, fi, vo, fin); lDraws++;
		}
		void VulkanCommandList::DrawIndexedIndirect(Buffer args, uint64_t offset, uint32_t drawCount, Buffer count, uint64_t countOffset)
		{
			BufferRec* a = dev->buffers.Get(args.id); BufferRec* c = dev->buffers.Get(count.id);
			if (!Ready() || !a) return;
			if (c && dev->caps.indirectCount) vkCmdDrawIndexedIndirectCount(cmd, a->buffer, offset, c->buffer, countOffset, drawCount, 20);
			else vkCmdDrawIndexedIndirect(cmd, a->buffer, offset, drawCount, 20);
			lDraws += drawCount;
		}
		void VulkanCommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
		{
			if (inPass) EndPass();
			if (skipping) return;
			vkCmdDispatch(cmd, x, y, z); lDispatches++;
		}
		void VulkanCommandList::DispatchIndirect(Buffer args, uint64_t offset)
		{
			BufferRec* a = dev->buffers.Get(args.id);
			if (inPass) EndPass();
			if (!skipping && a) vkCmdDispatchIndirect(cmd, a->buffer, offset);
		}
		void VulkanCommandList::CopyBuffer(Buffer from, uint64_t fo, Buffer to, uint64_t too, uint64_t size)
		{
			BufferRec* a = dev->buffers.Get(from.id); BufferRec* b = dev->buffers.Get(to.id);
			if (inPass) EndPass();
			if (a && b) { VkBufferCopy c = { fo, too, size }; vkCmdCopyBuffer(cmd, a->buffer, b->buffer, 1, &c); }
		}
		void VulkanCommandList::CopyTexture(Texture from, Texture to)
		{
			TextureRec* a = dev->textures.Get(from.id); TextureRec* b = dev->textures.Get(to.id);
			if (inPass) EndPass();
			if (!a || !b) return;
			dev->Transition(cmd, *a, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); dev->Transition(cmd, *b, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkImageCopy c = {}; c.srcSubresource.aspectMask = c.dstSubresource.aspectMask = IsDepth(a->desc.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			c.srcSubresource.layerCount = c.dstSubresource.layerCount = 1; c.extent = { a->desc.width, a->desc.height, 1 };
			vkCmdCopyImage(cmd, a->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
		}

		// ---------------------------------------------------------------- the device
		bool VulkanDevice::Init(const SurfaceSource &s)
		{
			if (volkInitialize() != VK_SUCCESS) { fprintf(stderr, "Vulkan: no loader\n"); return false; }
			std::vector<const char*> ext = { VK_KHR_SURFACE_EXTENSION_NAME };
			VkInstanceCreateFlags flags = 0;
#if defined(__APPLE__)
			ext.push_back("VK_EXT_metal_surface");
			ext.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#elif defined(_WIN32)
			ext.push_back("VK_KHR_win32_surface");
#else
			ext.push_back(s.kind == SurfaceSource::Wayland ? "VK_KHR_wayland_surface" : "VK_KHR_xlib_surface");
#endif
			VkApplicationInfo app = {}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; app.pApplicationName = "Pyros3D"; app.apiVersion = VK_API_VERSION_1_3;
			VkInstanceCreateInfo ici = {}; ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.flags = flags; ici.pApplicationInfo = &app;
			ici.enabledExtensionCount = (uint32_t)ext.size(); ici.ppEnabledExtensionNames = ext.data();
			const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
			const char* v = getenv("PYROS_VK_VALIDATION");
			if (v && v[0] == '1')
			{
				// (only where the loader has it: asked for and missing, the instance is not made at all)
				uint32_t ln = 0; vkEnumerateInstanceLayerProperties(&ln, nullptr);
				std::vector<VkLayerProperties> lp(ln); vkEnumerateInstanceLayerProperties(&ln, lp.data());
				bool have = false; for (const VkLayerProperties &l : lp) if (strcmp(l.layerName, layers[0]) == 0) have = true;
				if (have) { ici.enabledLayerCount = 1; ici.ppEnabledLayerNames = layers; }
				else fprintf(stderr, "Vulkan: PYROS_VK_VALIDATION set, but the loader has no VK_LAYER_KHRONOS_validation (VK_LAYER_PATH) - without it\n");
			}
			if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) { fprintf(stderr, "Vulkan: no instance\n"); return false; }
			volkLoadInstance(instance);

#if defined(__APPLE__)
			if (s.kind == SurfaceSource::MetalLayer)
			{
				VkMetalSurfaceCreateInfoEXT mi = {}; mi.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT; mi.pLayer = (const CAMetalLayer*)s.a;
				// (through the instance: whatever platforms the loader's volk was built for)
				PFN_vkCreateMetalSurfaceEXT createMetal = (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT");
				if (!createMetal || createMetal(instance, &mi, nullptr, &surface) != VK_SUCCESS) { fprintf(stderr, "Vulkan: no surface\n"); return false; }
			}
#elif defined(_WIN32)
			if (s.kind == SurfaceSource::Win32)
			{
				VkWin32SurfaceCreateInfoKHR wi = {}; wi.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR; wi.hwnd = (HWND)s.a; wi.hinstance = (HINSTANCE)s.b;
				PFN_vkCreateWin32SurfaceKHR createWin32 = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
				if (!createWin32 || createWin32(instance, &wi, nullptr, &surface) != VK_SUCCESS) { fprintf(stderr, "Vulkan: no surface\n"); return false; }
			}
#endif
			// the first device that draws to the surface, a discrete one first
			uint32_t n = 0; vkEnumeratePhysicalDevices(instance, &n, nullptr);
			std::vector<VkPhysicalDevice> devs(n); vkEnumeratePhysicalDevices(instance, &n, devs.data());
			int best = -1;
			for (uint32_t i = 0; i < n; i++)
			{
				uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qn, nullptr);
				std::vector<VkQueueFamilyProperties> qs(qn); vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qn, qs.data());
				for (uint32_t q = 0; q < qn; q++)
				{
					VkBool32 present = VK_FALSE; if (surface) vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], q, surface, &present);
					if ((qs[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present)
					{
						VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(devs[i], &pp);
						const int score = pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
						if (score > best) { best = score; phys = devs[i]; family = q; }
						break;
					}
				}
			}
			if (!phys) { fprintf(stderr, "Vulkan: no device draws to this surface\n"); return false; }
			VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(phys, &pp);
			timestampPeriod = pp.limits.timestampPeriod;
			caps.name = std::string(pp.deviceName) + " (Vulkan " + std::to_string(VK_API_VERSION_MAJOR(pp.apiVersion)) + "." + std::to_string(VK_API_VERSION_MINOR(pp.apiVersion)) + ")";
			caps.timestamps = pp.limits.timestampComputeAndGraphics;
			caps.multiDrawIndirect = true; caps.indirectCount = VK_API_VERSION_MINOR(pp.apiVersion) >= 2;

			uint32_t en = 0; vkEnumerateDeviceExtensionProperties(phys, nullptr, &en, nullptr);
			std::vector<VkExtensionProperties> exts(en); vkEnumerateDeviceExtensionProperties(phys, nullptr, &en, exts.data());
			auto has = [&](const char* name) { for (auto &e : exts) if (!strcmp(e.extensionName, name)) return true; return false; };
			std::vector<const char*> dext = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
			if (has("VK_KHR_portability_subset")) dext.push_back("VK_KHR_portability_subset");

			// (what the device has, asked first: only that is asked for - MoltenVK has no indirect count)
			// (and ray queries, where the device has the three extensions and their features - PYROS_RAY_QUERY=0 to go without)
			const bool rtExts = has(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) && has(VK_KHR_RAY_QUERY_EXTENSION_NAME) && has(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
				&& !(getenv("PYROS_RAY_QUERY") && getenv("PYROS_RAY_QUERY")[0] == '0');
			VkPhysicalDeviceAccelerationStructureFeaturesKHR hAs = {}; hAs.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
			VkPhysicalDeviceRayQueryFeaturesKHR hRq = {}; hRq.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR; hRq.pNext = &hAs;
			VkPhysicalDeviceVulkan13Features h13 = {}; h13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES; h13.pNext = rtExts ? (void*)&hRq : nullptr;
			VkPhysicalDeviceVulkan12Features h12 = {}; h12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES; h12.pNext = &h13;
			VkPhysicalDeviceFeatures2 have = {}; have.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2; have.pNext = &h12;
			vkGetPhysicalDeviceFeatures2(phys, &have);
			if (!h13.dynamicRendering || !h13.synchronization2) { fprintf(stderr, "Vulkan: %s has no dynamic rendering / synchronization2\n", pp.deviceName); return false; }
			caps.indirectCount = h12.drawIndirectCount == VK_TRUE;
			VkPhysicalDeviceVulkan13Features f13 = {}; f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES; f13.dynamicRendering = VK_TRUE; f13.synchronization2 = VK_TRUE;
			VkPhysicalDeviceVulkan12Features f12 = {}; f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES; f12.pNext = &f13; f12.drawIndirectCount = h12.drawIndirectCount;
			VkPhysicalDeviceAccelerationStructureFeaturesKHR fAs = {}; fAs.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
			VkPhysicalDeviceRayQueryFeaturesKHR fRq = {}; fRq.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR; fRq.pNext = &fAs;
			caps.rayQuery = rtExts && hAs.accelerationStructure && hRq.rayQuery && h12.bufferDeviceAddress;
			if (caps.rayQuery)
			{
				fAs.accelerationStructure = VK_TRUE; fRq.rayQuery = VK_TRUE; f12.bufferDeviceAddress = VK_TRUE; f13.pNext = &fRq;
				dext.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME); dext.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME); dext.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
			}
			VkPhysicalDeviceFeatures2 f2 = {}; f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2; f2.pNext = &f12;
			f2.features.samplerAnisotropy = have.features.samplerAnisotropy; f2.features.multiDrawIndirect = have.features.multiDrawIndirect;
			f2.features.textureCompressionBC = have.features.textureCompressionBC; caps.bc = have.features.textureCompressionBC;
			const float prio = 1.f;
			VkDeviceQueueCreateInfo qci = {}; qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qci.queueFamilyIndex = family; qci.queueCount = 1; qci.pQueuePriorities = &prio;
			VkDeviceCreateInfo dci = {}; dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dci.pNext = &f2;
			dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci; dci.enabledExtensionCount = (uint32_t)dext.size(); dci.ppEnabledExtensionNames = dext.data();
			const VkResult dr = vkCreateDevice(phys, &dci, nullptr, &device);
			if (dr != VK_SUCCESS) { fprintf(stderr, "Vulkan: no device (%d)\n", (int)dr); return false; }
			volkLoadDevice(device);
			vkGetDeviceQueue(device, family, 0, &queue);

			VmaVulkanFunctions vf = {}; vf.vkGetInstanceProcAddr = vkGetInstanceProcAddr; vf.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
			VmaAllocatorCreateInfo ai = {}; ai.physicalDevice = phys; ai.device = device; ai.instance = instance; ai.vulkanApiVersion = VK_API_VERSION_1_3; ai.pVulkanFunctions = &vf;
			if (caps.rayQuery) ai.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
			vmaCreateAllocator(&ai, &vma);
			if (caps.rayQuery)
			{
				// (the scene's set, a frame each; the builds' command buffers; the scratch's alignment)
				VkPhysicalDeviceAccelerationStructurePropertiesKHR asp = {}; asp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
				VkPhysicalDeviceProperties2 p2 = {}; p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2; p2.pNext = &asp;
				vkGetPhysicalDeviceProperties2(phys, &p2);
				if (asp.minAccelerationStructureScratchOffsetAlignment) rtScratchAlign = asp.minAccelerationStructureScratchOffsetAlignment;
				VkDescriptorSetLayoutBinding rb = {}; rb.binding = 0; rb.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; rb.descriptorCount = 1;
				rb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
				VkDescriptorSetLayoutCreateInfo rl = {}; rl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; rl.bindingCount = 1; rl.pBindings = &rb;
				VkDescriptorPoolSize rs = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, kFrames };
				VkDescriptorPoolCreateInfo rp = {}; rp.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; rp.maxSets = kFrames; rp.poolSizeCount = 1; rp.pPoolSizes = &rs;
				VkCommandPoolCreateInfo cp = {}; cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cp.queueFamilyIndex = family;
				bool ok = vkCreateDescriptorSetLayout(device, &rl, nullptr, &rtLayout) == VK_SUCCESS && vkCreateDescriptorPool(device, &rp, nullptr, &rtPool) == VK_SUCCESS
					&& vkCreateCommandPool(device, &cp, nullptr, &rtCmdPool) == VK_SUCCESS && vkCreateAccelerationStructureKHR && vkCmdBuildAccelerationStructuresKHR;
				for (uint32_t i = 0; ok && i < kFrames; i++)
				{
					VkDescriptorSetAllocateInfo da = {}; da.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; da.descriptorPool = rtPool; da.descriptorSetCount = 1; da.pSetLayouts = &rtLayout;
					VkCommandBufferAllocateInfo ca = {}; ca.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ca.commandPool = rtCmdPool; ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount = 1;
					ok = vkAllocateDescriptorSets(device, &da, &rtSets[i]) == VK_SUCCESS && vkAllocateCommandBuffers(device, &ca, &rtCmds[i]) == VK_SUCCESS;
				}
				if (!ok) { fprintf(stderr, "Vulkan: ray queries could not be set up - traced by the shaders' own BVH\n"); caps.rayQuery = false; }
			}

			width = s.width; height = s.height; vsync = s.vsync;
			if (!CreateSwapchain()) return false;

			VkPipelineCacheCreateInfo pci = {}; pci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			vkCreatePipelineCache(device, &pci, nullptr, &pipelineCache);

			// group 0: a draw's data at a dynamic offset into the ring
			VkDescriptorSetLayoutBinding b = {}; b.binding = 0; b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; b.descriptorCount = 1;
			b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
			VkDescriptorSetLayoutCreateInfo lci = {}; lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; lci.bindingCount = 1; lci.pBindings = &b;
			vkCreateDescriptorSetLayout(device, &lci, nullptr, &drawLayout);
			VkDescriptorPoolSize sizes[] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 + 16384 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 4096 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 65536 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 16384 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16384 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16384 } };
			VkDescriptorPoolCreateInfo dp = {}; dp.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; dp.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			dp.maxSets = 32768; dp.poolSizeCount = (uint32_t)(sizeof(sizes) / sizeof(sizes[0])); dp.pPoolSizes = sizes;     // (all of them: a 5 here left the storage buffers - the bones - out)
			vkCreateDescriptorPool(device, &dp, nullptr, &descPool);
			BufferDesc rd; rd.size = kDrawDataBytes * kFrames; rd.usage = BufUniform; rd.memory = Memory::Upload;
			drawRing = CreateBuffer(rd, nullptr).id;
			VkDescriptorSetAllocateInfo da = {}; da.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; da.descriptorPool = descPool; da.descriptorSetCount = 1; da.pSetLayouts = &drawLayout;
			vkAllocateDescriptorSets(device, &da, &drawSet);
			VkDescriptorBufferInfo bi = { buffers.Get(drawRing)->buffer, 0, kDrawDataSlot };
			VkWriteDescriptorSet w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w.dstSet = drawSet; w.descriptorCount = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; w.pBufferInfo = &bi;
			vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

			for (uint32_t i = 0; i < kFrames; i++)
			{
				VkFenceCreateInfo fi = {}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
				vkCreateFence(device, &fi, nullptr, &frames[i].fence);
				VkSemaphoreCreateInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
				vkCreateSemaphore(device, &si, nullptr, &frames[i].acquired);
				if (caps.timestamps)
				{
					VkQueryPoolCreateInfo qp = {}; qp.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO; qp.queryType = VK_QUERY_TYPE_TIMESTAMP; qp.queryCount = kTimedPasses * 2;
					vkCreateQueryPool(device, &qp, nullptr, &frames[i].queries);
				}
			}
			const uint32_t threads = std::max(2u, std::thread::hardware_concurrency() / 2);
			for (uint32_t i = 0; i < threads; i++)
				workers.emplace_back([this]() {
					for (;;)
					{
						std::function<void()> job;
						{ std::unique_lock<std::mutex> l(jobMutex); jobCv.wait(l, [this]() { return stopping || !jobs.empty(); }); if (stopping && jobs.empty()) return; job = std::move(jobs.front()); jobs.pop_front(); }
						job();
					}
				});
			return true;
		}

		bool VulkanDevice::CreateSwapchain()
		{
			VkSurfaceCapabilitiesKHR sc; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &sc);
			uint32_t fn = 0; vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &fn, nullptr);
			std::vector<VkSurfaceFormatKHR> fs(fn); vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &fn, fs.data());
			VkSurfaceFormatKHR chosen = fs.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : fs[0];
			// (sRGB pixels in the sRGB colour space: the same format is offered in others - MoltenVK lists it
			// with linear and wide-gamut ones too - and in a linear one the system encodes the picture again)
			for (auto &f : fs) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }
			if (getenv("PYROS_SHOW_FORMATS")) for (auto &f : fs) fprintf(stderr, "Vulkan surface format %d colour space %d\n", (int)f.format, (int)f.colorSpace);
			swapFormat = chosen.format;
			uint32_t pn = 0; vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &pn, nullptr);
			std::vector<VkPresentModeKHR> pms(pn); vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &pn, pms.data());
			presentMode = VK_PRESENT_MODE_FIFO_KHR;
			if (!vsync) for (auto m : pms) { if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) { presentMode = m; break; } if (m == VK_PRESENT_MODE_MAILBOX_KHR) presentMode = m; }
			VkExtent2D ext = sc.currentExtent.width != 0xFFFFFFFFu ? sc.currentExtent : VkExtent2D{ width, height };
			width = ext.width; height = ext.height;
			VkSwapchainCreateInfoKHR ci = {}; ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; ci.surface = surface;
			ci.minImageCount = std::min(std::max(3u, sc.minImageCount), sc.maxImageCount ? sc.maxImageCount : 8u);
			ci.imageFormat = chosen.format; ci.imageColorSpace = chosen.colorSpace; ci.imageExtent = ext; ci.imageArrayLayers = 1;
			ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;     // (copied from: a test's capture)
			ci.preTransform = sc.currentTransform; ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; ci.presentMode = presentMode; ci.clipped = VK_TRUE;
			ci.oldSwapchain = swapchain;
			VkSwapchainKHR fresh = VK_NULL_HANDLE;
			if (vkCreateSwapchainKHR(device, &ci, nullptr, &fresh) != VK_SUCCESS) { fprintf(stderr, "Vulkan: no swapchain\n"); return false; }
			if (swapchain) { vkDeviceWaitIdle(device); for (uint32_t t : swapTextures) Destroy(Texture{ t }); for (VkSemaphore s : renderDone) vkDestroySemaphore(device, s, nullptr); vkDestroySwapchainKHR(device, swapchain, nullptr); }
			swapchain = fresh; swapTextures.clear(); renderDone.clear();
			uint32_t in = 0; vkGetSwapchainImagesKHR(device, swapchain, &in, nullptr);
			std::vector<VkImage> imgs(in); vkGetSwapchainImagesKHR(device, swapchain, &in, imgs.data());
			for (VkImage img : imgs)
			{
				TextureRec r; r.image = img; r.swapchain = true; r.desc.width = width; r.desc.height = height;
				r.desc.format = swapFormat == VK_FORMAT_B8G8R8A8_SRGB ? Format::BGRA8_sRGB : Format::BGRA8; r.desc.usage = TexColorTarget;
				VkImageViewCreateInfo vi = {}; vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; vi.image = img; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = swapFormat;
				vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				vkCreateImageView(device, &vi, nullptr, &r.view);
				swapTextures.push_back(textures.Add(r));
				VkSemaphoreCreateInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; VkSemaphore sem; vkCreateSemaphore(device, &si, nullptr, &sem); renderDone.push_back(sem);
			}
			std::string pm = presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? ", present immediate" : presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? ", present mailbox" : ", present fifo (vsync)";
			if (caps.name.find(", present") == std::string::npos) caps.name += pm;
			return true;
		}

		VulkanDevice::~VulkanDevice()
		{
			if (device) vkDeviceWaitIdle(device);
			for (Frame &f : frames) { for (std::function<void()> &r : f.retire) r(); f.retire.clear(); }     // (all done now)
			if (caps.rayQuery && device)
			{
				for (uint32_t i = 0; i < kFrames; i++) { if (rtScenes[i]) vkDestroyAccelerationStructureKHR(device, rtScenes[i], nullptr); RtFree(rtSceneStorage[i]); RtFree(rtInstances[i]); RtFree(rtScratch[i]); }
				for (size_t i = 0; i < rayGeometries.items.size(); i++) if (rayGeometries.alive[i]) { RayGeometryRec &g = rayGeometries.items[i]; if (g.as) vkDestroyAccelerationStructureKHR(device, g.as, nullptr); RtFree(g.storage); }
				if (rtCmdPool) vkDestroyCommandPool(device, rtCmdPool, nullptr);
				if (rtPool) vkDestroyDescriptorPool(device, rtPool, nullptr);
				if (rtLayout) vkDestroyDescriptorSetLayout(device, rtLayout, nullptr);
			}
			{ std::lock_guard<std::mutex> l(jobMutex); stopping = true; } jobCv.notify_all();
			for (std::thread &t : workers) t.join();
			// (the rest goes with the process: a test program; the engine's will free what it made)
		}

		Buffer VulkanDevice::CreateBuffer(const BufferDesc &d, const void* initial)
		{
			VkBufferCreateInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bi.size = std::max<uint64_t>(4, d.size);
			if (d.usage & BufVertex) bi.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			if (d.usage & BufIndex) bi.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			if (d.usage & BufUniform) bi.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			if (d.usage & BufStorage) bi.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			if (d.usage & BufIndirect) bi.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			bi.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			VmaAllocationCreateInfo ac = {}; ac.usage = VMA_MEMORY_USAGE_AUTO;
			if (d.memory == Memory::Upload) ac.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			if (d.memory == Memory::Readback) ac.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			BufferRec r; r.size = bi.size;
			VmaAllocationInfo info = {};
			if (vmaCreateBuffer(vma, &bi, &ac, &r.buffer, &r.alloc, &info) != VK_SUCCESS) return Buffer();
			r.mapped = info.pMappedData;
			Buffer h; h.id = buffers.Add(r);
			if (initial) Upload(h, 0, initial, d.size);
			return h;
		}
		void VulkanDevice::Destroy(Buffer b)
		{
			if (BufferRec* r = buffers.Get(b.id))
			{
				const VkBuffer buffer = r->buffer; const VmaAllocation alloc = r->alloc;
				frames[frame].retire.push_back([this, buffer, alloc] { vmaDestroyBuffer(vma, buffer, alloc); });
				buffers.Remove(b.id);
			}
		}
		void VulkanDevice::Destroy(Texture t)
		{
			if (TextureRec* r = textures.Get(t.id))
			{
				const VkImageView view = r->view; const VkImage image = r->swapchain ? VK_NULL_HANDLE : r->image; const VmaAllocation alloc = r->alloc;
				// (a swapchain's own images are the swapchain's, and it is remade only once the device is idle)
				if (r->swapchain) { if (view) vkDestroyImageView(device, view, nullptr); }
				else frames[frame].retire.push_back([this, view, image, alloc] { if (view) vkDestroyImageView(device, view, nullptr); if (image) vmaDestroyImage(vma, image, alloc); });
				textures.Remove(t.id);
			}
		}

		void VulkanDevice::OneShot(const std::function<void(VkCommandBuffer)> &f)
		{
			// (loading: a command buffer of its own, waited for - never in a frame)
			VkCommandPool pool; VkCommandPoolCreateInfo cp = {}; cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cp.queueFamilyIndex = family; cp.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			vkCreateCommandPool(device, &cp, nullptr, &pool);
			VkCommandBufferAllocateInfo ai = {}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ai.commandPool = pool; ai.commandBufferCount = 1; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			VkCommandBuffer cmd; vkAllocateCommandBuffers(device, &ai, &cmd);
			VkCommandBufferBeginInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(cmd, &bi); f(cmd); vkEndCommandBuffer(cmd);
			VkFence fence; VkFenceCreateInfo fi = {}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; vkCreateFence(device, &fi, nullptr, &fence);
			VkSubmitInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
			{ std::lock_guard<std::mutex> l(recordMutex); vkQueueSubmit(queue, 1, &si, fence); }
			vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
			vkDestroyFence(device, fence, nullptr); vkDestroyCommandPool(device, pool, nullptr);
		}

		void VulkanDevice::Upload(Buffer to, uint64_t offset, const void* data, uint64_t size)
		{
			BufferRec* r = buffers.Get(to.id);
			if (!r) return;
			if (r->mapped) { memcpy((uint8_t*)r->mapped + offset, data, size); return; }
			BufferDesc sd; sd.size = size; sd.memory = Memory::Upload;
			Buffer staging = CreateBuffer(sd, nullptr);
			memcpy(buffers.Get(staging.id)->mapped, data, size);
			VkBuffer src = buffers.Get(staging.id)->buffer, dst = r->buffer;
			OneShot([&](VkCommandBuffer cmd) { VkBufferCopy c = { 0, offset, size }; vkCmdCopyBuffer(cmd, src, dst, 1, &c); });
			Destroy(staging);
		}

		Texture VulkanDevice::CreateTexture(const TextureDesc &d, const TextureData* data, uint32_t count)
		{
			VkImageCreateInfo ii = {}; ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			ii.imageType = d.kind == TextureKind::Tex3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
			ii.format = ToVk(d.format); ii.extent = { d.width, d.height, d.kind == TextureKind::Tex3D ? d.depthOrLayers : 1u };
			ii.mipLevels = d.mips; ii.arrayLayers = d.kind == TextureKind::Cube ? 6u : (d.kind == TextureKind::Tex3D ? 1u : d.depthOrLayers);
			ii.samples = (VkSampleCountFlagBits)d.samples; ii.tiling = VK_IMAGE_TILING_OPTIMAL;
			if (d.kind == TextureKind::Cube) ii.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			if (d.usage & TexSampled) ii.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			if (d.usage & TexStorage) ii.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
			if (d.usage & TexColorTarget) ii.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			if (d.usage & TexDepthTarget) ii.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			ii.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			VmaAllocationCreateInfo ac = {}; ac.usage = VMA_MEMORY_USAGE_AUTO;
			TextureRec r; r.desc = d;
			if (vmaCreateImage(vma, &ii, &ac, &r.image, &r.alloc, nullptr) != VK_SUCCESS) return Texture();
			VkImageViewCreateInfo vi = {}; vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; vi.image = r.image; vi.format = ii.format;
			vi.viewType = d.kind == TextureKind::Cube ? VK_IMAGE_VIEW_TYPE_CUBE : d.kind == TextureKind::Tex2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : d.kind == TextureKind::Tex3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
			vi.subresourceRange = { (VkImageAspectFlags)(IsDepth(d.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, d.mips, 0, ii.arrayLayers };
			vkCreateImageView(device, &vi, nullptr, &r.view);
			Texture h; h.id = textures.Add(r);
			if (data && count) Upload(h, data, count);
			else if ((d.usage & TexSampled) && !IsDepth(d.format) && d.samples <= 1)
			{
				// (one sampled before anything is drawn into it - last frame's clouds on the first: in the layout a read
				// wants, its contents whatever they are)
				TextureRec* made = textures.Get(h.id);
				OneShot([&](VkCommandBuffer cmd) { Transition(cmd, *made, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); });
			}
			return h;
		}

		void VulkanDevice::Upload(Texture to, const TextureData* data, uint32_t count)
		{
			TextureRec* r = textures.Get(to.id);
			if (!r) return;
			uint64_t total = 0; for (uint32_t i = 0; i < count; i++) total += (data[i].size + 15) & ~15ull;
			BufferDesc sd; sd.size = total; sd.memory = Memory::Upload;
			Buffer staging = CreateBuffer(sd, nullptr);
			BufferRec* s = buffers.Get(staging.id);
			std::vector<VkBufferImageCopy> copies;
			uint64_t at = 0;
			for (uint32_t i = 0; i < count; i++)
			{
				memcpy((uint8_t*)s->mapped + at, data[i].bytes, data[i].size);
				VkBufferImageCopy c = {}; c.bufferOffset = at;
				// (a 3D texture's `layer` is its depth slice: at a z, in its one layer)
				const bool volume = r->desc.kind == TextureKind::Tex3D;
				c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, data[i].mip, volume ? 0u : data[i].layer, 1 };
				c.imageOffset = { 0, 0, volume ? (int32_t)data[i].layer : 0 };
				c.imageExtent = { std::max(1u, r->desc.width >> data[i].mip), std::max(1u, r->desc.height >> data[i].mip), 1 };
				copies.push_back(c); at += (data[i].size + 15) & ~15ull;
			}
			VkBuffer src = s->buffer;
			OneShot([&](VkCommandBuffer cmd) {
				Transition(cmd, *r, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				vkCmdCopyBufferToImage(cmd, src, r->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copies.size(), copies.data());
				Transition(cmd, *r, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});
			Destroy(staging);
		}

		Sampler VulkanDevice::CreateSampler(const SamplerDesc &d)
		{
			VkSamplerCreateInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			si.magFilter = d.mag == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST; si.minFilter = d.min == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
			si.mipmapMode = d.mip == Filter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
			si.addressModeU = ToVk(d.u); si.addressModeV = ToVk(d.v); si.addressModeW = ToVk(d.w);
			si.mipLodBias = d.lodBias; si.anisotropyEnable = d.anisotropy > 1.f; si.maxAnisotropy = std::min(16.f, d.anisotropy);
			si.compareEnable = d.compare; si.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; si.maxLod = VK_LOD_CLAMP_NONE;
			VkSampler s; vkCreateSampler(device, &si, nullptr, &s);
			Sampler h; h.id = samplers.Add(s); return h;
		}

		Shader VulkanDevice::CreateShader(const ShaderDesc &d)
		{
			if (!d.spirv) { fprintf(stderr, "Vulkan: shader '%s' has no SPIR-V\n", d.debugName ? d.debugName : "?"); return Shader(); }
			VkShaderModuleCreateInfo mi = {}; mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mi.codeSize = d.spirvWords * 4; mi.pCode = d.spirv;
			VkShaderModule m; if (vkCreateShaderModule(device, &mi, nullptr, &m) != VK_SUCCESS) return Shader();
			Shader h; h.id = shaders.Add(m); return h;
		}

		VkDescriptorSetLayout VulkanDevice::MaterialLayout(const std::vector<BindingDesc> &m)
		{
			// (a RayScene entry is no binding of group 1: the frame's scene is set 2, the device's)
			std::vector<BindingDesc> mm;
			for (const BindingDesc &b : m) if (b.kind != BindingKind::RayScene) mm.push_back(b);
			std::vector<VkDescriptorSetLayoutBinding> bs(mm.size());
			for (size_t i = 0; i < mm.size(); i++)
			{
				const std::vector<BindingDesc> &m = mm;
				bs[i] = VkDescriptorSetLayoutBinding(); bs[i].binding = m[i].binding; bs[i].descriptorCount = 1;
				bs[i].stageFlags = (m[i].vertex ? VK_SHADER_STAGE_VERTEX_BIT : 0) | (m[i].fragment ? VK_SHADER_STAGE_FRAGMENT_BIT : 0) | (m[i].compute ? VK_SHADER_STAGE_COMPUTE_BIT : 0);
				switch (m[i].kind)
				{
				case BindingKind::Sampler: case BindingKind::ComparisonSampler: bs[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; break;
				case BindingKind::Uniform: bs[i].descriptorType = m[i].dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
				case BindingKind::StorageRead: case BindingKind::StorageReadWrite: bs[i].descriptorType = m[i].dynamic ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
				default: bs[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
				}
			}
			VkDescriptorSetLayoutCreateInfo ci = {}; ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; ci.bindingCount = (uint32_t)bs.size(); ci.pBindings = bs.data();
			VkDescriptorSetLayout l; vkCreateDescriptorSetLayout(device, &ci, nullptr, &l); return l;
		}

		Pipeline VulkanDevice::CreatePipeline(const GraphicsPipelineDesc &d)
		{
			VkShaderModule* vs = shaders.Get(d.vertex.id); VkShaderModule* fs = shaders.Get(d.fragment.id);
			if (!vs || !*vs) return Pipeline();
			PipelineRec* r = new PipelineRec(); r->name = d.debugName ? d.debugName : "";
			r->material = MaterialLayout(d.material); r->bindings = d.material;
			for (const BindingDesc &b : d.material) if (b.kind == BindingKind::RayScene) r->rayScene = caps.rayQuery;
			VkDescriptorSetLayout sets[3] = { drawLayout, r->material, rtLayout };
			VkPipelineLayoutCreateInfo pl = {}; pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount = r->rayScene ? 3 : 2; pl.pSetLayouts = sets;
			vkCreatePipelineLayout(device, &pl, nullptr, &r->layout);
			cPending.fetch_add(1);
			// (copied: the compile is on a worker, after this returns)
			struct Desc { GraphicsPipelineDesc d; VkShaderModule vs, fs; VkFormat swap; };
			Desc* job = new Desc{ d, *vs, fs ? *fs : VK_NULL_HANDLE, swapFormat };
			Job([this, r, job]() {
				const GraphicsPipelineDesc &d = job->d;
				VkPipelineShaderStageCreateInfo st[2] = {};
				st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = job->vs; st[0].pName = "main";
				st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = job->fs; st[1].pName = "main";
				std::vector<VkVertexInputBindingDescription> vb; std::vector<VkVertexInputAttributeDescription> va;
				for (const VertexBinding &b : d.bindings) vb.push_back({ b.binding, b.stride, b.perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX });
				for (const VertexAttribute &a : d.attributes) va.push_back({ a.location, a.binding, VertexFormat(a.format), a.offset });
				VkPipelineVertexInputStateCreateInfo vi = {}; vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
				vi.vertexBindingDescriptionCount = (uint32_t)vb.size(); vi.pVertexBindingDescriptions = vb.data();
				vi.vertexAttributeDescriptionCount = (uint32_t)va.size(); vi.pVertexAttributeDescriptions = va.data();
				VkPipelineInputAssemblyStateCreateInfo ia = {}; ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				ia.topology = d.topology == Topology::Lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : d.topology == Topology::Points ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
					: d.topology == Topology::TriangleStrip ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				VkPipelineViewportStateCreateInfo vp = {}; vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vp.viewportCount = 1; vp.scissorCount = 1;
				VkPipelineRasterizationStateCreateInfo rs = {}; rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.f;
				rs.cullMode = d.cull == Cull::Back ? VK_CULL_MODE_BACK_BIT : d.cull == Cull::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
				// (the viewport turned over - clip space y up - makes Vulkan take winding as WebGPU and GL do:
				// counter-clockwise is counter-clockwise, nothing to swap)
				rs.frontFace = d.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
				rs.depthBiasEnable = d.depthBiasConstant != 0.f || d.depthBiasSlope != 0.f; rs.depthBiasConstantFactor = d.depthBiasConstant; rs.depthBiasSlopeFactor = d.depthBiasSlope;
				VkPipelineMultisampleStateCreateInfo ms = {}; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				ms.rasterizationSamples = (VkSampleCountFlagBits)std::max<uint8_t>(1, d.samples); ms.alphaToCoverageEnable = d.alphaToCoverage;
				VkPipelineDepthStencilStateCreateInfo ds = {}; ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
				ds.depthTestEnable = d.depthFormat != Format::Unknown && d.depthTest; ds.depthWriteEnable = d.depthFormat != Format::Unknown && d.depthWrite; ds.depthCompareOp = ToVk(d.depthCompare);
				VkPipelineColorBlendAttachmentState cb[8] = {};
				VkFormat colorFormats[8] = {};
				for (uint32_t i = 0; i < d.colorCount; i++)
				{
					colorFormats[i] = d.colorFormats[i] == Format::Unknown ? job->swap : ToVk(d.colorFormats[i]);
					cb[i].colorWriteMask = d.colorWrite ? 0xF : 0;
					if (d.blend[i] != Blend::Opaque)
					{
						cb[i].blendEnable = VK_TRUE; cb[i].colorBlendOp = cb[i].alphaBlendOp = VK_BLEND_OP_ADD;
						switch (d.blend[i])
						{
						case Blend::Alpha: cb[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; cb[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; cb[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; cb[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
						case Blend::PremultipliedAlpha: cb[i].srcColorBlendFactor = cb[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; cb[i].dstColorBlendFactor = cb[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
						case Blend::Additive: cb[i].srcColorBlendFactor = cb[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; cb[i].dstColorBlendFactor = cb[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE; break;
						default: cb[i].srcColorBlendFactor = cb[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_COLOR; cb[i].dstColorBlendFactor = cb[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; break;
						}
					}
				}
				VkPipelineColorBlendStateCreateInfo cbs = {}; cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cbs.attachmentCount = d.colorCount; cbs.pAttachments = cb;
				VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
				VkPipelineDynamicStateCreateInfo dy = {}; dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dy.dynamicStateCount = 2; dy.pDynamicStates = dyn;
				VkPipelineRenderingCreateInfo rci = {}; rci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
				rci.colorAttachmentCount = d.colorCount; rci.pColorAttachmentFormats = colorFormats;
				rci.depthAttachmentFormat = d.depthFormat != Format::Unknown ? ToVk(d.depthFormat) : VK_FORMAT_UNDEFINED;
				VkGraphicsPipelineCreateInfo gp = {}; gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; gp.pNext = &rci;
				gp.stageCount = job->fs ? 2 : 1; gp.pStages = st; gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia; gp.pViewportState = &vp;
				gp.pRasterizationState = &rs; gp.pMultisampleState = &ms; gp.pDepthStencilState = &ds; gp.pColorBlendState = &cbs; gp.pDynamicState = &dy; gp.layout = r->layout;
				const VkResult res = vkCreateGraphicsPipelines(device, pipelineCache, 1, &gp, nullptr, &r->pipeline);
				r->state.store(res == VK_SUCCESS ? 1 : 2);
				if (res != VK_SUCCESS) fprintf(stderr, "Vulkan: pipeline '%s' failed (%d)\n", d.debugName ? d.debugName : "?", (int)res);
				cFinished.fetch_add(1); cPending.fetch_sub(1);
				delete job;
			});
			Pipeline h; h.id = pipelines.Add(r); return h;
		}

		Pipeline VulkanDevice::CreatePipeline(const ComputePipelineDesc &d)
		{
			VkShaderModule* cs = shaders.Get(d.compute.id);
			if (!cs || !*cs) return Pipeline();
			PipelineRec* r = new PipelineRec(); r->compute = true;
			r->material = MaterialLayout(d.material); r->bindings = d.material;
			for (const BindingDesc &b : d.material) if (b.kind == BindingKind::RayScene) r->rayScene = caps.rayQuery;
			VkDescriptorSetLayout sets[3] = { drawLayout, r->material, rtLayout };
			VkPipelineLayoutCreateInfo pl = {}; pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount = r->rayScene ? 3 : 2; pl.pSetLayouts = sets;
			vkCreatePipelineLayout(device, &pl, nullptr, &r->layout);
			cPending.fetch_add(1);
			const VkShaderModule module = *cs;
			Job([this, r, module]() {
				VkComputePipelineCreateInfo ci = {}; ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; ci.layout = r->layout;
				ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; ci.stage.module = module; ci.stage.pName = "main";
				const VkResult res = vkCreateComputePipelines(device, pipelineCache, 1, &ci, nullptr, &r->pipeline);
				r->state.store(res == VK_SUCCESS ? 1 : 2);
				cFinished.fetch_add(1); cPending.fetch_sub(1);
			});
			Pipeline h; h.id = pipelines.Add(r); return h;
		}

		bool VulkanDevice::PipelineReady(Pipeline p) const
		{
			PipelineRec* const* r = const_cast<VulkanDevice*>(this)->pipelines.Get(p.id);
			return r && *r && (*r)->state.load() == 1;
		}

		void VulkanDevice::SavePipelineCache(const std::string &path)
		{
			size_t n = 0; vkGetPipelineCacheData(device, pipelineCache, &n, nullptr);
			std::vector<uint8_t> data(n); vkGetPipelineCacheData(device, pipelineCache, &n, data.data());
			if (FILE* f = fopen(path.c_str(), "wb")) { fwrite(data.data(), 1, n, f); fclose(f); }
		}

		bool VulkanDevice::RtBuffer(BufferRec &out, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible, VkDeviceAddress* address)
		{
			out = BufferRec();
			VkBufferCreateInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bi.size = std::max<VkDeviceSize>(size, 16); bi.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			VmaAllocationCreateInfo ac = {}; ac.usage = VMA_MEMORY_USAGE_AUTO;
			if (hostVisible) ac.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VmaAllocationInfo info = {};
			if (vmaCreateBuffer(vma, &bi, &ac, &out.buffer, &out.alloc, &info) != VK_SUCCESS) { out = BufferRec(); return false; }
			out.mapped = info.pMappedData; out.size = bi.size;
			if (address) { VkBufferDeviceAddressInfo ai = {}; ai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO; ai.buffer = out.buffer; *address = vkGetBufferDeviceAddress(device, &ai); }
			return true;
		}

		RayGeometry VulkanDevice::CreateRayGeometry(const float* positions, uint32_t vertexCount, uint32_t strideFloats, const uint32_t* indices, uint32_t indexCount)
		{
			if (!caps.rayQuery || !positions || !indices || vertexCount == 0 || indexCount < 3) return RayGeometry();
			const uint32_t triangles = indexCount / 3;
			const VkBufferUsageFlags input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			BufferRec vb, ib, scratch; VkDeviceAddress va = 0, ia = 0, sa = 0;
			if (!RtBuffer(vb, (VkDeviceSize)vertexCount * 12, input, true, &va) || !RtBuffer(ib, (VkDeviceSize)triangles * 12, input, true, &ia)) { RtFree(vb); RtFree(ib); return RayGeometry(); }
			float* vp = (float*)vb.mapped;
			for (uint32_t v = 0; v < vertexCount; v++) for (uint32_t k = 0; k < 3; k++) vp[(size_t)v * 3 + k] = positions[(size_t)v * strideFloats + k];
			uint32_t* ip = (uint32_t*)ib.mapped;
			for (uint32_t i = 0; i < triangles * 3; i++) ip[i] = indices[i] < vertexCount ? indices[i] : 0;     // (one past the vertices would be read: clamped)
			vmaFlushAllocation(vma, vb.alloc, 0, VK_WHOLE_SIZE); vmaFlushAllocation(vma, ib.alloc, 0, VK_WHOLE_SIZE);
			VkAccelerationStructureGeometryKHR geo = {}; geo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR; geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
			geo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
			geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; geo.geometry.triangles.vertexData.deviceAddress = va; geo.geometry.triangles.vertexStride = 12;
			geo.geometry.triangles.maxVertex = vertexCount - 1; geo.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32; geo.geometry.triangles.indexData.deviceAddress = ia;
			VkAccelerationStructureBuildGeometryInfoKHR info = {}; info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR; info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; info.geometryCount = 1; info.pGeometries = &geo;
			VkAccelerationStructureBuildSizesInfoKHR sizes = {}; sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &triangles, &sizes);
			RayGeometryRec g;
			bool ok = RtBuffer(g.storage, sizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, false, nullptr)
				&& RtBuffer(scratch, sizes.buildScratchSize + rtScratchAlign, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, &sa);
			if (ok)
			{
				VkAccelerationStructureCreateInfoKHR ac = {}; ac.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
				ac.buffer = g.storage.buffer; ac.size = sizes.accelerationStructureSize; ac.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				ok = vkCreateAccelerationStructureKHR(device, &ac, nullptr, &g.as) == VK_SUCCESS;
			}
			if (ok)
			{
				info.dstAccelerationStructure = g.as;
				info.scratchData.deviceAddress = (sa + rtScratchAlign - 1) & ~(VkDeviceAddress)(rtScratchAlign - 1);
				VkAccelerationStructureBuildRangeInfoKHR range = {}; range.primitiveCount = triangles;
				const VkAccelerationStructureBuildRangeInfoKHR* ranges = &range;
				// (built now, and waited on: a mesh is built once, before the frames that trace it)
				VkCommandBufferAllocateInfo ca = {}; ca.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ca.commandPool = rtCmdPool; ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount = 1;
				VkCommandBuffer cmd = VK_NULL_HANDLE;
				std::lock_guard<std::mutex> lk(recordMutex);
				ok = vkAllocateCommandBuffers(device, &ca, &cmd) == VK_SUCCESS;
				if (ok)
				{
					VkCommandBufferBeginInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
					vkBeginCommandBuffer(cmd, &bi);
					vkCmdBuildAccelerationStructuresKHR(cmd, 1, &info, &ranges);
					vkEndCommandBuffer(cmd);
					VkFenceCreateInfo fi = {}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; VkFence fence = VK_NULL_HANDLE; vkCreateFence(device, &fi, nullptr, &fence);
					VkSubmitInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
					ok = vkQueueSubmit(queue, 1, &si, fence) == VK_SUCCESS && vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
					vkDestroyFence(device, fence, nullptr);
					vkFreeCommandBuffers(device, rtCmdPool, 1, &cmd);
				}
			}
			RtFree(scratch); RtFree(vb); RtFree(ib);
			if (!ok) { if (g.as) vkDestroyAccelerationStructureKHR(device, g.as, nullptr); RtFree(g.storage); return RayGeometry(); }
			VkAccelerationStructureDeviceAddressInfoKHR ai = {}; ai.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR; ai.accelerationStructure = g.as;
			g.address = vkGetAccelerationStructureDeviceAddressKHR(device, &ai);
			RayGeometry h; h.id = rayGeometries.Add(g); return h;
		}

		void VulkanDevice::Destroy(RayGeometry g)
		{
			RayGeometryRec* r = rayGeometries.Get(g.id);
			if (!r) return;
			// (a scene in flight may reach it: let go once this frame is done)
			const VkAccelerationStructureKHR as = r->as; BufferRec storage = r->storage;
			frames[frame].retire.push_back([this, as, storage]() mutable { if (as) vkDestroyAccelerationStructureKHR(device, as, nullptr); RtFree(storage); });
			rayGeometries.Remove(g.id);
		}

		void VulkanDevice::SetRayScene(const RayInstance* instances, uint32_t count)
		{
			if (!caps.rayQuery) return;
			if (!rayNothing)
			{
				// (one triangle far below everything: a scene always exists, and an instance whose geometry is gone keeps its
				// place - the shaders find an instance's own data by its index)
				const float p[9] = { 0.f, -1e7f, 0.f, 1.f, -1e7f, 0.f, 0.f, -1e7f, 1.f }; const uint32_t ix[3] = { 0, 1, 2 };
				rayNothing = CreateRayGeometry(p, 3, 3, ix, 3);
			}
			std::vector<VkAccelerationStructureInstanceKHR> inst;
			inst.reserve(std::max(count, 1u));
			auto add = [&](RayGeometry geometry, const float* t)
			{
				RayGeometryRec* g = rayGeometries.Get(geometry.id);
				if (!g) g = rayGeometries.Get(rayNothing.id);
				if (!g) return;
				VkAccelerationStructureInstanceKHR d = {};
				for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) d.transform.matrix[r][c] = t[r * 4 + c];
				d.mask = 0xFF; d.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
				d.accelerationStructureReference = g->address;
				inst.push_back(d);
			};
			for (uint32_t i = 0; i < count; i++) add(instances[i].geometry, instances[i].transform);
			if (inst.empty()) { const float id3x4[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }; add(rayNothing, id3x4); }
			if (inst.empty()) return;
			// (this frame's slot: its last frame is done - BeginFrame waited on it)
			const uint32_t slot = frame, n = (uint32_t)inst.size();
			VkDeviceAddress instAddr = 0;
			if (rtInstances[slot].size < (VkDeviceSize)n * sizeof(VkAccelerationStructureInstanceKHR))
			{
				RtFree(rtInstances[slot]);
				if (!RtBuffer(rtInstances[slot], (VkDeviceSize)(n + n / 4 + 16) * sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, true, &instAddr)) return;
			}
			else { VkBufferDeviceAddressInfo ai = {}; ai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO; ai.buffer = rtInstances[slot].buffer; instAddr = vkGetBufferDeviceAddress(device, &ai); }
			memcpy(rtInstances[slot].mapped, inst.data(), (size_t)n * sizeof(VkAccelerationStructureInstanceKHR));
			vmaFlushAllocation(vma, rtInstances[slot].alloc, 0, VK_WHOLE_SIZE);
			VkAccelerationStructureGeometryKHR geo = {}; geo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR; geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
			geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR; geo.geometry.instances.data.deviceAddress = instAddr;
			VkAccelerationStructureBuildGeometryInfoKHR info = {}; info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR; info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
			info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; info.geometryCount = 1; info.pGeometries = &geo;
			VkAccelerationStructureBuildSizesInfoKHR sizes = {}; sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &n, &sizes);
			if (!rtScenes[slot] || rtSceneStorage[slot].size < sizes.accelerationStructureSize)
			{
				if (rtScenes[slot]) vkDestroyAccelerationStructureKHR(device, rtScenes[slot], nullptr);
				rtScenes[slot] = VK_NULL_HANDLE; RtFree(rtSceneStorage[slot]);
				if (!RtBuffer(rtSceneStorage[slot], sizes.accelerationStructureSize + sizes.accelerationStructureSize / 4, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, false, nullptr)) return;
				VkAccelerationStructureCreateInfoKHR ac = {}; ac.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
				ac.buffer = rtSceneStorage[slot].buffer; ac.size = rtSceneStorage[slot].size; ac.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
				if (vkCreateAccelerationStructureKHR(device, &ac, nullptr, &rtScenes[slot]) != VK_SUCCESS) { rtScenes[slot] = VK_NULL_HANDLE; return; }
				// (its set names the structure: written again whenever it is a new one)
				VkWriteDescriptorSetAccelerationStructureKHR asw = {}; asw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
				asw.accelerationStructureCount = 1; asw.pAccelerationStructures = &rtScenes[slot];
				VkWriteDescriptorSet w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w.pNext = &asw; w.dstSet = rtSets[slot]; w.dstBinding = 0; w.descriptorCount = 1;
				w.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
			}
			VkDeviceAddress sa = 0;
			if (rtScratch[slot].size < sizes.buildScratchSize + rtScratchAlign)
			{
				RtFree(rtScratch[slot]);
				if (!RtBuffer(rtScratch[slot], (sizes.buildScratchSize + rtScratchAlign) * 5 / 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, &sa)) return;
			}
			else { VkBufferDeviceAddressInfo ai = {}; ai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO; ai.buffer = rtScratch[slot].buffer; sa = vkGetBufferDeviceAddress(device, &ai); }
			info.dstAccelerationStructure = rtScenes[slot];
			info.scratchData.deviceAddress = (sa + rtScratchAlign - 1) & ~(VkDeviceAddress)(rtScratchAlign - 1);
			VkAccelerationStructureBuildRangeInfoKHR range = {}; range.primitiveCount = n;
			const VkAccelerationStructureBuildRangeInfoKHR* ranges = &range;
			VkCommandBuffer cmd = rtCmds[slot];
			vkResetCommandBuffer(cmd, 0);
			VkCommandBufferBeginInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(cmd, &bi);
			vkCmdBuildAccelerationStructuresKHR(cmd, 1, &info, &ranges);
			// (built before what traces it reads it - the frame's lists, after this in the same submit)
			VkMemoryBarrier mb = {}; mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			mb.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR; mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
			vkEndCommandBuffer(cmd);
			rtQueued[slot] = true;
			rtBuiltOn[slot] = frameNumber;
		}

		BindGroup VulkanDevice::CreateBindGroup(Pipeline layoutOf, const BindGroupEntry* e, uint32_t count)
		{
			PipelineRec** r = pipelines.Get(layoutOf.id);
			if (!r || !*r) return BindGroup();
			VkDescriptorSetAllocateInfo ai = {}; ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; ai.descriptorPool = descPool; ai.descriptorSetCount = 1; ai.pSetLayouts = &(*r)->material;
			VkDescriptorSet set = VK_NULL_HANDLE;
			const VkResult allocated = vkAllocateDescriptorSets(device, &ai, &set);
			if (allocated != VK_SUCCESS || set == VK_NULL_HANDLE)
			{
				fprintf(stderr, "Vulkan: a bind group for '%s' could not be made (%d)\n", (*r)->name.c_str(), (int)allocated);
				return BindGroup();
			}
			std::vector<VkWriteDescriptorSet> ws(count); std::vector<VkDescriptorImageInfo> ii(count); std::vector<VkDescriptorBufferInfo> bi(count);
			for (uint32_t i = 0; i < count; i++)
			{
				ws[i] = VkWriteDescriptorSet(); ws[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[i].dstSet = set; ws[i].dstBinding = e[i].binding; ws[i].descriptorCount = 1;
				if (TextureRec* t = textures.Get(e[i].texture.id)) { ii[i] = { VK_NULL_HANDLE, t->view, IsDepth(t->desc.format) ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }; ws[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; ws[i].pImageInfo = &ii[i]; }
				else if (VkSampler* s = samplers.Get(e[i].sampler.id)) { ii[i] = { *s, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }; ws[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; ws[i].pImageInfo = &ii[i]; }
				else if (BufferRec* b = buffers.Get(e[i].buffer.id))
				{
					// (as the pipeline said: a storage buffer was written as a uniform one before, whatever it was)
					VkDescriptorType t = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					for (const BindingDesc &bd : (*r)->bindings)
						if (bd.binding == e[i].binding)
						{
							const bool storage = bd.kind == BindingKind::StorageRead || bd.kind == BindingKind::StorageReadWrite;
							t = storage ? (bd.dynamic ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) : (bd.dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
						}
					bi[i] = { b->buffer, e[i].offset, e[i].size ? e[i].size : VK_WHOLE_SIZE }; ws[i].descriptorType = t; ws[i].pBufferInfo = &bi[i];
				}
			}
			vkUpdateDescriptorSets(device, count, ws.data(), 0, nullptr);
			BindGroup h; h.id = groups.Add(set); return h;
		}

		int VulkanDevice::TimedPass(const char* name)
		{
			std::lock_guard<std::mutex> l(timingMutex);
			Frame &f = frames[frame];
			if (!caps.timestamps || f.names.size() >= kTimedPasses) return -1;
			f.names.push_back(name ? name : "pass");
			return (int)f.names.size() - 1;
		}

		void VulkanDevice::BeginFrame()
		{
			// (begun again before it was presented - a second view drawn ahead of the frame's own, a scope's: the same frame)
			if (frameOpen) return;
			frameOpen = true;
			frame = (frame + 1) % kFrames;
			Frame &f = frames[frame];
			// (the frame this slot last held, done: its fence - two frames behind, so no wait in a steady state)
			vkWaitForFences(device, 1, &f.fence, VK_TRUE, UINT64_MAX);
			for (std::function<void()> &r : f.retire) r();
			f.retire.clear();
			if (f.timed && !f.names.empty())
			{
				std::vector<uint64_t> ts(f.names.size() * 2);
				if (vkGetQueryPoolResults(device, f.queries, 0, (uint32_t)ts.size(), ts.size() * 8, ts.data(), 8, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
				{
					std::lock_guard<std::mutex> l(statsMutex);
					gpuStats.frame = frameNumber >= kFrames ? frameNumber - kFrames : 0; gpuStats.passes.clear(); gpuStats.gpuMs = 0.0;
					for (size_t i = 0; i < f.names.size(); i++)
					{
						FrameStats::Pass p; p.name = f.names[i]; p.gpuMs = ts[i * 2 + 1] > ts[i * 2] ? (ts[i * 2 + 1] - ts[i * 2]) * (double)timestampPeriod * 1e-6 : 0.0;
						gpuStats.passes.push_back(p); gpuStats.gpuMs += p.gpuMs;
					}
				}
			}
			f.names.clear(); f.timed = false;
			for (VkCommandPool p : f.pools) vkResetCommandPool(device, p, 0);
			f.used = 0;
			{
				std::lock_guard<std::mutex> l(statsMutex);
				stats.draws = cDraws.exchange(0); stats.skippedDraws = cSkipped.exchange(0); stats.pipelineChanges = cPipelines.exchange(0);
				stats.bindGroupChanges = cBindGroups.exchange(0); stats.dispatches = cDispatches.exchange(0);
				stats.pipelinesFinished = cFinished.exchange(0); stats.pipelinesPending = cPending.load();
				stats.drawDataBytes = std::min<uint64_t>(frameCursor.load(), kDrawDataBytes);
			}
			frameNumber++;
			frameBase = (uint64_t)frame * kDrawDataBytes; frameCursor.store(0);
			haveImage = false; f.acquireWaited = false;
		}

		CommandList* VulkanDevice::NewCommandList(const char*)
		{
			std::lock_guard<std::mutex> l(recordMutex);
			Frame &f = frames[frame];
			VulkanCommandList* c = new VulkanCommandList(); c->dev = this; c->cmd = FrameCommandBuffer();
			if (caps.timestamps && !f.timed) { vkCmdResetQueryPool(c->cmd, f.queries, 0, kTimedPasses * 2); f.timed = true; }
			lists.push_back(c);
			return c;
		}

		VkCommandBuffer VulkanDevice::FrameCommandBuffer()
		{
			Frame &f = frames[frame];
			if (f.used >= f.cmds.size())
			{
				VkCommandPoolCreateInfo cp = {}; cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cp.queueFamilyIndex = family; cp.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
				VkCommandPool pool; vkCreateCommandPool(device, &cp, nullptr, &pool); f.pools.push_back(pool);
				VkCommandBufferAllocateInfo ai = {}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
				VkCommandBuffer c; vkAllocateCommandBuffers(device, &ai, &c); f.cmds.push_back(c);
			}
			VkCommandBuffer cmd = f.cmds[f.used++];
			VkCommandBufferBeginInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(cmd, &bi);
			return cmd;
		}

		void VulkanDevice::Submit(CommandList* const* l, uint32_t count)
		{
			// (closed, in order, for the frame's one submit - Flush)
			Frame &f = frames[frame];
			for (uint32_t i = 0; i < count; i++)
			{
				VulkanCommandList* c = static_cast<VulkanCommandList*>(l[i]);
				c->EndPass();
				cDraws += c->lDraws; cSkipped += c->lSkipped; cPipelines += c->lPipelines; cBindGroups += c->lBindGroups; cDispatches += c->lDispatches;
				vkEndCommandBuffer(c->cmd);
				f.pending.push_back(c->cmd);
				delete c;
			}
			lists.clear();
		}

		void VulkanDevice::Flush()
		{
			Frame &f = frames[frame];
			if (f.pending.empty()) return;
			std::lock_guard<std::mutex> lk(recordMutex);
			// (this frame's ray scene, built ahead of the lists that trace it)
			if (rtQueued[frame]) { f.pending.insert(f.pending.begin(), rtCmds[frame]); rtQueued[frame] = false; }
			// After the frame's lists: its test capture (PYROS_SHOT=<file.png>: the frame as it is shown),
			// and the backbuffer handed to presentation
			if (haveImage)
			{
				VkCommandBuffer cmd = FrameCommandBuffer();
				TextureRec &back = *textures.Get(swapTextures[imageIndex]);
				if (frameNumber == ShotFrame() && !shotBuffer.buffer && getenv("PYROS_SHOT"))
				{
					shotW = back.desc.width; shotH = back.desc.height;
					VkBufferCreateInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bi.size = (VkDeviceSize)shotW * shotH * 4; bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
					VmaAllocationCreateInfo ac = {}; ac.usage = VMA_MEMORY_USAGE_AUTO; ac.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
					VmaAllocationInfo info = {};
					if (vmaCreateBuffer(vma, &bi, &ac, &shotBuffer.buffer, &shotBuffer.alloc, &info) == VK_SUCCESS)
					{
						shotBuffer.mapped = info.pMappedData;
						Transition(cmd, back, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
						VkBufferImageCopy region = {}; region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; region.imageSubresource.layerCount = 1;
						region.imageExtent = { shotW, shotH, 1 };
						vkCmdCopyImageToBuffer(cmd, back.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, shotBuffer.buffer, 1, &region);
					}
				}
				Transition(cmd, back, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
				vkEndCommandBuffer(cmd);
				f.pending.push_back(cmd);
			}
			VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			si.commandBufferCount = (uint32_t)f.pending.size(); si.pCommandBuffers = f.pending.data();
			if (haveImage)
			{
				if (!f.acquireWaited) { si.waitSemaphoreCount = 1; si.pWaitSemaphores = &f.acquired; si.pWaitDstStageMask = &wait; }
				si.signalSemaphoreCount = 1; si.pSignalSemaphores = &renderDone[imageIndex];
			}
			vkResetFences(device, 1, &f.fence);
			vkQueueSubmit(queue, 1, &si, f.fence);
			f.pending.clear(); f.acquireWaited = false;
			if (shotBuffer.buffer && !shotWritten)
			{
				// (the copy done: its pixels, BGRA or RGBA as the swapchain is, written out)
				vkWaitForFences(device, 1, &f.fence, VK_TRUE, UINT64_MAX);
				vmaInvalidateAllocation(vma, shotBuffer.alloc, 0, VK_WHOLE_SIZE);
				std::vector<uint8_t> px((size_t)shotW * shotH * 4);
				const uint8_t* p = (const uint8_t*)shotBuffer.mapped;
				const bool bgra = swapFormat == VK_FORMAT_B8G8R8A8_SRGB || swapFormat == VK_FORMAT_B8G8R8A8_UNORM;
				for (size_t k = 0; k < (size_t)shotW * shotH; k++) { px[k * 4] = bgra ? p[k * 4 + 2] : p[k * 4]; px[k * 4 + 1] = p[k * 4 + 1]; px[k * 4 + 2] = bgra ? p[k * 4] : p[k * 4 + 2]; px[k * 4 + 3] = 255; }
				WritePng(getenv("PYROS_SHOT"), px.data(), shotW, shotH);
				shotWritten = true;
			}
		}

		void VulkanDevice::SubmitPending()
		{
			Frame &f = frames[frame];
			if (f.pending.empty()) return;
			VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo si = {}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			si.commandBufferCount = (uint32_t)f.pending.size(); si.pCommandBuffers = f.pending.data();
			// (they may draw on the frame's image: its acquire waited on here, once)
			if (haveImage && !f.acquireWaited) { si.waitSemaphoreCount = 1; si.pWaitSemaphores = &f.acquired; si.pWaitDstStageMask = &wait; f.acquireWaited = true; }
			std::lock_guard<std::mutex> lk(recordMutex);
			vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
			f.pending.clear();
		}

		bool VulkanDevice::ReadTexture(Texture t, std::vector<uint8_t> &out)
		{
			TextureRec* r = textures.Get(t.id);
			if (!r || !r->image) return false;
			const uint32_t bpt = BytesPerTexel(r->desc.format);
			if (!bpt || !(r->desc.usage & TexCopySrc) || r->desc.samples > 1) return false;     // (copied out: made with TexCopySrc)
			const uint32_t w = r->desc.width, h = r->desc.height;
			// What the frame drew so far first (the copy's barrier orders it after them on the queue)
			SubmitPending();
			VkBufferCreateInfo bi = {}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bi.size = (VkDeviceSize)w * h * bpt; bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VmaAllocationCreateInfo ac = {}; ac.usage = VMA_MEMORY_USAGE_AUTO; ac.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VmaAllocationInfo info = {};
			VkBuffer buffer = VK_NULL_HANDLE; VmaAllocation alloc = VK_NULL_HANDLE;
			if (vmaCreateBuffer(vma, &bi, &ac, &buffer, &alloc, &info) != VK_SUCCESS) return false;
			const VkImageLayout was = r->layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : r->layout;
			OneShot([&](VkCommandBuffer cmd) {
				Transition(cmd, *r, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				VkBufferImageCopy region = {}; region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; region.imageSubresource.layerCount = 1;
				region.imageExtent = { w, h, 1 };
				vkCmdCopyImageToBuffer(cmd, r->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
				Transition(cmd, *r, was);
			});
			vmaInvalidateAllocation(vma, alloc, 0, VK_WHOLE_SIZE);
			const uint8_t* p = (const uint8_t*)info.pMappedData;
			out.assign(p, p + (size_t)w * h * bpt);
			vmaDestroyBuffer(vma, buffer, alloc);
			return true;
		}

		Texture VulkanDevice::AcquireBackbuffer()
		{
			// (one a frame: a second pass over it - an editor's panels over its views - draws on the same, what it holds kept)
			if (haveImage) { Texture h; h.id = swapTextures[imageIndex]; return h; }
			Frame &f = frames[frame];
			VkResult r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &imageIndex);
			if (r == VK_ERROR_OUT_OF_DATE_KHR) { CreateSwapchain(); return Texture(); }
			haveImage = true;
			TextureRec* t = textures.Get(swapTextures[imageIndex]);
			t->layout = VK_IMAGE_LAYOUT_UNDEFINED;          // (its last contents are not wanted)
			Texture h; h.id = swapTextures[imageIndex]; return h;
		}

		void VulkanDevice::Present()
		{
			Flush();     // (the frame's lists, at once)
			frameOpen = false;
			if (!haveImage) return;
			VkPresentInfoKHR pi = {}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &renderDone[imageIndex];
			pi.swapchainCount = 1; pi.pSwapchains = &swapchain; pi.pImageIndices = &imageIndex;
			VkResult r;
			{ std::lock_guard<std::mutex> lk(recordMutex); r = vkQueuePresentKHR(queue, &pi); }
			if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) CreateSwapchain();
			haveImage = false;
		}

		const FrameStats &VulkanDevice::LastFrameStats()
		{
			std::lock_guard<std::mutex> l(statsMutex);
			stats.frame = gpuStats.frame; stats.passes = gpuStats.passes; stats.gpuMs = gpuStats.gpuMs;
			return stats;
		}
	}

	Device* CreateVulkanDevice(const SurfaceSource &surface)
	{
		VulkanDevice* d = new VulkanDevice();
		if (!d->Init(surface)) { delete d; return nullptr; }
		return d;
	}

} }
