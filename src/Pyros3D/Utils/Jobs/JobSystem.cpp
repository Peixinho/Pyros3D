//============================================================================
// Name        : JobSystem.cpp
// Author      : Duarte Peixinho
// Description : See JobSystem.h.
//============================================================================

#include <Pyros3D/Utils/Jobs/JobSystem.h>

#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define PYROS_JOBS_NO_THREADS 1
#endif

namespace p3d {

	namespace {

		struct Job
		{
			JobSystem::JobFn fn;
			void* ctx;
			std::function<void()> closure;
			JobCounter* counter;
		};

		void Execute(Job &job)
		{
			if (job.fn) job.fn(job.ctx);
			else job.closure();
			job.counter->pending.fetch_sub(1, std::memory_order_acq_rel);
		}

		// Performance cores only, minus the thread that submits: Box3D's
		// own advice is that efficiency cores give little and can hurt, and
		// the submitting thread helps while it waits.
		uint32 DefaultWorkerCount()
		{
			uint32 cores = 0;
#if defined(__APPLE__)
			int32_t perf = 0;
			size_t len = sizeof(perf);
			if (sysctlbyname("hw.perflevel0.physicalcpu", &perf, &len, NULL, 0) == 0 && perf > 0)
				cores = (uint32)perf;
#endif
			if (cores == 0)
				cores = std::thread::hardware_concurrency();
			if (cores <= 1) return 0;
			const uint32 workers = cores - 1;
			return workers > 15 ? 15 : workers;
		}
	}

	struct JobSystem::Impl
	{
		std::mutex mutex;
		std::condition_variable wake;
		std::deque<Job> queue;
		std::vector<std::thread> threads;
		bool quit = false;

		bool TryRunOne()
		{
			Job job;
			{
				std::lock_guard<std::mutex> lock(mutex);
				if (queue.empty()) return false;
				job = std::move(queue.front());
				queue.pop_front();
			}
			Execute(job);
			return true;
		}

		void WorkerLoop()
		{
			for (;;)
			{
				Job job;
				{
					std::unique_lock<std::mutex> lock(mutex);
					wake.wait(lock, [this] { return quit || !queue.empty(); });
					if (quit && queue.empty()) return;
					job = std::move(queue.front());
					queue.pop_front();
				}
				Execute(job);
			}
		}

		void Push(Job &&job)
		{
			job.counter->pending.fetch_add(1, std::memory_order_relaxed);
			if (threads.empty())
			{
				Execute(job);
				return;
			}
			{
				std::lock_guard<std::mutex> lock(mutex);
				queue.push_back(std::move(job));
			}
			wake.notify_one();
		}
	};

	JobSystem &JobSystem::Instance()
	{
		static JobSystem instance;
		return instance;
	}

	JobSystem::JobSystem() : impl(new Impl())
	{
		uint32 workers = DefaultWorkerCount();
		if (const char* env = std::getenv("PYROS_JOB_WORKERS"))
			workers = (uint32)std::strtoul(env, NULL, 10);
#if defined(PYROS_JOBS_NO_THREADS)
		workers = 0;
#endif
		for (uint32 i = 0; i < workers; i++)
			impl->threads.emplace_back(&Impl::WorkerLoop, impl);
	}

	JobSystem::~JobSystem()
	{
		{
			std::lock_guard<std::mutex> lock(impl->mutex);
			impl->quit = true;
		}
		impl->wake.notify_all();
		for (size_t i = 0; i < impl->threads.size(); i++)
			impl->threads[i].join();
		delete impl;
	}

	uint32 JobSystem::WorkerCount() const
	{
		return (uint32)impl->threads.size();
	}

	void JobSystem::Run(JobFn fn, void* ctx, JobCounter &counter)
	{
		Job job;
		job.fn = fn;
		job.ctx = ctx;
		job.counter = &counter;
		impl->Push(std::move(job));
	}

	void JobSystem::Run(const std::function<void()> &fn, JobCounter &counter)
	{
		Job job;
		job.fn = NULL;
		job.ctx = NULL;
		job.closure = fn;
		job.counter = &counter;
		impl->Push(std::move(job));
	}

	void JobSystem::Wait(JobCounter &counter)
	{
		// Help rather than sleep: the jobs this caller is waiting for may be
		// sitting in the queue behind others, and a job waiting on its own
		// sub-jobs would otherwise hold a worker hostage.
		while (counter.pending.load(std::memory_order_acquire) > 0)
		{
			if (!impl->TryRunOne())
				std::this_thread::yield();
		}
	}

	void JobSystem::ParallelFor(uint32 count, uint32 minBatch, const std::function<void(uint32, uint32)> &fn)
	{
		if (count == 0) return;
		if (minBatch == 0) minBatch = 1;
		const uint32 lanes = WorkerCount() + 1;
		uint32 batch = (count + lanes - 1) / lanes;
		if (batch < minBatch) batch = minBatch;
		if (batch >= count)
		{
			fn(0, count);
			return;
		}

		JobCounter counter;
		// The first batch is the caller's, run after the rest are queued.
		for (uint32 begin = batch; begin < count; begin += batch)
		{
			const uint32 end = begin + batch < count ? begin + batch : count;
			Run([&fn, begin, end]() { fn(begin, end); }, counter);
		}
		fn(0, batch);
		Wait(counter);
	}

}
