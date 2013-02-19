//============================================================================
// Name        : vec2.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : vector2
//============================================================================
#include "vec2.h"

namespace Fishy3D {

vec2::vec2() {
    x = y = 0;
}

vec2::vec2(float X, float Y) : x(X), y(Y)
{
}

float vec2::dotProduct(const vec2& v) const
{
    return (x*v.x) + (y*v.y);
}

float vec2::magnitude() const
{
    return sqrt(magnitudeSQR());
}

float vec2::magnitudeSQR() const
{
    return x*x + y*y;
}

float vec2::distance(const vec2& v) const
{
    vec2 dist = this->operator -(v);
    return dist.magnitude();
}

float vec2::distanceSQR(const vec2& v) const
{
    vec2 dist = *this - v;
    return dist.magnitudeSQR();
}

vec2 vec2::normalize() const
{
    float m = magnitude();
    if (m==0) m=1;
    return vec2(x/m, y/m);
}

vec2 vec2::negate() const
{
    return vec2(-x,-y);
}

vec2 vec2::operator+(const vec2 &v) const
{
    return vec2(x+v.x,y+v.y);
}
vec2 vec2::operator-(const vec2 &v) const
{
    return vec2(x-v.x,y-v.y);
}
vec2 vec2::operator*(const vec2 &v) const
{
    return vec2(x*v.x,y*v.y);
}
vec2 vec2::operator/(const vec2 &v) const
{
    return vec2(x/v.x,y/v.y);
}
vec2 vec2::operator+(const float &f) const
{
    return vec2(x+f,y+f);
}
vec2 vec2::operator-(const float &f) const
{
    return vec2(x-f,y-f);
}
vec2 vec2::operator*(const float &f) const
{
    return vec2(x*f,y*f);
}
vec2 vec2::operator/(const float &f) const
{
    return vec2(x/f,y/f);
}
void vec2::operator+=(const vec2 &v)
{
    x+=v.x; y+=v.y;
}
void vec2::operator-=(const vec2 &v)
{
    x-=v.x; y-=v.y;
}
void vec2::operator*=(const vec2 &v)
{
    x*=v.x; y*=v.y;
}
void vec2::operator/=(const vec2 &v)
{
    x/=v.x; y/=v.y;
}
void vec2::operator+=(const float &f)
{
    x+=f; y+=f;
}
void vec2::operator-=(const float &f)
{
    x-=f; y-=f;
}
void vec2::operator*=(const float &f)
{
    x*=f; y*=f;
}
void vec2::operator/=(const float &f)
{
    x/=f; y/=f;
}
bool vec2::operator==(const vec2 &v) const
{
    return (x==v.x && y==v.y);
}
bool vec2::operator!=(const vec2 &v) const
{
    return !(*this==v);
}

std::string vec2::toString() const
{
    std::ostringstream toStr; toStr << "Vector2(" << x << ", " << y << ")";
    return toStr.str();
}

const vec2 vec2::ZERO(0,0);

}

