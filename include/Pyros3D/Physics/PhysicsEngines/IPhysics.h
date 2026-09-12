//============================================================================
// Name        : IPhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Interface
//============================================================================

#ifndef IPHYSICS_H
#define IPHYSICS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Other/Export.h>
#include <memory>

namespace p3d {

	// Circular Dependency
	class PYROS3D_API IPhysicsComponent;

	// Result of a single closest-hit raycast (RayCast() below). Mirrors the
	// info every other engine's raycast returns: whether it hit anything,
	// where, which way the surface faces there, how far along the ray, and
	// which component was hit (so callers can identify/react to it, not
	// just know a hit happened).
	struct PYROS3D_API RayCastHit {
		bool hasHit;
		Vec3 point;
		Vec3 normal;
		f32 distance;
		IPhysicsComponent* component;

		RayCastHit() : hasHit(false), distance(0.f), component(NULL) {}
	};

	class PYROS3D_API IPhysics {

		friend class IPhysicsComponent;

	public:

		IPhysics();
		virtual ~IPhysics();

		virtual void InitPhysics() = 0;
		virtual void Update(const f64 &time, const uint32 steps) = 0;
		virtual void EnableDebugDraw() = 0;
		virtual void RenderDebugDraw(Projection projection, GameObject* Camera) = 0;
		virtual void DisableDebugDraw() = 0;
		virtual void EndPhysics() = 0;

		// Closest-hit raycast between two world-space points. Was entirely
		// absent from the engine before this - every "is this on the
		// ground", "what am I looking at", "where did this shot land" query
		// had no real answer.
		virtual RayCastHit RayCast(const Vec3 &from, const Vec3 &to) = 0;

		virtual void RemovePhysicsComponent(IPhysicsComponent* pcomp) = 0;

		virtual void UpdateTransformations(IPhysicsComponent* pcomp) = 0;

		bool IsInitialized() { return physicsInitialized; }

		virtual void UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position) = 0;
		// Where the solver actually put it. A body was write-only for
		// position: settable, never readable, so nothing could observe a body
		// that has no GameObject of its own to be written back into - every
		// part of a ragdoll except the one carrying the mesh.
		virtual Vec3 GetBodyPosition(IPhysicsComponent *pcomp) { return Vec3(); }
		virtual Quaternion GetBodyRotation(IPhysicsComponent *pcomp) { return Quaternion(); }
		virtual void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation) = 0;
		// The same thing without the Euler round trip. Orienting a body
		// ALONG something - a limb capsule along the bone it drives, a plank
		// along a slope - produces an arbitrary quaternion, and pushing that
		// through Euler angles costs an asin() and loses a degree of freedom
		// near the poles. The default keeps a backend that only speaks Euler
		// working; Box3D overrides it and sets the quaternion directly.
		virtual void UpdateRotationQuat(IPhysicsComponent *pcomp, const Quaternion &rotation)
		{
			Quaternion q = rotation;
			UpdateRotation(pcomp, q.GetEulerFromQuaternion());
		}
		virtual void CleanForces(IPhysicsComponent *pcomp) = 0;
		virtual void SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity) = 0;
		virtual void SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity) = 0;
		virtual void Activate(IPhysicsComponent *pcomp) = 0;
		// Real, previously-missing gap (same category as RayCast() above -
		// scripting/gameplay code had no way to read a body's current
		// velocity, push it, or change its mass at runtime, only set an
		// absolute velocity or clear forces outright). Mirrors the
		// Set*Velocity pair with real getters, and adds the two most
		// commonly needed force-application variants (central, i.e.
		// through the body's center of mass - no torque component) rather
		// than the full force/impulse-at-point Bullet API surface, to
		// keep this additive without chasing every overload.
		virtual Vec3 GetLinearVelocity(IPhysicsComponent *pcomp) = 0;
		virtual Vec3 GetAngularVelocity(IPhysicsComponent *pcomp) = 0;
		virtual void ApplyCentralForce(IPhysicsComponent *pcomp, const Vec3 &force) = 0;
		virtual void ApplyCentralImpulse(IPhysicsComponent *pcomp, const Vec3 &impulse) = 0;
		// The same push, but at a point in world space rather than through the
		// centre of mass - so it also spins the body. That is the whole
		// difference between a corpse that was SHOT and one that fell over:
		// a hit lands somewhere specific and turns the body about it.
		virtual void ApplyImpulseAtPoint(IPhysicsComponent *pcomp, const Vec3 &impulse, const Vec3 &worldPoint) {}
		virtual void ApplyForceAtPoint(IPhysicsComponent *pcomp, const Vec3 &force, const Vec3 &worldPoint) {}
		virtual void SetMass(IPhysicsComponent *pcomp, const f32 mass) = 0;

		// Create Physics Components
		virtual std::shared_ptr<IPhysicsComponent> CreateBox(const f32 width, const f32 height, const f32 depth, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateCapsule(const f32 radius, const f32 height, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateCone(const f32 radius, const f32 height, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateConvexHull(const std::vector<Vec3> &points, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateConvexTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateCylinder(const f32 radius, const f32 height, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateSphere(const f32 radius, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateStaticPlane(const Vec3 &Normal, const f32 Constant, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateTriangleMesh(RenderingComponent* rcomp, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f, bool ghost = false) = 0;
		virtual std::shared_ptr<IPhysicsComponent> CreateVehicle(const std::shared_ptr<IPhysicsComponent> &ChassisShape, bool ghost = false) = 0;
		virtual void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel) = 0;

		// ---- Joints ---------------------------------------------------
		//
		// Two bodies pinned together. This is what a ragdoll, a swinging
		// door, a rope bridge or a chain is made of, and the engine had no
		// way to express any of them: every physics body was an island.
		//
		// Not pure virtual, so an engine backend that has no joints keeps
		// compiling - it simply reports that it made none. Returns a handle,
		// or 0 if the joint could not be made.
		//
		// Spherical = ball-and-socket: the two bodies share a point and can
		// rotate freely about it. `coneAngle` (radians, <= 0 for none) limits
		// how far B can swing away from A's axis, which is the difference
		// between a shoulder and a rag.
		//
		// `worldAxis` is that axis, and it matters: the cone is centred on it
		// and the twist is measured about it. Passed as zero the joint frames
		// are world-aligned, so the cone ends up centred on world +Z and a
		// "60 degree neck" means 60 degrees away from whatever direction the
		// level happens to call forward. Pass the bone's own direction and
		// the limit means what it reads as - a limb may swing `coneAngle`
		// away from its rest direction, and twist about its own length
		// between `twistLower` and `twistUpper` (equal values = free twist).
		virtual uint32 CreateSphericalJoint(IPhysicsComponent* bodyA, IPhysicsComponent* bodyB,
			const Vec3 &worldAnchor, const f32 coneAngle = -1.f,
			const Vec3 &worldAxis = Vec3(0.f, 0.f, 0.f),
			const f32 twistLower = 0.f, const f32 twistUpper = 0.f) { return 0; }

		// Revolute = hinge: one shared point, one shared axis. `lower`/`upper`
		// are the swing limits in radians; pass lower >= upper for a free
		// hinge. A knee or an elbow is a hinge with a hard stop at 0.
		virtual uint32 CreateRevoluteJoint(IPhysicsComponent* bodyA, IPhysicsComponent* bodyB,
			const Vec3 &worldAnchor, const Vec3 &worldAxis,
			const f32 lower = 1.f, const f32 upper = 0.f) { return 0; }

		// Muscle tone. A joint with a spring pulls back toward the pose it was
		// created in instead of hanging completely limp, which is the
		// difference between a ragdoll and a sack: a real body resists its own
		// joints even after it stops driving them. `hertz` is the spring
		// frequency (low is soft - around 1-3 for a corpse), `damping` its
		// ratio (1 = critically damped, no wobble). hertz <= 0 turns it off.
		//
		// Box3D has had all of this since it was vendored and none of it was
		// reachable, the same way the joints themselves were not.
		virtual void SetJointSpring(const uint32 joint, const f32 hertz, const f32 damping) {}

		virtual void DestroyJoint(const uint32 joint) {}

		// Per-body damping and gravity scale. Box3D has had all three since
		// forever and none were reachable, so every dynamic body fell at
		// exactly 9.8 m/s^2 with no drag - which is right for a brick and
		// wrong for anything that is supposed to read as flesh. A ragdoll
		// with no damping snaps to the floor in about four frames.
		virtual void SetLinearDamping(IPhysicsComponent* pcomp, const f32 damping) {}
		virtual void SetAngularDamping(IPhysicsComponent* pcomp, const f32 damping) {}
		virtual void SetGravityScale(IPhysicsComponent* pcomp, const f32 scale) {}

	protected:

		virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp) = 0;

		// Save Physics Components List
		std::vector<IPhysicsComponent*> _PhysicsList;

		// Save Physics Initialization Flag
		bool physicsInitialized;

		// Timer
		f64 lastTime, timeInterval;
	};

};

#endif /*PHYSICS_H*/