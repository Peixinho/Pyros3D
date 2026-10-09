//============================================================================
// Name        : ThreadedRenderDevice.h
// Description : A render device that is another device, one thread away.
//
//               The calls a frame is made of - bind this, set that, draw -
//               are put in a queue and made on the real device by a thread
//               of its own, in the order they were asked for. The thread that
//               draws the scene (and runs the game) no longer waits for the
//               graphics API to take each of them, nor for a frame to be
//               handed to the GPU and presented.
//
//               Everything that has to answer - make a texture, compile a
//               shader, read something back - runs the queue dry first and
//               then asks the real device on the calling thread: the device
//               thread is idle then, so the device is never in two threads at
//               once. A few questions are answered from what this side has
//               already said (which target is bound, whether a frame is open).
//
//               Which call is which is tools/gen_threaded_device.py's
//               business; the calls themselves are written by it.
//============================================================================

#ifndef THREADEDRENDERDEVICE_H
#define THREADEDRENDERDEVICE_H

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <new>

namespace p3d {

	class PYROS3D_API ThreadedRenderDevice : public IRenderDevice {
	public:
		explicit ThreadedRenderDevice(const std::shared_ptr<IRenderDevice> &realDevice);
		virtual ~ThreadedRenderDevice();

		IRenderDevice* Real() const { return real; }
		// What the device thread waited, in ms, since this was last asked: for
		// the GPU to be done with an earlier frame (opening a frame) and for a
		// frame to be presented. And what the calling thread waited for the
		// device thread to catch up. Whoever paces the frames reads these: the
		// calls themselves return at once now.
		void TakeWaits(f64 &gpuMs, f64 &presentMs, f64 &behindMs);
		// The queue run dry: everything asked for has been done.
		void Finish() { Drain("Finish"); }
		// The device's own handle for one given out here (buffers, vertex arrays
		// and pipelines are made in the queue, so theirs are not the device's).
		// On the device thread, or with the queue run dry.
		DeviceHandle RealOf(const DeviceHandle given) const
		{
			if (given < kFirstGiven) return given;
			const std::unordered_map<DeviceHandle, DeviceHandle>::const_iterator it = realOf.find(given);
			return it != realOf.end() ? it->second : 0;
		}

#include <Pyros3D/Rendering/Device/ThreadedRenderDevice.generated.h>

	private:
		struct Entry { void (*run)(void*); void* at; };
		struct Batch {
			std::vector<Entry> entries;
			std::vector<std::unique_ptr<uchar[]> > chunks;
			std::vector<std::unique_ptr<uchar[]> > big;       // what was too big for a chunk: gone when the batch is done
			size_t chunk = 0, used = 0;
		};
		static const size_t kChunk = 256 * 1024;

		void* Alloc(const size_t bytes);
		const void* Keep(const void* data, const size_t bytes) { void* p = Alloc(bytes); std::memcpy(p, data, bytes); return p; }
		template<class L> void Push(L &&fn)
		{
			typedef typename std::decay<L>::type Fn;
			void* at = Alloc(sizeof(Fn));
			new (at) Fn(std::forward<L>(fn));
			Entry e;
			e.at = at;
			e.run = [](void* p) { Fn* f = static_cast<Fn*>(p); (*f)(); f->~Fn(); };
			Batch* into = Filling();
			into->entries.push_back(e);
			if (into->entries.size() >= 384) Kick();
		}
		// A second producer: see IRenderDevice::BeginParallelStream. The thread
		// that has entered one fills ITS batches, which the device thread takes
		// - at the place marked in the frame's own queue - as they are handed
		// over, until the stream is left.
		struct Said { bool frameOpen = false; DeviceHandle target = 0; };
		struct Stream {
			Batch* filling = NULL;
			std::deque<Batch*> ready;         // under `lock`
			bool closed = false;              // under `lock`
			uint64 handed = 0, ran = 0;       // batches handed over, and carried out: under `lock`
			Said said;
		};
		Batch* Filling();                     // this thread's: its stream's, or the frame's
		Said &SaidHere();
		Batch* TakeSpare();
		void RunStream(Stream* s);            // device thread
		void Kick();                          // what has been queued so far goes to the device thread
		void Drain(const char* why);          // ... and is waited for
		void Work();

		std::shared_ptr<IRenderDevice> keep;
		IRenderDevice* real;

		Batch* filling;
		std::vector<Batch*> spare;
		std::deque<Batch*> waiting;
		std::mutex lock;
		std::condition_variable wake, done;
		bool working = false, quit = false;
		std::thread worker;

		// what this side has said, for the questions asked of it
		Said said;
		std::atomic<int> framesBehind;        // frames ended here and not yet presented there
		std::atomic<uint64> gpuWaitUs, presentWaitUs;
		f64 behindWaitMs = 0.0;
		static const DeviceHandle kFirstGiven = 0x40000000u;
		std::atomic<DeviceHandle> nextGiven;
		std::unordered_map<DeviceHandle, DeviceHandle> realOf;      // the device thread's (and anybody's, with the queue dry)
		bool trace = false;
		uint32 drainsThisFrame = 0;
	};

	// The scope a GPU timing is filed under when a device is not on the thread
	// that knows (the frame profiler answers only on the frame's own thread).
	PYROS3D_API void SetGpuScopeHint(const char* name);
	PYROS3D_API const char* GpuScopeName();

}

#endif /* THREADEDRENDERDEVICE_H */
