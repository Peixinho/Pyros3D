// World and previous-world matrices through a parent/child hierarchy.
//
// The previous-frame matrix is what motion vectors are built from, so a
// parent whose "previous" equals its "current" reads as not moving at all.
// Each child's update used to re-run its parent's UpdateTransformation(),
// which rolled the parent's previous matrix a second time in the same frame.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/transform_hierarchy.cpp -o /tmp/transform_hierarchy \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/transform_hierarchy
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>

#include <cmath>
#include <cstdio>
#include <memory>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	fflush(stdout);
	if (!cond) failures++;
}

static bool near(const Vec3 &a, const Vec3 &b)
{
	return fabsf(a.x - b.x) < 1e-4f && fabsf(a.y - b.y) < 1e-4f && fabsf(a.z - b.z) < 1e-4f;
}

// Moves another object from inside its own Update() - i.e. after that object
// may already have been through this frame's traversal.
class Mover : public GameObject
{
public:
	GameObject* target = NULL;
	Vec3 to;
	void Update(const f64) override { if (target) target->SetPosition(to); }
};

int main()
{
	SceneGraph scene;

	std::shared_ptr<GameObject> parent = std::make_shared<GameObject>();
	std::shared_ptr<GameObject> child = std::make_shared<GameObject>();
	std::shared_ptr<GameObject> grandchild = std::make_shared<GameObject>();
	child->SetPosition(Vec3(0.f, 1.f, 0.f));
	grandchild->SetPosition(Vec3(0.f, 0.f, 1.f));
	parent->Add(child);
	child->Add(grandchild);
	scene.Add(parent);

	parent->SetPosition(Vec3(1.f, 0.f, 0.f));
	scene.Update(0.0);
	parent->SetPosition(Vec3(2.f, 0.f, 0.f));
	scene.Update(0.016);

	check(near(parent->GetWorldPosition(), Vec3(2.f, 0.f, 0.f)), "parent world position");
	check(near(child->GetWorldPosition(), Vec3(2.f, 1.f, 0.f)), "child world = parent * local");
	check(near(grandchild->GetWorldPosition(), Vec3(2.f, 1.f, 1.f)), "grandchild world");
	check(near(parent->GetPrvWorldTransformation().GetTranslation(), Vec3(1.f, 0.f, 0.f)), "parent previous world is last frame's");
	check(near(child->GetPrvWorldTransformation().GetTranslation(), Vec3(1.f, 1.f, 0.f)), "child previous world is last frame's");
	check(near(grandchild->GetPrvWorldTransformation().GetTranslation(), Vec3(1.f, 1.f, 1.f)), "grandchild previous world is last frame's");

	// Not moving: previous catches up with current after one more frame.
	scene.Update(0.032);
	check(near(parent->GetPrvWorldTransformation().GetTranslation(), Vec3(2.f, 0.f, 0.f)), "still parent: previous == current");

	// Outside any scene, RefreshTransformation() is the only update there is,
	// and it has to see an ancestor's change made since the last refresh.
	std::shared_ptr<GameObject> looseParent = std::make_shared<GameObject>();
	std::shared_ptr<GameObject> looseChild = std::make_shared<GameObject>();
	looseChild->SetPosition(Vec3(0.f, 0.f, 3.f));
	looseParent->Add(looseChild);
	looseParent->SetPosition(Vec3(5.f, 0.f, 0.f));
	looseChild->RefreshTransformation();
	check(near(looseChild->GetWorldPosition(), Vec3(5.f, 0.f, 3.f)), "RefreshTransformation walks dirty ancestors");
	looseParent->SetPosition(Vec3(6.f, 0.f, 0.f));
	looseChild->RefreshTransformation();
	check(near(looseChild->GetWorldPosition(), Vec3(6.f, 0.f, 3.f)), "RefreshTransformation sees a later ancestor move");

	// A child that moves its parent from its own Update(): the parent has
	// already been through this frame's traversal, so the child must still
	// see the new position in the same frame.
	SceneGraph scene2;
	std::shared_ptr<GameObject> p2 = std::make_shared<GameObject>();
	std::shared_ptr<Mover> c2 = std::make_shared<Mover>();
	c2->SetPosition(Vec3(0.f, 1.f, 0.f));
	p2->Add(c2);
	scene2.Add(p2);
	scene2.Update(0.0);
	c2->target = p2.get();
	c2->to = Vec3(0.f, 0.f, 7.f);
	scene2.Update(0.016);
	check(near(c2->GetWorldPosition(), Vec3(0.f, 1.f, 7.f)), "child sees a parent it moved this frame");

	printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
