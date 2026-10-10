//============================================================================
// Name        : JobSystem.h
// Author      : Duarte Peixinho
// Description : Process-wide worker pool. Fork/join only: a caller submits
//               jobs against a JobCounter and waits on it, and while it
//               waits it runs queued jobs itself instead of sleeping - so a
//               job may wait on jobs it spawned without deadlocking, which
//               is what Box3D/Box2D's task callbacks require.
//
//               Jobs must not touch Lua, the render device, or the scene
//               graph's lists: those are main-thread only. The frame
//               profiler ignores scopes opened on workers.
//
//               PYROS_JOB_WORKERS=<n> overrides the worker count; 0 runs
//               everything inline on the calling thread. Builds without
//               threads (web without pthreads) always run inline.
//============================================================================

#ifndef JOBSYSTEM_H
#define JOBSYSTEM_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <atomic>
#include <functional>

namespace p3d {

	// How many submitted jobs have not finished. Wait() on it before
	// reading anything the jobs wrote; it is reusable once it reaches zero.
	struct JobCounter
	{
		std::atomic<int32> pending{0};
	};

	class PYROS3D_API JobSystem
	{
	public:
		typedef void (*JobFn)(void* ctx);

		static JobSystem &Instance();

		// Worker threads, not counting callers who help while waiting.
		// 0 means every job runs inline inside Run().
		uint32 WorkerCount() const;

		void Run(JobFn fn, void* ctx, JobCounter &counter);
		void Run(const std::function<void()> &fn, JobCounter &counter);
		// Returns once counter reaches zero, running queued jobs meanwhile.
		void Wait(JobCounter &counter);

		// Splits [0, count) into batches of at least minBatch and calls
		// fn(begin, end) for each, the caller taking a share. Returns when
		// every batch has finished.
		void ParallelFor(uint32 count, uint32 minBatch, const std::function<void(uint32 begin, uint32 end)> &fn);

		// Whether a loop is quicker shared between the cores, found out rather than
		// assumed. Handing a range out costs something - the workers may be asleep,
		// and waking them is tens of microseconds - so a loop of a few thousand
		// cheap steps is quicker on its own thread on one machine and three times
		// quicker shared on another, or on the same machine once frames are short
		// enough that the workers never sleep. One of these is kept beside a loop
		// (static thread_local): Begin says which way to run it this time and End
		// is called when it has run. Every few hundred runs it times a handful
		// each way and keeps the quicker, with a margin so that it does not flap.
		//   PYROS_SHARED_LOOPS=0 never, =1 always (a test).
		struct PYROS3D_API SharedLoop
		{
			// units: how many steps the loop has this time; least: below this it is not worth asking
			bool Begin(const uint32 units, const uint32 least);
			void End();
			bool shared = false;
		private:
			uint32 run = 0, phase = 0, left = 0, units = 0;
			f64 sum[2] = { 0, 0 };
			uint32 count[2] = { 0, 0 };
			uint64 began = 0;
			bool timing = false, now = false;
		};
		// What the machine's cores are. A processor may have two kinds: fast ones
		// (performance) and slow ones (efficiency) that take two or three times as
		// long over the same work - and work a frame waits for is as slow as the
		// slowest thread it was given to. `performance` and `efficiency` count
		// whole cores, not the two threads some cores can run; where the machine
		// has one kind, they are all `performance`. known: the system said which
		// were which (else it is every core, taken as fast).
		struct Cores { uint32 performance = 0, efficiency = 0, logical = 0; bool known = false; };
		static const Cores &GetCores();
		// The calling thread is kept to the performance cores, at a priority that
		// work the frame waits for should have: the game's own thread, the render
		// device's, every worker. (Nothing where the system has one kind of core
		// and no say in the matter.)
		static void KeepOnPerformanceCores();

		// Prints what handing work out costs on this machine (PYROS_JOB_BENCH=1
		// has the player do it at start).
		void Benchmark();

	private:
		JobSystem();
		~JobSystem();
		JobSystem(const JobSystem &) = delete;
		JobSystem &operator=(const JobSystem &) = delete;

		struct Impl;
		Impl* impl;
	};

}

#endif /* JOBSYSTEM_H */
