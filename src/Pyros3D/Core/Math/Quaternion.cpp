//============================================================================
// Name        : Quaternion.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Quaternion
//============================================================================

#include <math.h>
#include <Pyros3D/Core/Math/Quaternion.h>

namespace p3d {

	namespace Math {

		Quaternion::Quaternion()
		{
			this->w = 1.0f;
			this->x = 0.0f;
			this->y = 0.0f;
			this->z = 0.0f;
		}

		Quaternion::Quaternion(const f32 w, const f32 x, const f32 y, const f32 z)
		{
			this->w = w;
			this->x = x;
			this->y = y;
			this->z = z;

			Normalize();
		}
		Quaternion::Quaternion(const f32 x, const f32 y, const f32 z)
		{
			SetRotationFromEuler(Vec3(x, y, z));
		}
		Quaternion::Quaternion(const Vec3& v, const f32 angle)
		{
			AxisToQuaternion(v, angle);
		}
		f32 Quaternion::Magnitude() const {
			return sqrt(w*w + x*x + y*y + z*z);
		}
		f32 Quaternion::Dot(const Quaternion& q) const
		{
			return w*q.w + x*q.x + y*q.y + z*q.z;
		}
		void Quaternion::Normalize()
		{
			f32 m = Magnitude();
			w = w / m;
			x = x / m;
			y = y / m;
			z = z / m;

			LIMIT_RANGE(-1.0f, w, 1.0f);
			LIMIT_RANGE(-1.0f, x, 1.0f);
			LIMIT_RANGE(-1.0f, y, 1.0f);
			LIMIT_RANGE(-1.0f, z, 1.0f);
		}

		Matrix Quaternion::ConvertToMatrix() const
		{

			Matrix m;
			m.m[0] = (1 - 2 * (y*y) - 2 * (z*z));   m.m[4] = (2 * x*y - 2 * w*z);           m.m[8] = (2 * x*z + 2 * w*y);
			m.m[1] = (2 * x*y + 2 * w*z);      m.m[5] = (w*w - x*x + y*y - z*z);  m.m[9] = (2 * y*z - 2 * w*x);
			m.m[2] = (2 * x*z - 2 * w*y);       m.m[6] = (2 * y*z + 2 * w*x);           m.m[10] = (1 - 2 * (x*x) - 2 * (y*y));

			return m;
		}
		Quaternion Quaternion::operator+ (const Quaternion& q) const
		{
			return Quaternion(w + q.w, x + q.x, y + q.y, z + q.z);
		}

		Quaternion Quaternion::operator- (const Quaternion& q) const
		{
			return Quaternion(w - q.w, x - q.x, y - q.y, z - q.z);
		}
		Quaternion Quaternion::operator- () const
		{
			return Quaternion(-w, -x, -y, -z);
		}
		void Quaternion::operator *=(const Quaternion& q)
		{
			Quaternion quat;
			quat = *this * q;
		}

		Quaternion Quaternion::operator *(const Quaternion& q) const
		{
			Quaternion quat;
			quat.w = w*q.w - x*q.x - y*q.y - z*q.z;
			quat.x = w*q.x + x*q.w + y*q.z - z*q.y;
			quat.y = w*q.y + y*q.w + z*q.x - x*q.z;
			quat.z = w*q.z + z*q.w + x*q.y - y*q.x;

			return quat;
		}

		Vec3 Quaternion::operator *(const Vec3 &v) const {

			f32 x = v.x, y = v.y, z = v.z, qx = x, qy = y, qz = z, qw = w;

			// calculate quat * Vec

			f32 ix = qw * x + qy * z - qz * y,
				iy = qw * y + qz * x - qx * z,
				iz = qw * z + qx * y - qy * x,
				iw = -qx * x - qy * y - qz * z;

			// calculate result * inverse quat

			Vec3 dest;
			dest.x = ix * qw + iw * -qx + iy * -qz - iz * -qy;
			dest.y = iy * qw + iw * -qy + iz * -qx - ix * -qz;
			dest.z = iz * qw + iw * -qz + ix * -qy - iy * -qx;

			return dest;

		}
		Quaternion Quaternion::operator* (const f32 scalar) const
		{
			return Quaternion(scalar*w, scalar*x, scalar*y, scalar*z);
		}
		bool Quaternion::operator ==(const Quaternion& q)
		{
			return (this->w == q.w && this->x == q.x && this->y == q.y && this->z == q.z);
		}
		bool Quaternion::operator !=(const Quaternion& q) {
			return (this->w != q.w || this->x != q.x || this->y != q.y || this->z != q.z);
		}

		void Quaternion::AxisToQuaternion(const Vec3 &v, const f32 angle)
		{
			f32 x, y, z; // temp vars of Vector
			f32 rad, scale; // temp vars

			if (v == Vec3(0, 0, 0)) // if axis is zero, then return quaternion (1,0,0,0)
			{
				w = 1.0f;
				x = 0.0f;
				y = 0.0f;
				z = 0.0f;
			}

			rad = angle * 0.5f;

			w = (f32)cosf(rad);

			scale = sinf(rad);

			x = v.x;
			y = v.y;
			z = v.z;

			this->x = f32(x * scale);
			this->y = f32(y * scale);
			this->z = f32(z * scale);

			Normalize(); // make sure a unit quaternion turns up
		}
		void Quaternion::Rotation(const f32 xAngle, const f32 yAngle, const f32 zAngle) {

			Quaternion Qx, Qy, Qz;
			Qx = Quaternion(cos(xAngle / 2), (sin(xAngle / 2)), 0, 0);
			Qy = Quaternion(cos(yAngle / 2), 0, sin(yAngle / 2), 0);
			Qz = Quaternion(cos(zAngle / 2), 0, 0, sin(zAngle / 2));

			*this = Qx * Qy * Qz;
		}

		Quaternion Quaternion::Slerp(const Quaternion &b, const f32 t) const
		{

			Quaternion a = *this;

			if (t == 0) return a;
			if (t == 1) return b;

			f32 x = a.x;
			f32 y = a.y;
			f32 z = a.z;
			f32 w = a.w;

			// http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/

			f32 cosHalfTheta = w * b.w + x * b.x + y * b.y + z * b.z;

			if (cosHalfTheta < 0) {

				a.w = -b.w;
				a.x = -b.x;
				a.y = -b.y;
				a.z = -b.z;

				cosHalfTheta = -cosHalfTheta;

			}
			else {

				a = b;
			}

			if (cosHalfTheta >= 1.0) {

				a.w = w;
				a.x = x;
				a.y = y;
				a.z = z;

				return a;

			}

			f32 halfTheta = acosf(cosHalfTheta);
			f32 sinHalfTheta = sqrt(1.0f - cosHalfTheta * cosHalfTheta);

			if (fabs(sinHalfTheta) < 0.001) {

				a.w = 0.5f * (w + a.w);
				a.x = 0.5f * (x + a.x);
				a.y = 0.5f * (y + a.y);
				a.z = 0.5f * (z + a.z);

				return a;

			}

			f32 ratioA = sinf((1 - t) * halfTheta) / sinHalfTheta,
				ratioB = sinf(t * halfTheta) / sinHalfTheta;

			a.w = (w * ratioA + a.w * ratioB);
			a.x = (x * ratioA + a.x * ratioB);
			a.y = (y * ratioA + a.y * ratioB);
			a.z = (z * ratioA + a.z * ratioB);

			return a;

		}
		Quaternion Quaternion::Nlerp(const Quaternion &b, const f32 t, bool shortestPath) const
		{
			Quaternion m;

			f32 fCos = this->Dot(b);
			if (fCos < 0.0f && shortestPath) {
				m = *this + (((-b) - *this)*t);
			}
			else {
				m = *this + ((b - *this)*t);
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

		void Quaternion::SetRotationFromEuler(const Vec3 &rotation, const uint32 order)
		{

			f32 c1 = cosf(rotation.x / 2.f);
			f32 c2 = cosf(rotation.y / 2.f);
			f32 c3 = cosf(rotation.z / 2.f);
			f32 s1 = sinf(rotation.x / 2.f);
			f32 s2 = sinf(rotation.y / 2.f);
			f32 s3 = sinf(rotation.z / 2.f);

			switch (order)
			{
			case RotationOrder::XYZ:

				x = s1 * c2 * c3 + c1 * s2 * s3;
				y = c1 * s2 * c3 - s1 * c2 * s3;
				z = c1 * c2 * s3 + s1 * s2 * c3;
				w = c1 * c2 * c3 - s1 * s2 * s3;

				break;
			case RotationOrder::YXZ:

				x = s1 * c2 * c3 + c1 * s2 * s3;
				y = c1 * s2 * c3 - s1 * c2 * s3;
				z = c1 * c2 * s3 - s1 * s2 * c3;
				w = c1 * c2 * c3 + s1 * s2 * s3;

				break;
			case RotationOrder::ZXY:

				x = s1 * c2 * c3 - c1 * s2 * s3;
				y = c1 * s2 * c3 + s1 * c2 * s3;
				z = c1 * c2 * s3 + s1 * s2 * c3;
				w = c1 * c2 * c3 - s1 * s2 * s3;

				break;
			case RotationOrder::ZYX:

				x = s1 * c2 * c3 - c1 * s2 * s3;
				y = c1 * s2 * c3 + s1 * c2 * s3;
				z = c1 * c2 * s3 - s1 * s2 * c3;
				w = c1 * c2 * c3 + s1 * s2 * s3;

				break;
			case RotationOrder::YZX:

				x = s1 * c2 * c3 + c1 * s2 * s3;
				y = c1 * s2 * c3 + s1 * c2 * s3;
				z = c1 * c2 * s3 - s1 * s2 * c3;
				w = c1 * c2 * c3 - s1 * s2 * s3;

				break;
			case RotationOrder::XZY:

				x = s1 * c2 * c3 - c1 * s2 * s3;
				y = c1 * s2 * c3 - s1 * c2 * s3;
				z = c1 * c2 * s3 + s1 * s2 * c3;
				w = c1 * c2 * c3 + s1 * s2 * s3;
				break;
			};

			Normalize();
		}

		Vec3 Quaternion::GetEulerFromQuaternion(const uint32 order)
		{

			Vec3 euler;

			f32 sqx = x*x;
			f32 sqy = y*y;
			f32 sqz = z*z;
			f32 sqw = w*w;

			switch (order)
			{
			case RotationOrder::XYZ:

				euler.x = atan2f(2 * (x * w - y * z), (sqw - sqx - sqy + sqz));
				euler.y = asinf(Clamp(2 * (x * z + y * w)));
				euler.z = atan2f(2 * (z * w - x * y), (sqw + sqx - sqy - sqz));

				break;
			case RotationOrder::YXZ:

				euler.x = asinf(Clamp(2 * (x * w - y * z)));
				euler.y = atan2f(2 * (x * z + y * w), (sqw - sqx - sqy + sqz));
				euler.z = atan2f(2 * (x * y + z * w), (sqw - sqx + sqy - sqz));

				break;
			case RotationOrder::ZXY:

				euler.x = asinf(Clamp(2 * (x * w + y * z)));
				euler.y = atan2f(2 * (y * w - z * x), (sqw - sqx - sqy + sqz));
				euler.z = atan2f(2 * (z * w - x * y), (sqw - sqx + sqy - sqz));

				break;
			case RotationOrder::ZYX:

				euler.x = atan2f(2 * (x * w + z * y), (sqw - sqx - sqy + sqz));
				euler.y = asinf(Clamp(2 * (y * w - x * z)));
				euler.z = atan2f(2 * (x * y + z * w), (sqw + sqx - sqy - sqz));

				break;
			case RotationOrder::YZX:

				euler.x = atan2f(2 * (x * w - z * y), (sqw - sqx + sqy - sqz));
				euler.y = atan2f(2 * (y * w - x * z), (sqw + sqx - sqy - sqz));
				euler.z = asinf(Clamp(2 * (x * y + z * w)));

				break;
			case RotationOrder::XZY:

				euler.x = atan2f(2 * (x * w + y * z), (sqw - sqx + sqy - sqz));
				euler.y = atan2f(2 * (x * z + y * w), (sqw + sqx - sqy - sqz));
				euler.z = asinf(Clamp(2 * (z * w - x * y)));

				break;
			};

			return euler;

		}

		std::string Quaternion::toString() const
		{
			std::ostringstream toStr;

			toStr << "| w: " << w << " | x: " << x << " | y: " << y << " | z: " << z << " |";

			return "Quaternion \n" + toStr.str();
		}
	};
};