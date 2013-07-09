//============================================================================
// Name        : Quaternion.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Quaternion
//============================================================================

#ifndef QUATERNION_H
#define	QUATERNION_H

#include "Math.h"
#include "Vec3.h"
#include <string>

namespace p3d {

    namespace Math {
    
        class Matrix;    

        class Quaternion {
        public:
            // vars
            f32 w, x, y, z;

            // methods
            Quaternion();
            Quaternion(const f32 &w, const f32 &x, const f32 &y, const f32 &z);
            Quaternion(const f32 &x, const f32 &y, const f32 &z);
            Quaternion(const Vec3 &v, const f32 angle);
            Matrix ConvertToMatrix() const;
            f32 Magnitude() const;
            f32 Dot (const Quaternion& q) const;
            void Normalize();
            void Rotation(const f32 &xAngle, const f32 &yAngle, const f32 &zAngle);
            void SetRotationFromEuler(const Vec3 &rotation, const uint32 &order = 0);
            Vec3 GetEulerFromQuaternion(const uint32 &order = 0);
            void AxisToQuaternion(const Vec3 &v, const f32 angle);
            Quaternion Slerp(Quaternion a, Quaternion b, f32 t) const;
            Quaternion Nlerp(Quaternion a, Quaternion b, f32 t, bool shortestPath = true) const;
            Quaternion Inverse();
            // operators
            Quaternion operator+(const Quaternion &q) const;
            Quaternion operator-(const Quaternion &q) const;
            Quaternion operator*(const Quaternion &q) const;
            Quaternion operator* (const f32 &scalar) const;
            Quaternion operator- () const;
            Vec3 operator*(const Vec3 &v) const;
            void operator*=(const Quaternion &q);
            bool operator==(const Quaternion &q);
            bool operator!=(const Quaternion &q);

            std::string toString() const;

        };
    };
}

#endif	/* QUATERNION_H */
