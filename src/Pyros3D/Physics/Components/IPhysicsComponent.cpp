//============================================================================
// Name        : IPhysicsComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Component Interface
//============================================================================

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>

namespace p3d {

	IPhysicsComponent::~IPhysicsComponent()
	{
		// Deliberately empty, not a gap: the real physics-side cleanup
		// (body/shapes/joints/handles) happens in
		// Box3DPhysics::RemovePhysicsComponent(), called from Unregister()
		// below, which the scene graph runs when the owning object leaves
		// the scene. A component still in a scene when its last reference
		// goes is the case this cannot serve - rigidBodyPTR is a void*
		// whose concrete type varies by shape, so it cannot be blindly
		// deleted here, and by this point there is no knowing whether the
		// engine is still alive to be asked.
	}

	void IPhysicsComponent::Register(SceneGraph* Scene)
	{
		if (!Registered && PhysicsEngine != NULL)
		{
			Registered = true;
			PhysicsEngine->CreatePhysicsComponent(this);
		}
	}

	// Unregistering does NOT forget which engine this component belongs to.
	//
	// It used to null PhysicsEngine here, which made removing an object from
	// the scene and adding it back a crash: Register() above dereferences the
	// pointer, and re-adding is not exotic - the editor does it every time it
	// stops play mode. The null also reached the other twenty-odd methods
	// below, none of which checked it, so a component the script still held a
	// reference to was a live grenade from the moment its object left the
	// scene.
	//
	// Keeping the pointer costs nothing it was actually buying. Nulling it
	// looked like protection against the engine outliving the component, but
	// EndPhysics() destroys the world without touching a single component, so
	// nothing ever cleared these pointers on that path anyway - the null only
	// ever fired on scene removal, which is precisely the case that wants the
	// pointer kept.
	//
	// Idempotent, because nothing guarantees it is called once:
	// GameObject::UnregisterComponents() walks the whole list unconditionally,
	// and both RemoveComponent() and SceneGraph::Remove() reach it.
	void IPhysicsComponent::Unregister(SceneGraph* Scene)
	{
		if (!Registered) return;
		Registered = false;
		if (PhysicsEngine != NULL) PhysicsEngine->RemovePhysicsComponent(this);
	}
	void IPhysicsComponent::Init()
	{
		Registered = false;
	}
	void IPhysicsComponent::Destroy()
	{

	}

	const f32 IPhysicsComponent::GetMass() const
	{
		return mass;
	}
	const unsigned IPhysicsComponent::GetShape() const
	{
		return Shape;
	}

	void IPhysicsComponent::Update(const f64 time)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->UpdateTransformations((IPhysicsComponent*)this);
	}

	void IPhysicsComponent::SetPosition(const Vec3 &position)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->UpdatePosition(this, position);
	}
	Vec3 IPhysicsComponent::GetPosition() const
	{
		if (PhysicsEngine == NULL) return Vec3();
		return PhysicsEngine->GetBodyPosition(const_cast<IPhysicsComponent*>(this));
	}
	Quaternion IPhysicsComponent::GetRotationQuat() const
	{
		if (PhysicsEngine == NULL) return Quaternion();
		return PhysicsEngine->GetBodyRotation(const_cast<IPhysicsComponent*>(this));
	}
	void IPhysicsComponent::SetLinearDamping(const f32 damping)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->SetLinearDamping(this, damping);
	}
	void IPhysicsComponent::SetAngularDamping(const f32 damping)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->SetAngularDamping(this, damping);
	}
	void IPhysicsComponent::SetGravityScale(const f32 scale)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->SetGravityScale(this, scale);
	}
	void IPhysicsComponent::SetRotation(const Vec3 &rotation)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->UpdateRotation(this, rotation);
	}
	void IPhysicsComponent::SetRotationQuat(const Quaternion &rotation)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->UpdateRotationQuat(this, rotation);
	}
	void IPhysicsComponent::CleanForces()
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->CleanForces(this);
	}
	void IPhysicsComponent::SetAngularVelocity(const Vec3 &velocity)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->SetAngularVelocity(this, velocity);
	}
	void IPhysicsComponent::SetLinearVelocity(const Vec3 &velocity)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->SetLinearVelocity(this, velocity);
	}
	void IPhysicsComponent::Activate()
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->Activate(this);
	}
	Vec3 IPhysicsComponent::GetLinearVelocity()
	{
		if (PhysicsEngine == NULL) return Vec3();
		return PhysicsEngine->GetLinearVelocity(this);
	}
	Vec3 IPhysicsComponent::GetAngularVelocity()
	{
		if (PhysicsEngine == NULL) return Vec3();
		return PhysicsEngine->GetAngularVelocity(this);
	}
	void IPhysicsComponent::ApplyCentralForce(const Vec3 &force)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->ApplyCentralForce(this, force);
	}
	void IPhysicsComponent::ApplyCentralImpulse(const Vec3 &impulse)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->ApplyCentralImpulse(this, impulse);
	}
	void IPhysicsComponent::ApplyImpulseAtPoint(const Vec3 &impulse, const Vec3 &worldPoint)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->ApplyImpulseAtPoint(this, impulse, worldPoint);
	}
	void IPhysicsComponent::ApplyForceAtPoint(const Vec3 &force, const Vec3 &worldPoint)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->ApplyForceAtPoint(this, force, worldPoint);
	}

	void IPhysicsComponent::SetMass(const f32 newMass)
	{
		mass = newMass;
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->SetMass(this, newMass);
	}
	void IPhysicsComponent::InternalAddWheel(const Vec3& WheelDirection, const Vec3& WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3& Position, bool isFrontWheel)
	{
		if (PhysicsEngine == NULL) return;
		PhysicsEngine->AddWheel(this, WheelDirection, WheelAxle, WheelRadius, WheelWidth, WheelFriction, WheelRollInfluence, Position, isFrontWheel);
	}
}