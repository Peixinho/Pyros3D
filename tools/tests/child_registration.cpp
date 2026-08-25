// Child GameObjects must take part in the scene like any other object: their
// components register, update, and unregister again when the subtree leaves.
// Only roots are ever in the SceneGraph's own lists, so this is entirely
// about the traversal walking into GetChildren().
//
// Uses lights as the probe because ILightComponent::GetLightsOnScene() is the
// scene registry a renderer actually reads, and building one needs no render
// device - so this runs headless.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/child_registration.cpp -o /tmp/child_registration \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/child_registration
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

#include <cstdio>
#include <memory>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	if (!cond) failures++;
}

static size_t lightsOn(SceneGraph* scene)
{
	return ILightComponent::GetLightsOnScene(scene).size();
}

int main()
{
	SceneGraph scene;

	std::shared_ptr<GameObject> root = std::make_shared<GameObject>();
	root->SetName("Root");
	std::shared_ptr<PointLight> rootLight = std::make_shared<PointLight>(Vec4(1, 1, 1, 1), 10.f);
	root->Add(std::static_pointer_cast<IComponent>(rootLight));

	std::shared_ptr<GameObject> child = std::make_shared<GameObject>();
	child->SetName("Child");
	child->SetPosition(Vec3(0.f, 5.f, 0.f));
	std::shared_ptr<PointLight> childLight = std::make_shared<PointLight>(Vec4(1, 0, 0, 1), 10.f);
	child->Add(std::static_pointer_cast<IComponent>(childLight));

	std::shared_ptr<GameObject> grandchild = std::make_shared<GameObject>();
	grandchild->SetName("Grandchild");
	std::shared_ptr<PointLight> grandLight = std::make_shared<PointLight>(Vec4(0, 1, 0, 1), 10.f);
	grandchild->Add(std::static_pointer_cast<IComponent>(grandLight));

	child->Add(grandchild);
	root->Add(child);

	// -------- registration reaches the whole subtree --------
	scene.Add(root);
	scene.Update(0.0);
	check(lightsOn(&scene) == 3, "all three lights registered (root + child + grandchild)");

	// -------- and children actually get their transforms --------
	scene.Update(0.016);
	check(child->GetWorldPosition().y == 5.f, "a child's world transform is computed");

	// -------- detaching a subtree takes its components with it --------
	root->Remove(child);
	check(lightsOn(&scene) == 1, "detaching the child unregistered it and its grandchild");

	// -------- and re-parenting registers it again --------
	root->Add(child);
	scene.Update(0.032);
	check(lightsOn(&scene) == 3, "re-parenting re-registers the subtree");

	// -------- removing the root removes everything --------
	scene.Remove(root);
	check(lightsOn(&scene) == 0, "removing the root unregistered the whole tree");

	// -------- a root added with children already attached --------
	{
		SceneGraph fresh;
		std::shared_ptr<GameObject> p = std::make_shared<GameObject>();
		std::shared_ptr<GameObject> c = std::make_shared<GameObject>();
		std::shared_ptr<PointLight> l = std::make_shared<PointLight>(Vec4(1, 1, 1, 1), 10.f);
		c->Add(std::static_pointer_cast<IComponent>(l));
		p->Add(c);
		fresh.Add(p);
		fresh.Update(0.0);
		check(lightsOn(&fresh) == 1, "a subtree built before Add() registers on the first update");
	}

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
