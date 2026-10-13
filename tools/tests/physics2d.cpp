// 2D physics against the real solver, with no window: shapes, collider
// offset, sensors, collision layers, one-way platforms, joints, queries, and
// what happens when a script moves an object that has a body.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/physics2d.cpp -o /tmp/physics2d \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan
//   /tmp/physics2d
//
// Each case builds its own scene and steps the world a fixed number of 60 Hz
// frames, so a number that comes out is the same number every run.
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Physics/Physics2D/Joint2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}
static void nearly(const f32 got, const f32 want, const f32 tol, const std::string &what)
{
	const bool ok = std::fabs(got - want) <= tol;
	printf("%s  %s (got %.4f, want %.4f +-%.3f)\n", ok ? "PASS" : "FAIL", what.c_str(), (double)got, (double)want, (double)tol);
	if (!ok) failures++;
}

// A scene, its world, and the objects made in it kept alive.
struct Rig {
	SceneGraph scene;
	Physics2DWorld world;
	std::vector<std::shared_ptr<GameObject> > objects;
	f64 time = 0.0;

	std::shared_ptr<Physics2D> Body(const std::string &name, const Vec2 &at, const uint32 type,
		const uint32 shape, const Vec2 &size, GameObject* parent = NULL)
	{
		std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
		go->SetName(name);
		go->SetPosition(Vec3(at.x, at.y, 0.f));
		std::shared_ptr<Physics2D> p = std::make_shared<Physics2D>(type, shape, size);
		go->Add(std::static_pointer_cast<IComponent>(p));
		if (parent) parent->Add(go); else scene.Add(go);
		objects.push_back(go);
		return p;
	}
	void Run(const int frames)
	{
		for (int i = 0; i < frames; i++)
		{
			time += 1.0 / 60.0;
			scene.Update(time);
			world.Sync(&scene);
			world.Step(1.0 / 60.0, &scene);
		}
	}
};

static Vec2 PosOf(const std::shared_ptr<Physics2D> &p)
{
	const Vec3 w = p->GetOwner()->GetWorldPosition();
	return Vec2(w.x, w.y);
}

int main()
{
	// --- shapes come to rest where their extents say ------------------------
	{
		Rig r;
		r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> box = r.Body("Box", Vec2(-4, 3), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<Physics2D> ball = r.Body("Ball", Vec2(-2, 3), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.4f, 0.4f));
		std::shared_ptr<Physics2D> cap = r.Body("Capsule", Vec2(0, 3), Body2DType::Dynamic, Shape2DType::Capsule, Vec2(0.3f, 0.9f));
		cap->SetFixedRotation(true);
		std::shared_ptr<Physics2D> tri = r.Body("Tri", Vec2(2, 3), Body2DType::Dynamic, Shape2DType::Polygon, Vec2(0.5f, 0.5f));
		std::vector<Vec2> pts;
		pts.push_back(Vec2(-0.5f, 0.f)); pts.push_back(Vec2(0.5f, 0.f)); pts.push_back(Vec2(0.f, 0.8f));
		check(tri->SetPolygon(pts), "a triangle is accepted as a polygon");
		tri->SetFixedRotation(true);
		std::vector<Vec2> crossed;
		crossed.push_back(Vec2(0, 0)); crossed.push_back(Vec2(1, 1)); crossed.push_back(Vec2(1, 0)); crossed.push_back(Vec2(0, 1));
		check(!tri->SetPolygon(crossed) && tri->GetPolygon().size() == 3, "an outline that crosses itself is refused and the old one kept");
		// The collider sits above the origin: a sprite pivoted at its feet.
		std::shared_ptr<Physics2D> feet = r.Body("Feet", Vec2(4, 3), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.6f));
		feet->SetOffset(Vec2(0.f, 0.6f));
		feet->SetFixedRotation(true);

		r.Run(240);
		nearly(PosOf(box).y, 0.5f, 0.02f, "a box rests half its height above the floor");
		nearly(PosOf(ball).y, 0.4f, 0.02f, "a ball rests its radius above the floor");
		nearly(PosOf(cap).y, 0.9f, 0.02f, "a capsule rests its half-height above the floor");
		nearly(PosOf(tri).y, 0.f, 0.02f, "a polygon rests on its lowest edge");
		nearly(PosOf(feet).y, 0.f, 0.02f, "an offset collider puts the ORIGIN on the floor");
		check(box->IsGrounded() && cap->IsGrounded(), "resting bodies report being grounded");
		const Vec2 gn = box->GetGroundNormal();
		nearly(gn.y, 1.f, 0.01f, "the ground normal of a flat floor points up");
		check(box->GetContactCount() >= 1, "a resting body counts its contact");
		check(box->GetMass() > 0.99f && box->GetMass() < 1.01f, "a 1x1 box of density 1 weighs 1");
	}

	// --- in the air: not grounded, gravity scale, damping -------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> a = r.Body("A", Vec2(0, 100), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<Physics2D> light = r.Body("Light", Vec2(3, 100), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		light->SetGravityScale(0.5f);
		std::shared_ptr<Physics2D> afloat = r.Body("Afloat", Vec2(6, 100), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		afloat->SetGravityScale(0.f);
		r.Run(60);
		check(!a->IsGrounded(), "a falling body is not grounded");
		const f32 fa = 100.f - PosOf(a).y, fl = 100.f - PosOf(light).y;
		nearly(fa, 5.f, 0.3f, "one second of free fall at g=10 is about 5 units");
		nearly(fl / fa, 0.5f, 0.02f, "gravity scale 0.5 falls half as far");
		nearly(PosOf(afloat).y, 100.f, 0.001f, "gravity scale 0 does not fall");
		// Changed while running: applied to the live body.
		afloat->SetGravityScale(1.f);
		r.Run(30);
		check(PosOf(afloat).y < 99.f, "a gravity scale set on a running body takes effect");
	}

	// --- sensors ------------------------------------------------------------
	{
		Rig r;
		r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> zone = r.Body("Zone", Vec2(0, 2), Body2DType::Static, Shape2DType::Box, Vec2(1, 0.5f));
		zone->SetSensor(true);
		std::shared_ptr<Physics2D> ball = r.Body("Ball", Vec2(0, 5), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.3f, 0.3f));
		int zoneEnter = 0, zoneExit = 0, ballEnter = 0, ballExit = 0, ballHitZone = 0;
		Physics2D* seenByZone = NULL;
		Physics2D* seenByBall = NULL;
		zone->OnTriggerEnter = [&](Physics2D* o) { zoneEnter++; seenByZone = o; };
		zone->OnTriggerExit = [&](Physics2D*) { zoneExit++; };
		ball->OnTriggerEnter = [&](Physics2D* o) { ballEnter++; seenByBall = o; };
		ball->OnTriggerExit = [&](Physics2D*) { ballExit++; };
		ball->OnCollisionEnter = [&](Physics2D* o) { if (o == zone.get()) ballHitZone++; };
		r.Run(240);
		check(zoneEnter == 1 && zoneExit == 1, "a sensor hears a body come in and go out, once each");
		check(ballEnter == 1 && ballExit == 1, "and so does the body that passed through");
		check(seenByZone == ball.get() && seenByBall == zone.get(), "each is told who the other was");
		check(ballHitZone == 0, "a sensor is not a collision");
		nearly(PosOf(ball).y, 0.3f, 0.02f, "the ball fell straight through it to the floor");
	}

	// --- contact detail -----------------------------------------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> floor = r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> box = r.Body("Box", Vec2(1, 4), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		int enters = 0, contacts = 0;
		Vec2 point, normal;
		f32 speed = 0.f;
		Physics2D* other = NULL;
		box->OnCollisionEnter = [&](Physics2D*) { enters++; };
		box->OnContact = [&](Physics2D* o, Vec2 p, Vec2 n, f32 s) { contacts++; other = o; point = p; normal = n; speed = s; };
		r.Run(180);
		check(enters == 1 && contacts == 1, "landing is one collision and one contact");
		check(other == floor.get(), "the contact names the floor");
		nearly(normal.y, 1.f, 0.01f, "its normal points from the floor up at the box");
		nearly(point.y, 0.f, 0.05f, "its point is on the floor's surface");
		check(speed > 5.f, "and it reports how fast they met");
	}

	// --- collision layers ---------------------------------------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> floor = r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> shelf = r.Body("Shelf", Vec2(0, 3), Body2DType::Static, Shape2DType::Box, Vec2(4, 0.25f));
		shelf->SetLayer(3);
		std::shared_ptr<Physics2D> solid = r.Body("Solid", Vec2(-1, 6), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.25f, 0.25f));
		std::shared_ptr<Physics2D> ghost = r.Body("Ghost", Vec2(1, 6), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.25f, 0.25f));
		ghost->SetCollidesWith(3, false);
		check(!ghost->CollidesWith(3) && ghost->CollidesWith(0), "a mask is edited a layer at a time");
		r.Run(240);
		nearly(PosOf(solid).y, 3.5f, 0.02f, "a body lands on a shelf on a layer it collides with");
		nearly(PosOf(ghost).y, 0.25f, 0.02f, "one that does not falls through to the floor");
		// The mask of a live body: the first one drops through too.
		solid->SetCollidesWith(3, false);
		solid->Wake();
		r.Run(180);
		nearly(PosOf(solid).y, 0.25f, 0.02f, "taking the layer out of a live body's mask drops it through");
		(void)floor;
	}

	// --- one-way platforms ----------------------------------------------------
	{
		Rig r;
		r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> plat = r.Body("Platform", Vec2(0, 2), Body2DType::Static, Shape2DType::Box, Vec2(3, 0.15f));
		plat->SetOneWay(true);
		// Jumps from the floor, up through the platform.
		std::shared_ptr<Physics2D> jumper = r.Body("Jumper", Vec2(0, 0.5f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.3f, 0.5f));
		jumper->SetFixedRotation(true);
		r.Run(30);
		jumper->SetLinearVelocity(Vec2(0.f, 9.f));
		f32 peak = 0.f;
		for (int i = 0; i < 240; i++) { r.Run(1); if (PosOf(jumper).y > peak) peak = PosOf(jumper).y; }
		check(peak > 3.2f, "a body jumping from below passes up through a one-way platform");
		nearly(PosOf(jumper).y, 2.65f, 0.03f, "and lands on top of it on the way down");
		check(jumper->IsGrounded(), "standing on it counts as grounded");
	}
	{
		// From the side at platform height: not blocked.
		Rig r;
		std::shared_ptr<Physics2D> plat = r.Body("Platform", Vec2(0, 2), Body2DType::Static, Shape2DType::Box, Vec2(3, 0.15f));
		plat->SetOneWay(true);
		std::shared_ptr<Physics2D> runner = r.Body("Runner", Vec2(-6, 2.0f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.2f, 0.2f));
		runner->SetGravityScale(0.f);
		r.Run(2);
		runner->SetLinearVelocity(Vec2(6.f, 0.f));
		r.Run(120);
		check(PosOf(runner).x > 3.5f, "a body crossing at its height from the side is not stopped");
		// And the same platform made ordinary again stops it.
		plat->SetOneWay(false);
		runner->GetOwner()->SetPosition(Vec3(-6.f, 2.f, 0.f));
		r.Run(1);
		runner->SetLinearVelocity(Vec2(6.f, 0.f));
		r.Run(120);
		check(PosOf(runner).x < -3.f, "with one-way off, the same platform is a wall from the side");
	}

	{
		// The same of a body whose shape is a chain - which is what a tile
		// map's collider is, so what a bridge painted in tiles is. A loop
		// the shape of a slab: walked into from the side at its height it
		// is not there, jumped at from below it is passed, fallen onto it
		// is stood on. (It was a wall from every side: a chain was never
		// told to ask about one-way.)
		Rig r;
		r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> slab = r.Body("Bridge", Vec2(0, 0), Body2DType::Static, Shape2DType::Chain, Vec2(1, 1));
		std::vector<Vec2> outline;
		outline.push_back(Vec2(-3.f, 1.6f)); outline.push_back(Vec2(3.f, 1.6f));
		outline.push_back(Vec2(3.f, 2.6f)); outline.push_back(Vec2(-3.f, 2.6f));
		check(slab->SetChain(outline, true), "a closed chain is accepted");
		slab->SetOneWay(true);
		std::shared_ptr<Physics2D> runner = r.Body("Runner", Vec2(-6, 1.0f), Body2DType::Dynamic, Shape2DType::Capsule, Vec2(0.4f, 1.0f));
		runner->SetFixedRotation(true);
		runner->SetFriction(0.f);
		r.Run(30);
		for (int i = 0; i < 150; i++) { runner->SetLinearVelocity(Vec2(6.f, runner->GetLinearVelocity().y)); r.Run(1); }
		check(PosOf(runner).x > 3.5f, "a one-way chain is walked under, head in it, from the side");
		runner->GetOwner()->SetPosition(Vec3(0.f, 1.0f, 0.f));
		r.Run(2);
		runner->SetLinearVelocity(Vec2(0.f, 12.f));
		f32 peak = 0.f;
		for (int i = 0; i < 240; i++) { r.Run(1); if (PosOf(runner).y > peak) peak = PosOf(runner).y; }
		check(peak > 4.0f, "and jumped up through from below");
		nearly(PosOf(runner).y, 3.6f, 0.05f, "and stood on, coming down");
	}

	{
		// A wheel turned hard keeps gaining speed: round bodies are not held to the solver's
		// quarter turn a step (47 radians a second), which is what stopped a driven wheel.
		Rig r;
		std::shared_ptr<Physics2D> wheel = r.Body("Wheel", Vec2(0, 5), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.33f, 0.33f));
		wheel->SetGravityScale(0.f);
		std::shared_ptr<Physics2D> crate = r.Body("Crate", Vec2(4, 5), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.33f, 0.33f));
		crate->SetGravityScale(0.f);
		r.Run(2);
		for (int i = 0; i < 120; i++) { wheel->ApplyTorque(-40.f); crate->ApplyTorque(-40.f); r.Run(1); }
		check(fabsf(wheel->GetAngularVelocity()) > 80.f, "a wheel driven hard turns faster than a quarter turn a step");
		check(fabsf(crate->GetAngularVelocity()) < 48.f, "a box is still held to it");
	}

	// --- a script moving an object moves its body ------------------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> plat = r.Body("Plat", Vec2(0, 0), Body2DType::Static, Shape2DType::Box, Vec2(2, 0.25f));
		std::shared_ptr<Physics2D> box = r.Body("Box", Vec2(0, 2), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.25f, 0.25f));
		r.Run(120);
		nearly(PosOf(box).y, 0.5f, 0.02f, "the box is resting on the platform");
		// The platform is moved by its OBJECT, as a script does it.
		plat->GetOwner()->SetPosition(Vec3(10.f, 0.f, 0.f));
		r.Run(120);
		check(PosOf(box).y < -5.f, "a static body follows its object: the box under it falls");
		nearly(plat->GetPosition().x, 10.f, 0.001f, "the body is where the object was put");
		// And a dynamic one teleported by its object stays teleported.
		box->GetOwner()->SetPosition(Vec3(10.f, 3.f, 0.f));
		box->SetLinearVelocity(Vec2(0.f, 0.f));
		r.Run(1);
		check(PosOf(box).y > 2.9f && fabsf(PosOf(box).x - 10.f) < 0.01f, "a dynamic body moved by its object is not snapped back");
		r.Run(180);
		nearly(PosOf(box).y, 0.5f, 0.02f, "and then falls onto the platform where it now is");
	}

	// --- kinematic MoveTo carries what stands on it -------------------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> lift = r.Body("Lift", Vec2(0, 0), Body2DType::Kinematic, Shape2DType::Box, Vec2(2, 0.25f));
		std::shared_ptr<Physics2D> rider = r.Body("Rider", Vec2(0, 1), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.25f, 0.25f));
		rider->SetFriction(0.9f);
		lift->SetFriction(0.9f);
		r.Run(60);
		for (int i = 0; i < 120; i++)
		{
			const f32 x = (f32)(i + 1) / 120.f * 4.f;
			lift->MoveTo(Vec2(x, 0.f), 0.f, 1.f / 60.f);
			r.Run(1);
		}
		lift->SetLinearVelocity(Vec2(0.f, 0.f));
		r.Run(30);
		nearly(PosOf(lift).x, 4.f, 0.05f, "MoveTo brings a kinematic body to its target");
		nearly(PosOf(rider).x, 4.f, 0.3f, "and what stands on it rides along");

		// A character whose controller sets its velocity every frame has no
		// friction to ride on: it asks what it stands on and adds that.
		std::shared_ptr<Physics2D> walker = r.Body("Walker", Vec2(5.2f, 1.f), Body2DType::Dynamic, Shape2DType::Capsule, Vec2(0.25f, 0.5f));
		walker->SetFriction(0.f);
		walker->SetFixedRotation(true);
		r.Run(60);
		check(walker->GetGroundBody() == lift.get(), "a body knows which body it is standing on");
		check(rider->GetGroundBody() == lift.get() && lift->GetGroundBody() == NULL, "and one in mid-air stands on nothing");
		for (int i = 0; i < 120; i++)
		{
			const f32 x = 4.f + (f32)(i + 1) / 120.f * 3.f;
			lift->MoveTo(Vec2(x, 0.f), 0.f, 1.f / 60.f);
			const Vec2 gv = walker->GetGroundVelocity();
			walker->SetLinearVelocity(Vec2(0.f + gv.x, walker->GetLinearVelocity().y));    // standing still, as a controller says it
			r.Run(1);
		}
		nearly(PosOf(walker).x, 8.2f, 0.15f, "adding the ground's velocity carries a frictionless character");
	}

	// --- a rebuilt body keeps what it was doing -----------------------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> b = r.Body("B", Vec2(0, 50), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		b->SetGravityScale(0.f);
		r.Run(2);
		b->SetLinearVelocity(Vec2(3.f, 0.f));
		r.Run(10);
		b->SetSize(Vec2(1.f, 1.f));     // a different shape: the body is made again
		r.Run(60);
		nearly(b->GetLinearVelocity().x, 3.f, 0.01f, "resizing a moving body does not stop it");
		check(PosOf(b).x > 3.f, "and it went on moving");
	}

	// --- a body under a parent is written back in its parent's space ----------------
	{
		Rig r;
		std::shared_ptr<GameObject> layer = std::make_shared<GameObject>();
		layer->SetName("Layer");
		layer->SetPosition(Vec3(100.f, 50.f, 0.f));
		r.scene.Add(layer);
		r.objects.push_back(layer);
		r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f), layer.get());
		std::shared_ptr<Physics2D> box = r.Body("Box", Vec2(0, 3), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f), layer.get());
		r.Run(240);
		nearly(box->GetOwner()->GetPosition().y, 0.5f, 0.02f, "its LOCAL position is local: resting on the layer's floor");
		nearly(PosOf(box).y, 50.5f, 0.02f, "and its world position is the layer's plus that");
		nearly(PosOf(box).x, 100.f, 0.02f, "x likewise");
	}

	// --- joints ------------------------------------------------------------------
	{
		// A pendulum on a hinge pinned to the world.
		Rig r;
		std::shared_ptr<Physics2D> bob = r.Body("Bob", Vec2(2, 0), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.2f, 0.2f));
		std::shared_ptr<Joint2D> hinge = std::make_shared<Joint2D>(Joint2DType::Hinge);
		hinge->SetAnchor(Vec2(-2.f, 0.f));      // the pin is 2 to the bob's left: the world origin
		bob->GetOwner()->Add(std::static_pointer_cast<IComponent>(hinge));
		f32 minY = 0.f, maxR = 0.f, minR = 99.f;
		for (int i = 0; i < 240; i++)
		{
			r.Run(1);
			const Vec2 p = PosOf(bob);
			const f32 rad = sqrtf(p.x * p.x + p.y * p.y);
			if (p.y < minY) minY = p.y;
			if (rad > maxR) maxR = rad;
			if (rad < minR) minR = rad;
		}
		check(r.world.JointCount() == 1, "the hinge was built");
		check(minY < -1.8f, "a pendulum swings down");
		check(maxR < 2.05f && minR > 1.95f, "and stays its arm's length from the pin");

		// A hinge with limits stops.
		Rig r2;
		std::shared_ptr<Physics2D> door = r2.Body("Door", Vec2(1, 0), Body2DType::Dynamic, Shape2DType::Box, Vec2(1.f, 0.1f));
		std::shared_ptr<Joint2D> h2 = std::make_shared<Joint2D>(Joint2DType::Hinge);
		h2->SetAnchor(Vec2(-1.f, 0.f));
		h2->SetLimits(true, -0.5f, 0.5f);
		door->GetOwner()->Add(std::static_pointer_cast<IComponent>(h2));
		r2.Run(300);
		nearly(door->GetAngle(), -0.5f, 0.03f, "a limited hinge hangs at its limit, not straight down");

		// A rope: free until taut, then no longer.
		Rig r3;
		std::shared_ptr<Physics2D> weight = r3.Body("Weight", Vec2(0, 0), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.2f, 0.2f));
		std::shared_ptr<Joint2D> rope = std::make_shared<Joint2D>(Joint2DType::Distance);
		rope->SetConnectedAnchor(Vec2(0.f, 0.f));
		rope->SetLimits(true, 0.f, 3.f);
		weight->GetOwner()->Add(std::static_pointer_cast<IComponent>(rope));
		r3.Run(300);
		nearly(PosOf(weight).y, -3.f, 0.05f, "a rope of 3 lets its weight fall 3 and holds it there");

		// A slider with a limit: falls along its rail and stops.
		Rig r4;
		std::shared_ptr<Physics2D> lift = r4.Body("Lift", Vec2(0, 0), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.2f));
		std::shared_ptr<Joint2D> rail = std::make_shared<Joint2D>(Joint2DType::Slider);
		rail->SetAxis(Vec2(0.f, 1.f));
		rail->SetLimits(true, -2.f, 1.f);
		lift->GetOwner()->Add(std::static_pointer_cast<IComponent>(rail));
		r4.Run(2);
		lift->ApplyImpulse(Vec2(5.f, 0.f));     // sideways: the rail does not allow it
		r4.Run(240);
		nearly(PosOf(lift).y, -2.f, 0.05f, "a slider stops at its lower limit");
		nearly(PosOf(lift).x, 0.f, 0.02f, "and never leaves its rail");
		// A motor lifts it back up, set on the running joint.
		rail->SetMotor(true, 2.f, 1000.f);
		r4.Run(240);
		nearly(PosOf(lift).y, 1.f, 0.05f, "a motor drives it to the other limit");

		// Two bodies welded fall as one; removing the joint lets them part.
		Rig r5;
		r5.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> a = r5.Body("A", Vec2(0, 3), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<Physics2D> b = r5.Body("B", Vec2(1.5f, 3), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		b->SetGravityScale(0.f);                // B floats; only the weld brings it down
		std::shared_ptr<Joint2D> weld = std::make_shared<Joint2D>(Joint2DType::Weld);
		weld->SetConnected("A");
		b->GetOwner()->Add(std::static_pointer_cast<IComponent>(weld));
		r5.Run(240);
		// B weighs nothing the ground can hold up, so the pair tips over as
		// A lands - what matters is that B came down and is still exactly
		// as far from A as it was welded.
		check(PosOf(b).y < 2.3f, "a body welded to a falling one comes down with it");
		{
			const Vec2 d(PosOf(b).x - PosOf(a).x, PosOf(b).y - PosOf(a).y);
			nearly(sqrtf(d.x * d.x + d.y * d.y), 1.5f, 0.03f, "and stays exactly as far from it");
		}
		b->GetOwner()->Remove(std::static_pointer_cast<IComponent>(weld));
		weld.reset();
		b->SetLinearVelocity(Vec2(0.f, 4.f));
		r5.Run(60);
		check(r5.world.JointCount() == 0, "a removed joint is gone from the world");
		check(PosOf(b).y > 3.f, "and what it held is free");

		// A joint naming an object that does not exist is simply not built.
		Rig r6;
		std::shared_ptr<Physics2D> lone = r6.Body("Lone", Vec2(0, 0), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<Joint2D> bad = std::make_shared<Joint2D>(Joint2DType::Weld);
		bad->SetConnected("Nobody");
		lone->GetOwner()->Add(std::static_pointer_cast<IComponent>(bad));
		r6.Run(30);
		check(r6.world.JointCount() == 0 && PosOf(lone).y < -0.5f, "a joint to a missing object is left unbuilt, and nothing breaks");
	}

	// --- queries ------------------------------------------------------------------
	{
		Rig r;
		std::shared_ptr<Physics2D> wallA = r.Body("WallA", Vec2(5, 0), Body2DType::Static, Shape2DType::Box, Vec2(0.5f, 2.f));
		std::shared_ptr<Physics2D> wallB = r.Body("WallB", Vec2(9, 0), Body2DType::Static, Shape2DType::Box, Vec2(0.5f, 2.f));
		wallB->SetLayer(2);
		std::shared_ptr<Physics2D> trig = r.Body("Trigger", Vec2(2, 0), Body2DType::Static, Shape2DType::Box, Vec2(0.5f, 2.f));
		trig->SetSensor(true);
		r.Run(2);

		Physics2DWorld::RayHit2D h = r.world.RayCast(Vec2(0, 0), Vec2(20, 0));
		check(h.hit && h.component == wallA.get(), "a ray passes a sensor and hits the first wall");
		nearly(h.point.x, 4.5f, 0.01f, "at its near face");
		nearly(h.normal.x, -1.f, 0.01f, "whose normal faces the ray");
		h = r.world.RayCast(Vec2(0, 0), Vec2(20, 0), wallA.get());
		check(h.hit && h.component == wallB.get(), "ignoring the first, it reaches the second");
		h = r.world.RayCast(Vec2(0, 0), Vec2(20, 0), NULL, NULL, 1u << 2);
		check(h.hit && h.component == wallB.get(), "a mask of layer 2 sees only the wall on layer 2");
		check(!r.world.RayCast(Vec2(0, 5), Vec2(20, 5)).hit, "a ray over the top hits nothing");

		const std::vector<Physics2DWorld::RayHit2D> all = r.world.RayCastAll(Vec2(0, 0), Vec2(20, 0));
		check(all.size() == 2 && all[0].component == wallA.get() && all[1].component == wallB.get(), "every body along a ray, nearest first");

		// A circle is stopped where a ray above it would pass.
		h = r.world.CircleCast(Vec2(0, 2.3f), Vec2(20, 2.3f), 0.5f);
		check(h.hit && h.component == wallA.get(), "a circle cast catches a wall the ray along its centre clears");
		check(!r.world.RayCast(Vec2(0, 2.3f), Vec2(20, 2.3f)).hit, "(the ray does clear it)");
		h = r.world.BoxCast(Vec2(0, 0), Vec2(20, 0), Vec2(0.25f, 0.25f));
		nearly(h.fraction, (4.5f - 0.25f) / 20.f, 0.005f, "a box cast stops with its own face against the wall");

		check(r.world.OverlapPoint(Vec2(5, 1)).size() == 1, "a point inside a wall finds it");
		check(r.world.OverlapPoint(Vec2(7, 1)).empty(), "a point in the open finds nothing");
		check(r.world.OverlapPoint(Vec2(2, 0)).empty(), "a sensor is left out of an overlap");
		check(r.world.OverlapPoint(Vec2(2, 0), 0xFFFFFFFFu, true).size() == 1, "unless sensors are asked for");
		check(r.world.OverlapCircle(Vec2(7, 0), 2.f).size() == 2, "a circle reaching both walls finds both");
		check(r.world.OverlapBox(Vec2(7, 0), Vec2(1.6f, 0.5f)).size() == 2, "and so does a box");
		check(r.world.OverlapCircle(Vec2(7, 0), 2.f, 1u << 2).size() == 1, "a masked overlap finds only its layer");
		check(r.world.BodyCount() == 3, "the world counts its bodies");
	}

	// --- handlers that change the world -----------------------------------------------
	{
		Rig r;
		r.Body("Floor", Vec2(0, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20, 0.5f));
		std::shared_ptr<Physics2D> a = r.Body("A", Vec2(0, 2), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<GameObject> owner = r.objects.back();
		bool removed = false;
		a->OnCollisionEnter = [&](Physics2D*) {
			if (removed) return;
			removed = true;
			r.scene.Remove(owner);      // "destroy me when I land"
		};
		r.Run(240);
		check(removed, "a body can be removed from inside its own collision handler");
		check(r.world.BodyCount() == 1, "and its body goes with it");
	}

	// --- outlines the solver cannot take whole: concave, and chains ---------
	{
		// An L: 3 wide and 3 tall with the top-right 2x2 cut out. Area 5.
		std::vector<Vec2> L;
		L.push_back(Vec2(0.f, 0.f)); L.push_back(Vec2(3.f, 0.f)); L.push_back(Vec2(3.f, 1.f));
		L.push_back(Vec2(1.f, 1.f)); L.push_back(Vec2(1.f, 3.f)); L.push_back(Vec2(0.f, 3.f));
		std::vector<std::vector<Vec2> > parts;
		Physics2D::DecomposePolygon(L, parts);
		f32 area = 0.f;
		bool allConvex = true;
		for (size_t q = 0; q < parts.size(); q++)
		{
			f32 a = 0.f;
			for (size_t k = 0; k < parts[q].size(); k++)
			{
				const Vec2 &u = parts[q][k], &v = parts[q][(k + 1) % parts[q].size()], &w = parts[q][(k + 2) % parts[q].size()];
				a += u.x * v.y - v.x * u.y;
				if ((v.x - u.x) * (w.y - u.y) - (v.y - u.y) * (w.x - u.x) < -1e-5f) allConvex = false;
			}
			area += a * 0.5f;
			if (parts[q].size() > 8) allConvex = false;
		}
		check(parts.size() == 2, "an L is cut into two convex pieces, not four triangles");
		check(allConvex, "every piece is convex and has 8 points or fewer");
		nearly(area, 5.f, 1e-3f, "and together they are the L's area");
		std::vector<Vec2> clockwise(L.rbegin(), L.rend());
		Physics2D::DecomposePolygon(clockwise, parts);
		check(parts.size() == 2, "drawn the other way round, the same");

		std::vector<Vec2> sq;
		sq.push_back(Vec2(-1.f, -1.f)); sq.push_back(Vec2(1.f, -1.f)); sq.push_back(Vec2(1.f, 1.f)); sq.push_back(Vec2(-1.f, 1.f));
		Physics2D::DecomposePolygon(sq, parts);
		check(parts.size() == 1 && parts[0].size() == 4, "a convex outline is left as one piece");

		// Twelve points round a circle: convex, but more than the solver's eight.
		std::vector<Vec2> ring;
		for (int i = 0; i < 12; i++) ring.push_back(Vec2(cosf((f32)i / 12.f * 6.2831853f), sinf((f32)i / 12.f * 6.2831853f)));
		Physics2D::DecomposePolygon(ring, parts);
		f32 ringArea = 0.f;
		for (size_t q = 0; q < parts.size(); q++)
			for (size_t k = 0; k < parts[q].size(); k++)
			{ const Vec2 &u = parts[q][k], &v = parts[q][(k + 1) % parts[q].size()]; ringArea += (u.x * v.y - v.x * u.y) * 0.5f; }
		check(parts.size() >= 2 && parts.size() <= 4, "a convex outline of twelve points is a few pieces of eight or fewer");
		nearly(ringArea, 3.f, 1e-3f, "with nothing lost");      // a regular 12-gon of radius 1

		std::shared_ptr<Physics2D> probe = std::make_shared<Physics2D>(Body2DType::Static, Shape2DType::Polygon, Vec2(1.f, 1.f));
		check(probe->SetPolygon(L), "a concave outline is accepted");
		std::vector<Vec2> bow;
		bow.push_back(Vec2(0.f, 0.f)); bow.push_back(Vec2(2.f, 2.f)); bow.push_back(Vec2(2.f, 0.f)); bow.push_back(Vec2(0.f, 2.f));
		check(!probe->SetPolygon(bow) && probe->GetPolygon().size() == 6, "one that crosses itself is refused and the last good one kept");
		std::vector<Vec2> line;
		line.push_back(Vec2(0.f, 0.f)); line.push_back(Vec2(1.f, 0.f)); line.push_back(Vec2(2.f, 0.f));
		check(!probe->SetPolygon(line), "and so is one with no area");

		// A ball dropped into the L's notch lands on the notch's floor (y = 1),
		// not on the lid a convex hull would have put over it.
		{
			Rig r;
			std::shared_ptr<Physics2D> ell = r.Body("L", Vec2(0.f, 0.f), Body2DType::Static, Shape2DType::Polygon, Vec2(1.f, 1.f));
			ell->SetPolygon(L);
			std::shared_ptr<Physics2D> ball = r.Body("Ball", Vec2(2.f, 4.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.25f, 0.25f));
			r.Run(240);
			nearly(PosOf(ball).y, 1.25f, 0.05f, "a ball falls into a concave shape's notch and rests on its floor");
			check(PosOf(ball).x > 1.2f, "inside the notch, not pushed out of it");
		}
		// A concave DYNAMIC body weighs what its area says.
		{
			Rig r;
			std::shared_ptr<Physics2D> ell = r.Body("L", Vec2(0.f, 5.f), Body2DType::Dynamic, Shape2DType::Polygon, Vec2(1.f, 1.f));
			ell->SetPolygon(L);
			ell->SetDensity(2.f);
			r.Run(2);
			nearly(ell->GetMass(), 10.f, 0.05f, "a concave body's mass is its area times its density");
		}
		// An open chain: a V. A ball settles at its bottom from above; one
		// thrown up from underneath is stopped too.
		{
			Rig r;
			std::shared_ptr<Physics2D> vee = r.Body("V", Vec2(0.f, 0.f), Body2DType::Static, Shape2DType::Chain, Vec2(1.f, 1.f));
			std::vector<Vec2> pts;
			pts.push_back(Vec2(-4.f, 2.f)); pts.push_back(Vec2(-2.f, 1.f)); pts.push_back(Vec2(0.f, 0.f));
			pts.push_back(Vec2(2.f, 1.f)); pts.push_back(Vec2(4.f, 2.f));
			check(vee->SetChain(pts, false), "a chain of five points is accepted");
			std::shared_ptr<Physics2D> ball = r.Body("Ball", Vec2(-2.5f, 4.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.25f, 0.25f));
			ball->SetFriction(0.f);
			ball->SetLinearDamping(1.5f);
			r.Run(900);
			nearly(PosOf(ball).x, 0.f, 0.2f, "a ball dropped on a V-shaped chain rolls to its bottom");
			check(PosOf(ball).y > 0.2f && PosOf(ball).y < 0.5f, "and rests on it");
			std::shared_ptr<Physics2D> under = r.Body("Under", Vec2(-2.f, -1.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.2f, 0.2f));
			under->SetGravityScale(0.f);
			r.Run(2);
			under->SetLinearVelocity(Vec2(0.f, 6.f));
			r.Run(40);
			// (it slides along the underside of the slope, so "stopped" is
			// "still under the line", which there is y = -x / 2)
			check(PosOf(under).y < -PosOf(under).x * 0.5f && PosOf(under).y > -0.5f,
				"an open chain stops what comes at it from underneath as well");
		}
		// A short chain (too few points for the solver's chain) is plain edges.
		{
			Rig r;
			std::shared_ptr<Physics2D> floor = r.Body("Floor", Vec2(0.f, 0.f), Body2DType::Static, Shape2DType::Chain, Vec2(1.f, 1.f));
			std::vector<Vec2> pts;
			pts.push_back(Vec2(-3.f, 0.f)); pts.push_back(Vec2(3.f, 0.f));
			check(floor->SetChain(pts, false), "two points are a chain");
			std::shared_ptr<Physics2D> box = r.Body("Box", Vec2(0.f, 2.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.3f, 0.3f));
			r.Run(240);
			nearly(PosOf(box).y, 0.3f, 0.05f, "and a box rests on a two-point one");
			std::vector<Vec2> same;
			same.push_back(Vec2(1.f, 1.f)); same.push_back(Vec2(1.f, 1.f));
			check(!floor->SetChain(same, false), "two points in the same place are not");
		}
		// A loop is the skin of something solid, whichever way it was drawn.
		{
			Rig r;
			std::shared_ptr<Physics2D> rock = r.Body("Rock", Vec2(0.f, 0.f), Body2DType::Static, Shape2DType::Chain, Vec2(1.f, 1.f));
			std::vector<Vec2> pts;       // clockwise on purpose
			pts.push_back(Vec2(-2.f, 1.f)); pts.push_back(Vec2(2.f, 1.f)); pts.push_back(Vec2(2.f, -1.f)); pts.push_back(Vec2(-2.f, -1.f));
			check(rock->SetChain(pts, true) && rock->IsChainLoop(), "a closed loop of four is accepted");
			std::shared_ptr<Physics2D> ball = r.Body("Ball", Vec2(0.5f, 3.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.25f, 0.25f));
			r.Run(240);
			nearly(PosOf(ball).y, 1.25f, 0.05f, "a ball rests on top of a loop drawn clockwise");
		}
	}

	// --- the joints the solver has no kind for, and two it has ---------------
	{
		// A pulley: two equal boxes hang level and stay there; make one
		// heavier and it goes down as far as the other goes up.
		{
			Rig r;
			std::shared_ptr<Physics2D> a = r.Body("A", Vec2(-2.f, 0.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.4f));
			std::shared_ptr<Physics2D> b = r.Body("B", Vec2(2.f, 0.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.4f));
			a->SetFixedRotation(true); b->SetFixedRotation(true);
			std::shared_ptr<Joint2D> j = std::make_shared<Joint2D>(Joint2DType::Pulley);
			j->SetConnected("B");
			j->SetGroundAnchors(Vec2(-2.f, 5.f), Vec2(2.f, 5.f));
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(j));
			r.Run(120);
			check(j->IsBuilt() && r.world.JointCount() == 1, "a pulley between two bodies is held by the world");
			nearly(PosOf(a).y, 0.f, 0.05f, "two equal weights on a pulley hang where they were");
			nearly(PosOf(b).y, 0.f, 0.05f, "both of them");
			a->SetDensity(4.f);
			r.Run(45);
			const f32 down = -PosOf(a).y, up = PosOf(b).y;
			check(down > 0.5f, "made heavier, one side goes down");
			nearly(up, down, 0.06f, "and the other comes up by as much");
			nearly(PosOf(a).x, -2.f, 0.02f, "straight down, under its wheel");
		}
		// With a ratio of 2 the other side moves half as far.
		{
			Rig r;
			std::shared_ptr<Physics2D> a = r.Body("A", Vec2(-2.f, 0.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.4f));
			std::shared_ptr<Physics2D> b = r.Body("B", Vec2(2.f, 0.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.4f));
			a->SetFixedRotation(true); b->SetFixedRotation(true);
			a->SetDensity(6.f);
			std::shared_ptr<Joint2D> j = std::make_shared<Joint2D>(Joint2DType::Pulley);
			j->SetConnected("B");
			j->SetGroundAnchors(Vec2(-2.f, 6.f), Vec2(2.f, 6.f));
			j->SetRatio(2.f);
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(j));
			r.Run(40);
			const f32 down = -PosOf(a).y, up = PosOf(b).y;
			check(down > 0.5f, "a block and tackle's heavy side goes down");
			nearly(up, down * 0.5f, 0.06f, "and the other comes up half as far at a ratio of 2");
		}
		// A rope only pulls: lift one side and the other does not go down.
		{
			Rig r;
			r.Body("Floor", Vec2(0.f, -1.f), Body2DType::Static, Shape2DType::Box, Vec2(10.f, 0.5f));
			std::shared_ptr<Physics2D> a = r.Body("A", Vec2(-2.f, -0.1f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.4f));
			std::shared_ptr<Physics2D> b = r.Body("B", Vec2(2.f, -0.1f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.4f, 0.4f));
			std::shared_ptr<Joint2D> j = std::make_shared<Joint2D>(Joint2DType::Pulley);
			j->SetConnected("B");
			j->SetGroundAnchors(Vec2(-2.f, 5.f), Vec2(2.f, 5.f));
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(j));
			r.Run(30);
			a->SetGravityScale(-0.5f);       // floats up: its rope goes slack
			r.Run(60);
			check(PosOf(a).y > 0.5f, "one side lifted, its rope goes slack");
			nearly(PosOf(b).y, -0.1f, 0.03f, "and the other, resting on the floor, is not pushed through it");
		}
		// Gears: two wheels hinged to the world. Drive one, the other turns
		// the other way; at a ratio of 2 it turns half as fast.
		for (int pass = 0; pass < 2; pass++)
		{
			Rig r;
			r.world.SetGravity(Vec2(0.f, 0.f));
			std::shared_ptr<Physics2D> a = r.Body("A", Vec2(-1.f, 0.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(1.f, 1.f));
			std::shared_ptr<Physics2D> b = r.Body("B", Vec2(1.f, 0.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(1.f, 1.f));
			a->SetLayer(1); a->SetCollidesWith(2, false);
			b->SetLayer(2); b->SetCollidesWith(1, false);
			std::shared_ptr<Joint2D> ha = std::make_shared<Joint2D>(Joint2DType::Hinge);
			ha->SetMotor(true, 2.f, 100000.f);
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(ha));
			std::shared_ptr<Joint2D> hb = std::make_shared<Joint2D>(Joint2DType::Hinge);
			b->GetOwner()->Add(std::static_pointer_cast<IComponent>(hb));
			std::shared_ptr<Joint2D> g = std::make_shared<Joint2D>(Joint2DType::Gear);
			g->SetConnected("B");
			const f32 ratio = pass == 0 ? 1.f : 2.f;
			g->SetRatio(ratio);
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(g));
			r.Run(180);
			nearly(a->GetAngularVelocity(), 2.f, 0.1f, "a geared wheel is still driven at its motor's speed");
			nearly(b->GetAngularVelocity(), -2.f / ratio, 0.1f,
				pass == 0 ? "and the wheel geared to it turns the other way as fast" : "at a ratio of 2, the other turns half as fast");
			// Three seconds at 2 rad/s is nearly a full turn: the angles
			// have wrapped, and the two are still in step.
			f32 phase = a->GetAngle() + ratio * b->GetAngle();
			while (phase > 3.14159265f) phase -= 6.2831853f;
			while (phase < -3.14159265f) phase += 6.2831853f;
			if (pass == 0) nearly(phase, 0.f, 0.05f, "and they stay in step past a full turn");
		}
		// An object holds one joint; a second on the same body goes on a child
		// with no body of its own, and ties its parent's.
		{
			Rig r;
			r.world.SetGravity(Vec2(0.f, 0.f));
			std::shared_ptr<Physics2D> a = r.Body("A", Vec2(-1.f, 0.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(1.f, 1.f));
			std::shared_ptr<Physics2D> b = r.Body("B", Vec2(1.f, 0.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(1.f, 1.f));
			a->SetLayer(1); a->SetCollidesWith(2, false);
			b->SetLayer(2); b->SetCollidesWith(1, false);
			std::shared_ptr<Joint2D> ha = std::make_shared<Joint2D>(Joint2DType::Hinge);
			ha->SetMotor(true, 2.f, 100000.f);
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(ha));
			std::shared_ptr<Joint2D> hb = std::make_shared<Joint2D>(Joint2DType::Hinge);
			b->GetOwner()->Add(std::static_pointer_cast<IComponent>(hb));
			std::shared_ptr<GameObject> link = std::make_shared<GameObject>();
			link->SetName("Link");
			std::shared_ptr<Joint2D> g = std::make_shared<Joint2D>(Joint2DType::Gear);
			g->SetConnected("B");
			link->Add(std::static_pointer_cast<IComponent>(g));
			a->GetOwner()->Add(link);
			r.objects.push_back(link);
			r.Run(180);
			check(g->IsBuilt(), "a joint on a child with no body ties its parent's");
			nearly(b->GetAngularVelocity(), -2.f, 0.1f, "and works as it would have on the parent");
			check(r.world.JointCount() == 3, "alongside the parent's own joint");
		}
		// Target: a body with no gravity is drawn to where it is told.
		{
			Rig r;
			std::shared_ptr<Physics2D> a = r.Body("A", Vec2(0.f, 0.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.3f, 0.3f));
			a->SetGravityScale(0.f);
			std::shared_ptr<Joint2D> j = std::make_shared<Joint2D>(Joint2DType::Target);
			j->SetMotor(false, 0.f, 1000.f);
			a->GetOwner()->Add(std::static_pointer_cast<IComponent>(j));
			r.Run(30);
			nearly(PosOf(a).x, 0.f, 0.02f, "a target joint with no target holds its body where it is");
			j->SetTarget(Vec2(3.f, 2.f));
			r.Run(180);
			nearly(PosOf(a).x, 3.f, 0.1f, "given a target, the body goes to it");
			nearly(PosOf(a).y, 2.f, 0.1f, "in both directions");
			j->SetTarget(Vec2(-1.f, 0.f));
			r.Run(180);
			nearly(PosOf(a).x, -1.f, 0.1f, "and follows when the target moves");
			check(r.world.JointCount() == 1, "without the joint being made again each time");
		}
		// Ignore: two boxes that would rest on each other pass through.
		{
			Rig r;
			r.Body("Floor", Vec2(0.f, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(10.f, 0.5f));
			std::shared_ptr<Physics2D> low = r.Body("Low", Vec2(0.f, 0.5f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
			std::shared_ptr<Physics2D> high = r.Body("High", Vec2(0.f, 3.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
			std::shared_ptr<Joint2D> j = std::make_shared<Joint2D>(Joint2DType::Ignore);
			j->SetConnected("Low");
			high->GetOwner()->Add(std::static_pointer_cast<IComponent>(j));
			r.Run(180);
			nearly(PosOf(high).y, 0.5f, 0.05f, "two bodies told to ignore each other pass through one another");
			nearly(PosOf(low).y, 0.5f, 0.05f, "while both still rest on the floor");
		}
	}

	// --- a body whose component is gone does not stay behind -------------------
	{
		Rig r;
		r.Body("Floor", Vec2(0.f, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(10.f, 0.5f));
		// A wide static shelf, and a ball resting on it.
		std::shared_ptr<GameObject> shelf = std::make_shared<GameObject>();
		shelf->SetName("Shelf");
		shelf->SetPosition(Vec3(0.f, 3.f, 0.f));
		std::shared_ptr<Physics2D> first = std::make_shared<Physics2D>(Body2DType::Static, Shape2DType::Box, Vec2(2.f, 0.25f));
		shelf->Add(std::static_pointer_cast<IComponent>(first));
		r.scene.Add(shelf);
		std::shared_ptr<Physics2D> ball = r.Body("Ball", Vec2(1.5f, 5.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.25f, 0.25f));
		r.Run(120);
		nearly(PosOf(ball).y, 3.5f, 0.05f, "a ball rests on a shelf");

		// What an undo does: the component is taken off and a NEW one put
		// on, narrower - and the allocator is free to give the new one the
		// old one's address, which is the case that used to leave the old
		// body in the world. Tried until that happens, or eight times.
		bool sameAddress = false;
		for (int attempt = 0; attempt < 8 && !sameAddress; attempt++)
		{
			const Physics2D* was = NULL;
			for (size_t i = 0; i < shelf->GetComponents().size(); i++)
				if (shelf->GetComponents()[i]->GetComponentType() == ComponentType::Physics2D) was = static_cast<Physics2D*>(shelf->GetComponents()[i].get());
			shelf->Remove(const_cast<Physics2D*>(was));
			first.reset();
			std::shared_ptr<Physics2D> again = std::make_shared<Physics2D>(Body2DType::Static, Shape2DType::Box, Vec2(0.5f, 0.25f));
			sameAddress = (again.get() == was);
			shelf->Add(std::static_pointer_cast<IComponent>(again));
			r.Run(2);
		}
		r.Run(180);
		// The shelf is now 1 wide (x -0.5 .. 0.5) and the ball is at x 1.5:
		// with the old body gone it falls to the floor.
		nearly(PosOf(ball).y, 0.25f, 0.05f, sameAddress
			? "a body replaced by a new component at the same address leaves no ghost behind"
			: "a body replaced by a new component leaves no ghost behind");
	}

	// --- a joint that gives way ----------------------------------------------
	{
		// Two weights on rods from the ceiling: one rod breaks at less than
		// its weight pulls, the other at far more.
		Rig r;
		std::shared_ptr<Physics2D> beam = r.Body("Beam", Vec2(0.f, 5.f), Body2DType::Static, Shape2DType::Box, Vec2(4.f, 0.2f));
		std::shared_ptr<Physics2D> weak = r.Body("Weak", Vec2(-2.f, 3.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<Physics2D> strong = r.Body("Strong", Vec2(2.f, 3.f), Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		(void)beam;
		int heard = 0;
		std::shared_ptr<Joint2D> jw = std::make_shared<Joint2D>((uint32)Joint2DType::Weld);
		jw->SetConnected("Beam");
		jw->SetBreakForce(weak->GetDensity() * 1.f * 3.f);        // mass 1 x g ~10: far more than this
		jw->OnBreak = [&heard]() { heard++; };
		weak->GetOwner()->Add(std::static_pointer_cast<IComponent>(jw));
		std::shared_ptr<Joint2D> js = std::make_shared<Joint2D>((uint32)Joint2DType::Weld);
		js->SetConnected("Beam");
		js->SetBreakForce(100000.f);
		strong->GetOwner()->Add(std::static_pointer_cast<IComponent>(js));
		r.Run(90);
		check(jw->IsBroken() && heard == 1, "a joint asked to hold more than its break force is undone, and says so once");
		check(PosOf(weak).y < 1.f, "what it held falls");
		check(!js->IsBroken(), "one that can take the load holds");
		nearly(PosOf(strong).y, 3.f, 0.05f, "and what it holds stays");
		jw->Mend();
		weak->GetOwner()->SetPosition(Vec3(-2.f, 3.f, 0.f));
		jw->SetBreakForce(0.f);
		r.Run(60);
		check(!jw->IsBroken() && jw->HaveJoint(), "mended, it is made again");
	}

	// --- a body made before the scene has walked its object ------------------
	{
		// A subtree put into a running scene - a prefab a script spawns -
		// and the world synced before the scene's next update: the bodies
		// are made where the objects are, not at the origin.
		Rig r;
		r.Body("Floor", Vec2(0.f, -0.5f), Body2DType::Static, Shape2DType::Box, Vec2(20.f, 0.5f));
		r.Run(2);
		std::shared_ptr<GameObject> root = std::make_shared<GameObject>();
		root->SetName("Level");
		root->SetPosition(Vec3(3.f, 0.f, 0.f));
		std::shared_ptr<GameObject> a = std::make_shared<GameObject>(), b = std::make_shared<GameObject>();
		a->SetPosition(Vec3(4.f, 0.5f, 0.f));
		b->SetPosition(Vec3(8.f, 0.5f, 0.f));
		std::shared_ptr<Physics2D> pa = std::make_shared<Physics2D>(Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		std::shared_ptr<Physics2D> pb = std::make_shared<Physics2D>(Body2DType::Dynamic, Shape2DType::Box, Vec2(0.5f, 0.5f));
		a->Add(std::static_pointer_cast<IComponent>(pa));
		b->Add(std::static_pointer_cast<IComponent>(pb));
		root->Add(a);
		root->Add(b);
		r.scene.Add(root);
		r.objects.push_back(root);
		r.world.Sync(&r.scene);
		check(pa->HaveBody() && pb->HaveBody(), "bodies of a subtree just added are made at the next sync");
		nearly(pa->GetPosition().x, 7.f, 0.01f, "a body made before the scene walked its object is where the object is");
		nearly(pb->GetPosition().x, 11.f, 0.01f, "and so is the one beside it");
		r.Run(60);
		nearly(PosOf(pa).x, 7.f, 0.05f, "the two stay where they were put");
		nearly(PosOf(pb).x, 11.f, 0.05f, "neither thrown by the other");
	}

	// --- a force asked for every frame pushes the same at any frame rate ----
	{
		const f64 rates[] = { 60.0, 30.0, 120.0, 144.0, 50.0, 75.0 };
		f32 speed[6], spin[6];
		for (int k = 0; k < 6; k++)
		{
			Rig r;
			std::shared_ptr<Physics2D> p = r.Body("Pushed", Vec2(0.f, 0.f), Body2DType::Dynamic, Shape2DType::Circle, Vec2(0.5f, 0.5f));
			p->SetGravityScale(0.f);
			p->SetFixedRotation(false);
			r.world.Sync(&r.scene);
			const f64 dt = 1.0 / rates[k];
			const int frames = (int)(2.0 * rates[k] + 0.5);          // two seconds of it
			for (int i = 0; i < frames; i++)
			{
				r.time += dt;
				r.scene.Update(r.time);
				r.world.Sync(&r.scene);
				p->ApplyForce(Vec2(3.f * p->GetMass(), 0.f));        // three metres a second, each second
				p->ApplyTorque(1.f);
				r.world.Step(dt, &r.scene);
			}
			speed[k] = p->GetLinearVelocity().x;
			spin[k] = p->GetAngularVelocity();
		}
		nearly(speed[0], 6.f, 0.12f, "a force held for two seconds gives the speed it should");
		check(fabsf(spin[0]) > 0.1f, "and a torque held turns the body");
		const char* names[] = { "", "at 30 frames a second it is the same", "and at 120", "and at 144", "and at 50", "and at 75" };
		for (int k = 1; k < 6; k++)
		{
			nearly(speed[k], speed[0], 0.03f * fabsf(speed[0]) + 0.06f, names[k]);
			nearly(spin[k], spin[0], 0.03f * fabsf(spin[0]) + 0.02f * fabsf(spin[0]), "(its spin too)");
		}
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
