//============================================================================
// Name        : Random.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Random
//============================================================================

#include <Pyros3D/Core/Math/Random.h>
#include <ctime>

namespace p3d {

	namespace Math {

		Random::Random() : state(0)
		{
			Seed((uint32)time(NULL));
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
