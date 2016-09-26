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
            Vehicle
        };
    };


    class PYROS3D_API IPhysicsComponent : public IComponent 
    {
        
        friend class GameObject;
        
        public:
            
            virtual void Register(SceneGraph* Scene);
            virtual void Init();
            virtual void Update();
            virtual void Destroy();
            virtual void Unregister(SceneGraph* Scene);
            
            const f32 GetMass() const;
            const uint32 GetShape() const;
            
            void SaveRigidBodyPTR(void* ptr) { rigidBodyPTR = ptr; rigidBodyRegistered = true; }
            void* GetRigidBodyPTR() { return rigidBodyPTR; }
            bool RigidBodyRegistered() { return rigidBodyRegistered; }
            
            // Physics Methods Attributes
            virtual void SetPosition(const Vec3 &position);
            virtual void SetRotation(const Vec3 &rotation);
            virtual void CleanForces();
            virtual void SetAngularVelocity(const Vec3 &velocity);
            virtual void SetLinearVelocity(const Vec3 &velocity);
            virtual void Activate();
            
            virtual ~IPhysicsComponent();
            
        protected:
            
            // Add Wheel to Vehicle
            void InternalAddWheel(const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);
            
            // Protected Constructor
            IPhysicsComponent(const f32 Mass, const uint32 shape, IPhysics* engine) : mass(Mass), Shape(shape), rigidBodyRegistered(false), PhysicsEngine(engine), IComponent() {}
            

            // Keep Shape Type
            unsigned Shape;
            f32 mass;
            void* rigidBodyPTR;
            bool rigidBodyRegistered;
            IPhysics* PhysicsEngine;
    };

};

#endif /*IPHYSICSCOMPONENT_H*/