//============================================================================
// Name        : Card
// Author      : Duarte Peixinho
// Description : See Card.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Card.h>

namespace p3d {

	Card::Card(const f32 left, const f32 right, const f32 bottom, const f32 top, const bool crossed)
		: left(left), right(right), bottom(bottom), top(top), crossed(crossed)
	{
		isFlipped = false;
		isSmooth = false;
		calculateTangentBitangent = false;

		const Vec3 up(0.f, 1.f, 0.f);
		const int quads = crossed ? 2 : 1;
		for (int q = 0; q < quads; q++)
		{
			// q 0 across X, facing +Z; q 1 across Z, facing +X.
			auto at = [q](const f32 across, const f32 y) { return q == 0 ? Vec3(across, y, 0.f) : Vec3(0.f, y, -across); };
			const uint32 base = (uint32)geometry->tVertex.size();
			geometry->tVertex.push_back(at(left, bottom));  geometry->tNormal.push_back(up); geometry->tTexcoord.push_back(Vec2(0.f, 1.f));
			geometry->tVertex.push_back(at(right, bottom)); geometry->tNormal.push_back(up); geometry->tTexcoord.push_back(Vec2(1.f, 1.f));
			geometry->tVertex.push_back(at(right, top));    geometry->tNormal.push_back(up); geometry->tTexcoord.push_back(Vec2(1.f, 0.f));
			geometry->tVertex.push_back(at(left, top));     geometry->tNormal.push_back(up); geometry->tTexcoord.push_back(Vec2(0.f, 0.f));
			geometry->index.push_back(base + 0); geometry->index.push_back(base + 1); geometry->index.push_back(base + 2);
			geometry->index.push_back(base + 2); geometry->index.push_back(base + 3); geometry->index.push_back(base + 0);
		}

		Build();

		const f32 reach = crossed ? Max(fabsf(left), fabsf(right)) : 0.f;
		minBounds = Vec3(left, bottom, -reach);
		maxBounds = Vec3(right, top, reach);
		BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
		BoundingSphereRadius = minBounds.distance(BoundingSphereCenter);
	}
};
