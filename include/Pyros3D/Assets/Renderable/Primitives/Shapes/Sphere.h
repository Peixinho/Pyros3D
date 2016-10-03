//============================================================================
// Name        : Sphere
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Sphere Geometry
//============================================================================

#ifndef SPHERE_H
#define SPHERE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Sphere : public Primitive {

	public:

		Sphere(const f32 radius, const uint32 segmentsW, const uint32 segmentsH, bool smooth = false, bool HalfSphere = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}

	};
};

#endif /* SPHERE_H */