//============================================================================
// Name        : Plane
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Plane Geometry
//============================================================================

#ifndef PLANE_H
#define PLANE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Plane : public Primitive {

	public:

		Plane(const f32 width, const f32 height, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}

	};
};

#endif /* PLANE_H */