//============================================================================
// Name        : ImpostorQuad
// Author      : Duarte Peixinho
// Description : An octahedral impostor's mesh (ShaderUsage::Impostor): one quad of corners -1..1, which
//               a shader is to turn to face the eye and size to the tree (the renderer does not draw octahedral
//               impostors yet - a far forest's kinds are drawn as cards). Its
//               bounds are the tree's - a sphere of `radius` about `centre` metres above its foot - so it
//               is culled as the tree it stands for, not as the unit square it is made of.
//============================================================================

#ifndef IMPOSTORQUAD_H
#define IMPOSTORQUAD_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	class PYROS3D_API ImpostorQuad : public Primitive {

	public:

		ImpostorQuad(const f32 radius = 1.f, const f32 centre = 0.f);

		virtual void CalculateBounding() {}

		// Custom: the scene serializer knows it by its type.
		virtual uint32 GetPrimitiveType() const { return PrimitiveType::Custom; }
		f32 GetRadius() const { return radius; }
		f32 GetCentre() const { return centre; }

	protected:

		f32 radius, centre;
	};
};

#endif /* IMPOSTORQUAD_H */
