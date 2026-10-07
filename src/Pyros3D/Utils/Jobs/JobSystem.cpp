//============================================================================
// Name        : JobSystem.cpp
// Author      : Duarte Peixinho
// Description : See JobSystem.h.
//============================================================================

#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif
#include <chrono>
#include <atomic>
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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif
#if defined(_MSC_VER) && defined(_M_ARM64)
#include <intrin.h>
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

		inline void CpuRelax()
		{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
			_mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
	#if defined(_MSC_VER)
			__yield();
	#else
			__asm__ __volatile__("yield");
	#endif
#else
			std::this_thread::yield();
#endif
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
		// How many jobs are queued, readable without the lock: what a worker
		// that has just finished one looks at before deciding to sleep.
		std::atomic<uint32> queued{ 0 };
		// How many workers are asleep on `wake` (nobody is told to wake up
		// when nobody is asleep).
		std::atomic<uint32> asleep{ 0 };

		bool TryRunOne()
		{
			Job job;
			{
				std::lock_guard<std::mutex> lock(mutex);
				if (queue.empty()) return false;
				job = std::move(queue.front());
				queue.pop_front();
				queued.fetch_sub(1, std::memory_order_relaxed);
			}
			Execute(job);
			return true;
		}

		// A worker that has run out of work does not go to sleep at once. A
		// frame hands out work in bursts a fraction of a millisecond apart,
		// and a sleeping thread takes longer than that to come back - which
		// made small jobs cost more than doing them on the spot. So it looks
		// again and again for a moment (kSpin), and only then sleeps: hot
		// while a frame is being made, asleep between frames.
		void WorkerLoop()
		{
#if defined(__APPLE__)
			// Work the frame is waiting for: on the performance cores, not
			// wherever the scheduler puts a thread nobody has spoken for.
			pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#endif
			// (PYROS_JOB_SPIN_US: how long, in microseconds - a quarter of a
			// millisecond unless said. A game running flat out, frame after
			// frame, wants its workers up for the whole of each: longer than
			// the gaps between one burst and the next.)
			static const std::chrono::microseconds kSpin([]() -> long long {
				const char* v = std::getenv("PYROS_JOB_SPIN_US");
				const long long us = v ? std::strtoll(v, NULL, 10) : 250;
				return us < 0 ? 0 : us;
			}());
			for (;;)
			{
				if (TryRunOne()) continue;
				const std::chrono::steady_clock::time_point idleSince = std::chrono::steady_clock::now();
				bool found = false;
				while (std::chrono::steady_clock::now() - idleSince < kSpin)
				{
					if (queued.load(std::memory_order_relaxed) > 0) { found = true; break; }
					for (int k = 0; k < 16; k++) CpuRelax();
				}
				if (found) continue;
				std::unique_lock<std::mutex> lock(mutex);
				if (quit && queue.empty()) return;
				if (!queue.empty()) continue;
				asleep.fetch_add(1, std::memory_order_relaxed);
				wake.wait(lock, [this] { return quit || !queue.empty(); });
				asleep.fetch_sub(1, std::memory_order_relaxed);
				if (quit && queue.empty()) return;
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
				queued.fetch_add(1, std::memory_order_relaxed);
			}
			if (asleep.load(std::memory_order_relaxed) > 0) wake.notify_one();
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

	// PYROS_JOB_BENCH=1: what handing work out costs on this machine, printed
	// once. Two kinds of loop - many cheap items, fewer dear ones - each done
	// on the caller alone and through ParallelFor, back to back and with a
	// pause between (a frame's bursts are not back to back).
	void JobSystem::Benchmark()
	{
		struct Kind { const char* name; uint32 items; int spins; };
		const Kind kinds[2] = { { "4000 items of ~40 ns", 4000, 12 }, { "500 items of ~5 us", 500, 1500 } };
		std::vector<f32> sink(4096, 1.f);
		for (int k = 0; k < 2; k++)
		{
			const Kind &kind = kinds[k];
			const std::function<void(uint32, uint32)> work = [&](uint32 begin, uint32 end) {
				for (uint32 i = begin; i < end; i++)
				{
					f32 v = (f32)i;
					for (int n = 0; n < kind.spins; n++) v = v * 1.0000001f + 0.5f;
					sink[i & 4095] = v;
				}
			};
			for (int pause = 0; pause < 2; pause++)
			{
				f64 serial = 0.0, parallel = 0.0;
				const int rounds = 200;
				for (int r = 0; r < rounds; r++)
				{
					if (pause) std::this_thread::sleep_for(std::chrono::milliseconds(4));
					std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
					work(0, kind.items);
					serial += std::chrono::duration<f64, std::micro>(std::chrono::steady_clock::now() - t0).count();
					if (pause) std::this_thread::sleep_for(std::chrono::milliseconds(4));
					t0 = std::chrono::steady_clock::now();
					ParallelFor(kind.items, 16, work);
					parallel += std::chrono::duration<f64, std::micro>(std::chrono::steady_clock::now() - t0).count();
				}
				fprintf(stderr, "[jobs] %u workers, %s, %s: alone %.0f us, shared out %.0f us (x%.2f)\n", WorkerCount(), kind.name,
					pause ? "4 ms apart" : "back to back", serial / rounds, parallel / rounds, serial / (parallel > 0.0 ? parallel : 1.0));
			}
		}
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
				for (int k = 0; k < 8; k++) CpuRelax();
		}
	}

	// One range, and every lane takes the next piece of it until none is
	// left: a lane on a slow core, or one that started late, simply takes
	// fewer pieces - the range is not cut into as many equal parts as there
	// are lanes and left waiting on the slowest of them. (And one job a lane,
	// not one a piece.)
	void JobSystem::ParallelFor(uint32 count, uint32 minBatch, const std::function<void(uint32, uint32)> &fn)
	{
		if (count == 0) return;
		if (minBatch == 0) minBatch = 1;
		const uint32 lanes = WorkerCount() + 1;
		if (lanes == 1 || count <= minBatch)
		{
			fn(0, count);
			return;
		}
		// (four pieces a lane where the range allows: enough to even out, few
		// enough that taking one is not the cost)
		uint32 piece = count / (lanes * 4);
		if (piece < minBatch) piece = minBatch;
		const uint32 pieces = (count + piece - 1) / piece;
		struct Range
		{
			std::atomic<uint32> next{ 0 };
			uint32 count, piece;
			const std::function<void(uint32, uint32)>* fn;
			static void Take(void* ctx)
			{
				Range* r = static_cast<Range*>(ctx);
				for (;;)
				{
					const uint32 begin = r->next.fetch_add(r->piece, std::memory_order_relaxed);
					if (begin >= r->count) return;
					const uint32 end = begin + r->piece < r->count ? begin + r->piece : r->count;
					(*r->fn)(begin, end);
				}
			}
		} range;
		range.count = count; range.piece = piece; range.fn = &fn;

		JobCounter counter;
		const uint32 helpers = pieces - 1 < lanes - 1 ? pieces - 1 : lanes - 1;
		for (uint32 h = 0; h < helpers; h++) Run(&Range::Take, &range, counter);
		Range::Take(&range);
		Wait(counter);
	}

}
