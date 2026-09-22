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
#include <memory>
#include <vector>

namespace p3d {

	class SceneGraph;
	class GameObject;

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

	// One object's contribution, and what is needed to move it.
	//
	// Triangles are baked into world space, which is right for the
	// tracer's inner loop and wrong the moment anything moves. Keeping
	// the local-space copy and the matrix it was baked with means a
	// moved object can be re-transformed without walking the scene
	// graph again, without re-reading vertex buffers, and without
	// rebuilding the tree.
	struct PYROS3D_API RayInstance
	{
		// Weak, deliberately: a RayScene outlives individual objects,
		// and an expired pointer is how it finds out that the scene's
		// contents - not just its transforms - have changed.
		std::weak_ptr<GameObject> owner;
		uint32 firstTriangle, triangleCount;
		Matrix world;
		RayInstance() : firstTriangle(0), triangleCount(0) {}
	};

	// What RefreshTransforms found.
	namespace RaySceneChange
	{
		enum Enum
		{
			// Nothing moved; the tree and the triangles still describe
			// the scene.
			None = 0,
			// Something moved. The triangles have been updated; the BVH
			// has not, and the caller must refit it.
			Moved = 1,
			// An object this scene was built from no longer exists, so
			// the triangle list itself is wrong. A refit cannot help -
			// the scene has to be extracted again.
			NeedsRebuild = 2
		};
	}

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

		// Per-object records, in the order they were extracted.
		std::vector<RayInstance> instances;
		// The same triangles before their world transform, parallel to
		// `triangles`. What lets an instance be re-transformed in place.
		std::vector<RayTriangle> localTriangles;

		// Re-transforms any instance whose owner has moved since the
		// last call, and reports what it found.
		//
		// Costs one matrix comparison per object and, for those that
		// moved, one matrix multiply per vertex. That is the cheap half
		// of following moving geometry; RefitBVH is the other half and
		// the caller runs it when this returns Moved.
		RaySceneChange::Enum RefreshTransforms();

		// Recomputes every node's bounds bottom-up, leaving the tree's
		// SHAPE alone.
		//
		// Orders of magnitude cheaper than Build() - one pass over the
		// nodes, no sorting, no SAH evaluation - and the trade is
		// quality: the split planes were chosen for where the geometry
		// used to be, so a tree refitted over and over as things move
		// far from their original positions grows overlapping nodes and
		// traverses worse. Right for animation and for objects moving
		// within their own neighbourhood; a scene that has been rearranged
		// wants a real rebuild.
		void RefitBVH();

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

		// Flattens into the exact float layout resources/shaders/gi/
		// raytrace.glsl unpacks. Kept next to the traversal it feeds
		// rather than in the caller, because the two are one contract:
		// the strides and the field order are duplicated in GLSL and a
		// silent disagreement produces garbage intersections, not an
		// error.
		//
		// Triangles: 9 vec4 each (3 positions, 3 normals, material, 2
		// spare). Nodes: 2 vec4 each, with firstOrLeft and count
		// bit-cast into the w components - std430 would pad a vec3 to 16
		// bytes anyway, so those slots are free.
		void PackForGPU(std::vector<f32> &outTriangles, std::vector<f32> &outNodes,
			std::vector<uint32> &outIndices) const;

		static const uint32 kTriangleStrideFloats = 9 * 4;
		static const uint32 kNodeStrideFloats = 2 * 4;

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
