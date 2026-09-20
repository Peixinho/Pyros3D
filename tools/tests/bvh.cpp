// The BVH, checked against the answer that cannot be wrong.
//
// A BVH bug does not crash and does not look broken. A slightly wrong
// slab test drops occasional hits, which reads as speckled noise in a
// path tracer and gets blamed on sampling. A wrong traversal order
// returns a farther hit than the nearest one, which reads as light
// leaking through walls and gets blamed on the GI algorithm. So every
// check here compares BVH traversal against a linear scan over every
// triangle, on the same rays, and requires the SAME triangle and the
// same t - not merely "a hit".
//
//   c++ -std=c++17 -I include tools/tests/bvh.cpp \
//       -o /tmp/bvh -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/bvh
#include <Pyros3D/Rendering/GI/RayScene.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace p3d;
static int failures = 0;

static void check(bool c, const std::string &what, const std::string &extra = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", what.c_str(), extra.empty()?"":" - ", extra.c_str());
	if(!c) failures++;
}

// Deterministic, so a failure is reproducible. rand() is not specified
// across platforms and this test compares two implementations against
// each other on identical input - that input has to be identical.
static uint32 rngState = 12345u;
static f32 Rand01()
{
	rngState = rngState * 1664525u + 1013904223u;
	return (f32)((rngState >> 8) & 0xFFFFFF) / (f32)0xFFFFFF;
}
static f32 RandRange(f32 a, f32 b) { return a + (b - a) * Rand01(); }

static void AddTriangle(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c)
{
	RayTriangle t;
	t.v0 = a; t.v1 = b; t.v2 = c;
	const Vec3 n = (b - a).cross(c - a).normalize();
	t.n0 = t.n1 = t.n2 = n;
	t.materialIndex = 0;
	s.triangles.push_back(t);
}

int main()
{
	// ---- an empty scene must not pretend ------------------------------
	{
		RayScene s;
		s.Build();
		RayHit h;
		check(!s.Intersect(Vec3(0,0,0), Vec3(0,0,1), 0.001f, 1e30f, h), "an empty BVH hits nothing");
		check(s.NodeCount() == 0, "an empty scene builds no nodes");
	}

	// ---- one triangle, hit and miss by construction --------------------
	{
		RayScene s;
		AddTriangle(s, Vec3(-1,-1,5), Vec3(1,-1,5), Vec3(0,1,5));
		s.Build();
		RayHit h;
		check(s.Intersect(Vec3(0,0,0), Vec3(0,0,1), 0.001f, 1e30f, h), "a ray down the axis hits");
		check(fabsf(h.t - 5.f) < 1e-4f, "and at the right distance", "t=" + std::to_string(h.t));
		check(!s.Intersect(Vec3(0,0,0), Vec3(0,0,-1), 0.001f, 1e30f, h), "the opposite direction misses");
		check(!s.Intersect(Vec3(0,5,0), Vec3(0,0,1), 0.001f, 1e30f, h), "a ray passing above misses");
		// tMax must actually clip.
		check(!s.Intersect(Vec3(0,0,0), Vec3(0,0,1), 0.001f, 4.f, h), "tMax clips a hit beyond it");
	}

	// ---- nearest hit, not any hit --------------------------------------
	//
	// Three parallel walls. Traversal that returns the wrong one is the
	// bug that reads as light leaking through geometry.
	{
		RayScene s;
		AddTriangle(s, Vec3(-9,-9,10), Vec3(9,-9,10), Vec3(0,9,10));
		AddTriangle(s, Vec3(-9,-9,20), Vec3(9,-9,20), Vec3(0,9,20));
		AddTriangle(s, Vec3(-9,-9,30), Vec3(9,-9,30), Vec3(0,9,30));
		s.Build(1);
		RayHit h;
		check(s.Intersect(Vec3(0,0,0), Vec3(0,0,1), 0.001f, 1e30f, h), "hits the stack of walls");
		check(fabsf(h.t - 10.f) < 1e-4f, "returns the NEAREST wall", "t=" + std::to_string(h.t));
		// And from behind, the nearest is the far one.
		check(s.Intersect(Vec3(0,0,40), Vec3(0,0,-1), 0.001f, 1e30f, h) && fabsf(h.t - 10.f) < 1e-4f,
			"nearest from the other side too");
	}

	// ---- the real check: agree with brute force on random rays ---------
	//
	// A pseudo-random soup of triangles, then thousands of random rays
	// through it. Every ray must produce the identical result from the
	// tree and from the linear scan.
	{
		RayScene s;
		const uint32 kTris = 900;
		for (uint32 i = 0; i < kTris; i++)
		{
			const Vec3 c(RandRange(-20,20), RandRange(-20,20), RandRange(-20,20));
			// Mixed sizes on purpose: uniform triangles make a suspiciously
			// well-balanced tree and hide splitting bugs.
			const f32 sz = RandRange(0.2f, 3.5f);
			AddTriangle(s,
				c + Vec3(RandRange(-sz,sz), RandRange(-sz,sz), RandRange(-sz,sz)),
				c + Vec3(RandRange(-sz,sz), RandRange(-sz,sz), RandRange(-sz,sz)),
				c + Vec3(RandRange(-sz,sz), RandRange(-sz,sz), RandRange(-sz,sz)));
		}
		s.Build(4);
		check(s.NodeCount() > 1, "the tree actually subdivided",
			std::to_string(s.NodeCount()) + " nodes, depth " + std::to_string(s.MaxDepth()));
		// A tree that degenerated to a list would still pass every
		// correctness check below while being useless.
		check(s.MaxDepth() < 64, "depth fits the traversal stack",
			"depth " + std::to_string(s.MaxDepth()));

		const uint32 kRays = 4000;
		uint32 mismatches = 0, hits = 0;
		f32 worstT = 0.f;
		for (uint32 r = 0; r < kRays; r++)
		{
			const Vec3 o(RandRange(-30,30), RandRange(-30,30), RandRange(-30,30));
			Vec3 d(RandRange(-1,1), RandRange(-1,1), RandRange(-1,1));
			if (d.magnitude() < 1e-4f) continue;
			d.normalizeSelf();

			RayHit a, b;
			const bool ha = s.Intersect(o, d, 0.001f, 1e30f, a);
			const bool hb = s.IntersectBruteForce(o, d, 0.001f, 1e30f, b);
			if (ha != hb) { mismatches++; continue; }
			if (!ha) continue;
			hits++;
			// Same triangle, or at worst the same distance - two
			// coincident surfaces can legitimately tie.
			if (a.triangle != b.triangle && fabsf(a.t - b.t) > 1e-4f) mismatches++;
			worstT = std::max(worstT, fabsf(a.t - b.t));
		}
		check(hits > kRays / 20, "the random rays actually hit things",
			std::to_string(hits) + " of " + std::to_string(kRays));
		check(mismatches == 0, "BVH traversal agrees with brute force on every ray",
			std::to_string(mismatches) + " mismatches, worst |dt| = " + std::to_string(worstT));
	}

	// ---- degenerate input must not hang or explode ---------------------
	//
	// Coincident centroids give the splitter nothing to split on. The
	// naive response is to recurse forever.
	{
		RayScene s;
		for (uint32 i = 0; i < 64; i++)
			AddTriangle(s, Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0));
		s.Build(4);
		check(s.NodeCount() > 0, "64 identical triangles build without recursing forever",
			std::to_string(s.NodeCount()) + " nodes");
		RayHit h;
		check(s.Intersect(Vec3(0.25f,0.25f,-1), Vec3(0,0,1), 0.001f, 1e30f, h),
			"and are still hit correctly");
	}

	printf("\n%s  bvh: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
