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
