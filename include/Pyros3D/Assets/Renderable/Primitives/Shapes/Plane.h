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

		// `segments`: how many quads a side (1: the one quad). A plane a vertex shader is to move -
		// water with waves, cloth - needs vertices to move.
		Plane(const f32 width, const f32 height, bool smooth = false, bool flip = false, bool TangentBitangent = false, const uint32 segments = 1);

		virtual void CalculateBounding() {}

		virtual uint32 GetPrimitiveType() const { return PrimitiveType::Plane; }
		f32 GetWidth() const { return width; }
		f32 GetHeight() const { return height; }
		uint32 GetSegments() const { return segments; }

	protected:

		f32 width, height;
		uint32 segments = 1;
	};
};

#endif /* PLANE_H */