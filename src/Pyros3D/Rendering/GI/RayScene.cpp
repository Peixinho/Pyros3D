//============================================================================
// Name        : RayScene.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See RayScene.h.
//============================================================================

#include <Pyros3D/Rendering/GI/RayScene.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace p3d {

	namespace {

		inline Vec3 MinV(const Vec3 &a, const Vec3 &b)
		{
			return Vec3(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z);
		}
		inline Vec3 MaxV(const Vec3 &a, const Vec3 &b)
		{
			return Vec3(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z);
		}

		// Moller-Trumbore. Returns t and barycentrics; no backface
		// rejection, because indirect light bounces off whatever it hits
		// and single-sided geometry is a rendering convention, not a
		// property of the surface.
		inline bool IntersectTriangle(const Vec3 &o, const Vec3 &d, const RayTriangle &tri,
			f32 &outT, f32 &outU, f32 &outV)
		{
			const Vec3 e1 = tri.v1 - tri.v0;
			const Vec3 e2 = tri.v2 - tri.v0;
			const Vec3 p = d.cross(e2);
			const f32 det = e1.dotProduct(p);
			if (fabsf(det) < 1e-9f)
				return false;
			const f32 invDet = 1.f / det;
			const Vec3 tv = o - tri.v0;
			outU = tv.dotProduct(p) * invDet;
			if (outU < 0.f || outU > 1.f)
				return false;
			const Vec3 q = tv.cross(e1);
			outV = d.dotProduct(q) * invDet;
			if (outV < 0.f || outU + outV > 1.f)
				return false;
			outT = e2.dotProduct(q) * invDet;
			return true;
		}

		// Slab test. Returns the near intersection distance, or FLT_MAX
		// for a miss - traversal uses that distance to visit the closer
		// child first and to cull a box already further away than the
		// best hit so far.
		inline f32 IntersectAABB(const Vec3 &o, const Vec3 &invD, const Vec3 &bmin, const Vec3 &bmax, const f32 tMax)
		{
			const f32 tx1 = (bmin.x - o.x) * invD.x, tx2 = (bmax.x - o.x) * invD.x;
			f32 tmin = tx1 < tx2 ? tx1 : tx2;
			f32 tmaxv = tx1 > tx2 ? tx1 : tx2;
			const f32 ty1 = (bmin.y - o.y) * invD.y, ty2 = (bmax.y - o.y) * invD.y;
			tmin = std::max(tmin, ty1 < ty2 ? ty1 : ty2);
			tmaxv = std::min(tmaxv, ty1 > ty2 ? ty1 : ty2);
			const f32 tz1 = (bmin.z - o.z) * invD.z, tz2 = (bmax.z - o.z) * invD.z;
			tmin = std::max(tmin, tz1 < tz2 ? tz1 : tz2);
			tmaxv = std::min(tmaxv, tz1 > tz2 ? tz1 : tz2);
			if (tmaxv >= tmin && tmin < tMax && tmaxv > 0.f)
				return tmin > 0.f ? tmin : 0.f;
			return FLT_MAX;
		}

	} // namespace

	void RayScene::Clear()
	{
		triangles.clear();
		materials.clear();
		nodes.clear();
		triangleIndices.clear();
	}

	bool RayScene::BuildFromScene(SceneGraph *scene)
	{
		Clear();
		if (scene == NULL)
			return false;

		std::vector<RenderingMesh*> meshes = RenderingComponent::GetRenderingMeshesSorted(scene);
		for (size_t m = 0; m < meshes.size(); m++)
		{
			RenderingMesh *rm = meshes[m];
			if (rm == NULL || rm->Geometry == NULL || rm->renderingComponent == NULL)
				continue;
			GameObject *owner = rm->renderingComponent->GetOwner();
			if (owner == NULL)
				continue;

			const std::vector<uint32> &idx = rm->Geometry->GetIndexData();
			const std::vector<Vec3> &pos = rm->Geometry->GetVertexData();
			const std::vector<Vec3> &nrm = rm->Geometry->GetNormalData();
			if (idx.size() < 3 || pos.empty())
				continue;

			const Matrix world = owner->GetWorldTransformation();

			// One material per mesh. Emissive is left at zero: the engine
			// has no emissive material term to read, and inventing one
			// from the albedo would make every bright surface a light.
			RayMaterial mat;
			GenericShaderMaterial *gm = dynamic_cast<GenericShaderMaterial*>(rm->Material.get());
			if (gm != NULL)
			{
				const Vec4 c = gm->GetColor();
				mat.albedo = Vec3(c.x, c.y, c.z);
			}
			const uint32 materialIndex = (uint32)materials.size();
			materials.push_back(mat);

			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				const uint32 a = idx[i], b = idx[i+1], c = idx[i+2];
				if (a >= pos.size() || b >= pos.size() || c >= pos.size())
					continue;

				RayTriangle tri;
				tri.v0 = world * pos[a];
				tri.v1 = world * pos[b];
				tri.v2 = world * pos[c];

				if (a < nrm.size() && b < nrm.size() && c < nrm.size())
				{
					// w = 0 so the translation drops out - a normal is a
					// direction, and Matrix*Vec3 would move it. Non-uniform
					// scale would additionally need the inverse transpose;
					// meshes here are uniformly scaled in practice, and a
					// skewed normal costs shading accuracy on a bounce
					// rather than a wrong intersection.
					tri.n0 = (world * Vec4(nrm[a], 0.f)).xyz().normalize();
					tri.n1 = (world * Vec4(nrm[b], 0.f)).xyz().normalize();
					tri.n2 = (world * Vec4(nrm[c], 0.f)).xyz().normalize();
				}
				else
				{
					const Vec3 fn = (tri.v1 - tri.v0).cross(tri.v2 - tri.v0).normalize();
					tri.n0 = tri.n1 = tri.n2 = fn;
				}
				tri.materialIndex = materialIndex;
				triangles.push_back(tri);
			}
		}
		return !triangles.empty();
	}

	void RayScene::UpdateNodeBounds(const uint32 nodeIndex)
	{
		BVHNode &node = nodes[nodeIndex];
		node.boundsMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
		node.boundsMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (uint32 i = 0; i < node.count; i++)
		{
			const RayTriangle &t = triangles[triangleIndices[node.firstOrLeft + i]];
			node.boundsMin = MinV(node.boundsMin, MinV(t.v0, MinV(t.v1, t.v2)));
			node.boundsMax = MaxV(node.boundsMax, MaxV(t.v0, MaxV(t.v1, t.v2)));
		}
	}

	void RayScene::Subdivide(const uint32 nodeIndex, const uint32 maxLeafSize, std::vector<Vec3> &centroids)
	{
		BVHNode &node = nodes[nodeIndex];
		if (node.count <= maxLeafSize)
			return;

		// Surface area heuristic over a fixed set of candidate planes per
		// axis. Binned rather than exact (which would test every centroid
		// as a split candidate): binning is O(n) per axis instead of
		// O(n log n) and lands within a couple of percent of the exact
		// tree, which is the standard trade for a builder that has to run
		// when a scene loads.
		const uint32 kBins = 12;
		f32 bestCost = FLT_MAX;
		uint32 bestAxis = 0;
		f32 bestSplit = 0.f;

		for (uint32 axis = 0; axis < 3; axis++)
		{
			f32 lo = FLT_MAX, hi = -FLT_MAX;
			for (uint32 i = 0; i < node.count; i++)
			{
				const Vec3 &c = centroids[triangleIndices[node.firstOrLeft + i]];
				const f32 v = (axis == 0) ? c.x : (axis == 1) ? c.y : c.z;
				lo = std::min(lo, v); hi = std::max(hi, v);
			}
			if (hi - lo < 1e-9f)
				continue;

			for (uint32 b = 1; b < kBins; b++)
			{
				const f32 plane = lo + (hi - lo) * ((f32)b / (f32)kBins);
				Vec3 lmin(FLT_MAX,FLT_MAX,FLT_MAX), lmax(-FLT_MAX,-FLT_MAX,-FLT_MAX);
				Vec3 rmin(FLT_MAX,FLT_MAX,FLT_MAX), rmax(-FLT_MAX,-FLT_MAX,-FLT_MAX);
				uint32 lcount = 0, rcount = 0;
				for (uint32 i = 0; i < node.count; i++)
				{
					const uint32 ti = triangleIndices[node.firstOrLeft + i];
					const Vec3 &c = centroids[ti];
					const f32 v = (axis == 0) ? c.x : (axis == 1) ? c.y : c.z;
					const RayTriangle &t = triangles[ti];
					const Vec3 tmin = MinV(t.v0, MinV(t.v1, t.v2));
					const Vec3 tmax = MaxV(t.v0, MaxV(t.v1, t.v2));
					if (v < plane) { lmin = MinV(lmin, tmin); lmax = MaxV(lmax, tmax); lcount++; }
					else           { rmin = MinV(rmin, tmin); rmax = MaxV(rmax, tmax); rcount++; }
				}
				if (lcount == 0 || rcount == 0)
					continue;
				const Vec3 le = lmax - lmin, re = rmax - rmin;
				const f32 la = 2.f * (le.x*le.y + le.y*le.z + le.z*le.x);
				const f32 ra = 2.f * (re.x*re.y + re.y*re.z + re.z*re.x);
				const f32 cost = la * (f32)lcount + ra * (f32)rcount;
				if (cost < bestCost) { bestCost = cost; bestAxis = axis; bestSplit = plane; }
			}
		}

		// No split was better than keeping this a leaf. Happens for
		// coincident centroids, and stopping is the right answer: forcing
		// a split there recurses forever.
		if (bestCost == FLT_MAX)
			return;

		// Partition in place.
		uint32 i = node.firstOrLeft;
		uint32 j = node.firstOrLeft + node.count - 1;
		while (i <= j)
		{
			const Vec3 &c = centroids[triangleIndices[i]];
			const f32 v = (bestAxis == 0) ? c.x : (bestAxis == 1) ? c.y : c.z;
			if (v < bestSplit) i++;
			else
			{
				std::swap(triangleIndices[i], triangleIndices[j]);
				if (j == node.firstOrLeft) break;
				j--;
			}
		}
		const uint32 leftCount = i - node.firstOrLeft;
		if (leftCount == 0 || leftCount == node.count)
			return;

		const uint32 leftIndex = (uint32)nodes.size();
		nodes.push_back(BVHNode());
		nodes.push_back(BVHNode());
		// `node` is a reference into `nodes`, which just reallocated.
		nodes[nodeIndex].firstOrLeft = leftIndex;
		const uint32 first = node.firstOrLeft; // read before the write above invalidates intent
		(void)first;

		nodes[leftIndex].firstOrLeft = i - leftCount;
		nodes[leftIndex].count = leftCount;
		nodes[leftIndex + 1].firstOrLeft = i;
		nodes[leftIndex + 1].count = nodes[nodeIndex].count - leftCount;
		nodes[nodeIndex].count = 0; // interior

		UpdateNodeBounds(leftIndex);
		UpdateNodeBounds(leftIndex + 1);
		Subdivide(leftIndex, maxLeafSize, centroids);
		Subdivide(leftIndex + 1, maxLeafSize, centroids);
	}

	void RayScene::Build(const uint32 maxLeafSize)
	{
		nodes.clear();
		triangleIndices.clear();
		if (triangles.empty())
			return;

		triangleIndices.resize(triangles.size());
		std::vector<Vec3> centroids(triangles.size());
		for (size_t i = 0; i < triangles.size(); i++)
		{
			triangleIndices[i] = (uint32)i;
			const RayTriangle &t = triangles[i];
			centroids[i] = (t.v0 + t.v1 + t.v2) * (1.f / 3.f);
		}

		// Reserved up front because Subdivide holds references into this
		// vector across pushes - it re-reads through nodes[] after every
		// growth, but reserving also avoids the repeated reallocation of
		// a tree with thousands of nodes.
		nodes.reserve(triangles.size() * 2 + 1);
		nodes.push_back(BVHNode());
		nodes[0].firstOrLeft = 0;
		nodes[0].count = (uint32)triangles.size();
		UpdateNodeBounds(0);
		Subdivide(0, maxLeafSize == 0 ? 1 : maxLeafSize, centroids);
	}

	uint32 RayScene::MaxDepth() const
	{
		if (nodes.empty())
			return 0;
		// Iterative, because a degenerate tree is exactly the case this
		// is measuring and recursing through it would overflow the stack
		// before reporting the number.
		std::vector<std::pair<uint32,uint32> > stack;
		stack.push_back(std::make_pair(0u, 1u));
		uint32 deepest = 0;
		while (!stack.empty())
		{
			const uint32 n = stack.back().first;
			const uint32 d = stack.back().second;
			stack.pop_back();
			deepest = std::max(deepest, d);
			if (nodes[n].count == 0)
			{
				stack.push_back(std::make_pair(nodes[n].firstOrLeft, d + 1));
				stack.push_back(std::make_pair(nodes[n].firstOrLeft + 1, d + 1));
			}
		}
		return deepest;
	}

	bool RayScene::Intersect(const Vec3 &origin, const Vec3 &direction, const f32 tMin, const f32 tMax, RayHit &outHit) const
	{
		outHit = RayHit();
		if (nodes.empty())
			return false;

		const Vec3 invD(1.f / direction.x, 1.f / direction.y, 1.f / direction.z);
		f32 best = tMax;

		// Explicit stack, the same shape the compute kernel will use -
		// the CPU version exists to be the thing that kernel is checked
		// against, so it should not be cleverer than the GPU one can be.
		uint32 stack[64];
		uint32 depth = 0;
		uint32 current = 0;

		while (true)
		{
			const BVHNode &node = nodes[current];
			if (node.count > 0)
			{
				for (uint32 i = 0; i < node.count; i++)
				{
					const uint32 ti = triangleIndices[node.firstOrLeft + i];
					f32 t, u, v;
					if (IntersectTriangle(origin, direction, triangles[ti], t, u, v)
						&& t > tMin && t < best)
					{
						best = t;
						outHit.t = t; outHit.u = u; outHit.v = v;
						outHit.triangle = ti; outHit.hit = true;
					}
				}
			}
			else
			{
				uint32 near = node.firstOrLeft;
				uint32 far = node.firstOrLeft + 1;
				f32 dNear = IntersectAABB(origin, invD, nodes[near].boundsMin, nodes[near].boundsMax, best);
				f32 dFar  = IntersectAABB(origin, invD, nodes[far].boundsMin, nodes[far].boundsMax, best);
				// Closer child first: the far one is often culled by the
				// hit the near one produces, which is the whole reason a
				// BVH beats a list.
				if (dNear > dFar) { std::swap(dNear, dFar); std::swap(near, far); }
				if (dNear != FLT_MAX)
				{
					if (dFar != FLT_MAX && depth < 64)
						stack[depth++] = far;
					current = near;
					continue;
				}
			}
			if (depth == 0)
				break;
			current = stack[--depth];
		}
		return outHit.hit;
	}

	bool RayScene::IntersectBruteForce(const Vec3 &origin, const Vec3 &direction, const f32 tMin, const f32 tMax, RayHit &outHit) const
	{
		outHit = RayHit();
		f32 best = tMax;
		for (size_t i = 0; i < triangles.size(); i++)
		{
			f32 t, u, v;
			if (IntersectTriangle(origin, direction, triangles[i], t, u, v) && t > tMin && t < best)
			{
				best = t;
				outHit.t = t; outHit.u = u; outHit.v = v;
				outHit.triangle = (uint32)i; outHit.hit = true;
			}
		}
		return outHit.hit;
	}

};
