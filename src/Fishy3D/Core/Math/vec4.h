//============================================================================
// Name        : vec4.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : vector4
//============================================================================

#ifndef VEC4_H
#define	VEC4_H

#include <sstream>
#include <math.h>

namespace Fishy3D {

    class vec4 {
        public:

            // vars
            float x,y,z,w;

            // methods
            vec4();
            vec4(float X, float Y, float Z, float W);

            // float
            float dotProduct(const vec4 &v) const;

            // vec4
            vec4 negate() const;

            // toString
            std::string toString() const;

            // operators
            vec4 operator+(const vec4 &v) const;
            vec4 operator-(const vec4 &v) const;
            vec4 operator*(const vec4 &v) const;
            vec4 operator/(const vec4 &v) const;
            vec4 operator+(const float &f) const;
            vec4 operator-(const float &f) const;
            vec4 operator*(const float &f) const;
            vec4 operator/(const float &f) const;
            void operator+=(const vec4 &v);
            void operator-=(const vec4 &v);
            void operator*=(const vec4 &v);
            void operator/=(const vec4 &v);
            void operator+=(const float &f);
            void operator-=(const float &f);
            void operator*=(const float &f);
            void operator/=(const float &f);
            bool operator==(const vec4 &v) const;
            bool operator!=(const vec4 &v) const;

            
            static const vec4 ZERO;
    };

}

#endif	/* VEC4_H */

