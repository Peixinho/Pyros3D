//============================================================================
// Name        : FrameClock.h
// Description : The time of the frame being built - one value for the whole
//               frame, and steady from one frame to the next.
//============================================================================

#ifndef FRAMECLOCK_H
#define	FRAMECLOCK_H

#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	// A window context ticks this once a frame with the best clock it has
	// (seconds, as fine as it can get them) and hands Seconds() to whoever
	// asks what time it is.
	//
	// Two things were wrong with asking the OS clock directly. It was read
	// in whole milliseconds, so at 60 Hz a frame was "16" or "17" ms long;
	// and it was read wherever it was asked for, so two readings in one
	// frame differed, and the step between frames depended on when in the
	// frame the code ran - 11 ms one frame, 22 the next, on a display that
	// showed every frame for exactly 16.7. Everything that moves by speed
	// times time lurched.
	//
	// The frame's time is latched once, and the step is the average of the
	// last few real steps: a display is regular even when the moment a frame
	// is measured is not. The clock is pulled gently towards the real one so
	// the averaging never lets it drift, and after a stall (a load, a
	// breakpoint) it jumps straight to the real time rather than smearing
	// the stall over the frames that follow.
	class FrameClock {
	public:
		FrameClock() : started(false), origin(0.0), raw(0.0), paced(0.0), count(0), next(0) {}

		void Tick(const f64 nowSeconds)
		{
			if (!started) { started = true; origin = raw = paced = nowSeconds; return; }
			const f64 step = nowSeconds - raw;
			raw = nowSeconds;
			if (step <= 0.0) return;
			if (step > 0.1) { paced = nowSeconds; count = next = 0; return; }
			steps[next] = step;
			next = (next + 1) % Window;
			if (count < Window) count++;
			f64 sum = 0.0;
			for (int i = 0; i < count; i++) sum += steps[i];
			paced += sum / count;
			paced += (nowSeconds - paced) * 0.08;
			if (paced > nowSeconds) paced = nowSeconds;      // never ahead of the real clock
		}
		// Counted from the first tick, not from whenever the clock that feeds
		// this one began. A performance counter starts when the machine
		// boots: weeks later it reads in the millions of seconds, and much
		// of what takes the time takes it as a 32-bit float, which that far
		// out moves in quarter-second steps - skeletal animation ran at four
		// poses a second on a machine that had been up for 47 days.
		f64 Seconds() const { return paced - origin; }

	private:
		enum { Window = 10 };
		bool started;
		f64 origin, raw, paced;
		f64 steps[Window];
		int count, next;
	};
};

#endif	/* FRAMECLOCK_H */
