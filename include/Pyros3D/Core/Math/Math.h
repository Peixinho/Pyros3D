//============================================================================
// Name        : Math.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Math
//============================================================================

#ifndef MATH_H
#define	MATH_H

#include <Pyros3D/Other/Export.h>
#include <stdint.h>

namespace p3d {

	// Type Definition
	typedef uint8_t uint8;
	typedef int8_t int8;
	typedef uint16_t uint16;
	typedef int16_t int16;
	typedef int32_t int32;
	typedef uint32_t uint32;
	typedef int64_t int64;
	typedef uint64_t uint64;
	typedef float f32;
	typedef unsigned char uchar;
	typedef double f64;
};

#include <Pyros3D/Core/Math/Vec2.h>
#include <Pyros3D/Core/Math/Vec3.h>
#include <Pyros3D/Core/Math/Vec4.h>
#include <Pyros3D/Core/Math/Matrix.h>
#include <Pyros3D/Core/Math/Quaternion.h>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <math.h>

// Constants
#ifndef NULL
#define NULL 0
#endif
#define EPSILON 1e-8 
#define PI 3.14159265358979323846
#define DEGTORAD(x)	( ((x) * PI) / 180.0 )
#define RADTODEG(x)	( ((x) * 180.0) / PI )

#define SQR(x) ( (x) * (x) )

#define LIMIT_RANGE(low, value, high) { if (value < low)	value = low; else if(value > high) value = high; }

#ifndef INT_MIN
#define INT_MIN -2147483647
#endif
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

// Were function-like macros. A macro named Min/Max is not scoped to
// anything, so it rewrites those identifiers everywhere - including inside
// unrelated third-party headers included afterwards. ImGui's ImRect has
// members literally called Min and Max, so any translation unit that
// included this before imgui_internal.h failed to compile with
// "too few arguments provided to function-like macro invocation", which
// says nothing at all about the actual cause. Templates keep the spelling
// at all 15 call sites while confining them to p3d::Math, which every user
// already pulls in via the using-directive below.
//
// They also evaluate their arguments once each, where the macros evaluated
// one of them twice - Max(i++, n) was a latent bug waiting for a caller.
namespace p3d {
	namespace Math {
		// Two type parameters, returning the common type, because the macros
		// these replace were happily called with mixed int/float arguments
		// (Mouse3D's ray-slab test does exactly that) and the usual
		// arithmetic conversions are the behaviour those call sites expect.
		//
		// The return type is decltype(a + b), NOT decltype(a > b ? a : b),
		// and the difference is not cosmetic. `a` and `b` are lvalues, so the
		// conditional expression is an lvalue too, and that decltype yields
		// `const A&` - a reference to a by-value parameter, dangling the
		// moment the function returns. It compiled, and GL and Vulkan
		// happened to survive it; Metal's codegen reused the stack slot, so
		// ParticleSystem's `sqrtf(Max(0.0f, 1.0f - z*z))` came back holding
		// whatever had last been written there (the adjacent `phi`), every
		// particle was emitted with a garbage direction, and the whole
		// system sat at the origin. decltype(a + b) is a prvalue with the
		// usual arithmetic conversions, which is what was meant all along.
		template <typename A, typename B> inline auto Min(const A a, const B b) -> decltype(a + b) { return a < b ? a : b; }
		template <typename A, typename B> inline auto Max(const A a, const B b) -> decltype(a + b) { return a > b ? a : b; }
		template <typename T> inline T Clamp(const T x) { return (T)Min(Max(x, (T)-1), (T)1); }
	}
}

using namespace p3d::Math;

namespace p3d {

	namespace Math {

		namespace RotationOrder {
			enum {
				XYZ = 0,
				YXZ,
				ZXY,
				ZYX,
				YZX,
				XZY
			};
		};
		template <typename T>
		std::string NumberToString(T Number)
		{
			std::ostringstream ss;
			ss << Number;
			return ss.str();
		}

		// Barrycentric
		const f32 barryCentric(const Vec3 &p1, const Vec3 &p2, const Vec3 &p3, const Vec2 &pos);
	};
};

#endif	/* MATH_H */
