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
		virtual void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation) = 0;
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
		virtual uint32 CreateSphericalJoint(IPhysicsComponent* bodyA, IPhysicsComponent* bodyB,
			const Vec3 &worldAnchor, const f32 coneAngle = -1.f) { return 0; }

		// Revolute = hinge: one shared point, one shared axis. `lower`/`upper`
		// are the swing limits in radians; pass lower >= upper for a free
		// hinge. A knee or an elbow is a hinge with a hard stop at 0.
		virtual uint32 CreateRevoluteJoint(IPhysicsComponent* bodyA, IPhysicsComponent* bodyB,
			const Vec3 &worldAnchor, const Vec3 &worldAxis,
			const f32 lower = 1.f, const f32 upper = 0.f) { return 0; }

		virtual void DestroyJoint(const uint32 joint) {}

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