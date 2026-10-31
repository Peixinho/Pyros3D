//============================================================================
// Name        : Plane
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Plane Geometry
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>

namespace p3d {

	Plane::Plane(const f32 width, const f32 height, bool smooth, bool flip, bool TangentBitangent, const uint32 segments_)
	{
		isFlipped = flip;
		isSmooth = smooth;
		calculateTangentBitangent = TangentBitangent;
		this->width = width;
		this->height = height;

		f32 w2 = width; f32 h2 = height;

		segments = segments_ < 1 ? 1 : (segments_ > 1024 ? 1024 : segments_);
		const Vec3 normal(0.f, 0.f, 1.f);
		// (the same corners, winding and texture coordinates as the one quad this was: a row of vertices at a time)
		for (uint32 r = 0; r <= segments; r++)
			for (uint32 c = 0; c <= segments; c++)
			{
				const f32 u = (f32)c / (f32)segments, v = (f32)r / (f32)segments;
				geometry->tVertex.push_back(Vec3(-w2 + 2.f * w2 * u, -h2 + 2.f * h2 * v, 0.f));
				geometry->tNormal.push_back(normal);
				geometry->tTexcoord.push_back(Vec2(1.f - u, v));
			}
		for (uint32 r = 0; r < segments; r++)
			for (uint32 c = 0; c < segments; c++)
			{
				const uint32 i0 = r * (segments + 1) + c, i1 = i0 + 1, i2 = i1 + segments + 1, i3 = i0 + segments + 1;
				geometry->index.push_back(i0); geometry->index.push_back(i1); geometry->index.push_back(i2);
				geometry->index.push_back(i2); geometry->index.push_back(i3); geometry->index.push_back(i0);
			}

		for (size_t i = 0; i < geometry->tTexcoord.size(); i++) geometry->tTexcoord[i].y = 1 - geometry->tTexcoord[i].y;

		// Build and Send Buffers
		Build();

		// Bounding Box
		minBounds = Vec3(-w2, -h2, 0);
		maxBounds = Vec3(w2, h2, 0);

		// Bounding Sphere
		BoundingSphereCenter = Vec3(0, 0, 0);
		// Was Max(w2, h2) - the half-EDGE, not the half-DIAGONAL, so the
		// sphere didn't contain the plane's own corners: a 50x50 quad got
		// radius 25 when it needs sqrt(25^2+25^2) = 35.36, leaving each
		// corner 10.4 units outside. Culling by that sphere then
		// frustum-culled quads that were still partly on screen, which
		// showed up as whole tiles vanishing from a tiled water surface at
		// specific camera angles (island SSR demo) - deterministic, and
		// identical on GL/Vulkan/Metal since the test is CPU-side. Every
		// other primitive here already derives this from its bounding box
		// corner (see Cube/Cone/Cylinder/Torus's min.distance(ZERO));
		// Plane was the one that didn't. Only ever makes the sphere
		// bigger, so this cannot cull anything it previously drew.
		BoundingSphereRadius = minBounds.distance(Vec3::ZERO);
	}
};