//============================================================================
// Name        : Cube
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Cube Geometry
//============================================================================

#ifndef CUBE_H
#define CUBE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Cube : public Primitive {

	public:

		Cube(const f32 width, const f32 height, const f32 depth, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}
		
	};
};

#endif /* CUBE_H */