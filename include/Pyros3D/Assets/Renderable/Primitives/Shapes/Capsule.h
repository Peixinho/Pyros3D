//============================================================================
// Name        : Capsule
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Capsule Geometry
//============================================================================

#ifndef CAPSULE_H
#define CAPSULE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Capsule : public Primitive {

	public:

		Capsule(const f32 radius, const f32 height, const uint32 numRings, const uint32 segmentsW, const uint32 segmentsH, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}

	};
};

#endif /* CAPSULE_H */