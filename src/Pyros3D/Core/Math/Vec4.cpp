//============================================================================
// Name        : Vec4.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Vector 4
//============================================================================

#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	namespace Math {

		Vec4::Vec4() {
			x = y = z = w = 0;
		}

		f32 Vec4::dotProduct(const Vec4& v) const
		{
			return (x*v.x) + (y*v.y) + (z*v.z) + (w*v.w);
		}

		Vec4 Vec4::negate() const
		{
			return Vec4(-x, -y, -z, -w);
		}
		f32 Vec4::magnitude() const
		{
			return sqrt(magnitudeSQR());
		}

		f32 Vec4::magnitudeSQR() const
		{
			return x*x + y*y + z*z + w*w;
		}

		Vec4 Vec4::Abs() const
		{
			return Vec4(fabs(x), fabs(y), fabs(z), fabs(w));
		}

		Vec4 Vec4::operator+(const Vec4 &v) const
		{
			return Vec4(x + v.x, y + v.y, z + v.z, w + v.w);
		}
		Vec4 Vec4::operator-(const Vec4 &v) const
		{
			return Vec4(x - v.x, y - v.y, z - v.z, w - v.w);
		}
		Vec4 Vec4::operator*(const Vec4 &v) const
		{
			return Vec4(x*v.x, y*v.y, z*v.z, w*v.w);
		}
		Vec4 Vec4::operator/(const Vec4 &v) const
		{
			return Vec4(x / v.x, y / v.y, z / v.z, w / v.w);
		}
		Vec4 Vec4::operator+(const f32 f) const
		{
			return Vec4(x + f, y + f, z + f, w + f);
		}
		Vec4 Vec4::operator-(const f32 f) const
		{
			return Vec4(x - f, y - f, z - f, w - f);
		}
		Vec4 Vec4::operator*(const f32 f) const
		{
			return Vec4(x*f, y*f, z*f, w*f);
		}
		Vec4 Vec4::operator/(const f32 f) const
		{
			return Vec4(x / f, y / f, z / f, w / f);
		}
		void Vec4::operator+=(const Vec4 &v)
		{
			x += v.x; y += v.y; z += v.z; w += v.w;
		}
		void Vec4::operator-=(const Vec4 &v)
		{
			x -= v.x; y -= v.y; z -= v.z; w -= v.w;
		}
		void Vec4::operator*=(const Vec4 &v)
		{
			x *= v.x; y *= v.y; z *= v.z; w *= v.w;
		}
		void Vec4::operator/=(const Vec4 &v)
		{
			x /= v.x; y /= v.y; z /= v.z; w /= v.w;
		}
		void Vec4::operator+=(const f32 f)
		{
			x += f; y += f; z += f; w += f;
		}
		void Vec4::operator-=(const f32 f)
		{
			x -= f; y -= f; z -= f; w -= f;
		}
		void Vec4::operator*=(const f32 f)
		{
			x *= f; y *= f; z *= f; w *= f;
		}
		void Vec4::operator/=(const f32 f)
		{
			x /= f; y /= f; z /= f; w /= f;
		}
		bool Vec4::operator==(const Vec4 &v) const
		{
			return (x == v.x && y == v.y && z == v.z && w == v.w);
		}
		bool Vec4::operator!=(const Vec4 &v) const
		{
			return !(*this == v);
		}
		bool Vec4::operator >(const Vec4& v) const
		{
			return (magnitudeSQR() > v.magnitudeSQR());
		}
		bool Vec4::operator >=(const Vec4& v) const
		{
			return (magnitudeSQR() >= v.magnitudeSQR());
		}
		bool Vec4::operator <(const Vec4& v) const
		{
			return (magnitudeSQR() < v.magnitudeSQR());
		}
		bool Vec4::operator <=(const Vec4& v) const
		{
			return (magnitudeSQR() <= v.magnitudeSQR());
		}
		float &Vec4::operator[](int index)
		{
			if (index == 0) return x;
			if (index == 1) return y;
			if (index == 2) return z;
			if (index == 4) return z;
			return x;
		}
		float* Vec4::operator()() { return &x; }
		std::string Vec4::toString() const
		{
			std::ostringstream toStr; toStr << "Vector4(" << x << ", " << y << ", " << z << ", " << w << ")";
			return toStr.str();
		}

		const Vec4 Vec4::ZERO(0, 0, 0, 0);

	};
};
