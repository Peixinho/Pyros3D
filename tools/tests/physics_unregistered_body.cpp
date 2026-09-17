// A physics component must be inert between construction and registration.
//
// A body is not built when addComponent() returns - it is built when the
// scene graph next picks the component up. Everything a script does while
// assembling a rig in init() (a ragdoll lays out fifteen bodies, sets their
// mass and damping, then joints them) therefore runs in that window, and the
// Box3D backend's answer is to fetch the component's stored handles and
// no-op when there are none.
//
// That answer only works if "none" is spelled NULL. rigidBodyPTR used to be
// left uninitialized by the constructor, so before registration it held
// whatever the allocator had in that word; the guards read it as a live
// pointer and dereferenced it. Zeroed fresh pages on macOS and Linux made it
// look fine, while on Windows - where the heap recycles blocks - building a
// ragdoll in init() was an access violation inside Box3DPhysics::SetMass.
//
// So the memory is poisoned here rather than trusted: an allocator that hands
// back zeros would let the old bug pass on every platform.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       -I src/Pyros3D/Ext/box3d/include \
//       tools/tests/physics_unregistered_body.cpp -o /tmp/physics_unregistered_body \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//
// The box3d include path is not optional: this pulls in Box3DPhysics.h, whose
// <box3d/box3d.h> lives under the vendored Ext tree and is not on the default
// search path. Without it the build dies before the test says anything, which
// reads as "this test is broken" rather than "the command is missing a -I".
//   /tmp/physics_unregistered_body
#include <Pyros3D/Physics/Components/Box/PhysicsBox.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>

#include <cstdio>
#include <cstring>
#include <new>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	if (!cond) failures++;
}

int main()
{
	// Every byte 0xFF, so an uninitialized member reads back as a pointer
	// that is non-NULL and unmapped - exactly the shape of the value the
	// Windows heap was handing over, and the one whose dereference faults at
	// 0xffffffffffffffff.
	alignas(16) unsigned char storage[sizeof(PhysicsBox) * 2];
	memset(storage, 0xFF, sizeof(storage));

	Box3DPhysics physics;
	PhysicsBox* box = new (storage) PhysicsBox(&physics, 0.5f, 0.5f, 0.5f, 10.f, false);

	check(box->GetRigidBodyPTR() == NULL,
		"a freshly constructed body has no backend handles");
	check(box->RigidBodyRegistered() == false,
		"and does not claim to be registered");

	// The calls a rig makes on a body it has just created. Each one reaches
	// Box3DPhysics, which must find no handles and return. Reaching the end
	// of this block at all is the test: the old code faulted here.
	box->SetMass(12.f);
	box->SetLinearDamping(0.35f);
	box->SetAngularDamping(0.25f);
	box->SetGravityScale(1.f);
	box->SetPosition(Vec3(1.f, 2.f, 3.f));
	box->SetRotation(Vec3(0.f, 1.f, 0.f));
	box->SetLinearVelocity(Vec3(0.f, 0.f, 0.f));
	box->CleanForces();
	box->Activate();
	check(true, "every setter is a no-op before registration, not a crash");

	// The cached mass still updates - SetMass owns that half regardless of
	// whether a body exists yet, so the value is there when one does.
	check(box->GetMass() == 12.f, "SetMass still records the mass it was given");

	// A joint between two unregistered bodies reports "not made" rather than
	// building one against invalid ids.
	PhysicsBox* other = new (storage + sizeof(PhysicsBox))
		PhysicsBox(&physics, 0.5f, 0.5f, 0.5f, 10.f, false);
	check(physics.CreateSphericalJoint(box, other, Vec3(), 0.5f) == 0,
		"jointing two unregistered bodies makes no joint");

	other->~PhysicsBox();
	box->~PhysicsBox();

	// ---- leaving a scene and coming back ------------------------------
	//
	// The editor removes objects from the scene and adds them back every
	// time it stops play mode. Unregister() used to null the component's
	// engine pointer, so the Register() on the way back in dereferenced
	// NULL - a crash at 0x0 on every platform, which nobody reached while
	// play itself was still crashing on Windows.
	//
	// Both halves matter: it must not crash, AND the body must actually
	// come back. Silently returning an object to the scene with no physics
	// in it would be the same bug wearing a quieter coat.
	{
		Box3DPhysics world;
		world.InitPhysics();

		SceneGraph scene;
		std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
		std::shared_ptr<IPhysicsComponent> body = world.CreateBox(0.5f, 0.5f, 0.5f, 10.f, false);
		go->Add(body);
		scene.Add(go);
		scene.Update(0.016);
		check(body->RigidBodyRegistered(), "a body in the scene has its backend handles");

		scene.Remove(go);
		check(!body->RigidBodyRegistered(), "leaving the scene gives them up");

		scene.Add(go);
		scene.Update(0.016);
		check(body->RigidBodyRegistered(), "and coming back builds them again");

		// Unregister is reached unconditionally by
		// GameObject::UnregisterComponents(), so it has to survive being
		// called on a component that has already left.
		scene.Remove(go);
		scene.Remove(go);
		check(!body->RigidBodyRegistered(), "removing twice is harmless");
	}

	printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
	return failures ? 1 : 0;
}
