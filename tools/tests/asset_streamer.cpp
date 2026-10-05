// AssetStreamer + AssetBundle: priority order, cancellation in every state,
// Pump's budget, nested submits under Flush, and a bundle filled on a loader
// thread handing its pixels to the main thread. No render device needed.
// Run with one loader thread (so ordering is deterministic) and inline:
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/asset_streamer.cpp -o /tmp/asset_streamer \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   PYROS_STREAM_WORKERS=1 /tmp/asset_streamer && PYROS_STREAM_WORKERS=0 /tmp/asset_streamer
//
// Optional argument: a PNG to decode through a bundle.
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Utils/Streaming/AssetBundle.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	fflush(stdout);
	if (!cond) failures++;
}

int main(int argc, char** argv)
{
	AssetStreamer &s = AssetStreamer::Instance();
	const bool threaded = s.WorkerCount() > 0;
	printf("loader threads: %u\n", s.WorkerCount());

	// Work runs off the main thread when there are loaders; finish always on it.
	{
		const std::thread::id mainId = std::this_thread::get_id();
		std::thread::id workId, finishId;
		s.Submit([&] { workId = std::this_thread::get_id(); }, [&] { finishId = std::this_thread::get_id(); });
		s.Flush();
		check(finishId == mainId, "finish runs on the main thread");
		check(threaded ? workId != mainId : workId == mainId, "work runs on a loader thread (inline without one)");
	}

	// Priority: park the single loader on a gate, queue out of order, release.
	{
		std::mutex m;
		std::vector<int> order;
		std::atomic<bool> gate(false);
		s.Submit([&] { while (!gate.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }, nullptr, -100.f);
		if (threaded) std::this_thread::sleep_for(std::chrono::milliseconds(20));
		const int prios[] = { 5, 1, 3, 1, 4 };
		for (int i = 0; i < 5; i++)
		{
			const int tag = i;
			s.Submit([&, tag] { std::lock_guard<std::mutex> l(m); order.push_back(tag); }, nullptr, (f32)prios[i]);
		}
		gate = true;
		s.Flush();
		const std::vector<int> want = { 1, 3, 2, 4, 0 };	// by priority, ties by submission
		// Several loaders race each other, so order is only exact with one.
		if (s.WorkerCount() <= 1) check(order == want, "lower priority first, ties in submission order");
		else printf("SKIP  priority order (needs PYROS_STREAM_WORKERS=1)\n");
	}

	// SetPriority reorders a queued request.
	{
		std::vector<int> order;
		std::atomic<bool> gate(false);
		s.Submit([&] { while (!gate.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }, nullptr, -100.f);
		if (threaded) std::this_thread::sleep_for(std::chrono::milliseconds(20));
		s.Submit(nullptr, [&] { order.push_back(0); }, 1.f);
		const AssetStreamer::Ticket late = s.Submit(nullptr, [&] { order.push_back(1); }, 2.f);
		s.SetPriority(late, 0.f);
		gate = true;
		s.Flush();
		if (s.WorkerCount() <= 1) check(order.size() == 2 && order[0] == 1, "SetPriority moves a queued request ahead");
	}

	// Cancel while queued, while loading, and once ready.
	{
		std::atomic<int> worked(0);
		int finished = 0;
		std::atomic<bool> gate(false);
		std::atomic<bool> started(false);
		const AssetStreamer::Ticket loading = s.Submit([&] { started = true; while (!gate.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); worked++; }, [&] { finished++; }, -1.f);
		if (threaded) while (!started.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		// Every OTHER loader thread is given something to hold as well: with one
		// of them idle, the request below was not queued at all - it was picked
		// up at once, and "cancel a queued request" cancelled one already working
		// (one run in a few hundred).
		std::atomic<int> held(0);
		const uint32 others = s.WorkerCount() > 1 ? s.WorkerCount() - 1 : 0;
		for (uint32 i = 0; i < others; i++)
			s.Submit([&] { held++; while (!gate.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }, nullptr, -1.f);
		while (held.load() < (int)others) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		const AssetStreamer::Ticket queued = s.Submit([&] { worked++; }, [&] { finished++; }, 0.f);
		check(s.Cancel(queued), "cancel a queued request");
		if (threaded)
		{
			check(s.GetState(loading) == AssetStreamer::Loading, "state reports Loading");
			check(s.Cancel(loading), "cancel a loading request");
		}
		gate = true;
		s.Flush();
		check(threaded ? finished == 0 : finished >= 0, "a cancelled request never finishes");
		check(worked.load() <= 1, "a cancelled queued request never works");

		const AssetStreamer::Ticket ready = s.Submit([] {}, [&] { finished++; });
		if (threaded) while (s.GetState(ready) != AssetStreamer::Ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		const int before = finished;
		if (threaded) check(s.Cancel(ready), "cancel a ready request");
		else s.Cancel(ready);
		s.Flush();
		check(finished == before, "a cancelled ready request never finishes");
		check(!s.Cancel(ready) && s.GetState(ready) == AssetStreamer::Unknown, "a gone ticket is Unknown and not cancellable");
		check(s.PendingCount() == 0, "nothing left pending");
	}

	// Both requests' work done (or the streamer has no threads, where
	// Pump does it): what the Pump checks below assume.
	auto waitReady = [&s](const std::vector<AssetStreamer::Ticket> &ts) {
		for (int i = 0; i < 5000; i++)
		{
			bool all = true;
			for (size_t k = 0; k < ts.size(); k++) all = all && s.GetState(ts[k]) == AssetStreamer::Ready;
			if (all) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	};

	// Pump honours the budget: ten 5 ms finishes against a 12 ms budget.
	{
		int finished = 0;
		std::vector<AssetStreamer::Ticket> tickets;
		for (int i = 0; i < 10; i++)
			tickets.push_back(s.Submit([] {}, [&] { finished++; std::this_thread::sleep_for(std::chrono::milliseconds(5)); }));
		// Until the loader has marked all ten Ready - a fixed sleep was a
		// race on a loaded CI machine.
		waitReady(tickets);
		const uint32 ran = s.Pump(12.0);
		check(ran >= 1 && ran <= 4, "Pump stops once the budget is spent");
		check(s.Pump(0.0) == 1, "a zero budget still makes progress");
		s.Flush();
		check(finished == 10, "Flush finishes the rest");
	}

	// A stepped finish resumes across Pumps and holds its place in line.
	{
		int steps = 0;
		bool other = false;
		const AssetStreamer::Ticket a = s.SubmitSteps([] {}, [&]() { return ++steps >= 3; });
		// The line is the order work FINISHED in, and with two loader threads
		// either of two requests submitted together can finish first: `a` is
		// let finish before `b` is so much as submitted. (Submitted together,
		// this passed or failed by which thread won - it failed on Linux CI.)
		waitReady({ a });
		const AssetStreamer::Ticket b = s.Submit([] {}, [&] { other = true; });
		waitReady({ b });
		s.Pump(0.0);
		check(steps == 1 && !other, "one step per zero-budget Pump");
		s.Pump(0.0);
		s.Pump(0.0);
		check(steps == 3 && !other, "the stepped finish completes before the one behind it");
		s.Pump(0.0);
		check(other, "then the next finish runs");
		check(s.PendingCount() == 0, "a completed stepped request is forgotten");
	}

	// A finish that submits more work is drained by the same Flush.
	{
		int depth = 0;
		std::function<void()> chain = [&] {
			depth++;
			if (depth < 5) s.Submit([] {}, chain);
		};
		s.Submit([] {}, chain);
		s.Flush();
		check(depth == 5, "Flush drains work submitted by a finish");
	}

	// A bundle filled on a loader thread; the main thread takes the pixels.
	if (argc > 1)
	{
		const std::string png = argv[1];
		std::shared_ptr<AssetBundle> bundle = std::make_shared<AssetBundle>();
		bool took = false;
		Texture::DecodedImage image;
		s.Submit([bundle, png] { bundle->Fill(std::vector<std::string>(), std::vector<std::string>(1, png), false); },
			[&, bundle, png] {
				AssetBundle::Use use(*bundle);
				took = AssetBundle::TakeImage(png, image);
			});
		s.Flush();
		check(took && image.width > 0 && image.pixels.size() == (size_t)image.width * image.height * 4, "bundle decodes off-thread, main thread takes it");
		check(bundle->ImageCount() == 0 && bundle->ByteSize() == 0, "a taken image leaves the bundle");
		check(!AssetBundle::TakeImage(png, image), "no bundle open, nothing to take");
	}

	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
