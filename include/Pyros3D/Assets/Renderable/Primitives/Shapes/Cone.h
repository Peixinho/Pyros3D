//============================================================================
// Name        : Cone
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Cone Geometry
//============================================================================

#ifndef CONE_H
#define CONE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Cone : public Primitive {

	public:

		f32 segmentsW, segmentsH;

		Cone(const f32 radius, const f32 height, const uint32 segmentsW, const uint32 segmentsH, const bool openEnded, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}
		
	};
};

#endif /* CONE_H */