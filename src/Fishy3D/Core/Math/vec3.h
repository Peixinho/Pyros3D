//============================================================================
// Name        : vec3.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : vector3
//============================================================================

#ifndef VEC3_H
#define	VEC3_H

#include <sstream>
#include <math.h>

namespace Fishy3D {
    
    class vec3 {
        public:

            // vars
            float x,y,z;

            // mthods
            vec3();
            vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

            // floats
            float dotProduct(const vec3 &v) const;
            float magnitude() const;
            float magnitudeSQR() const;
            float distance(const vec3 &v) const;
            float distanceSQR(const vec3 &v) const;

            // vec3
            vec3 cross(const vec3 &v) const;
            vec3 normalize() const;
            void normalizeSelf();
            vec3 negate() const;

            // toString
            std::string toString() const;

            // operators
            vec3 operator+(const vec3 &v) const;
            vec3 operator-(const vec3 &v) const;
            vec3 operator*(const vec3 &v) const;
            vec3 operator/(const vec3 &v) const;
            vec3 operator+(const float &f) const;
            vec3 operator-(const float &f) const;
            vec3 operator*(const float &f) const;
            vec3 operator/(const float &f) const;
            void operator+=(const vec3 &v);
            void operator-=(const vec3 &v);
            void operator*=(const vec3 &v);
            void operator/=(const vec3 &v);
            void operator+=(const float &f);
            void operator-=(const float &f);
            void operator*=(const float &f);
            void operator/=(const float &f);
            bool operator==(const vec3 &v) const;
            bool operator!=(const vec3 &v) const;

            // vector UP
            static const vec3 UP;
            static const vec3 ZERO;

    };

}

#endif	/* VEC3_H */

