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
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sched.h>
#include <fstream>
#include <string>
#include <set>
#endif
#include <cstdio>

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

		// The cores, asked of the system once. (See JobSystem::Cores.)
		struct CoreMap
		{
			JobSystem::Cores cores;
#if defined(_WIN32)
			std::vector<GROUP_AFFINITY> fast;        // the performance cores' logical processors, by group
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
			std::vector<int> fast;                   // ... by number
#endif
		};
		CoreMap FindCores()
		{
			CoreMap m;
			m.cores.logical = std::thread::hardware_concurrency();
#if defined(__APPLE__)
			int32_t perf = 0, eff = 0;
			size_t len = sizeof(perf);
			if (sysctlbyname("hw.perflevel0.physicalcpu", &perf, &len, NULL, 0) == 0 && perf > 0)
			{
				m.cores.performance = (uint32)perf;
				len = sizeof(eff);
				if (sysctlbyname("hw.perflevel1.physicalcpu", &eff, &len, NULL, 0) == 0 && eff > 0) m.cores.efficiency = (uint32)eff;
				m.cores.known = true;
			}
#elif defined(_WIN32)
			// Every core, with the class the system gives it: the highest class is
			// the performance cores (a processor of one kind has one class).
			DWORD bytes = 0;
			GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &bytes);
			if (bytes > 0)
			{
				std::vector<uchar> buffer(bytes);
				if (GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)&buffer[0], &bytes))
				{
					BYTE best = 0;
					for (DWORD at = 0; at < bytes;)
					{
						const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* e = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)&buffer[at];
						if (e->Relationship == RelationProcessorCore && e->Processor.EfficiencyClass > best) best = e->Processor.EfficiencyClass;
						at += e->Size;
					}
					for (DWORD at = 0; at < bytes;)
					{
						const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* e = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)&buffer[at];
						if (e->Relationship == RelationProcessorCore)
						{
							if (e->Processor.EfficiencyClass == best)
							{
								m.cores.performance++;
								for (WORD g = 0; g < e->Processor.GroupCount; g++)
								{
									const GROUP_AFFINITY &ga = e->Processor.GroupMask[g];
									bool merged = false;
									for (size_t k = 0; k < m.fast.size(); k++)
										if (m.fast[k].Group == ga.Group) { m.fast[k].Mask |= ga.Mask; merged = true; }
									if (!merged) m.fast.push_back(ga);
								}
							}
							else m.cores.efficiency++;
						}
						at += e->Size;
					}
					m.cores.known = m.cores.performance > 0;
				}
			}
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
			// A processor of two kinds lists its fast cores' numbers here ("0-15"):
			// a file that is not there on a processor of one kind.
			auto parse = [](const std::string &text, std::vector<int> &out) {
				size_t at = 0;
				while (at < text.size())
				{
					size_t end = text.find(',', at);
					if (end == std::string::npos) end = text.size();
					const std::string part = text.substr(at, end - at);
					const size_t dash = part.find('-');
					if (!part.empty() && part[0] >= '0' && part[0] <= '9')
					{
						const int a = std::atoi(part.c_str());
						const int b = dash == std::string::npos ? a : std::atoi(part.c_str() + dash + 1);
						for (int c = a; c <= b && c < 4096; c++) out.push_back(c);
					}
					at = end + 1;
				}
			};
			std::ifstream fastList("/sys/devices/cpu_core/cpus");
			std::string line;
			if (fastList && std::getline(fastList, line)) parse(line, m.fast);
			if (!m.fast.empty())
			{
				// whole cores among them: one for each set of threads that share one
				std::set<std::string> whole;
				for (size_t k = 0; k < m.fast.size(); k++)
				{
					std::ifstream sib("/sys/devices/system/cpu/cpu" + std::to_string(m.fast[k]) + "/topology/thread_siblings_list");
					std::string who;
					if (sib && std::getline(sib, who)) whole.insert(who); else whole.insert(std::to_string(m.fast[k]));
				}
				m.cores.performance = (uint32)whole.size();
				std::vector<int> slow;
				std::ifstream slowList("/sys/devices/cpu_atom/cpus");
				if (slowList && std::getline(slowList, line)) parse(line, slow);
				m.cores.efficiency = (uint32)slow.size();
				m.cores.known = true;
			}
#endif
			if (m.cores.performance == 0)
			{
				// One kind, or nobody to ask: every core, and the threads a core can
				// run two of are not counted twice where that can be told.
				uint32 whole = m.cores.logical;
#if defined(_WIN32)
				DWORD bytes2 = 0;
				GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &bytes2);
				if (bytes2 > 0)
				{
					std::vector<uchar> buffer(bytes2);
					if (GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)&buffer[0], &bytes2))
					{
						whole = 0;
						for (DWORD at = 0; at < bytes2;)
						{
							const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* e = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)&buffer[at];
							if (e->Relationship == RelationProcessorCore) whole++;
							at += e->Size;
						}
					}
				}
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
				std::set<std::string> cores;
				for (uint32 c = 0; c < m.cores.logical; c++)
				{
					std::ifstream sib("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/topology/thread_siblings_list");
					std::string who;
					if (sib && std::getline(sib, who)) cores.insert(who);
				}
				if (!cores.empty()) whole = (uint32)cores.size();
#endif
				m.cores.performance = whole;
			}
			return m;
		}
		const CoreMap &TheCores()
		{
			static const CoreMap map = FindCores();
			return map;
		}

		// A worker for every performance core but two: the thread that hands the
		// work out helps while it waits, and the render device's thread is busy the
		// whole frame through - a worker put on its core, or on an efficiency core,
		// makes the frame wait for it. (Box3D's own advice is the same: efficiency
		// cores give little and can hurt.) Never fewer than one where there are two
		// cores at all.
		uint32 DefaultWorkerCount()
		{
			const uint32 cores = TheCores().cores.performance;
			if (cores <= 1) return 0;
			const uint32 workers = cores > 3 ? cores - 2 : cores - 1;
			return workers > 30 ? 30 : workers;
		}
	}

	const JobSystem::Cores &JobSystem::GetCores() { return TheCores().cores; }

	void JobSystem::KeepOnPerformanceCores()
	{
#if defined(__APPLE__)
		// (the system has no way to name cores: this class of work is what it
		// keeps on the performance ones)
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#elif defined(_WIN32)
		const CoreMap &m = TheCores();
		// Only where there are two kinds: on a processor of one kind the system
		// places threads better than a fixed list would.
		if (m.cores.known && m.cores.efficiency > 0 && !m.fast.empty())
		{
			// (a thread lives in one group of processors: the first that has fast cores)
			GROUP_AFFINITY want = m.fast[0];
			GROUP_AFFINITY now;
			if (GetThreadGroupAffinity(GetCurrentThread(), &now))
				for (size_t k = 0; k < m.fast.size(); k++) if (m.fast[k].Group == now.Group) want = m.fast[k];
			want.Reserved[0] = want.Reserved[1] = want.Reserved[2] = 0;
			SetThreadGroupAffinity(GetCurrentThread(), &want, NULL);
		}
		// Not slowed to save power, whatever the system thinks of a window that
		// is not in front or a machine on a battery plan.
		{
			struct Throttling { ULONG Version; ULONG ControlMask; ULONG StateMask; } t = { 1, 0x1 /* execution speed */, 0 };
			typedef BOOL (WINAPI *SetInfo)(HANDLE, int, LPVOID, DWORD);
			if (HMODULE k32 = GetModuleHandleA("kernel32.dll"))
				if (SetInfo set = (SetInfo)GetProcAddress(k32, "SetThreadInformation"))
					set(GetCurrentThread(), 3 /* ThreadPowerThrottling */, &t, sizeof(t));
		}
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
		const CoreMap &m = TheCores();
		if (m.cores.known && m.cores.efficiency > 0 && !m.fast.empty())
		{
			cpu_set_t set;
			CPU_ZERO(&set);
			for (size_t k = 0; k < m.fast.size(); k++) if (m.fast[k] < CPU_SETSIZE) CPU_SET(m.fast[k], &set);
			sched_setaffinity(0, sizeof(set), &set);
		}
#endif
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

		// Only a job that was handed out with this context: what a ParallelFor
		// waits with. Helping with WHATEVER is queued while waiting for one's own
		// range ran other threads' work in the middle of this one's - a whole
		// pass of drawing, begun inside another pass's list-keeping, each writing
		// to the stream of commands the thread had bound: a crash in the device.
		bool TryRunWith(void* ctx)
		{
			Job job;
			{
				std::lock_guard<std::mutex> lock(mutex);
				std::deque<Job>::iterator at = queue.begin();
				for (; at != queue.end(); ++at) if (at->ctx == ctx) break;
				if (at == queue.end()) return false;
				job = std::move(*at);
				queue.erase(at);
				queued.fetch_sub(1, std::memory_order_relaxed);
			}
			Execute(job);
			return true;
		}

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
			// Work the frame is waiting for: on the performance cores, not
			// wherever the scheduler puts a thread nobody has spoken for.
			JobSystem::KeepOnPerformanceCores();
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
		// (PYROS_JOB_BENCH or PYROS_LOG_CORES: what was found, said once)
		if (std::getenv("PYROS_LOG_CORES") || std::getenv("PYROS_JOB_BENCH"))
		{
			const Cores &c = GetCores();
			fprintf(stderr, "[jobs] cores: %u performance, %u efficiency, %u logical (%s); %u workers\n", c.performance, c.efficiency, c.logical,
				c.known ? "as the system says" : "one kind, or not told", workers);
		}
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

	namespace {
		uint64 NowNs() { return (uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
		int SharedLoopsForced()
		{
			static const int v = []() { const char* e = std::getenv("PYROS_SHARED_LOOPS"); return e == NULL ? -1 : (e[0] == '0' ? 0 : 1); }();
			return v;
		}
	}
	bool JobSystem::SharedLoop::Begin(const uint32 n, const uint32 least)
	{
		timing = false;
		if (n < least || JobSystem::Instance().WorkerCount() == 0) return now = false;
		const int forced = SharedLoopsForced();
		if (forced >= 0) return now = (forced == 1);
		// (a look every 300 runs, every 60 for the first thousand: 16 runs on its
		// own thread, 16 shared - the first two of each not counted, a way of
		// running that has just been changed to is not at its pace yet)
		run++;
		const uint32 every = run < 1000 ? 60 : 300;
		if (phase == 0 && run % every == 0) { phase = 1; left = 16; sum[0] = sum[1] = 0; count[0] = count[1] = 0; }
		if (phase == 0) return now = shared;
		now = phase == 2;
		units = n;
		timing = true;
		began = NowNs();
		return now;
	}
	void JobSystem::SharedLoop::End()
	{
		if (!timing) return;
		timing = false;
		const f64 ns = (f64)(NowNs() - began) / (f64)(units > 0 ? units : 1);
		const uint32 slot = phase - 1;
		if (left <= 14) { sum[slot] += ns; count[slot]++; }
		if (--left > 0) return;
		if (phase == 1) { phase = 2; left = 16; return; }
		phase = 0;
		const f64 alone = sum[0] / (f64)(count[0] > 0 ? count[0] : 1), together = sum[1] / (f64)(count[1] > 0 ? count[1] : 1);
		// (what is in use keeps its place unless the other is clearly quicker: 10%)
		if (!shared && together < alone * 0.90) shared = true;
		else if (shared && alone < together * 0.90) shared = false;
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
		// (its own helpers only, if any has not been started: see TryRunWith)
		while (counter.pending.load(std::memory_order_acquire) > 0)
		{
			if (!impl->TryRunWith(&range))
				for (int k = 0; k < 8; k++) CpuRelax();
		}
	}

}
