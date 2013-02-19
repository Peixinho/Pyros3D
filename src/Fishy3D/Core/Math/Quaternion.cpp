//============================================================================
// Name        : Quaternion.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Quaternion
//============================================================================

#include <math.h>
#include "Quaternion.h"

namespace Fishy3D {

Quaternion::Quaternion() 
{
    this->w = 1.0f;
    this->x = 0.0f;
    this->y = 0.0f;
    this->z = 0.0f; 
}

Quaternion::Quaternion(const float& w, const float& x, const float& y, const float& z) {

    this->w = w;
    this->x = x;
    this->y = y;
    this->z = z;
        
    Normalize();
}
Quaternion::Quaternion(const float& x, const float& y, const float& z) 
{
    EulerToQuaternion(vec3(x, y ,z));
}
Quaternion::Quaternion(const vec3& v, const float angle)
{
    AxisToQuaternion(v, angle);
}
float Quaternion::Magnitude() const {
    return sqrt(w*w + x*x + y*y + z*z);
}
float Quaternion::Dot (const Quaternion& q) const
{
    return w*q.w+x*q.x+y*q.y+z*q.z;
}
void Quaternion::Normalize() {
    float m = Magnitude();
    w = w/m;
    x = x/m;
    y = y/m;
    z = z/m;
    
    LIMIT_RANGE(-1.0f, w, 1.0f);
    LIMIT_RANGE(-1.0f, x, 1.0f);
    LIMIT_RANGE(-1.0f, y, 1.0f);
    LIMIT_RANGE(-1.0f, z, 1.0f);
    
}

Matrix Quaternion::ConvertToMatrix() const {

    Matrix m;
    m.m[0] = (1-2*(y*y)-2*(z*z));   m.m[4] = (2*x*y - 2*w*z);           m.m[8] = (2*x*z + 2*w*y);
    m.m[1] = (2*x*y + 2*w*z);      m.m[5] = (w*w - x*x + y*y - z*z);  m.m[9] = (2*y*z - 2*w*x);
    m.m[2] = (2*x*z - 2*w*y);       m.m[6] = (2*y*z + 2*w*x);           m.m[10] = (1-2*(x*x) - 2*(y*y));

    return m;

}
Quaternion Quaternion::operator+ (const Quaternion& q) const
{
    return Quaternion(w+q.w,x+q.x,y+q.y,z+q.z);
}

Quaternion Quaternion::operator- (const Quaternion& q) const
{
    return Quaternion(w-q.w,x-q.x,y-q.y,z-q.z);
}
Quaternion Quaternion::operator- () const
{
    return Quaternion(-w,-x,-y,-z);
}
void Quaternion::operator *=(const Quaternion& q) {
    Quaternion quat;
    quat = *this * q;
}

Quaternion Quaternion::operator *(const Quaternion& q) const {

    Quaternion quat;
    quat.w = w*q.w - x*q.x - y*q.y - z*q.z;
    quat.x = w*q.x + x*q.w + y*q.z - z*q.y;
    quat.y = w*q.y + y*q.w + z*q.x - x*q.z;
    quat.z = w*q.z + z*q.w + x*q.y - y*q.x;

    return quat;
}

vec3 Quaternion::operator *(const vec3 &v) const {

    float x = v.x,  y  = v.y,  z  = v.z,
          qx = x, qy = y, qz = z, qw = w;

    // calculate quat * vec

    float ix =  qw * x + qy * z - qz * y,
    iy =  qw * y + qz * x - qx * z,
    iz =  qw * z + qx * y - qy * x,
    iw = -qx * x - qy * y - qz * z;

    // calculate result * inverse quat

    vec3 dest;
    dest.x = ix * qw + iw * -qx + iy * -qz - iz * -qy;
    dest.y = iy * qw + iw * -qy + iz * -qx - ix * -qz;
    dest.z = iz * qw + iw * -qz + ix * -qy - iy * -qx;

    return dest;

}
Quaternion Quaternion::operator* (const float &scalar) const
{
    return Quaternion(scalar*w,scalar*x,scalar*y,scalar*z);
}
bool Quaternion::operator ==(const Quaternion& q)
{
    return (this->w == q.w && this->x == q.x && this->y == q.y && this->z == q.z);
}
bool Quaternion::operator !=(const Quaternion& q) {
    return (this->w != q.w || this->x != q.x || this->y != q.y || this->z != q.z);
}
void Quaternion::EulerToQuaternion(const vec3& v)
{
    double	ex, ey, ez; // temp half euler angles
    double	cr, cp, cy, sr, sp, sy, cpcy, spsy;// temp vars in roll,pitch yaw

    ex = DEGTORAD(x) * 0.5f; // convert to rads and half them
    ey = DEGTORAD(y) * 0.5f;
    ez = DEGTORAD(z) * 0.5f;

    cr = cosf(ex);
    cp = cosf(ey);
    cy = cosf(ez);

    sr = sinf(ex);
    sp = sinf(ey);
    sy = sinf(ez);

    cpcy = cp * cy;
    spsy = sp * sy;

    this->w = float(cr * cpcy + sr * spsy);

    this->x = float(sr * cpcy - cr * spsy);
    this->y = float(cr * sp * cy + sr * cp * sy);
    this->z = float(cr * cp * sy - sr * sp * cy);

    Normalize();
}
void Quaternion::AxisToQuaternion(const vec3 &v, const float angle)
{
    float x,y,z; // temp vars of vector
    double rad, scale; // temp vars

    if (v == vec3(0,0,0)) // if axis is zero, then return quaternion (1,0,0,0)
    {
            w	= 1.0f;
            x	= 0.0f;
            y	= 0.0f;
            z	= 0.0f;            
    }

    rad = angle * 0.5f;

    w = (float)cosf(rad);

    scale = sinf(rad);

    x = v.x;
    y = v.y;
    z = v.z;

    this->x = float(x * scale);
    this->y = float(y * scale);
    this->z = float(z * scale);

    Normalize(); // make sure a unit quaternion turns up
}
void Quaternion::Rotation(const float& xAngle, const float& yAngle, const float& zAngle) {

    Quaternion Qx, Qy, Qz;
    Qx = Quaternion(cos(xAngle/2), (sin(xAngle/2)), 0, 0);
    Qy = Quaternion(cos(yAngle/2), 0, sin(yAngle/2), 0);
    Qz = Quaternion(cos(zAngle/2), 0, 0, sin(zAngle/2));

    *this = Qx * Qy * Qz;
}

Quaternion Quaternion::Slerp(Quaternion a, Quaternion b, float t) const
{
    Quaternion m;
    
    float cosHalfTheta = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;

    if ( fabs( cosHalfTheta ) >= 1.0 ) {

            m.w = a.w; m.x = a.x; m.y = a.y; m.z = a.z;
            return m;

    }

    float halfTheta = (float)(acos( cosHalfTheta )), sinHalfTheta = (float)(sqrt( 1.0 - cosHalfTheta * cosHalfTheta ));

    if ( fabs( sinHalfTheta ) < 0.001 ) {

            m.w = (float)(0.5 * ( a.w + b.w ));
            m.x = (float)(0.5 * ( a.x + b.x ));
            m.y = (float)(0.5 * ( a.y + b.y ));
            m.z = (float)(0.5 * ( a.z + b.z ));

            return m;

    }

    float ratioA = sin( ( 1 - t ) * halfTheta ) / sinHalfTheta,
    ratioB = sin( t * halfTheta ) / sinHalfTheta;

    m.w = ( a.w * ratioA + b.w * ratioB );
    m.x = ( a.x * ratioA + b.x * ratioB );
    m.y = ( a.y * ratioA + b.y * ratioB );
    m.z = ( a.z * ratioA + b.z * ratioB );

    return m;

}
Quaternion Quaternion::Nlerp(Quaternion a, Quaternion b, float t, bool shortestPath) const
{
    Quaternion m;
       
    float fCos = a.Dot(b);
    if (fCos<0.0f && shortestPath) {
        m = a + (((-b) - a)*t);
    } else {
        m = a + ((b - a)*t);
    }

    return m;

}

Quaternion Quaternion::Inverse()
{
    Quaternion inverseQuat = *this;
    inverseQuat.x *= -1;
    inverseQuat.y *= -1;
    inverseQuat.z *= -1;

    return inverseQuat;
}

std::string Quaternion::toString() const
{
       std::ostringstream toStr;

       toStr << "| w: " << w << " | x: " << x << " | y: " << y << " | z: " << z << " |";

       return "Quaternion \n" + toStr.str();   
}

}
