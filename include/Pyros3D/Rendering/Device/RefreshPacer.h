//============================================================================
// Name        : RefreshPacer.h
// Author      : Duarte Peixinho
// Description : Frames begun on the display's refresh, on every backend and
//               system. Begun when the window system hands a drawable back
//               (Metal's nextDrawable, a Vulkan acquire, a GL swap), they came
//               unevenly - two at once now and then - so a still scene's
//               frames went 33 ms, 0.8, then 16.7 apart though the display
//               showed one every 16.7: a zig-zag in the profiler's graph, and
//               each frame's time step with it. Uncapped (IMMEDIATE, no GL
//               swap interval) the frames came as fast as they could, never
//               the same twice. Begun on the refresh, a frame waits for the
//               next one - unless one came since the last frame began, then
//               it goes at once (a frame that ran long loses nothing more).
//
//               Where the refreshes come from: a Mac's display link; Windows'
//               vertical blank (DXGI) on the monitor the window is on; else a
//               steady clock at the display's rate (SetRefreshRate).
//
//               PYROS_FRAME_PACE=0, or SetEnabled(false) - a game's explicit
//               frame-rate limit, a benchmark wanting every frame there is:
//               frames as fast as they come, as they were.
//============================================================================

#ifndef REFRESHPACER_H
#define REFRESHPACER_H

#include <Pyros3D/Other/Export.h>

namespace p3d {

	class PYROS3D_API RefreshPacer {
	public:
		// Called by the render devices as a frame begins. layer: a Mac's CAMetalLayer, when the device
		// draws into one (it says which display the window is on). The milliseconds waited (0 when
		// pacing is off) - waiting for the display, not the frame's work.
		static double Wait(void* layer = 0);
		static void SetEnabled(const bool on);
		// This thread never waits (Wait returns 0): the device thread's, whose frames are begun by the
		// thread that hands them over - the frame the profiler measures is that thread's, and paced on
		// the device thread the two came apart again (14-21 ms frames on the main thread).
		static void NotOnThisThread();
		static bool IsEnabled();
		// The display's refresh rate, where the system has no refresh to wait on (Linux): told by the
		// window context, as the window opens and as it moves to another display. 0: 60.
		static void SetRefreshRate(const double hz);
	};

}

#endif /* REFRESHPACER_H */
