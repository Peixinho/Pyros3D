//============================================================================
// Name        : RefreshPacer.cpp
// Author      : Duarte Peixinho
// Description : Frames begun on the display's refresh - see RefreshPacer.h.
//============================================================================

#include <Pyros3D/Rendering/Device/RefreshPacer.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && !TARGET_OS_IPHONE
extern "C" double PyrosAppleWaitForRefresh(void* layer);
#endif

namespace p3d {

	namespace {

		std::atomic<bool> g_enabled(true);
		thread_local bool t_notHere = false;
		std::atomic<double> g_refreshHz(0.0);

		bool PacingAsked()
		{
			static const bool off = []() {
				const char* a = std::getenv("PYROS_FRAME_PACE");
				const char* b = std::getenv("PYROS_METAL_PACE");
				return (a && a[0] == '0') || (b && b[0] == '0');
			}();
			return !off && g_enabled.load();
		}

#if !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
		// The refreshes, counted by a thread of their own (Windows' vertical blank): each one a count a frame can take
		struct Ticks {
			std::mutex m;
			std::condition_variable cv;
			unsigned pending = 0;
			void Tick() { { std::lock_guard<std::mutex> l(m); pending++; } cv.notify_one(); }
			// one tick, not waiting past a limit; then the rest are dropped (they are refreshes already past)
			void Take(const std::chrono::milliseconds limit)
			{
				std::unique_lock<std::mutex> l(m);
				cv.wait_for(l, limit, [this]() { return pending > 0; });
				pending = 0;
			}
		};
		Ticks g_ticks;
		std::once_flag g_started;
#endif

		// Where the system has no refresh to wait on: a steady clock at the display's rate, slept to by
		// the thread that begins the frames (a thread of its own ticking it was late waking that one, by
		// milliseconds at a time). The ticks are where they would be - a frame past its tick goes at
		// once, and the next is the first tick after it.
		std::atomic<bool> g_useClock(false);
		double ClockWait()
		{
			static std::chrono::steady_clock::time_point next;
			static bool said = false;
			const double hz = g_refreshHz.load() >= 20.0 ? g_refreshHz.load() : 60.0;
			if (!said) { said = true; std::fprintf(stderr, "[pacer] frames begun on a clock at the display's rate (%.0f Hz)\n", hz); }
			const std::chrono::steady_clock::duration period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / hz));
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			if (next.time_since_epoch().count() == 0 || next - t0 > period * 2) next = t0;   // (first, or the rate changed)
			if (t0 < next)
			{
				// (sleep to within a millisecond, then the rest in short naps: sleeps overshoot)
				if (next - t0 > std::chrono::milliseconds(2)) std::this_thread::sleep_until(next - std::chrono::milliseconds(1));
				while (std::chrono::steady_clock::now() < next) std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			while (next <= now) next += period;
			return std::chrono::duration<double, std::milli>(now - t0).count();
		}

#if defined(_WIN32)
		// Our process's biggest visible window: the one the game is drawn in
		struct Biggest { DWORD pid; HWND hwnd; LONG area; };
		BOOL CALLBACK FindBiggest(HWND hwnd, LPARAM p)
		{
			Biggest* b = (Biggest*)p;
			DWORD pid = 0;
			GetWindowThreadProcessId(hwnd, &pid);
			RECT r;
			if (pid == b->pid && IsWindowVisible(hwnd) && GetWindowRect(hwnd, &r))
			{
				const LONG area = (r.right - r.left) * (r.bottom - r.top);
				if (area > b->area) { b->area = area; b->hwnd = hwnd; }
			}
			return TRUE;
		}

		// The vertical blank of the monitor the window is on (looked at again every second: it moves)
		void VBlankTicks()
		{
			IDXGIFactory1* factory = NULL;
			if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) || factory == NULL) { g_useClock.store(true); g_ticks.Tick(); return; }
			IDXGIOutput* output = NULL;
			HMONITOR outputMonitor = NULL;
			std::chrono::steady_clock::time_point looked;
			unsigned failures = 0;
			for (;;)
			{
				const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
				if (output == NULL || now - looked > std::chrono::seconds(1))
				{
					looked = now;
					Biggest b = { GetCurrentProcessId(), NULL, 0 };
					EnumWindows(FindBiggest, (LPARAM)&b);
					const HMONITOR monitor = b.hwnd ? MonitorFromWindow(b.hwnd, MONITOR_DEFAULTTOPRIMARY) : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
					if (output == NULL || monitor != outputMonitor)
					{
						IDXGIOutput* found = NULL;
						IDXGIAdapter1* adapter = NULL;
						for (UINT a = 0; found == NULL && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; a++)
						{
							IDXGIOutput* o = NULL;
							for (UINT i = 0; found == NULL && adapter->EnumOutputs(i, &o) != DXGI_ERROR_NOT_FOUND; i++)
							{
								DXGI_OUTPUT_DESC desc;
								if (SUCCEEDED(o->GetDesc(&desc)) && desc.Monitor == monitor) { found = o; std::fprintf(stderr, "[pacer] frames begun on the vertical blank of %ls\n", desc.DeviceName); }
								else o->Release();
							}
							adapter->Release();
						}
						if (found != NULL) { if (output) output->Release(); output = found; outputMonitor = monitor; }
					}
				}
				if (output != NULL && SUCCEEDED(output->WaitForVBlank())) { failures = 0; g_ticks.Tick(); continue; }
				// (no output, or it will not wait - a remote session, a display asleep: the clock instead)
				if (++failures > 30) { std::fprintf(stderr, "[pacer] no vertical blank to wait on\n"); if (output) output->Release(); factory->Release(); g_useClock.store(true); g_ticks.Tick(); return; }
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
				g_ticks.Tick();
			}
		}
#endif
#endif
	}

	double RefreshPacer::Wait(void* layer)
	{
		if (t_notHere || !PacingAsked()) return 0.0;
		// (PYROS_FRAME_PACE=clock: the steady clock wherever this runs - Linux's way, to try it elsewhere)
		static const bool clock = []() { const char* v = std::getenv("PYROS_FRAME_PACE"); return v != NULL && v[0] == 'c'; }();
#if defined(__APPLE__) && !TARGET_OS_IPHONE
		if (!clock) return PyrosAppleWaitForRefresh(layer);
#endif
#if defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
		// (the browser and iOS pace frames themselves)
		(void)layer; (void)clock;
		return 0.0;
#else
		(void)layer;
#if defined(_WIN32)
		if (!clock && !g_useClock.load())
		{
			std::call_once(g_started, []() { std::thread(VBlankTicks).detach(); });
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			// (not forever: a refresh not come in 50 ms is not coming - asleep, unplugged)
			g_ticks.Take(std::chrono::milliseconds(50));
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		}
#endif
		return ClockWait();
#endif
	}

	void RefreshPacer::SetEnabled(const bool on) { g_enabled.store(on); }
	void RefreshPacer::NotOnThisThread() { t_notHere = true; }
	bool RefreshPacer::IsEnabled() { return PacingAsked(); }
	void RefreshPacer::SetRefreshRate(const double hz) { g_refreshHz.store(hz); }

}
