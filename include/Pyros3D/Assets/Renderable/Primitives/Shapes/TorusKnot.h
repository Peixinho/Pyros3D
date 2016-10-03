//============================================================================
// Name        : Torus Knot
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Torus Knot Geometry
//============================================================================

#ifndef TORUSKNOT_H
#define TORUSKNOT_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API TorusKnot : public Primitive {

	public:

		f32 p, q, radius, heightScale;

		TorusKnot(const f32 radius, const f32 tube, const uint32 segmentsW = 60, const uint32 segmentsH = 6, const f32 p = 2, const f32 q = 3, const uint32 heightscale = 1, bool smooth = false, bool flip = false, bool TangentBitangent = false);

		virtual void CalculateBounding() {}

	private:

		Vec3 GetPos(const f32 u, const f32 v) const;
		
	};
};

#endif /* TORUSKNOT_H */