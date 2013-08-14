//============================================================================
// Name        : Vec3.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector3
//============================================================================

#ifndef Vec3_H
#define	Vec3_H

#include "Math.h"
#include <string>

namespace p3d {
    
    namespace Math {
        
        class Vec3 {
            public:

                // vars
                f32 x,y,z;

                // mthods
                Vec3();
                Vec3(f32 X, f32 Y, f32 Z) : x(X), y(Y), z(Z) {}

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
                
                // toString
                std::string toString() const;

                // operators
                Vec3 operator+(const Vec3 &v) const;
                Vec3 operator-(const Vec3 &v) const;
                Vec3 operator*(const Vec3 &v) const;
                Vec3 operator/(const Vec3 &v) const;
                Vec3 operator+(const f32 &f) const;
                Vec3 operator-(const f32 &f) const;
                Vec3 operator*(const f32 &f) const;
                Vec3 operator/(const f32 &f) const;
                void operator+=(const Vec3 &v);
                void operator-=(const Vec3 &v);
                void operator*=(const Vec3 &v);
                void operator/=(const Vec3 &v);
                void operator+=(const f32 &f);
                void operator-=(const f32 &f);
                void operator*=(const f32 &f);
                void operator/=(const f32 &f);
                bool operator==(const Vec3 &v) const;
                bool operator!=(const Vec3 &v) const;
                bool operator>(const Vec3 &v) const;
                bool operator>=(const Vec3 &v) const;
                bool operator<(const Vec3 &v) const;
                bool operator<=(const Vec3 &v) const;

                // Vector UP
                static const Vec3 UP;
                static const Vec3 ZERO;

        };
    };
}

#endif	/* Vec3_H */

