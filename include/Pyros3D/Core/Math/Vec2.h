//============================================================================
// Name        : Vec2.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector2
//============================================================================

#ifndef VEC2_H
#define	VEC2_H

#include "Math.h"
#include <string>

namespace p3d {

    namespace Math {
    
        class PYROS3D_API Vec2 {
            public:

                // vars
                f32 x,y;

                // methods
                Vec2();
                Vec2(f32 X, f32 Y);

                // f32s
                f32 dotProduct(const Vec2 &v) const;
                f32 magnitude() const;
                f32 magnitudeSQR() const;
                f32 distance(const Vec2 &v) const;
                f32 distanceSQR(const Vec2 &v) const;

                // Vec2
                Vec2 normalize() const;
                Vec2 negate() const;
                Vec2 Abs() const;
                
                // toString
                std::string toString() const;

                // operators
                Vec2 operator+(const Vec2 &v) const;
                Vec2 operator-(const Vec2 &v) const;
                Vec2 operator*(const Vec2 &v) const;
                Vec2 operator/(const Vec2 &v) const;
                Vec2 operator+(const f32 &f) const;
                Vec2 operator-(const f32 &f) const;
                Vec2 operator*(const f32 &f) const;
                Vec2 operator/(const f32 &f) const;
                void operator+=(const Vec2 &v);
                void operator-=(const Vec2 &v);
                void operator*=(const Vec2 &v);
                void operator/=(const Vec2 &v);
                void operator+=(const f32 &f);
                void operator-=(const f32 &f);
                void operator*=(const f32 &f);
                void operator/=(const f32 &f);
                bool operator==(const Vec2 &v) const;
                bool operator!=(const Vec2 &v) const;
                bool operator>(const Vec2 &v) const;
                bool operator>=(const Vec2 &v) const;
                bool operator<(const Vec2 &v) const;
                bool operator<=(const Vec2 &v) const;
                
                static const Vec2 ZERO;

        };
    };
};

#endif	/* VEC2_H */

