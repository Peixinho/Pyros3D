//============================================================================
// Name        : RayScene.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : The scene as triangles plus a BVH over them - what a ray
//               tracing kernel needs and a renderer does not have.
//============================================================================

#ifndef RAYSCENE_H
#define RAYSCENE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	class SceneGraph;

	// One world-space triangle. Positions and normals are pre-transformed
	// at build time rather than carrying a per-instance matrix: a ray
	// tracer's inner loop should not be doing matrix work, and this is
	// rebuilt when the scene changes anyway.
	struct PYROS3D_API RayTriangle
	{
		Vec3 v0, v1, v2;
		Vec3 n0, n1, n2;
		uint32 materialIndex;
		RayTriangle() : materialIndex(0) {}
	};

	// Deliberately tiny. A path tracer for indirect light needs to know
	// what colour a surface reflects and whether it emits; texture
	// lookups, normal maps and the rest of the material system would
	// multiply the cost of every bounce for detail that a diffuse bounce
	// integrates away. Textured albedo is a later refinement.
	struct PYROS3D_API RayMaterial
	{
		Vec3 albedo;
		Vec3 emissive;
		RayMaterial() : albedo(0.8f, 0.8f, 0.8f), emissive(0.f, 0.f, 0.f) {}
	};

	// A node of the flattened BVH.
	//
	// Flat and pointer-free because the whole point is that a compute
	// shader can walk it out of an SSBO. `count` distinguishes the two
	// cases: non-zero means a leaf holding `count` triangles starting at
	// `firstOrLeft` in the index array; zero means an interior node whose
	// left child is at `firstOrLeft` and whose right child is always
	// `firstOrLeft + 1` - storing one child index rather than two is the
	// standard trick and halves the pointer chasing.
	struct PYROS3D_API BVHNode
	{
		Vec3 boundsMin, boundsMax;
		uint32 firstOrLeft;
		uint32 count;
		BVHNode() : boundsMin(0.f,0.f,0.f), boundsMax(0.f,0.f,0.f), firstOrLeft(0), count(0) {}
	};

	struct PYROS3D_API RayHit
	{
		f32 t;
		uint32 triangle;
		f32 u, v;          // barycentrics, for interpolating the normal
		bool hit;
		RayHit() : t(0.f), triangle(0), u(0.f), v(0.f), hit(false) {}
	};

	class PYROS3D_API RayScene
	{
	public:

		std::vector<RayTriangle> triangles;
		std::vector<RayMaterial> materials;
		// Flattened BVH. Empty until Build() runs.
		std::vector<BVHNode> nodes;
		// Triangle indices, reordered by the build so each leaf's
		// triangles are contiguous. The BVH indexes THIS, not `triangles`
		// directly - sorting an index array leaves the triangle data
		// where it is and keeps the build cheap.
		std::vector<uint32> triangleIndices;

		// Walks the scene graph and collects every rendering mesh's
		// CPU-side geometry, transformed to world space. Returns false if
		// nothing renderable was found.
		bool BuildFromScene(SceneGraph *scene);

		// Builds the BVH over whatever is in `triangles`. Split by the
		// surface area heuristic, which is worth the build cost here: the
		// alternative (split at the midpoint of the longest axis) is
		// simpler and produces trees that are routinely twice as
		// expensive to traverse on scenes with uneven triangle density -
		// which is every real scene.
		void Build(const uint32 maxLeafSize = 4);

		// Closest hit. The CPU reference implementation, and the thing
		// the compute kernel is verified against.
		bool Intersect(const Vec3 &origin, const Vec3 &direction, const f32 tMin, const f32 tMax, RayHit &outHit) const;

		// Same query with no BVH at all - every triangle, in order. Only
		// for tests: a BVH bug produces a plausible image rather than an
		// obvious error, so the only way to know the tree is right is to
		// compare it against the answer that cannot be wrong.
		bool IntersectBruteForce(const Vec3 &origin, const Vec3 &direction, const f32 tMin, const f32 tMax, RayHit &outHit) const;

		void Clear();
		uint32 TriangleCount() const { return (uint32)triangles.size(); }
		uint32 NodeCount() const { return (uint32)nodes.size(); }
		// Deepest leaf. A tree whose depth approaches the triangle count
		// has degenerated to a list and will traverse like one.
		uint32 MaxDepth() const;

	private:

		void Subdivide(const uint32 nodeIndex, const uint32 maxLeafSize, std::vector<Vec3> &centroids);
		void UpdateNodeBounds(const uint32 nodeIndex);
	};

};

#endif /* RAYSCENE_H */
