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

	// -------- and removing a component from a child unregisters it --------
	// Not the same path as detaching the object: GameObject::Remove(
	// IComponent*) has to find the scene by walking up, because a child
	// never holds a Scene pointer of its own.
	child->Remove(std::static_pointer_cast<IComponent>(childLight));
	check(lightsOn(&scene) == 2, "removing a component from a child unregisters just that one");
	child->Add(std::static_pointer_cast<IComponent>(childLight));
	scene.Update(0.048);
	check(lightsOn(&scene) == 3, "and adding it back registers it again");

	// -------- a child is not also a root --------
	// SceneGraph::Add() puts an object in the scene's root lists, and
	// SaveScene writes every entry there as a root - so an object that
	// stayed in those lists after being re-parented was traversed twice and
	// written to the scene file twice, once nested and once at top level.
	{
		SceneGraph s2;
		std::shared_ptr<GameObject> a = std::make_shared<GameObject>();
		std::shared_ptr<GameObject> b = std::make_shared<GameObject>();
		std::shared_ptr<PointLight> bl = std::make_shared<PointLight>(Vec4(1, 1, 1, 1), 10.f);
		b->Add(std::static_pointer_cast<IComponent>(bl));
		s2.Add(a);
		s2.Add(b);
		check(s2.GetAllGameObjectList().size() == 2, "two roots before re-parenting");
		a->Add(b);
		check(s2.GetAllGameObjectList().size() == 1, "re-parenting takes the child out of the scene's root list");
		s2.Update(0.0);
		check(lightsOn(&s2) == 1, "and its components stay registered exactly once");
	}

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

	// -------- a child that outlives its parent --------
	//
	// _Owner is a raw back-pointer and FindScene() walks it upwards from any
	// node, so a child still holding a dead parent is a walk through freed
	// memory the next time one of its components is removed. That is not a
	// contrived ownership puzzle: the editor's SceneObject registry holds a
	// reference to every object, and so does any script handle, so a child
	// routinely outlives the parent whose shared_ptr the scene just dropped.
	// It crashed Stop Play on Windows.
	//
	// Checked as state rather than by provoking the crash: whether a freed
	// page is still readable is the allocator's business, and on macOS it
	// usually is - so the dangling read "passes" on this machine while
	// failing on Windows. The pointer being cleared is the thing that is
	// actually true or false.
	{
		SceneGraph s3;
		std::shared_ptr<GameObject> p = std::make_shared<GameObject>();
		std::shared_ptr<GameObject> c = std::make_shared<GameObject>();
		p->Add(c);
		s3.Add(p);
		s3.Update(0.0);
		check(c->HaveParent() && c->GetParent() == p.get(), "a child knows its parent");

		// The scene lets go, and nothing else holds the parent - but `c` is
		// still a live reference to the child.
		s3.Remove(p);
		p.reset();
		check(!c->HaveParent(), "a destroyed parent leaves no parent behind");
		check(c->GetParent() == NULL, "and no pointer to walk into");
	}

	// -------- a component that outlives its owner --------
	//
	// IComponent::Owner is the same kind of raw back-pointer as _Owner above,
	// and the one the editor actually calls through:
	// SceneObjects::DestroySceneObject() does
	//
	//     if (component->GetOwner() != NULL) component->GetOwner()->Remove(component);
	//
	// so a stale Owner is not a stale pointer sitting harmlessly in a field,
	// it is a method call on freed memory. That was the Stop Play crash on
	// Windows, inside Remove() -> FindScene().
	{
		SceneGraph s4;
		std::shared_ptr<PointLight> l = std::make_shared<PointLight>(Vec4(1, 1, 1, 1), 10.f);
		std::shared_ptr<IComponent> comp = std::static_pointer_cast<IComponent>(l);
		{
			std::shared_ptr<GameObject> owner = std::make_shared<GameObject>();
			owner->Add(comp);
			s4.Add(owner);
			s4.Update(0.0);
			check(comp->GetOwner() == owner.get(), "a component knows its owner");
			s4.Remove(owner);
		}   // the last reference to the GameObject goes here
		check(comp->GetOwner() == NULL, "a destroyed owner leaves no owner behind");
	}

	// -------- an object that outlives its scene --------
	//
	// GameObject::Scene and RenderingComponent::Scene are both raw SceneGraph
	// pointers, and an object outliving the scene is the normal case when a
	// document is closed: the editor's registry still holds it. FindScene()
	// returns whatever Scene says and hands it to Unregister().
	{
		std::shared_ptr<GameObject> survivor = std::make_shared<GameObject>();
		std::shared_ptr<PointLight> l = std::make_shared<PointLight>(Vec4(1, 1, 1, 1), 10.f);
		survivor->Add(std::static_pointer_cast<IComponent>(l));
		{
			SceneGraph doomed;
			doomed.Add(survivor);
			doomed.Update(0.0);
			check(lightsOn(&doomed) == 1, "the light registers with its scene");
		}   // the scene dies here, the object does not
		check(survivor->GetScene() == NULL, "a destroyed scene leaves no scene behind");
		// And the component came out unregistered, so nothing is still listed
		// in a scene that no longer exists.
		survivor->Remove(l.get());
		check(true, "removing its component afterwards is not a crash");
	}

	// -------- look-at, in both directions --------
	//
	// UpdateTransformation() dereferences _IsLookingAtGameObjectPTR every
	// frame the flag is set, and a look-at target is not owned by the object
	// watching it - so the target dying is a dangling read per frame, with no
	// event to hang a fix on except the target's own destructor.
	{
		SceneGraph s5;
		std::shared_ptr<GameObject> watcher = std::make_shared<GameObject>();
		{
			std::shared_ptr<GameObject> target = std::make_shared<GameObject>();
			target->SetPosition(Vec3(5.f, 0.f, 0.f));
			watcher->LookAt(target.get());
			s5.Add(watcher);
			s5.Add(target);
			s5.Update(0.0);
			check(watcher->IsLookingAtGameObject(), "the watcher is aimed at the target");
			s5.Remove(target);
		}   // target destroyed, watcher still live and still in the scene
		check(!watcher->IsLookingAtGameObject(), "a destroyed target stops being looked at");
		// The read that used to be a dangling one happens in here.
		s5.Update(0.016);
		check(true, "and the next update does not read through it");
	}

	// The other direction: the WATCHER dying must not leave the target's
	// list naming it, or the target's destructor writes into freed memory.
	{
		std::shared_ptr<GameObject> target = std::make_shared<GameObject>();
		{
			std::shared_ptr<GameObject> watcher = std::make_shared<GameObject>();
			watcher->LookAt(target.get());
		}   // watcher destroyed first
		check(true, "a destroyed watcher deregisters itself from its target");
	}   // target destroyed second - must not touch the dead watcher

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
