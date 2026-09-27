//============================================================================
// Name        : AssetStreamer.h
// Author      : Duarte Peixinho
// Description : Background loading. A request is two closures: work, run on
//               a loader thread (read files, decode, fill an AssetBundle),
//               and finish, run on the main thread by Pump() once work is
//               done (create the GPU objects, add to the scene). Pump() is
//               given a time budget per frame, so a burst of finished loads
//               spreads over several frames instead of hitching one.
//
//               Loader threads are the streamer's own, at background
//               priority, never the JobSystem's: a decode that takes 80 ms
//               must not be picked up by a physics step helping out while
//               it waits on its own jobs.
//
//               work must follow the JobSystem rules - no Lua, no render
//               device, no scene graph. finish may do anything.
//
//               PYROS_STREAM_WORKERS=<n> sets the thread count (default 2);
//               0, or a web build without threads, runs work inline inside
//               Pump() instead, still one request at a time within budget.
//============================================================================

#ifndef ASSETSTREAMER_H
#define ASSETSTREAMER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <functional>

namespace p3d {

	class PYROS3D_API AssetStreamer
	{
	public:
		typedef uint64 Ticket;
		// Unknown once a request has finished or been cancelled - the
		// streamer forgets it then - and for a ticket it never issued.
		enum State { Unknown, Queued, Loading, Ready };

		static AssetStreamer &Instance();

		// Lower priority runs first; ties run in submission order.
		Ticket Submit(const std::function<void()> &work, const std::function<void()> &finish, const f32 priority = 0.f);

		// finish in slices: Pump() calls step until it returns true, one
		// call per turn against the same budget, so a finish too big for a
		// frame (a cell with a dozen texture uploads) is spread over
		// several. Other finished requests wait behind it - order holds.
		Ticket SubmitSteps(const std::function<void()> &work, const std::function<bool()> &step, const f32 priority = 0.f);

		// Only reorders requests still waiting for a loader thread.
		void SetPriority(const Ticket ticket, const f32 priority);

		// finish will not run. A request already on a loader thread still
		// completes its work, which is then discarded. False when finish
		// has already run, or the ticket is unknown.
		bool Cancel(const Ticket ticket);

		State GetState(const Ticket ticket) const;

		// Main thread, once per frame. Runs finished requests' finish
		// closures (or steps), oldest first, until budgetMs is spent -
		// always at least one, so a budget smaller than any one finish
		// still makes progress. Returns how many calls it made.
		uint32 Pump(const f64 budgetMs);

		// Main thread. Blocks until nothing is queued or loading, then
		// finishes everything - for a loading screen, or a test.
		void Flush();

		// Queued + loading + waiting for Pump.
		uint32 PendingCount() const;
		uint32 WorkerCount() const;

		// Joins the loader threads and drops every pending request without
		// finishing it. Called by the destructor; call it earlier when
		// finish closures hold things that must die before static
		// destruction (anything owning GPU objects).
		void Shutdown();

	private:
		AssetStreamer();
		~AssetStreamer();
		AssetStreamer(const AssetStreamer &) = delete;
		AssetStreamer &operator=(const AssetStreamer &) = delete;

		struct Impl;
		Impl* impl;
	};

}

#endif /* ASSETSTREAMER_H */
