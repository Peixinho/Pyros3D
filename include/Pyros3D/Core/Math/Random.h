//============================================================================
// Name        : Random.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Random
//============================================================================

#ifndef RANDOM_H
#define	RANDOM_H

#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	namespace Math {

		// Minimal seedable PRNG (xorshift32) - not cryptographic, just fast
		// and deterministic-if-seeded, for gameplay/VFX randomization (e.g.
		// ParticleSystem's per-particle spawn velocity/rotation/size jitter).
		// No Random/RNG utility existed anywhere in the engine before this -
		// every existing use (e.g. SSAOEffect.cpp's kernel noise) was ad hoc
		// rand()/srand().
		class PYROS3D_API Random {
		public:

			// Seeds from the current time.
			Random();
			explicit Random(const uint32 seed);

			void Seed(const uint32 seed);

			// [0,1)
			f32 NextFloat01();
			// [min,max)
			f32 Range(const f32 min, const f32 max);

		private:

			uint32 state;
		};
	};

}

#endif	/* RANDOM_H */
