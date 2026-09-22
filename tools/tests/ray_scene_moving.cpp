// Geometry that moves must move the light it blocks.
//
// The triangles are baked into world space at extraction time and the
// BVH is built over them once - which is right for the tracer's inner
// loop and wrong the moment anything moves. Until RefreshTransforms
// and RefitBVH existed, a door that opened went on blocking light
// where it used to be, and nothing said so.
//
// Two things are checked here and they are different: that the
// TRIANGLES follow the object, and that the TREE follows the
// triangles. The second is the one that fails silently - a stale BVH
// still returns hits, just against bounds that no longer contain
// anything, so rays miss geometry that is plainly in front of them.
//
//   c++ -std=c++17 -I include $(pkg-config --cflags freetype2) \
//       tools/tests/ray_scene_moving.cpp -o /tmp/ray_scene_moving \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
#include <Pyros3D/Rendering/GI/RayScene.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <memory>
#include <cmath>
#include <cstdio>
#include <string>

using namespace p3d;
static int failures = 0;
static void check(bool c, const std::string &w, const std::string &e = std::string())
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", w.c_str(), e.empty()?"":" - ", e.c_str());
	if(!c) failures++;
}

// A quad, built directly rather than through a scene graph: this test
// is about RayScene's own bookkeeping, and a SceneGraph needs a render
// device to hold a material.
static void AddQuad(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d)
{
	RayTriangle t1, t2;
	t1.v0=a; t1.v1=b; t1.v2=c;
	t2.v0=a; t2.v1=c; t2.v2=d;
	const Vec3 n = (b-a).cross(c-a).normalize();
	t1.n0=t1.n1=t1.n2=n; t2.n0=t2.n1=t2.n2=n;
	s.triangles.push_back(t1); s.triangles.push_back(t2);
}

int main()
{
	// A blocker at x = 0, and rays fired along +X at it.
	RayScene scene;
	AddQuad(scene, Vec3(0,-1,-1), Vec3(0,-1,1), Vec3(0,1,1), Vec3(0,1,-1));
	// Something else to give the tree more than one node to refit.
	AddQuad(scene, Vec3(6,-1,-1), Vec3(6,-1,1), Vec3(6,1,1), Vec3(6,1,-1));
	scene.Build(2);
	check(scene.TriangleCount() == 4, "the scene has both quads");
	check(scene.NodeCount() > 1, "and a tree with more than a root",
		std::to_string(scene.NodeCount()));

	RayHit hit;
	const Vec3 from(-3.f, 0.f, 0.f), along(1.f, 0.f, 0.f);
	check(scene.Intersect(from, along, 1e-4f, 100.f, hit) && fabsf(hit.t - 3.f) < 1e-3f,
		"a ray hits the blocker where it stands", "t = " + std::to_string(hit.t));

	// ---- move the triangles by hand, then refit --------------------------
	//
	// RefreshTransforms needs a GameObject to read a matrix from, which
	// needs a scene and a render device. Moving the triangles directly
	// tests the half that matters here: whether the TREE follows.
	{
		// ACROSS the ray, not along it.
		//
		// Moving it along +X leaves it inside its own (flat) bounds,
		// the ray still enters the node, and the moved triangle is
		// found anyway - so a stale tree would have looked fine and
		// this test would have proved nothing. Moving it aside puts
		// the geometry outside bounds the ray never crosses, which is
		// exactly how a stale tree loses geometry in a real scene.
		for (uint32 t = 0; t < 2; t++)
		{
			scene.triangles[t].v0.y += 3.f;
			scene.triangles[t].v1.y += 3.f;
			scene.triangles[t].v2.y += 3.f;
		}
		const Vec3 acrossFrom(-3.f, 3.f, 0.f);

		// Before the refit the tree still describes the old position:
		// no error, no warning, the blocker has simply vanished.
		RayHit stale;
		const bool hitStale = scene.Intersect(acrossFrom, along, 1e-4f, 100.f, stale);
		check(!hitStale, "a stale tree loses geometry that has moved out of its bounds",
			hitStale ? "still hit at t = " + std::to_string(stale.t) : "missed, as expected");

		scene.RefitBVH();
		RayHit moved;
		check(scene.Intersect(acrossFrom, along, 1e-4f, 100.f, moved),
			"after a refit the ray finds the blocker again");
		check(fabsf(moved.t - 3.f) < 1e-3f, "at its new position",
			"t = " + std::to_string(moved.t) + ", expected 3");

		// And the refit must agree with a full rebuild - same answers,
		// not merely plausible ones.
		RayScene rebuilt;
		rebuilt.triangles = scene.triangles;
		rebuilt.Build(2);
		f32 worst = 0.f;
		uint32 disagreements = 0;
		for (int32 i = -20; i <= 20; i++)
		{
			Vec3 dir(1.f, (f32)i * 0.03f, (f32)i * 0.017f);
			dir.normalizeSelf();
			RayHit a, b;
			// Fired from both heights, so the sweep covers the moved
			// quad and the one that stayed put.
			const Vec3 origin = (i % 2) ? from : acrossFrom;
			const bool ha = scene.Intersect(origin, dir, 1e-4f, 100.f, a);
			const bool hb = rebuilt.Intersect(origin, dir, 1e-4f, 100.f, b);
			if (ha != hb) { disagreements++; continue; }
			if (ha) worst = fmaxf(worst, fabsf(a.t - b.t));
		}
		check(disagreements == 0 && worst < 1e-4f,
			"and a refitted tree answers exactly as a rebuilt one does",
			std::to_string(disagreements) + " disagreements, worst |dt| = "
			+ std::to_string(worst));
	}

	// ---- the bounds have to actually shrink, not just grow ---------------
	//
	// A refit that only ever unions is the easy mistake: everything
	// still gets hit, nothing is ever missed, and the tree degenerates
	// into one big box that traverses like a list. Moving an object
	// back must give the bounds back.
	{
		RayScene s2;
		AddQuad(s2, Vec3(0,-1,-1), Vec3(0,-1,1), Vec3(0,1,1), Vec3(0,1,-1));
		s2.Build(2);
		const Vec3 before = s2.nodes[0].boundsMax;
		for (uint32 t = 0; t < s2.triangles.size(); t++)
		{
			s2.triangles[t].v0.x += 10.f;
			s2.triangles[t].v1.x += 10.f;
			s2.triangles[t].v2.x += 10.f;
		}
		s2.RefitBVH();
		const Vec3 moved = s2.nodes[0].boundsMin;
		check(moved.x > before.x + 5.f,
			"a refit moves bounds rather than only growing them",
			"min.x " + std::to_string(before.x) + " -> " + std::to_string(moved.x));
	}

	// ---- the other half: does an object moving move its triangles? -------
	//
	// RefreshTransforms compares each instance's owner against the
	// matrix its triangles were baked with. Built by hand here rather
	// than through BuildFromScene, because a RenderingComponent needs
	// real geometry buffers and therefore a render device, and none of
	// what is being tested does.
	{
		RayScene s3;
		AddQuad(s3, Vec3(0,-1,-1), Vec3(0,-1,1), Vec3(0,1,1), Vec3(0,1,-1));
		s3.localTriangles = s3.triangles;      // identity so far
		s3.Build(2);

		std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
		go->RefreshTransformation();
		RayInstance inst;
		inst.owner = go;
		inst.firstTriangle = 0;
		inst.triangleCount = (uint32)s3.triangles.size();
		inst.world = go->GetWorldTransformation();
		s3.instances.push_back(inst);

		check(s3.RefreshTransforms() == RaySceneChange::None,
			"an object that has not moved reports no change");

		go->SetPosition(Vec3(0.f, 4.f, 0.f));
		// SetPosition only flips a dirty flag - SceneGraph::Update
		// normally does this. See GameObject::RefreshTransformation.
		go->RefreshTransformation();
		check(s3.RefreshTransforms() == RaySceneChange::Moved,
			"and one that has moved reports that it did");
		check(fabsf(s3.triangles[0].v0.y - 3.f) < 1e-4f,
			"the triangles followed it",
			"v0.y = " + std::to_string(s3.triangles[0].v0.y) + ", expected 3");
		check(s3.RefreshTransforms() == RaySceneChange::None,
			"and a second call finds nothing left to do");

		// The owner going away is not a move - the triangle list is
		// describing objects that no longer exist, and a refit cannot
		// fix that.
		go.reset();
		check(s3.RefreshTransforms() == RaySceneChange::NeedsRebuild,
			"a destroyed owner asks for a rebuild rather than a refit");
	}

	printf("\n%s  ray_scene_moving: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
