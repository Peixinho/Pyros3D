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
    
        class PYROS3D_API Vec4 {
            
            public:

                // vars
                f32 x,y,z,w;

                // methods
                Vec4();
                Vec4(const f32 X, const f32 Y, const f32 Z, const f32 W);

                // f32
                f32 dotProduct(const Vec4 &v) const;

                // Vec4
                Vec4 negate() const;

                f32 magnitude() const;
                f32 magnitudeSQR() const;
                Vec4 Abs() const;
                
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
                static const Vec4 ZERO;
        };
    };

}

#endif	/* VEC4_H */

