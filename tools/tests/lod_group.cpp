// An object's parts change level of detail together.
//
// A tree is two RenderingComponents on one object - wood and leaves - with
// spheres of their own (the wood's low on the stem, the leaves' up in the
// crown). Measured each from its own sphere they switched at different camera
// distances, and in between, one level's leaves hung where the other level's
// branches were not. GetLODSphere() is one sphere round every part that has
// levels, so they all measure the same distance.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/lod_group.cpp -o /tmp/lod_group -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/lod_group

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <cstdio>
#include <cmath>
#include <memory>

using namespace p3d;

// A part with no mesh, only the sphere a real one would give (no render device needed).
class Part : public Renderable {
public:
	Part(const Vec3 &centre, const f32 radius) { BoundingSphereCenter = centre; BoundingSphereRadius = radius; }
};
static std::shared_ptr<Renderable> part(const Vec3 &c, const f32 r) { return std::make_shared<Part>(c, r); }

static int failures = 0;
static void check(bool ok, const char* what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) failures++; }

int main()
{
	GameObject go;
	// a tree: the stem's sphere low and thin, the crown's high and wide; each part has a
	// coarser level past 80 m
	std::shared_ptr<RenderingComponent> small = std::make_shared<RenderingComponent>(part(Vec3(0.f, 3.f, 0.f), 3.5f));
	std::shared_ptr<RenderingComponent> big = std::make_shared<RenderingComponent>(part(Vec3(0.f, 8.f, 0.f), 5.f));
	small->SetFirstLODDistance(80.f); small->AddLOD(part(Vec3(0.f, 3.f, 0.f), 3.5f), 1e9f);
	big->SetFirstLODDistance(80.f); big->AddLOD(part(Vec3(0.f, 8.f, 0.f), 5.f), 1e9f);
	go.Add(small); go.Add(big);

	Vec3 cs, cb; f32 rs, rb;
	small->GetLODSphere(cs, rs);
	big->GetLODSphere(cb, rb);
	check(fabsf(rs - rb) < 1e-4f && (cs - cb).magnitude() < 1e-4f, "both parts measure from the same sphere");
	check(rb >= 5.f - 1e-4f && (Vec3(0.f, 8.f, 0.f) - cb).magnitude() + 5.f <= rb + 1e-3f
		&& (Vec3(0.f, 3.f, 0.f) - cb).magnitude() + 3.5f <= rb + 1e-3f, "and it holds both parts");
	// the same camera distance gives the same level to both
	// a camera level with the crown, out to the side, so the two old centres are at different distances
	bool same = true, disagreedBefore = false;
	for (f32 x = 70.f; x <= 95.f; x += 0.25f)
	{
		const Vec3 eye(x, 8.f, 0.f);
		const f32 ds = std::max(0.f, eye.distance(cs) - rs), db = std::max(0.f, eye.distance(cb) - rb);
		if (small->LevelAtDistanceSQR(ds * ds) != big->LevelAtDistanceSQR(db * db)) same = false;
		const f32 os = std::max(0.f, eye.distance(Vec3(0.f, 3.f, 0.f)) - 3.5f), ob = std::max(0.f, eye.distance(Vec3(0.f, 8.f, 0.f)) - 5.f);
		if (small->LevelAtDistanceSQR(os * os) != big->LevelAtDistanceSQR(ob * ob)) disagreedBefore = true;
	}
	check(same, "and so the same level at every distance");
	check(disagreedBefore, "(each from its own sphere, they disagreed for a band of distances)");

	// a part alone keeps its own sphere
	GameObject lone;
	std::shared_ptr<RenderingComponent> one = std::make_shared<RenderingComponent>(part(Vec3(0.f, 1.f, 0.f), 2.f));
	one->SetFirstLODDistance(80.f); one->AddLOD(part(Vec3(0.f, 1.f, 0.f), 2.f), 1e9f);
	lone.Add(one);
	Vec3 c1; f32 r1;
	one->GetLODSphere(c1, r1);
	check(fabsf(r1 - 2.f) < 1e-4f && (c1 - Vec3(0.f, 1.f, 0.f)).magnitude() < 1e-4f, "a part on its own keeps its own sphere");

	printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
	return failures ? 1 : 0;
}
