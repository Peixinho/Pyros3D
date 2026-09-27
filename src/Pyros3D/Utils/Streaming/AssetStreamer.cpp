//============================================================================
// Name        : AssetStreamer.cpp
// Author      : Duarte Peixinho
// Description : See AssetStreamer.h.
//============================================================================

#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define PYROS_STREAM_NO_THREADS 1
#endif

namespace p3d {

	namespace {

		struct Request
		{
			AssetStreamer::Ticket id;
			uint64 seq;
			f32 priority;
			std::function<void()> work;
			// Returns true when done; a plain finish is one step.
			std::function<bool()> step;
			AssetStreamer::State state;
			// Read by a loader thread outside the lock, before it runs work.
			std::atomic<bool> cancelled{false};
		};

		typedef std::shared_ptr<Request> RequestPtr;

		uint32 DefaultWorkerCount()
		{
#if defined(PYROS_STREAM_NO_THREADS)
			return 0;
#else
			if (const char* v = std::getenv("PYROS_STREAM_WORKERS"))
			{
				const int n = std::atoi(v);
				return n < 0 ? 0 : (n > 8 ? 8 : (uint32)n);
			}
			return 2;
#endif
		}

		// Loading must never compete with the frame for a core.
		void LowerThreadPriority()
		{
#if defined(__APPLE__)
			pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#elif defined(_WIN32)
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
		}

		void RunGuarded(const std::function<void()> &fn, const char* what)
		{
			if (!fn) return;
			try { fn(); }
			catch (const std::exception &e) { echo(std::string("ERROR: AssetStreamer - ") + what + " threw: " + e.what()); }
			catch (...) { echo(std::string("ERROR: AssetStreamer - ") + what + " threw"); }
		}
	}

	struct AssetStreamer::Impl
	{
		mutable std::mutex mutex;
		std::condition_variable wake;	// loader threads: work queued / quit
		std::condition_variable idle;	// Flush: a work closure finished
		std::vector<RequestPtr> queued;	// unordered; picked by (priority, seq)
		std::deque<RequestPtr> ready;	// work done, awaiting Pump, oldest first
		std::map<Ticket, RequestPtr> live;
		std::vector<std::thread> threads;
		uint32 loading = 0;
		Ticket nextId = 1;
		uint64 nextSeq = 0;
		bool quit = false;

		// Caller holds mutex.
		RequestPtr PopBest()
		{
			if (queued.empty()) return RequestPtr();
			size_t best = 0;
			for (size_t i = 1; i < queued.size(); i++)
			{
				const Request &a = *queued[i], &b = *queued[best];
				if (a.priority < b.priority || (a.priority == b.priority && a.seq < b.seq)) best = i;
			}
			RequestPtr r = queued[best];
			queued[best] = queued.back();
			queued.pop_back();
			r->state = Loading;
			loading++;
			return r;
		}

		// Runs r's work outside the lock, then files it as ready.
		void RunWork(const RequestPtr &r, std::unique_lock<std::mutex> &lock)
		{
			lock.unlock();
			if (!r->cancelled) RunGuarded(r->work, "work");
			r->work = nullptr;	// release what it captured, on this thread
			lock.lock();
			loading--;
			if (r->cancelled)
				live.erase(r->id);
			else
			{
				r->state = Ready;
				ready.push_back(r);
			}
			idle.notify_all();
		}

		void LoaderLoop()
		{
			LowerThreadPriority();
			std::unique_lock<std::mutex> lock(mutex);
			for (;;)
			{
				wake.wait(lock, [this] { return quit || !queued.empty(); });
				if (quit) return;
				RequestPtr r = PopBest();
				RunWork(r, lock);
			}
		}
	};

	AssetStreamer &AssetStreamer::Instance()
	{
		static AssetStreamer instance;
		return instance;
	}

	AssetStreamer::AssetStreamer() : impl(new Impl())
	{
		const uint32 n = DefaultWorkerCount();
		for (uint32 i = 0; i < n; i++)
			impl->threads.push_back(std::thread([this] { impl->LoaderLoop(); }));
	}

	AssetStreamer::~AssetStreamer()
	{
		Shutdown();
		delete impl;
	}

	void AssetStreamer::Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(impl->mutex);
			impl->quit = true;
		}
		impl->wake.notify_all();
		for (size_t i = 0; i < impl->threads.size(); i++)
			if (impl->threads[i].joinable()) impl->threads[i].join();
		impl->threads.clear();
		std::lock_guard<std::mutex> lock(impl->mutex);
		impl->queued.clear();
		impl->ready.clear();
		impl->live.clear();
		impl->loading = 0;
	}

	AssetStreamer::Ticket AssetStreamer::Submit(const std::function<void()> &work, const std::function<void()> &finish, const f32 priority)
	{
		std::function<void()> f = finish;
		return SubmitSteps(work, [f]() { if (f) f(); return true; }, priority);
	}

	AssetStreamer::Ticket AssetStreamer::SubmitSteps(const std::function<void()> &work, const std::function<bool()> &step, const f32 priority)
	{
		RequestPtr r = std::make_shared<Request>();
		r->work = work;
		r->step = step;
		r->priority = priority;
		r->state = Queued;
		{
			std::lock_guard<std::mutex> lock(impl->mutex);
			if (impl->quit) return 0;
			r->id = impl->nextId++;
			r->seq = impl->nextSeq++;
			impl->queued.push_back(r);
			impl->live[r->id] = r;
		}
		impl->wake.notify_one();
		return r->id;
	}

	void AssetStreamer::SetPriority(const Ticket ticket, const f32 priority)
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		std::map<Ticket, RequestPtr>::iterator it = impl->live.find(ticket);
		if (it != impl->live.end() && it->second->state == Queued)
			it->second->priority = priority;
	}

	bool AssetStreamer::Cancel(const Ticket ticket)
	{
		RequestPtr dropped;	// destroyed after the lock is released
		std::lock_guard<std::mutex> lock(impl->mutex);
		std::map<Ticket, RequestPtr>::iterator it = impl->live.find(ticket);
		if (it == impl->live.end()) return false;
		dropped = it->second;
		dropped->cancelled = true;
		switch (dropped->state)
		{
		case Queued:
			for (size_t i = 0; i < impl->queued.size(); i++)
				if (impl->queued[i] == dropped)
				{
					impl->queued[i] = impl->queued.back();
					impl->queued.pop_back();
					break;
				}
			impl->live.erase(it);
			break;
		case Ready:
			for (std::deque<RequestPtr>::iterator q = impl->ready.begin(); q != impl->ready.end(); ++q)
				if (*q == dropped) { impl->ready.erase(q); break; }
			impl->live.erase(it);
			break;
		default:
			// Loading: the loader thread sees the flag and forgets it.
			break;
		}
		return true;
	}

	AssetStreamer::State AssetStreamer::GetState(const Ticket ticket) const
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		std::map<Ticket, RequestPtr>::const_iterator it = impl->live.find(ticket);
		if (it == impl->live.end() || it->second->cancelled) return Unknown;
		return it->second->state;
	}

	uint32 AssetStreamer::Pump(const f64 budgetMs)
	{
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		uint32 ran = 0;
		for (;;)
		{
			RequestPtr r;
			{
				std::unique_lock<std::mutex> lock(impl->mutex);
				// No loader threads: this thread does the work too, one
				// request per turn, so the budget still bounds the frame.
				if (impl->ready.empty() && impl->threads.empty())
				{
					RequestPtr w = impl->PopBest();
					if (w) impl->RunWork(w, lock);
				}
				if (impl->ready.empty()) break;
				// Left at the front while it steps: a stepped finish is
				// resumed next turn, ahead of anything that finished later.
				r = impl->ready.front();
			}
			bool done = true;
			if (r->step)
			{
				try { done = r->step(); }
				catch (const std::exception &e) { echo(std::string("ERROR: AssetStreamer - finish threw: ") + e.what()); }
				catch (...) { echo("ERROR: AssetStreamer - finish threw"); }
			}
			if (done)
			{
				std::lock_guard<std::mutex> lock(impl->mutex);
				// The step itself may have cancelled this request.
				if (!impl->ready.empty() && impl->ready.front() == r) impl->ready.pop_front();
				impl->live.erase(r->id);
			}
			ran++;
			const f64 spent = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
			if (spent >= budgetMs) break;
		}
		return ran;
	}

	void AssetStreamer::Flush()
	{
		// A finish may submit more work (a cell that streams its own
		// neighbours), so drain until nothing is left at all.
		while (PendingCount() > 0)
		{
			{
				std::unique_lock<std::mutex> lock(impl->mutex);
				if (!impl->threads.empty())
					impl->idle.wait(lock, [this] { return impl->queued.empty() && impl->loading == 0; });
			}
			Pump(1e30);
		}
	}

	uint32 AssetStreamer::PendingCount() const
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		return (uint32)(impl->queued.size() + impl->loading + impl->ready.size());
	}

	uint32 AssetStreamer::WorkerCount() const
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		return (uint32)impl->threads.size();
	}

}
