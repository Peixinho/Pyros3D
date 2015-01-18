//============================================================================
// Name        : Vec3.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector3
//============================================================================

#include <Pyros3D/Core/Math/Vec3.h>

namespace p3d {
    
    namespace Math {

        Vec3::Vec3() {
            x = y = z = 0;
        }

        f32 Vec3::dotProduct(const Vec3& v) const
        {
            return (x*v.x) + (y*v.y) + (z*v.z);
        }

        f32 Vec3::magnitude() const
        {
            return sqrt(magnitudeSQR());
        }

        f32 Vec3::magnitudeSQR() const
        {
            return x*x + y*y + z*z;
        }

        f32 Vec3::distance(const Vec3& v) const
        {
            Vec3 dist = *this -v;
            return dist.magnitude();
        }

        f32 Vec3::distanceSQR(const Vec3& v) const
        {
            Vec3 dist = *this - v;
            return dist.magnitudeSQR();
        }

        Vec3 Vec3::Abs() const
        {
            return Vec3(fabs(x),fabs(y),fabs(z));
        }
        
        Vec3 Vec3::cross(const Vec3& v) const
        {
            Vec3 vcross;
            vcross.x = y*v.z-z*v.y;
            vcross.y = z*v.x-x*v.z;
            vcross.z = x*v.y-y*v.x;
            
            // avoid -0 value
            vcross.x=vcross.x==-0?0:vcross.x;
            vcross.y=vcross.y==-0?0:vcross.y;
            vcross.z=vcross.z==-0?0:vcross.z;
            
            return vcross;
        }

        Vec3 Vec3::normalize() const
        {
            f32 m = magnitude();
            if (m==0) m=1;
            return Vec3(x/m, y/m, z/m);
        }

        void Vec3::normalizeSelf()
        {
            f32 m = magnitude();
            if (m==0) m=1;
            x/=m, y/=m, z/=m;
        }

        void Vec3::negateSelf()
        {
            x*=-1.f;y*=-1.f;z*=-1.f;
        }
        Vec3 Vec3::negate() const
        {
            return Vec3(-x,-y,-z);
        }        

        Vec3 Vec3::Lerp(const Vec3 &b, const f32 t) const
        {
            Vec3 v;
            v.x = x + ( b.x - x ) * t;
            v.y = y + ( b.y - y ) * t;
            v.z = z + ( b.z - z ) * t;

            return v;
        }

        Vec3 Vec3::operator+(const Vec3 &v) const
        {
            return Vec3(x+v.x,y+v.y,z+v.z);
        }
        Vec3 Vec3::operator-(const Vec3 &v) const
        {
            return Vec3(x-v.x,y-v.y,z-v.z);
        }
        Vec3 Vec3::operator*(const Vec3 &v) const
        {
            return Vec3(x*v.x,y*v.y,z*v.z);
        }
        Vec3 Vec3::operator/(const Vec3 &v) const
        {
            return Vec3(x/v.x,y/v.y,z/v.z);
        }
        Vec3 Vec3::operator+(const f32 f) const
        {
            return Vec3(x+f,y+f,z+f);
        }
        Vec3 Vec3::operator-(const f32 f) const
        {
            return Vec3(x-f,y-f,z-f);
        }
        Vec3 Vec3::operator*(const f32 f) const
        {
            return Vec3(x*f,y*f,z*f);
        }
        Vec3 Vec3::operator/(const f32 f) const
        {
            return Vec3(x/f,y/f,z/f);
        }
        void Vec3::operator+=(const Vec3 &v)
        {
            x+=v.x; y+=v.y; z+=v.z;
        }
        void Vec3::operator-=(const Vec3 &v)
        {
            x-=v.x; y-=v.y; z-=v.z;
        }
        void Vec3::operator*=(const Vec3 &v)
        {
            x*=v.x; y*=v.y; z*=v.z;
        }
        void Vec3::operator/=(const Vec3 &v)
        {
            x/=v.x; y/=v.y; z/=v.z;
        }
        void Vec3::operator+=(const f32 f)
        {
            x+=f; y+=f; z+=f;
        }
        void Vec3::operator-=(const f32 f)
        {
            x-=f; y-=f; z-=f;
        }
        void Vec3::operator*=(const f32 f)
        {
            x*=f; y*=f; z*=f;
        }
        void Vec3::operator/=(const f32 f)
        {
            x/=f; y/=f; z/=f;
        }
        bool Vec3::operator==(const Vec3 &v) const
        {
            return (x==v.x && y==v.y && z==v.z);
        }
        bool Vec3::operator!=(const Vec3 &v) const
        {
            return !(*this==v);
        }
        bool Vec3::operator >(const Vec3& v) const
        {
            return (magnitudeSQR()>v.magnitudeSQR());
        }
        bool Vec3::operator >=(const Vec3& v) const
        {
            return (magnitudeSQR()>=v.magnitudeSQR());
        }
        bool Vec3::operator <(const Vec3& v) const
        {
            return (magnitudeSQR()<v.magnitudeSQR());
        }
        bool Vec3::operator <=(const Vec3& v) const
        {
            return (magnitudeSQR()<=v.magnitudeSQR());
        }
        std::string Vec3::toString() const
        {
            std::ostringstream toStr; toStr << "Vector3(" << x << ", " << y << ", " << z << ")";
            return toStr.str();
        }

        const Vec3 Vec3::UP(0,1,0);
        const Vec3 Vec3::ZERO(0,0,0);

    };
};
