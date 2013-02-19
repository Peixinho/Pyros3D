//============================================================================
// Name        : vec4.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : vector 4
//============================================================================

#include "vec4.h"

namespace Fishy3D {

vec4::vec4() {
    x = y = z = w = 0;
}

vec4::vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W)
{
}

float vec4::dotProduct(const vec4& v) const
{
    return (x*v.x) + (y*v.y) + (z*v.z) + (w*v.w);
}

vec4 vec4::negate() const
{
    return vec4(-x,-y,-z,-w);
}

vec4 vec4::operator+(const vec4 &v) const
{
    return vec4(x+v.x,y+v.y,z+v.z,w+v.w);
}
vec4 vec4::operator-(const vec4 &v) const
{
    return vec4(x-v.x,y-v.y,z-v.z,w-v.w);
}
vec4 vec4::operator*(const vec4 &v) const
{
    return vec4(x*v.x,y*v.y,z*v.z,w*v.w);
}
vec4 vec4::operator/(const vec4 &v) const
{
    return vec4(x/v.x,y/v.y,z/v.z,w/v.w);
}
vec4 vec4::operator+(const float &f) const
{
    return vec4(x+f,y+f,z+f,w+f);
}
vec4 vec4::operator-(const float &f) const
{
    return vec4(x-f,y-f,z-f,w-f);
}
vec4 vec4::operator*(const float &f) const
{
    return vec4(x*f,y*f,z*f,w*f);
}
vec4 vec4::operator/(const float &f) const
{
    return vec4(x/f,y/f,z/f,w/f);
}
void vec4::operator+=(const vec4 &v)
{
    x+=v.x; y+=v.y; z+=v.z; w+=v.w;
}
void vec4::operator-=(const vec4 &v)
{
    x-=v.x; y-=v.y; z-=v.z; w-=v.w;
}
void vec4::operator*=(const vec4 &v)
{
    x*=v.x; y*=v.y; z*=v.z; w*=v.w;
}
void vec4::operator/=(const vec4 &v)
{
    x/=v.x; y/=v.y; z/=v.z; w/=v.w;
}
void vec4::operator+=(const float &f)
{
    x+=f; y+=f; z+=f; w+=f;
}
void vec4::operator-=(const float &f)
{
    x-=f; y-=f; z-=f; w-=f;
}
void vec4::operator*=(const float &f)
{
    x*=f; y*=f; z*=f; w*=f;
}
void vec4::operator/=(const float &f)
{
    x/=f; y/=f; z/=f; w/=f;
}
bool vec4::operator==(const vec4 &v) const
{
    return (x==v.x && y==v.y && z==v.z && w==v.w);
}
bool vec4::operator!=(const vec4 &v) const
{
    return !(*this==v);
}

std::string vec4::toString() const
{
    std::ostringstream toStr; toStr << "Vector4(" << x << ", " << y << ", " << z << ", " << w << ")";
    return toStr.str();
}

const vec4 vec4::ZERO(0,0,0,0);

}
