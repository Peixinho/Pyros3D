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
#include <Pyros3D/Physics/Physics.h>
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

		void SaveRigidBodyPTR(void* ptr) { rigidBodyPTR = ptr; rigidBodyRegistered = true; }
		void* GetRigidBodyPTR() { return rigidBodyPTR; }
		bool RigidBodyRegistered() { return rigidBodyRegistered; }
		bool IsGhost() { return isGhost; }

		// Physics Methods Attributes
		virtual void SetPosition(const Vec3 &position);
		virtual void SetRotation(const Vec3 &rotation);
		virtual void CleanForces();
		virtual void SetAngularVelocity(const Vec3 &velocity);
		virtual void SetLinearVelocity(const Vec3 &velocity);
		virtual void Activate();
		virtual Vec3 GetLinearVelocity();
		virtual Vec3 GetAngularVelocity();
		virtual void ApplyCentralForce(const Vec3 &force);
		virtual void ApplyCentralImpulse(const Vec3 &impulse);
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
		// by BulletPhysics::Update()'s post-step contact-manifold scan;
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
		IPhysicsComponent(const f32 Mass, const uint32 shape, IPhysics* engine, bool ghost = false) : mass(Mass), Shape(shape), rigidBodyRegistered(false), PhysicsEngine(engine), isGhost(ghost), IComponent() {}


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