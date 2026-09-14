//============================================================================
// Name        : IPhysicsComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Component Interface
//============================================================================

#ifndef IPHYSICSCOMPONENT_H
#define	IPHYSICSCOMPONENT_H

#include <iostream>
#include <functional>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	namespace CollisionShapes {
		enum {
			Box = 0,
			Sphere,
			Cylinder,
			Cone,
			Capsule,
			MultipleSphere,
			ConvexHull,
			ConvexTriangleMesh,
			TriangleMesh,
			HeightFieldTerrain,
			StaticPlane,
			Vehicle,
			Ghost
		};
	};


	class PYROS3D_API IPhysicsComponent : public IComponent
	{

		friend class GameObject;

	public:

		virtual void Register(SceneGraph* Scene);
		virtual void Init();
		virtual void Update(const f64 time = 0);
		virtual void Destroy();
		virtual void Unregister(SceneGraph* Scene);

		const f32 GetMass() const;
		const uint32 GetShape() const;

		virtual uint32 GetComponentType() const { return ComponentType::Physics; }

		void SaveRigidBodyPTR(void* ptr) { rigidBodyPTR = ptr; rigidBodyRegistered = true; }
		void ClearRigidBodyPTR() { rigidBodyPTR = NULL; rigidBodyRegistered = false; }
		void* GetRigidBodyPTR() { return rigidBodyPTR; }
		bool RigidBodyRegistered() { return rigidBodyRegistered; }
		bool IsGhost() { return isGhost; }

		// Physics Methods Attributes
		virtual void SetPosition(const Vec3 &position);
		virtual Vec3 GetPosition() const;
		virtual Quaternion GetRotationQuat() const;
		virtual void SetLinearDamping(const f32 damping);
		virtual void SetAngularDamping(const f32 damping);
		virtual void SetGravityScale(const f32 scale);
		virtual void SetRotation(const Vec3 &rotation);
		// Orientation without the Euler round trip - see
		// IPhysics::UpdateRotationQuat. GetRotationQuat() above has always
		// returned a quaternion; there was no way to put one back.
		virtual void SetRotationQuat(const Quaternion &rotation);
		virtual void CleanForces();
		virtual void SetAngularVelocity(const Vec3 &velocity);
		virtual void SetLinearVelocity(const Vec3 &velocity);
		virtual void Activate();
		virtual Vec3 GetLinearVelocity();
		virtual Vec3 GetAngularVelocity();
		virtual void ApplyCentralForce(const Vec3 &force);
		virtual void ApplyCentralImpulse(const Vec3 &impulse);
		// Impulse/force at a world-space point: pushes AND spins.
		virtual void ApplyImpulseAtPoint(const Vec3 &impulse, const Vec3 &worldPoint);
		virtual void ApplyForceAtPoint(const Vec3 &force, const Vec3 &worldPoint);
		// Keeps GetMass()'s own cached `mass` member (see below) in sync,
		// unlike the other setters here - position/rotation/velocity
		// aren't cached on this class at all (GetOwner()'s GameObject
		// transform is the live source of truth, kept in sync every
		// Update() via UpdateTransformations()), but mass is, so this one
		// setter has to update both.
		virtual void SetMass(const f32 mass);

		// Real, previously-entirely-absent collision notification -
		// before this, nothing (C++ or Lua) had any way to know two
		// bodies had touched. Plain std::function fields, not a
		// Gallant::Signal (this codebase's other event mechanism, used
		// by InputManager) - Signal's Connect() only accepts a
		// compile-time member-function-pointer delegate, no
		// std::function overload, which would force an unused wrapper
		// object here for no benefit; a single std::function per
		// component matches every other "let something external hook a
		// lifecycle moment" pattern in this codebase (see e.g.
		// LUA_GameObject::on_update in PyrosBindings.h) and binds to Lua
        // as a plain assignable property the same proven way. Populated
		// by Box3DPhysics::Update()'s post-step contact/sensor events;
		// fires with the *other* component involved. Left unset (empty)
		// costs nothing to check per-frame for a component that doesn't
		// use it.
		std::function<void(IPhysicsComponent*)> OnCollisionEnter;
		std::function<void(IPhysicsComponent*)> OnCollisionExit;

		virtual ~IPhysicsComponent();

	protected:

		// Add Wheel to Vehicle
		void InternalAddWheel(const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);

		// Protected Constructor
		//
		// rigidBodyPTR starts NULL and stays that way until the physics
		// backend registers this component and hands its handles over. That
		// window is not a corner case: a component only registers when the
		// scene graph next picks it up, NOT when addComponent() returns, so
		// every call a script makes on a body it has just created - which is
		// most of what building a rig in init() consists of - runs while this
		// is still unset. The backend's accessors all check it for NULL and
		// no-op; leaving it uninitialized handed them whatever the allocator
		// had lying in that word instead. On a platform whose fresh pages
		// come back zeroed that reads as NULL and the checks hold, which is
		// why it went unnoticed; on Windows, where the heap recycles blocks,
		// it is a live pointer-shaped value that passes every guard and is
		// then dereferenced.
		//
		// Members are listed here in declaration order - the compiler
		// initializes them in that order regardless of how they are written,
		// and the previous shuffled ordering is what made the one MISSING
		// entry so easy to miss.
		IPhysicsComponent(const f32 Mass, const uint32 shape, IPhysics* engine, bool ghost = false)
			: IComponent(), Shape(shape), mass(Mass), rigidBodyPTR(NULL),
			rigidBodyRegistered(false), isGhost(ghost), PhysicsEngine(engine) {}


		// Keep Shape Type
		unsigned Shape;
		f32 mass;
		void* rigidBodyPTR;
		bool rigidBodyRegistered;
		bool isGhost;
		IPhysics* PhysicsEngine;
	};

};

#endif /*IPHYSICSCOMPONENT_H*/