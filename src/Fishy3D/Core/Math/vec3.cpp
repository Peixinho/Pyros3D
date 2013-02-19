//============================================================================
// Name        : vec3.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : vector3
//============================================================================

#include "vec3.h"

namespace Fishy3D {

vec3::vec3() {
    x = y = z = 0;
}

float vec3::dotProduct(const vec3& v) const
{
    return (x*v.x) + (y*v.y) + (z*v.z);
}

float vec3::magnitude() const
{
    return sqrt(magnitudeSQR());
}

float vec3::magnitudeSQR() const
{
    return x*x + y*y + z*z;
}

float vec3::distance(const vec3& v) const
{
    vec3 dist = this->operator -(v);
    return dist.magnitude();
}

float vec3::distanceSQR(const vec3& v) const
{
    vec3 dist = *this - v;
    return dist.magnitudeSQR();
}

vec3 vec3::cross(const vec3& v) const
{
    vec3 vcross;
    vcross.x = y*v.z-z*v.y;
    vcross.y = z*v.x-x*v.z;
    vcross.z = x*v.y-y*v.x;
    return vcross;
}

vec3 vec3::normalize() const
{
    float m = magnitude();
    if (m==0) m=1;
    return vec3(x/m, y/m, z/m);
}

void vec3::normalizeSelf()
{
    float m = magnitude();
    if (m==0) m=1;
    x/=m, y/=m, z/=m;
}

vec3 vec3::negate() const
{
    return vec3(-x,-y,-z);
}

vec3 vec3::operator+(const vec3 &v) const
{
    return vec3(x+v.x,y+v.y,z+v.z);
}
vec3 vec3::operator-(const vec3 &v) const
{
    return vec3(x-v.x,y-v.y,z-v.z);
}
vec3 vec3::operator*(const vec3 &v) const
{
    return vec3(x*v.x,y*v.y,z*v.z);
}
vec3 vec3::operator/(const vec3 &v) const
{
    return vec3(x/v.x,y/v.y,z/v.z);
}
vec3 vec3::operator+(const float &f) const
{
    return vec3(x+f,y+f,z+f);
}
vec3 vec3::operator-(const float &f) const
{
    return vec3(x-f,y-f,z-f);
}
vec3 vec3::operator*(const float &f) const
{
    return vec3(x*f,y*f,z*f);
}
vec3 vec3::operator/(const float &f) const
{
    return vec3(x/f,y/f,z/f);
}
void vec3::operator+=(const vec3 &v)
{
    x+=v.x; y+=v.y; z+=v.z;
}
void vec3::operator-=(const vec3 &v)
{
    x-=v.x; y-=v.y; z-=v.z;
}
void vec3::operator*=(const vec3 &v)
{
    x*=v.x; y*=v.y; z*=v.z;
}
void vec3::operator/=(const vec3 &v)
{
    x/=v.x; y/=v.y; z/=v.z;
}
void vec3::operator+=(const float &f)
{
    x+=f; y+=f; z+=f;
}
void vec3::operator-=(const float &f)
{
    x-=f; y-=f; z-=f;
}
void vec3::operator*=(const float &f)
{
    x*=f; y*=f; z*=f;
}
void vec3::operator/=(const float &f)
{
    x/=f; y/=f; z/=f;
}
bool vec3::operator==(const vec3 &v) const
{
    return (x==v.x && y==v.y && z==v.z);
}
bool vec3::operator!=(const vec3 &v) const
{
    return !(*this==v);
}

std::string vec3::toString() const
{
    std::ostringstream toStr; toStr << "Vector3(" << x << ", " << y << ", " << z << ")";
    return toStr.str();
}

const vec3 vec3::UP(0,1,0);
const vec3 vec3::ZERO(0,0,0);

}
