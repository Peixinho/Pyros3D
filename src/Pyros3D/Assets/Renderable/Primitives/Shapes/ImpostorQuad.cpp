//============================================================================
// Name        : ImpostorQuad
// Author      : Duarte Peixinho
// Description : See ImpostorQuad.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/ImpostorQuad.h>

namespace p3d {

	ImpostorQuad::ImpostorQuad(const f32 radius, const f32 centre) : radius(radius), centre(centre)
	{
		isFlipped = false;
		isSmooth = false;
		calculateTangentBitangent = false;

		const Vec3 p[4] = { Vec3(-1.f, 1.f, 0.f), Vec3(1.f, 1.f, 0.f), Vec3(1.f, -1.f, 0.f), Vec3(-1.f, -1.f, 0.f) };
		const Vec2 uv[4] = { Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(1.f, 1.f), Vec2(0.f, 1.f) };
		for (int k = 0; k < 4; k++)
		{
			geometry->tVertex.push_back(p[k]);
			geometry->tNormal.push_back(Vec3(0.f, 0.f, 1.f));
			geometry->tTexcoord.push_back(uv[k]);
		}
		geometry->index.push_back(0); geometry->index.push_back(2); geometry->index.push_back(1);
		geometry->index.push_back(0); geometry->index.push_back(3); geometry->index.push_back(2);

		Build();

		minBounds = Vec3(-radius, centre - radius, -radius);
		maxBounds = Vec3(radius, centre + radius, radius);
		BoundingSphereCenter = Vec3(0.f, centre, 0.f);
		BoundingSphereRadius = radius;
	}
};
