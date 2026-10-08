//============================================================================
// Name        : ThreadedRenderDevice.cpp
// Description : A render device that is another device, one thread away
//               (see the header).
//============================================================================

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
		: keep(realDevice), real(realDevice.get()), filling(new Batch()), framesBehind(0), gpuWaitUs(0), presentWaitUs(0)
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

	void* ThreadedRenderDevice::Alloc(const size_t bytes)
	{
		Batch &b = *filling;
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
		if (filling->entries.empty()) return;
		Batch* next = NULL;
		{
			std::lock_guard<std::mutex> g(lock);
			waiting.push_back(filling);
			if (!spare.empty()) { next = spare.back(); spare.pop_back(); }
		}
		wake.notify_one();
		filling = next ? next : new Batch();
	}

	void ThreadedRenderDevice::Drain(const char* why)
	{
		Kick();
		std::unique_lock<std::mutex> g(lock);
		if (!waiting.empty() || working)
		{
			if (trace && saidFrameOpen) { drainsThisFrame++; fprintf(stderr, "[device thread] waited for, inside a frame: %s\n", why); }
			done.wait(g, [this]() { return waiting.empty() && !working; });
		}
	}

	void ThreadedRenderDevice::Work()
	{
#if defined(__APPLE__)
		pthread_setname_np("Pyros render device");
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
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
		saidFrameOpen = true;
		drainsThisFrame = 0;
		Push([this]() {
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			real->BeginFrame();
			gpuWaitUs += (uint64)(MsSince(t0) * 1000.0);
		});
	}

	void ThreadedRenderDevice::EndFrame()
	{
		saidFrameOpen = false;
		framesBehind++;
		Push([this]() {
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			real->EndFrame();
			presentWaitUs += (uint64)(MsSince(t0) * 1000.0);
			framesBehind--;
			// (whoever waits in BeginFrame is waiting on this count)
			{ std::lock_guard<std::mutex> g(lock); }
			done.notify_all();
		});
		Kick();
	}

	void ThreadedRenderDevice::BindFramebuffer(const uint32 nativeAccess, const DeviceHandle fbo, const bool finalizePending)
	{
		saidTarget = fbo;
		// The pass this opens is timed on the GPU under the scope the frame is
		// in NOW, on this thread: by the time the device thread opens it the
		// frame is somewhere else.
		struct Name { char text[48]; } name;
		const char* scope = FrameProfiler::Instance().CurrentScopeName();
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

	bool ThreadedRenderDevice::IsFrameInProgress() const { return saidFrameOpen; }
	DeviceHandle ThreadedRenderDevice::GetCurrentRenderTarget() { return saidTarget; }
	CommandBufferHandle ThreadedRenderDevice::BeginCommandBuffer() { return (saidFrameOpen || saidTarget != 0) ? 1 : 0; }
	void ThreadedRenderDevice::WaitIdle() { Drain("WaitIdle"); real->WaitIdle(); }

	// ---------------------------------------------------------------- written by tools/gen_threaded_device.py
#include "ThreadedRenderDevice.generated.inl"

}
