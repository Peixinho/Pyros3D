//============================================================================
// Name        : Random.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Random
//============================================================================

#include <atomic>
#include <Pyros3D/Core/Math/Random.h>
#include <ctime>

namespace p3d {

	namespace Math {

		namespace { std::atomic<uint32> g_defaultSeed(0), g_made(0); }

		void Random::SetDefaultSeed(const uint32 seed) { g_defaultSeed = seed; g_made = 0; }

		Random::Random() : state(0)
		{
			const uint32 fixed = g_defaultSeed.load();
			Seed(fixed != 0 ? fixed + g_made.fetch_add(1) * 2654435761u : (uint32)time(NULL));
		}

		Random::Random(const uint32 seed) : state(0)
		{
			Seed(seed);
		}

		void Random::Seed(const uint32 seed)
		{
			// xorshift32 requires a non-zero state
			state = (seed == 0) ? 1u : seed;
		}

		f32 Random::NextFloat01()
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return (f32)state / (f32)4294967295u;
		}

		f32 Random::Range(const f32 min, const f32 max)
		{
			return min + (max - min) * NextFloat01();
		}
	};

}
