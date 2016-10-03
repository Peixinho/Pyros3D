//============================================================================
// Name        : Cylinder
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Cylinder Geometry
//============================================================================

#ifndef CYLINDER_H
#define CYLINDER_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Cylinder : public Primitive {

	public:

		f32 segmentsH;
		f32 height;

		Cylinder(const f32 radius, const f32 height, const uint32 segmentsW, const uint32 segmentsH, bool openEnded, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}
		
	};
};

#endif /* CYLINDER_H */