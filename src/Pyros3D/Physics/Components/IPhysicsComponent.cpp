//============================================================================
// Name        : IPhysicsComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Component Interface
//============================================================================

#include "IPhysicsComponent.h"
#include "../../SceneGraph/SceneGraph.h"

namespace p3d {
    
    IPhysicsComponent::~IPhysicsComponent()
    {
//      delete rigidBodyPTR;
    }

    void IPhysicsComponent::Register(SceneGraph* Scene)
    {
        if (!Registered)
        {
            Registered = true;
            PhysicsEngine->CreatePhysicsComponent(this);
        }
    }
        
    void IPhysicsComponent::Unregister(SceneGraph* Scene)
    {
        Registered = false;
        
        PhysicsEngine->RemovePhysicsComponent(this);
    }
    void IPhysicsComponent::Init()
    {
        
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

    void IPhysicsComponent::Update()
    {
        PhysicsEngine->UpdateTransformations((IPhysicsComponent*)this);
    }

    void IPhysicsComponent::SetPosition(const Vec3 &position)
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->UpdatePosition(this,position);
    }
    void IPhysicsComponent::SetRotation(const Vec3 &rotation)
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->UpdateRotation(this,rotation);
    }
    void IPhysicsComponent::CleanForces()
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->CleanForces(this);
    }
    void IPhysicsComponent::SetAngularVelocity(const Vec3 &velocity)
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->SetAngularVelocity(this, velocity);
    }
    void IPhysicsComponent::SetLinearVelocity(const Vec3 &velocity)
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->SetLinearVelocity(this, velocity);
    }
    void IPhysicsComponent::Activate()
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->Activate(this);
    }
    void IPhysicsComponent::InternalAddWheel(const Vec3& WheelDirection, const Vec3& WheelAxle, const f32& WheelRadius, const f32& WheelWidth, const f32& WheelFriction, const f32& WheelRollInfluence, const Vec3& Position, bool isFrontWheel)
    {
//            SceneGraph* scene = static_cast<SceneGraph*> (RegisterPTR);
//            Physics* PhysicsPTR = (Physics*) scene->GetPhysics();
//            PhysicsPTR->AddWheel(this,WheelDirection,WheelAxle,WheelRadius,WheelWidth,WheelFriction,WheelRollInfluence,Position,isFrontWheel);
    }
}