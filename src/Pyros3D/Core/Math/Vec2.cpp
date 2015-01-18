//============================================================================
// Name        : Vec2.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector2
//============================================================================
#include <Pyros3D/Core/Math/Vec2.h>

namespace p3d {

    namespace Math {
    
        Vec2::Vec2() {
            x = y = 0;
        }

        Vec2::Vec2(const f32 X, const f32 Y) : x(X), y(Y)
        {
        }

        f32 Vec2::dotProduct(const Vec2& v) const
        {
            return (x*v.x) + (y*v.y);
        }

        f32 Vec2::magnitude() const
        {
            return sqrt(magnitudeSQR());
        }

        f32 Vec2::magnitudeSQR() const
        {
            return x*x + y*y;
        }

        f32 Vec2::distance(const Vec2& v) const
        {
            Vec2 dist = this->operator -(v);
            return dist.magnitude();
        }

        f32 Vec2::distanceSQR(const Vec2& v) const
        {
            Vec2 dist = *this - v;
            return dist.magnitudeSQR();
        }

        Vec2 Vec2::normalize() const
        {
            f32 m = magnitude();
            if (m==0) m=1;
            return Vec2(x/m, y/m);
        }

        Vec2 Vec2::negate() const
        {
            return Vec2(-x,-y);
        }

        Vec2 Vec2::Abs() const
        {
            return Vec2(fabs(x),fabs(y));
        }
        
        Vec2 Vec2::operator+(const Vec2 &v) const
        {
            return Vec2(x+v.x,y+v.y);
        }
        Vec2 Vec2::operator-(const Vec2 &v) const
        {
            return Vec2(x-v.x,y-v.y);
        }
        Vec2 Vec2::operator*(const Vec2 &v) const
        {
            return Vec2(x*v.x,y*v.y);
        }
        Vec2 Vec2::operator/(const Vec2 &v) const
        {
            return Vec2(x/v.x,y/v.y);
        }
        Vec2 Vec2::operator+(const f32 f) const
        {
            return Vec2(x+f,y+f);
        }
        Vec2 Vec2::operator-(const f32 f) const
        {
            return Vec2(x-f,y-f);
        }
        Vec2 Vec2::operator*(const f32 f) const
        {
            return Vec2(x*f,y*f);
        }
        Vec2 Vec2::operator/(const f32 f) const
        {
            return Vec2(x/f,y/f);
        }
        void Vec2::operator+=(const Vec2 &v)
        {
            x+=v.x; y+=v.y;
        }
        void Vec2::operator-=(const Vec2 &v)
        {
            x-=v.x; y-=v.y;
        }
        void Vec2::operator*=(const Vec2 &v)
        {
            x*=v.x; y*=v.y;
        }
        void Vec2::operator/=(const Vec2 &v)
        {
            x/=v.x; y/=v.y;
        }
        void Vec2::operator+=(const f32 f)
        {
            x+=f; y+=f;
        }
        void Vec2::operator-=(const f32 f)
        {
            x-=f; y-=f;
        }
        void Vec2::operator*=(const f32 f)
        {
            x*=f; y*=f;
        }
        void Vec2::operator/=(const f32 f)
        {
            x/=f; y/=f;
        }
        bool Vec2::operator==(const Vec2 &v) const
        {
            return (x==v.x && y==v.y);
        }
        bool Vec2::operator!=(const Vec2 &v) const
        {
            return !(*this==v);
        }
        bool Vec2::operator >(const Vec2& v) const
        {
            return (magnitudeSQR()>v.magnitudeSQR());
        }
        bool Vec2::operator >=(const Vec2& v) const
        {
            return (magnitudeSQR()>=v.magnitudeSQR());
        }
        bool Vec2::operator <(const Vec2& v) const
        {
            return (magnitudeSQR()<v.magnitudeSQR());
        }
        bool Vec2::operator <=(const Vec2& v) const
        {
            return (magnitudeSQR()<=v.magnitudeSQR());
        }
        
        std::string Vec2::toString() const
        {
            std::ostringstream toStr; toStr << "Vector2(" << x << ", " << y << ")";
            return toStr.str();
        }

        const Vec2 Vec2::ZERO(0,0);

    };
};
