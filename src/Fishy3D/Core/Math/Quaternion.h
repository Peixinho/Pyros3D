//============================================================================
// Name        : Quaternion.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Quaternion
//============================================================================

#ifndef QUATERNION_H
#define	QUATERNION_H

#include "Matrix.h"
#include "vec3.h"

namespace Fishy3D {

class Matrix;    
    
class Quaternion {
public:
    // vars
    float w, x, y, z;

    // methods
    Quaternion();
    Quaternion(const float &w, const float &x, const float &y, const float &z);
    Quaternion(const float &x, const float &y, const float &z);
    Quaternion(const vec3 &v, const float angle);
    Matrix ConvertToMatrix() const;
    float Magnitude() const;
    float Dot (const Quaternion& q) const;
    void Normalize();
    void Rotation(const float &xAngle, const float &yAngle, const float &zAngle);
    void EulerToQuaternion(const vec3 &v);
    void AxisToQuaternion(const vec3 &v, const float angle);
    Quaternion Slerp(Quaternion a, Quaternion b, float t) const;
    Quaternion Nlerp(Quaternion a, Quaternion b, float t, bool shortestPath = true) const;
    Quaternion Inverse();
    // operators
    Quaternion operator+(const Quaternion &q) const;
    Quaternion operator-(const Quaternion &q) const;
    Quaternion operator*(const Quaternion &q) const;
    Quaternion operator* (const float &scalar) const;
    Quaternion operator- () const;
    vec3 operator*(const vec3 &v) const;
    void operator*=(const Quaternion &q);
    bool operator==(const Quaternion &q);
    bool operator!=(const Quaternion &q);
    
    std::string toString() const;

};

}

#endif	/* QUATERNION_H */
