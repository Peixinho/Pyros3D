//============================================================================
// Name        : LoadProgress.cpp
// Author      : Duarte Peixinho
// Description : See LoadProgress.h.
//============================================================================

#include <Pyros3D/Utils/LoadProgress/LoadProgress.h>

namespace p3d {

	namespace LoadProgress {

		namespace {
			Handler handler;
			float from = 0.f, to = 1.f, current = 0.f;
			std::string stage;
			bool reporting = false;           // (a handler that loads something itself must not be called back into)
		}

		void SetHandler(const Handler &h) { handler = h; from = 0.f; to = 1.f; current = 0.f; stage.clear(); }
		void ClearHandler() { handler = Handler(); }
		bool HasHandler() { return (bool)handler; }

		void Phase(const float a, const float b, const std::string &name)
		{
			from = a; to = b < a ? a : b; stage = name;
			Report(0.f);
		}

		void Report(const float within)
		{
			if (!handler || reporting) return;
			const float w = within < 0.f ? 0.f : (within > 1.f ? 1.f : within);
			const float f = from + (to - from) * w;
			if (f > current) current = f;               // (a bar never runs backwards)
			reporting = true;
			handler(current, stage);
			reporting = false;
		}

		float Current() { return current; }
	}

}
