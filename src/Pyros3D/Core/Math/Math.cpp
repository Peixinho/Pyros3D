//============================================================================
// Name        : Math.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Math
//============================================================================

#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	namespace Math {

		const f32 barryCentric(const Vec3 &p1, const Vec3 &p2, const Vec3 &p3, const Vec2 &pos) {
			f32 det = (p2.z - p3.z) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.z - p3.z);
			f32 l1 = ((p2.z - p3.z) * (pos.x - p3.x) + (p3.x - p2.x) * (pos.y - p3.z)) / det;
			f32 l2 = ((p3.z - p1.z) * (pos.x - p3.x) + (p1.x - p3.x) * (pos.y - p3.z)) / det;
			f32 l3 = 1.0f - l1 - l2;
			return l1 * p1.y + l2 * p2.y + l3 * p3.y;
		};

	};
};