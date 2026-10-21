//============================================================================
// Name        : CharacterController.cpp
// Description : A character walking through the physics world, not pushed by it.
//============================================================================

#include <Pyros3D/Physics/CharacterController.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <box3d/box3d.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace p3d {

	namespace {
		const int kMaxPlanes = 32;
		struct Planes { b3CollisionPlane p[kMaxPlanes]; int n = 0; Vec3 up; f32 minUp; bool ground = false; Vec3 groundNormal; bool steepAsWalls = false; };

		bool CollectPlane(b3ShapeId shapeId, const b3PlaneResult* planes, int count, void* context)
		{
			if (b3Shape_IsSensor(shapeId)) return true;
			Planes* out = (Planes*)context;
			for (int i = 0; i < count && out->n < kMaxPlanes; i++)
			{
				b3CollisionPlane &c = out->p[out->n++];
				c.plane = planes[i].plane;
				// Walking, a slope too steep to climb is a wall: its normal laid flat, so the solver
				// slides along it instead of up it (up a little, back down: it shook in place).
				const f32 ny = planes[i].plane.normal.y;
				if (out->steepAsWalls && ny > 0.f && ny < out->minUp)
				{
					const f32 hx = planes[i].plane.normal.x, hz = planes[i].plane.normal.z, l = sqrtf(hx * hx + hz * hz);
					// (the offset kept: planes are relative to the mover, so its separation now - minus the
					// offset, whatever the normal - is unchanged; only the way out of it is)
					if (l > 1e-4f) c.plane.normal = b3Vec3{ hx / l, 0.f, hz / l };
				}
				c.pushLimit = FLT_MAX;
				c.push = 0.f;
				c.clipVelocity = true;
				const Vec3 nrm(planes[i].plane.normal.x, planes[i].plane.normal.y, planes[i].plane.normal.z);
				if (nrm.y >= out->minUp) { out->ground = true; out->groundNormal = nrm; }
			}
			return true;
		}
		bool NotSensor(b3ShapeId shapeId, void*) { return !b3Shape_IsSensor(shapeId); }

		b3Pos ToPos(const Vec3 &v) { b3Pos p; p.x = v.x; p.y = v.y; p.z = v.z; return p; }
		b3Vec3 ToB3(const Vec3 &v) { return b3Vec3{ v.x, v.y, v.z }; }
		Vec3 FromB3(const b3Vec3 &v) { return Vec3(v.x, v.y, v.z); }
	}

	CharacterController::CharacterController(IPhysics* physics)
		: physics(physics), radius(0.35f), height(1.8f), stepHeight(0.35f), maxSlope(46.f), gravity(-9.81f),
		  verticalSpeed(0.f), jumpWanted(0.f), groundNormal(0.f, 1.f, 0.f), grounded(false), running(false), lastTime(-1.0)
	{
	}

	void CharacterController::Teleport(const Vec3 &p)
	{
		GameObject* owner = GetOwner();
		if (!owner) return;
		owner->SetPosition(owner->GetParent() ? owner->GetParent()->GetWorldTransformation().Inverse() * p : p);
		verticalSpeed = 0.f;
	}

	bool CharacterController::Step(Vec3 &pos, const Vec3 &delta, const bool walking)
	{
		Box3DPhysics* box = dynamic_cast<Box3DPhysics*>(physics);
		const b3WorldId world = box->GetWorld();
		b3Capsule capsule;
		capsule.center1 = b3Vec3{ 0.f, radius, 0.f };
		capsule.center2 = b3Vec3{ 0.f, height - radius, 0.f };
		capsule.radius = radius;
		const b3QueryFilter filter = b3DefaultQueryFilter();
		Vec3 target = delta;
		// (a few rounds: each leaves along the planes it meets, the next goes on from there)
		for (int round = 0; round < 5; round++)
		{
			Planes planes;
			planes.minUp = cosf(maxSlope * 3.14159265f / 180.f);
			planes.steepAsWalls = walking;
			b3World_CollideMover(world, ToPos(pos), &capsule, filter, CollectPlane, &planes);
			const b3PlaneSolverResult solved = b3SolvePlanes(ToB3(target), planes.p, planes.n);
			const Vec3 want = FromB3(solved.delta);
			const f32 fraction = b3World_CastMover(world, ToPos(pos), &capsule, solved.delta, filter, NotSensor, NULL);
			const Vec3 moved = want * fraction;
			pos = pos + moved;
			if (fraction >= 1.f) return true;
			// (what is left of the way, along whatever stopped it)
			target = FromB3(b3ClipVector(ToB3(target - moved), planes.p, planes.n));
			if (target.magnitude() < 1e-4f) return false;
		}
		return false;
	}

	bool CharacterController::Probe(const Vec3 &pos, const f32 down, f32 &dropped, Vec3 &normal) const
	{
		Box3DPhysics* box = dynamic_cast<Box3DPhysics*>(physics);
		const b3WorldId world = box->GetWorld();
		b3Capsule capsule;
		capsule.center1 = b3Vec3{ 0.f, radius, 0.f };
		capsule.center2 = b3Vec3{ 0.f, height - radius, 0.f };
		capsule.radius = radius;
		const b3QueryFilter filter = b3DefaultQueryFilter();
		const f32 fraction = b3World_CastMover(world, ToPos(pos), &capsule, b3Vec3{ 0.f, -down, 0.f }, filter, NotSensor, NULL);
		dropped = down * fraction;
		if (fraction >= 1.f) return false;
		// What it would stand on there: ground only if not too steep.
		Planes planes;
		planes.minUp = cosf(maxSlope * 3.14159265f / 180.f);
		b3World_CollideMover(world, ToPos(pos + Vec3(0.f, -dropped - 0.01f, 0.f)), &capsule, filter, CollectPlane, &planes);
		normal = planes.groundNormal;
		return planes.ground;
	}

	void CharacterController::Update(const f64 time)
	{
		f32 dt = lastTime >= 0.0 ? (f32)(time - lastTime) : 0.f;
		lastTime = time;
		GameObject* owner = GetOwner();
		if (!running || !owner || dt <= 0.f || !dynamic_cast<Box3DPhysics*>(physics)) return;
		if (dt > 0.05f) dt = 0.05f;                       // (a stall is not a fall through the floor)

		Vec3 pos = owner->GetWorldPosition();
		const Vec3 start = pos;
		const bool wasGrounded = grounded;

		// Up and down: a jump from the ground, gravity in the air, a little press on the ground
		// (so a slope down is followed, not left).
		if (jumpWanted > 0.f && grounded) { verticalSpeed = jumpWanted; grounded = false; }
		jumpWanted = 0.f;
		if (grounded && verticalSpeed <= 0.f) verticalSpeed = 0.f;
		else verticalSpeed += gravity * dt;

		const Vec3 horizontal = move * dt;
		// Along the ground; and when that is stopped short, once more from a step's height up -
		// a stair, a kerb - coming down onto it after.
		Vec3 low = pos;
		Step(low, horizontal, grounded);
		pos = low;
		// (stopped short by how far it got - the planes may have taken the whole move away before
		// the cast, which then "arrives" having gone nowhere)
		const Vec3 got = low - start;
		const f32 wanted2 = horizontal.x * horizontal.x + horizontal.z * horizontal.z;
		const bool shortOf = got.x * got.x + got.z * got.z < wanted2 * 0.81f;
		if (shortOf && grounded && stepHeight > 0.f && wanted2 > 1e-8f)
		{
			Vec3 high = start;
			Step(high, Vec3(0.f, stepHeight, 0.f));
			const f32 rose = high.y - start.y;
			Step(high, horizontal, true);
			f32 fell = 0.f; Vec3 n;
			Probe(high, rose + 0.05f, fell, n);
			high.y -= fell;
			// Worth it only if it got further, and has something to stand on ahead: what is under the
			// capsule's leading edge, looked for straight down. (The capsule itself, just over a step's
			// edge, touches the corner - a normal steeper than any slope it may climb - and asking it
			// refused every step after the first.)
			const Vec3 dl = low - start, dh = high - start;
			bool lands = false;
			const f32 run = sqrtf(horizontal.x * horizontal.x + horizontal.z * horizontal.z);
			if (run > 1e-6f && dh.x * dh.x + dh.z * dh.z > dl.x * dl.x + dl.z * dl.z + 1e-6f && high.y > start.y + 0.01f)
			{
				const Vec3 ahead = high + Vec3(horizontal.x, 0.f, horizontal.z) * (radius * 0.9f / run);
				const RayCastHit hit = physics->RayCast(ahead + Vec3(0.f, stepHeight + 0.05f, 0.f), ahead - Vec3(0.f, 0.1f, 0.f));
				lands = hit.hasHit && hit.normal.y >= cosf(maxSlope * 3.14159265f / 180.f);
			}
			static const bool trace = getenv("PYROS_CC_TRACE") != NULL;
			if (trace)
			{
				static int printed = 0;
				if (printed++ < 12) fprintf(stderr, "[cc] step-up: low (%.3f,%.3f) high (%.3f,%.3f) start.y %.3f rose %.3f fell %.3f run %.4f lands %d\n",
					low.x, low.y, high.x, high.y, start.y, rose, fell, run, (int)lands);
			}
			if (lands) pos = high;
		}
		// Then up or down.
		if (fabsf(verticalSpeed) > 0.f) Step(pos, Vec3(0.f, verticalSpeed * dt, 0.f));

		// On the ground? (Looked for a step's height below while it was walking on it, and drawn
		// down onto it: down stairs and slopes it stays on its feet.)
		f32 dropped = 0.f; Vec3 n(0.f, 1.f, 0.f);
		const f32 reach = (wasGrounded && verticalSpeed <= 0.f) ? stepHeight + 0.05f : 0.05f;
		grounded = verticalSpeed <= 0.f && Probe(pos, reach, dropped, n);
		if (grounded) { pos.y -= dropped; groundNormal = n; verticalSpeed = 0.f; }
		else groundNormal = Vec3(0.f, 1.f, 0.f);

		velocity = (pos - start) / dt;
		owner->SetPosition(owner->GetParent() ? owner->GetParent()->GetWorldTransformation().Inverse() * pos : pos);
	}
}
