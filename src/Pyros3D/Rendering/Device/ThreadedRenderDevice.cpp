//============================================================================
// Name        : ThreadedRenderDevice.cpp
// Description : A render device that is another device, one thread away
//               (see the header).
//============================================================================

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdlib>
// (a stack to print: where the C library has one - not a browser's, not Windows')
#if defined(__APPLE__) || defined(__GLIBC__)
#define PYROS_HAS_BACKTRACE 1
#include <execinfo.h>
#endif
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Rendering/Device/ThreadedRenderDevice.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace p3d {

	namespace {
		thread_local const char* g_scopeHint = NULL;
		thread_local void* t_stream = NULL;          // the stream this thread has entered (a ThreadedRenderDevice::Stream)
		f64 MsSince(const std::chrono::steady_clock::time_point &t0)
		{
			return std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
		}
	}
	static std::atomic<ThreadedRenderDevice*> g_threaded(NULL);
	void QuiesceRenderDevice() { if (ThreadedRenderDevice* t = g_threaded.load()) t->Finish(); }
	void SetGpuScopeHint(const char* name) { g_scopeHint = name; }
	const char* GpuScopeName() { return g_scopeHint ? g_scopeHint : FrameProfiler::Instance().CurrentScopeName(); }

	ThreadedRenderDevice::ThreadedRenderDevice(const std::shared_ptr<IRenderDevice> &realDevice)
		: keep(realDevice), real(realDevice.get()), filling(new Batch()), framesBehind(0), gpuWaitUs(0), presentWaitUs(0), nextGiven(kFirstGiven)
	{
		trace = std::getenv("PYROS_DEVICE_THREAD_TRACE") != NULL;
		worker = std::thread([this]() { Work(); });
		g_threaded = this;
	}

	ThreadedRenderDevice::~ThreadedRenderDevice()
	{
		Drain("shutdown");
		if (g_threaded.load() == this) g_threaded = NULL;
		{
			std::lock_guard<std::mutex> g(lock);
			quit = true;
		}
		wake.notify_all();
		if (worker.joinable()) worker.join();
		delete filling;
		for (size_t i = 0; i < spare.size(); i++) delete spare[i];
	}

	// PYROS_VERIFY_STREAMS=1: the frame's own stream has ONE writer at a time - the thread
	// recording the frame. Any other thread that writes into it while that one is at work
	// (a job of the game's logic that asks the device for something, on a worker that is
	// in no stream) is named, with where it came from.
	namespace {
		const bool g_verifyStreams = std::getenv("PYROS_VERIFY_STREAMS") != NULL;
		std::atomic<size_t> g_frameRecorder(0);       // the thread recording the frame beside the logic (0: none is)
		inline size_t ThisThread() { return std::hash<std::thread::id>()(std::this_thread::get_id()); }
	}
	void SetFrameRecorderHere(const bool on) { g_frameRecorder.store(on ? ThisThread() : 0); }
	ThreadedRenderDevice::Batch* ThreadedRenderDevice::Filling()
	{
		if (g_verifyStreams && !t_stream)
		{
			const size_t rec = g_frameRecorder.load();
			if (rec != 0 && rec != ThisThread())
			{
				static std::atomic<uint32> said(0);
				const uint32 n = said.fetch_add(1);
#if defined(PYROS_HAS_BACKTRACE)
				if (n < 12)
				{
					void* frames[24]; const int count = backtrace(frames, 24);
					char** names = backtrace_symbols(frames, count);
					std::string text;
					for (int i = 1; i < count && i < 16; i++) { std::string l = names[i]; const size_t at = l.find("0x"); text += "    " + (at != std::string::npos ? l.substr(l.find(' ', at) + 1) : l).substr(0, 110) + "\n"; }
					free(names);
					fprintf(stderr, "[streams] WRONG: a thread in no stream writes into the frame's while another records it:\n%s", text.c_str());
				}
				else
#endif
				if (n % 5000 == 0) fprintf(stderr, "[streams] %u such writes so far\n", n);
			}
		}
		return t_stream ? static_cast<Stream*>(t_stream)->filling : filling;
	}
	ThreadedRenderDevice::Said &ThreadedRenderDevice::SaidHere()
	{
		return t_stream ? static_cast<Stream*>(t_stream)->said : said;
	}
	ThreadedRenderDevice::Batch* ThreadedRenderDevice::TakeSpare()
	{
		Batch* b = NULL;
		{
			std::lock_guard<std::mutex> g(lock);
			if (!spare.empty()) { b = spare.back(); spare.pop_back(); }
		}
		return b ? b : new Batch();
	}

	void* ThreadedRenderDevice::BeginParallelStream()
	{
		if (t_stream) return NULL;            // (one inside another: recorded in place)
		Stream* s = new Stream();
		s->filling = TakeSpare();
		s->said = said;
		Push([this, s]() { RunStream(s); });
		Kick();
		return s;
	}
	void* ThreadedRenderDevice::NewDetachedStream()
	{
		Stream* s = new Stream();
		s->filling = TakeSpare();
		s->said = said;
		return s;
	}
	void ThreadedRenderDevice::PlaceStream(void* stream)
	{
		Stream* s = static_cast<Stream*>(stream);
		Push([this, s]() { RunStream(s); });
		Kick();
	}
	// (One inside another: a thread that is in a stream of its own - game logic,
	// while another thread records the frame - and lends a hand with a pass's
	// run, which has its own. It goes back to its own when it leaves that one:
	// it used to come out in none, and went on to write into the frame's.)
	void ThreadedRenderDevice::EnterParallelStream(void* stream)
	{
		static_cast<Stream*>(stream)->outer = t_stream;
		t_stream = stream;
	}
	void ThreadedRenderDevice::LeaveParallelStream(void* stream)
	{
		Stream* s = static_cast<Stream*>(stream);
		void* const outer = s->outer;
		t_stream = stream;
		Kick();
		t_stream = outer;
		{
			std::lock_guard<std::mutex> g(lock);
			s->closed = true;
		}
		wake.notify_all();
	}
	// The device thread, at the place marked: what the stream's thread hands
	// over, as it comes, until the stream is left.
	void ThreadedRenderDevice::RunStream(Stream* s)
	{
		{
			std::lock_guard<std::mutex> g(lock);
			s->reached = true;
		}
		done.notify_all();
		for (;;)
		{
			Batch* b = NULL;
			{
				std::unique_lock<std::mutex> g(lock);
				wake.wait(g, [s]() { return !s->ready.empty() || s->closed; });
				if (s->ready.empty()) break;
				b = s->ready.front();
				s->ready.pop_front();
			}
			for (size_t i = 0; i < b->entries.size(); i++) b->entries[i].run(b->entries[i].at);
			b->entries.clear();
			b->big.clear();
			if (b->chunks.size() > 8) b->chunks.resize(8);
			b->chunk = 0; b->used = 0;
			{
				std::lock_guard<std::mutex> g(lock);
				spare.push_back(b);
				s->ran++;
			}
			done.notify_all();
		}
		// (nobody else has it any more: it was left before `closed` was set)
		delete s->filling;
		delete s;
	}

	void* ThreadedRenderDevice::Alloc(const size_t bytes)
	{
		Batch &b = *Filling();
		const size_t need = (bytes + 15) & ~(size_t)15;
		if (need > kChunk)
		{
			b.big.push_back(std::unique_ptr<uchar[]>(new uchar[need]));
			return b.big.back().get();
		}
		if (b.chunks.empty()) { b.chunks.push_back(std::unique_ptr<uchar[]>(new uchar[kChunk])); b.chunk = 0; b.used = 0; }
		if (b.used + need > kChunk)
		{
			b.chunk++;
			if (b.chunk >= b.chunks.size()) b.chunks.push_back(std::unique_ptr<uchar[]>(new uchar[kChunk]));
			b.used = 0;
		}
		uchar* p = b.chunks[b.chunk].get() + b.used;
		b.used += need;
		return p;
	}

	void ThreadedRenderDevice::Kick()
	{
		if (t_stream)
		{
			Stream* s = static_cast<Stream*>(t_stream);
			if (s->filling->entries.empty()) return;
			Batch* next = NULL;
			{
				std::lock_guard<std::mutex> g(lock);
				s->ready.push_back(s->filling);
				s->handed++;
				if (!spare.empty()) { next = spare.back(); spare.pop_back(); }
			}
			wake.notify_all();
			s->filling = next ? next : new Batch();
			return;
		}
		if (filling->entries.empty()) return;
		Batch* next = NULL;
		{
			std::lock_guard<std::mutex> g(lock);
			waiting.push_back(filling);
			if (!spare.empty()) { next = spare.back(); spare.pop_back(); }
		}
		wake.notify_all();
		filling = next ? next : new Batch();
	}

	void ThreadedRenderDevice::Drain(const char* why)
	{
		Kick();
		if (t_stream)
		{
			// A stream's thread: until the device thread has carried out all it
			// was handed. It then waits for more of this stream and touches
			// nothing - the caller may ask the device itself.
			Stream* s = static_cast<Stream*>(t_stream);
			if (trace) fprintf(stderr, "[device thread] a stream's thread asked the device itself: %s\n", why);
			std::unique_lock<std::mutex> g(lock);
			// (...and until it has COME to this stream's place at all: a stream that
			// is placed later - game logic running while another thread records the
			// frame - has handed nothing over yet and was let straight through, to
			// ask the device itself while the device thread was in the middle of that
			// frame. A window made full screen in such a moment took the swapchain
			// away from under it.)
			done.wait(g, [s]() { return s->reached && s->ran == s->handed; });
			return;
		}
		std::unique_lock<std::mutex> g(lock);
		if (!waiting.empty() || working)
		{
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			done.wait(g, [this]() { return waiting.empty() && !working; });
			// (between frames too: a script's update is not inside one, and what it
			// asks of the device - a font's picture sent again - waits all the same)
			if (trace)
			{
				static const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
				if (said.frameOpen) drainsThisFrame++;
				fprintf(stderr, "[device thread] t=%.1f s: waited %.2f ms for, %s: %s\n", std::chrono::duration<f64>(t0 - began).count(), MsSince(t0),
					said.frameOpen ? "inside a frame" : "between frames", why);
			}
		}
	}

	void ThreadedRenderDevice::Work()
	{
#if defined(__APPLE__)
		pthread_setname_np("Pyros render device");
#endif
		// (the thread every draw goes through: a fast core, and not slowed)
		JobSystem::KeepOnPerformanceCores();
		for (;;)
		{
			Batch* b = NULL;
			{
				std::unique_lock<std::mutex> g(lock);
				wake.wait(g, [this]() { return quit || !waiting.empty(); });
				if (waiting.empty()) return;
				b = waiting.front();
				waiting.pop_front();
				working = true;
			}
			for (size_t i = 0; i < b->entries.size(); i++) b->entries[i].run(b->entries[i].at);
			b->entries.clear();
			b->big.clear();
			// (a batch that grew a great many chunks for one big upload gives them back)
			if (b->chunks.size() > 8) b->chunks.resize(8);
			b->chunk = 0; b->used = 0;
			{
				std::lock_guard<std::mutex> g(lock);
				spare.push_back(b);
				working = false;
			}
			done.notify_all();
		}
	}

	void ThreadedRenderDevice::TakeWaits(f64 &gpuMs, f64 &presentMs, f64 &behindMs)
	{
		gpuMs = (f64)gpuWaitUs.exchange(0) / 1000.0;
		presentMs = (f64)presentWaitUs.exchange(0) / 1000.0;
		behindMs = behindWaitMs;
		behindWaitMs = 0.0;
	}

	// ---------------------------------------------------------------- by hand
	static std::atomic<int> g_lateFrameStart{ -1 };
	void ThreadedRenderDevice::SetLateFrameStart(const bool on) { g_lateFrameStart = on ? 1 : 0; }
	bool ThreadedRenderDevice::GetLateFrameStart()
	{
		if (g_lateFrameStart.load() < 0) g_lateFrameStart = (std::getenv("PYROS_LATE_INPUT") != NULL && std::getenv("PYROS_LATE_INPUT")[0] == '1') ? 1 : 0;
		return g_lateFrameStart.load() == 1;
	}

	void ThreadedRenderDevice::WaitForFrameRoom()
	{
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		if (framesBehind.load() >= 2)
		{
			Kick();
			std::unique_lock<std::mutex> g(lock);
			done.wait(g, [this]() { return framesBehind.load() < 2; });
		}
		// The device thread still has a frame to finish: this one need not be begun until
		// just long enough before that for it to be ready when the thread is. Begun now,
		// its input would be that much older when it is seen - a frame of the GPU's, when
		// the GPU is the slower side. (Asked for: SetLateFrameStart, or PYROS_LATE_INPUT=1.)
		const bool late = GetLateFrameStart();
		const uint32_t takes = deviceFrameUs.load();
		if (late && framesBehind.load() >= 1 && takes > 0)
		{
			const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			const int64_t began = deviceInFrame.load() ? deviceFrameStartUs.load() : nowUs;
			int64_t waitUs = began + (int64_t)takes - (int64_t)(sayingMs * 1250.0) - 300 - nowUs;
			if (waitUs > 100000) waitUs = 100000;
			if (waitUs > 200)
			{
				std::unique_lock<std::mutex> g(lock);
				done.wait_for(g, std::chrono::microseconds(waitUs), [this]() { return framesBehind.load() < 1; });
			}
		}
		const f64 waited = MsSince(t0);
		if (waited > 0.05) behindWaitMs += waited;
		sayingSince = std::chrono::steady_clock::now();
	}

	void ThreadedRenderDevice::BeginFrame()
	{
		// No more than a frame ahead of the device thread: what is drawn is seen
		// soon, and the queue stays short. (Time waited here is time the GPU
		// side is the slower one.)
		if (framesBehind.load() >= 2)
		{
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			Kick();
			std::unique_lock<std::mutex> g(lock);
			done.wait(g, [this]() { return framesBehind.load() < 2; });
			behindWaitMs += MsSince(t0);
		}
		said.frameOpen = true;
		drainsThisFrame = 0;
		Push([this]() {
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			deviceFrameStartUs = std::chrono::duration_cast<std::chrono::microseconds>(t0.time_since_epoch()).count();
			deviceInFrame = true;
			real->BeginFrame();
			gpuWaitUs += (uint64)(MsSince(t0) * 1000.0);
		});
	}

	void ThreadedRenderDevice::EndFrame()
	{
		said.frameOpen = false;
		if (sayingSince.time_since_epoch().count() != 0) sayingMs = sayingMs * 0.8 + MsSince(sayingSince) * 0.2;
		framesBehind++;
		const std::chrono::steady_clock::time_point readAt = sayingSince;
		Push([this, readAt]() {
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			real->EndFrame();
			if (readAt.time_since_epoch().count() != 0) inputToShownUs = (uint32_t)(MsSince(readAt) * 1000.0);
			presentWaitUs += (uint64)(MsSince(t0) * 1000.0);
			{
				const int64_t endUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				const int64_t took = endUs - deviceFrameStartUs.load();
				if (took > 0 && took < 2000000) deviceFrameUs = (uint32_t)(deviceFrameUs.load() == 0 ? took : (deviceFrameUs.load() * 4 + took) / 5);
				deviceInFrame = false;
			}
			framesBehind--;
			// (whoever waits in BeginFrame is waiting on this count)
			{ std::lock_guard<std::mutex> g(lock); }
			done.notify_all();
		});
		Kick();
	}

	void ThreadedRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending)
	{
		SaidHere().target = fbo;
		// The pass this opens is timed on the GPU under the scope the frame is
		// in NOW, on this thread: by the time the device thread opens it the
		// frame is somewhere else.
		struct Name { char text[48]; } name;
		// (a stream's thread has no scope of its own: the pass is named where it is carried out)
		const char* scope = t_stream ? "Renderer.ShadowMaps" : FrameProfiler::Instance().CurrentScopeName();
		std::strncpy(name.text, scope ? scope : "", sizeof(name.text) - 1);
		name.text[sizeof(name.text) - 1] = 0;
		Push([r = real, nativeAccess, fbo, finalizePending, name]() {
			SetGpuScopeHint(name.text);
			r->BindFramebuffer(nativeAccess, fbo, finalizePending);
			SetGpuScopeHint(NULL);
		});
	}

	void ThreadedRenderDevice::SetClearColor(const Vec4 &color)
	{
		lastClearColor = color;
		Push([r = real, c = Vec4(color)]() { r->SetClearColor(c); });
	}

	bool ThreadedRenderDevice::IsFrameInProgress() const { return const_cast<ThreadedRenderDevice*>(this)->SaidHere().frameOpen; }
	DeviceHandle ThreadedRenderDevice::GetCurrentRenderTarget() { return SaidHere().target; }
	CommandBufferHandle ThreadedRenderDevice::BeginCommandBuffer() { const Said &h = SaidHere(); return (h.frameOpen || h.target != 0) ? 1 : 0; }
	void ThreadedRenderDevice::WaitIdle() { Drain("WaitIdle"); real->WaitIdle(); }

	// Made in the queue, in their turn, under a handle given out here at once: nothing waits.
	DeviceHandle ThreadedRenderDevice::CreateBuffer(const uint32 bufferType, const uint32 bufferDraw, const void *data, const uint32 length)
	{
		const DeviceHandle given = nextGiven.fetch_add(1);
		const void* kept = data ? Keep(data, (size_t)length) : NULL;
		Push([this, given, bufferType, bufferDraw, kept, length]() { realOf[given] = real->CreateBuffer(bufferType, bufferDraw, kept, length); });
		return given;
	}
	DeviceHandle ThreadedRenderDevice::CreateBufferOwned(const uint32 bufferType, const uint32 bufferDraw, std::vector<uchar> &&data)
	{
		const DeviceHandle given = nextGiven.fetch_add(1);
		std::shared_ptr<std::vector<uchar> > kept = std::make_shared<std::vector<uchar> >(std::move(data));
		Push([this, given, bufferType, bufferDraw, kept]() { realOf[given] = real->CreateBuffer(bufferType, bufferDraw, kept->empty() ? NULL : &(*kept)[0], (uint32)kept->size()); });
		return given;
	}
	DeviceHandle ThreadedRenderDevice::CreateVertexArray()
	{
		const DeviceHandle given = nextGiven.fetch_add(1);
		Push([this, given]() { realOf[given] = real->CreateVertexArray(); });
		return given;
	}
	DeviceHandle ThreadedRenderDevice::CreatePipeline(const PipelineDesc &desc)
	{
		// (a pipeline is made for the target bound when it is asked for: in the
		// queue that is the target bound at this point of it)
		const DeviceHandle given = nextGiven.fetch_add(1);
		Push([this, given, d = PipelineDesc(desc)]() { realOf[given] = real->CreatePipeline(d); });
		return given;
	}
	DeviceHandle ThreadedRenderDevice::CreateTextureObject()
	{
		const DeviceHandle given = nextGiven.fetch_add(1);
		Push([this, given]() { realOf[given] = real->CreateTextureObject(); });
		return given;
	}
	void ThreadedRenderDevice::DestroyTextureObject(const DeviceHandle texture)
	{
		Push([this, texture]() { real->DestroyTextureObject(RealOf(texture)); realOf.erase(texture); });
	}
	void ThreadedRenderDevice::UploadTexture2D(const uint32 target, const uint32 level, const uint32 internalFormat, const uint32 width, const uint32 height, const uint32 format, const uint32 type, const void *data, const bool willMipmap)
	{
		// (the picture is kept until the upload is made)
		const void* kept = data ? Keep(data, (size_t)real->GetTextureUploadSize(internalFormat, format, width, height)) : NULL;
		Push([r = real, target, level, internalFormat, width, height, format, type, kept, willMipmap]() {
			r->UploadTexture2D(target, level, internalFormat, width, height, format, type, kept, willMipmap);
		});
	}
	// Every frame, where there is one: asked the first time, and afterwards
	// queued like a draw - what it answered last is what it is taken to answer.
	bool ThreadedRenderDevice::RunTemporalUpscale(const TemporalUpscale &frame)
	{
		if (!upscaleAsked)
		{
			Drain("RunTemporalUpscale");
			TemporalUpscale f = frame;
			f.color = RealOf(f.color); f.depth = RealOf(f.depth); f.motion = RealOf(f.motion); f.output = RealOf(f.output);
			upscaleAsked = true;
			upscaleWorked.store(real->RunTemporalUpscale(f));
			return upscaleWorked.load();
		}
		if (!upscaleWorked.load()) return false;
		Push([this, f = TemporalUpscale(frame)]() mutable {
			f.color = RealOf(f.color); f.depth = RealOf(f.depth); f.motion = RealOf(f.motion); f.output = RealOf(f.output);
			upscaleWorked.store(real->RunTemporalUpscale(f));
		});
		return true;
	}
	DeviceHandle ThreadedRenderDevice::CreateUniformBuffer(const uint32 sizeBytes, const uint32 bindingPoint)
	{
		const DeviceHandle given = nextGiven.fetch_add(1);
		Push([this, given, sizeBytes, bindingPoint]() { realOf[given] = real->CreateUniformBuffer(sizeBytes, bindingPoint); });
		return given;
	}
	void ThreadedRenderDevice::DestroyUniformBuffer(const DeviceHandle buffer)
	{
		Push([this, buffer]() { real->DestroyUniformBuffer(RealOf(buffer)); realOf.erase(buffer); });
	}
	void ThreadedRenderDevice::DestroyBuffer(const DeviceHandle buffer)
	{
		Push([this, buffer]() { real->DestroyBuffer(RealOf(buffer)); realOf.erase(buffer); });
	}
	void ThreadedRenderDevice::DeleteVertexArray(const DeviceHandle vao)
	{
		Push([this, vao]() { real->DeleteVertexArray(RealOf(vao)); realOf.erase(vao); });
	}
	void ThreadedRenderDevice::DestroyPipeline(const DeviceHandle pipeline)
	{
		Push([this, pipeline]() { real->DestroyPipeline(RealOf(pipeline)); realOf.erase(pipeline); });
	}

	// ---------------------------------------------------------------- written by tools/gen_threaded_device.py
#include "ThreadedRenderDevice.generated.inl"

}
