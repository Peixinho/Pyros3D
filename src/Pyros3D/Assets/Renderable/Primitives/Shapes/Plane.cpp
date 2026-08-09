//============================================================================
// Name        : Plane
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Plane Geometry
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>

namespace p3d {

	Plane::Plane(const f32 width, const f32 height, bool smooth, bool flip, bool TangentBitangent)
	{
		isFlipped = flip;
		isSmooth = smooth;
		calculateTangentBitangent = TangentBitangent;
		this->width = width;
		this->height = height;

		f32 w2 = width; f32 h2 = height;

		Vec3 a = Vec3(-w2, -h2, 0); Vec3 b = Vec3(w2, -h2, 0); Vec3 c = Vec3(w2, h2, 0); Vec3 d = Vec3(-w2, h2, 0);
		Vec3 normal = ((c - b).cross(a - b)).normalize();
		geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1, 0));
		geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0, 0));
		geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(0, 1));
		geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);      geometry->tTexcoord.push_back(Vec2(1, 1));

		geometry->index.push_back(0);
		geometry->index.push_back(1);
		geometry->index.push_back(2);
		geometry->index.push_back(2);
		geometry->index.push_back(3);
		geometry->index.push_back(0);

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
		// corner 10.4 units outside. IRenderer::CullingSphereTest() then
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