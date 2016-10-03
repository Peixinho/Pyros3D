//============================================================================
// Name        : Torus
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Torus Geometry
//============================================================================

#ifndef TORUS_H
#define TORUS_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Torus : public Primitive {

	public:

		Torus(const f32 radius, const f32 tube, const uint32 segmentsW = 60, const uint32 segmentsH = 6, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}
		
	};
};

#endif /* TORUS_H */