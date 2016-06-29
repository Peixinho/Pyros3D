//============================================================================
// Name        : Vec4.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector4
//============================================================================

#ifndef VEC4_H
#define	VEC4_H

#include <Pyros3D/Core/Math/Math.h>
#include <string>

namespace p3d {

    namespace Math {
    
		class Vec2;
		class Vec3;

        class PYROS3D_API Vec4 {
            
            public:

                // vars
                f32 x,y,z,w;

                // methods
                Vec4();
                Vec4(const f32 X, const f32 Y, const f32 Z, const f32 W) : x(X), y(Y), z(Z), w(W) {}
				Vec4(const Vec2 vec, const f32 Z, const f32 W) : x(vec.x), y(vec.y), z(Z), w(W) {}
				Vec4(const Vec3 vec, const f32 W) : x(vec.x), y(vec.y), z(vec.z), w(W) {}

                // f32
                f32 dotProduct(const Vec4 &v) const;

                // Vec4
                Vec4 negate() const;

                f32 magnitude() const;
                f32 magnitudeSQR() const;
                Vec4 Abs() const;

				const Vec2 xy() const { return Vec2(x, y); }
				const Vec3 xyz() const { return Vec3(x, y, z); }

                // toString
                std::string toString() const;

                // operators
                Vec4 operator+(const Vec4 &v) const;
                Vec4 operator-(const Vec4 &v) const;
                Vec4 operator*(const Vec4 &v) const;
                Vec4 operator/(const Vec4 &v) const;
                Vec4 operator+(const f32 f) const;
                Vec4 operator-(const f32 f) const;
                Vec4 operator*(const f32 f) const;
                Vec4 operator/(const f32 f) const;
                void operator+=(const Vec4 &v);
                void operator-=(const Vec4 &v);
                void operator*=(const Vec4 &v);
                void operator/=(const Vec4 &v);
                void operator+=(const f32 f);
                void operator-=(const f32 f);
                void operator*=(const f32 f);
                void operator/=(const f32 f);
                bool operator==(const Vec4 &v) const;
                bool operator!=(const Vec4 &v) const;
                bool operator>(const Vec4 &v) const;
                bool operator>=(const Vec4 &v) const;
                bool operator<(const Vec4 &v) const;
                bool operator<=(const Vec4 &v) const;
				float &operator[](int index);
				float* operator()();
                static const Vec4 ZERO;
        };
    };

}

#endif	/* VEC4_H */

