//============================================================================
// Name        : vec2.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : vector2
//============================================================================

#ifndef VEC2_H
#define	VEC2_H

#include <sstream>
#include <math.h>

namespace Fishy3D {

    class vec2 {
        public:

            // vars
            float x,y;

            // methods
            vec2();
            vec2(float X, float Y);

            // floats
            float dotProduct(const vec2 &v) const;
            float magnitude() const;
            float magnitudeSQR() const;
            float distance(const vec2 &v) const;
            float distanceSQR(const vec2 &v) const;

            // vec2
            vec2 normalize() const;
            vec2 negate() const;

            // toString
            std::string toString() const;

            // operators
            vec2 operator+(const vec2 &v) const;
            vec2 operator-(const vec2 &v) const;
            vec2 operator*(const vec2 &v) const;
            vec2 operator/(const vec2 &v) const;
            vec2 operator+(const float &f) const;
            vec2 operator-(const float &f) const;
            vec2 operator*(const float &f) const;
            vec2 operator/(const float &f) const;
            void operator+=(const vec2 &v);
            void operator-=(const vec2 &v);
            void operator*=(const vec2 &v);
            void operator/=(const vec2 &v);
            void operator+=(const float &f);
            void operator-=(const float &f);
            void operator*=(const float &f);
            void operator/=(const float &f);
            bool operator==(const vec2 &v) const;
            bool operator!=(const vec2 &v) const;

            static const vec2 ZERO;
            
    };

}

#endif	/* VEC2_H */

