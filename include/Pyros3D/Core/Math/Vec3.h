//============================================================================
// Name        : Vec3.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector3
//============================================================================

#ifndef VEC3_H
#define	VEC3_H

#include <Pyros3D/Core/Math/Math.h>
#include <string>

namespace p3d {

	namespace Math {

		class PYROS3D_API Vec3 {
		public:

			// vars
			f32 x, y, z;

			// mthods
			Vec3();
			Vec3(const f32 X, const f32 Y, const f32 Z) : x(X), y(Y), z(Z) {}
			Vec3(const Vec2 vec, const f32 Z) : x(vec.x), y(vec.y), z(Z) {}

			// f32s
			f32 dotProduct(const Vec3 &v) const;
			f32 magnitude() const;
			f32 magnitudeSQR() const;
			f32 distance(const Vec3 &v) const;
			f32 distanceSQR(const Vec3 &v) const;

			// Vec3
			Vec3 cross(const Vec3 &v) const;
			Vec3 normalize() const;
			void normalizeSelf();
			Vec3 negate() const;
			void negateSelf();
			Vec3 Abs() const;

			const Vec2 xy() const { return Vec2(x, y); }

			// Lerp
			Vec3 Lerp(const Vec3 &b, const f32 t) const;

			// toString
			std::string toString() const;

			// operators
			Vec3 operator+(const Vec3 &v) const;
			Vec3 operator-(const Vec3 &v) const;
			Vec3 operator*(const Vec3 &v) const;
			Vec3 operator/(const Vec3 &v) const;
			Vec3 operator+(const f32 f) const;
			Vec3 operator-(const f32 f) const;
			Vec3 operator*(const f32 f) const;
			Vec3 operator/(const f32 f) const;
			void operator+=(const Vec3 &v);
			void operator-=(const Vec3 &v);
			void operator*=(const Vec3 &v);
			void operator/=(const Vec3 &v);
			void operator+=(const f32 f);
			void operator-=(const f32 f);
			void operator*=(const f32 f);
			void operator/=(const f32 f);
			bool operator==(const Vec3 &v) const;
			bool operator!=(const Vec3 &v) const;
			bool operator>(const Vec3 &v) const;
			bool operator>=(const Vec3 &v) const;
			bool operator<(const Vec3 &v) const;
			bool operator<=(const Vec3 &v) const;
			float &operator[](int index);
			float* operator()();
			// Vector UP
			static const Vec3 UP;
			static const Vec3 ZERO;

		};
	};
}

#endif	/* VEC3_H */

