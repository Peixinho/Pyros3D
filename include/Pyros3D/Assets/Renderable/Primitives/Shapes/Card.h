//============================================================================
// Name        : Card
// Author      : Duarte Peixinho
// Description : A vertical quad in the XY plane, facing +Z, spanning
//               explicit bounds - an impostor card that stands on its model's
//               base rather than being centred on it (Plane is centred, and
//               takes half extents). u runs left to right seen from +Z; v
//               runs from the top edge (0) to the bottom (1), so an image's
//               first row is the card's top.
//
//               crossed: a second quad through the same axis at right angles
//               (in the ZY plane), so a card turned edge-on by an instance's
//               yaw still shows. Both point their normals straight up: the
//               shading of an impostor is baked into its picture, and an up
//               normal lights either face the way the ground around it is lit.
//============================================================================

#ifndef CARD_H
#define CARD_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API Card : public Primitive {

	public:

		Card(const f32 left, const f32 right, const f32 bottom, const f32 top, const bool crossed = true);

		virtual void CalculateBounding() {}

		// Custom: the scene serializer knows a Card by its type.
		virtual uint32 GetPrimitiveType() const { return PrimitiveType::Custom; }
		f32 GetLeft() const { return left; }
		f32 GetRight() const { return right; }
		f32 GetBottom() const { return bottom; }
		f32 GetTop() const { return top; }
		bool IsCrossed() const { return crossed; }

	protected:

		f32 left, right, bottom, top;
		bool crossed;
	};
};

#endif /* CARD_H */
