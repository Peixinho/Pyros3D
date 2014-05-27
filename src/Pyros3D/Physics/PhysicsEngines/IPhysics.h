//============================================================================
// Name        : IPhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Interface
//============================================================================

#ifndef IPHYSICS_H
#define IPHYSICS_H

#include "../../Core/Math/Math.h"
#include "../../Rendering/Components/Rendering/RenderingComponent.h"
#include "../../Core/Projection/Projection.h"
#include "../../Other/Export.h"

namespace p3d {
    
    // Circular Dependency
    class PYROS3D_API IPhysicsComponent;
    
    class PYROS3D_API IPhysics {
        
        friend class IPhysicsComponent;
        
        public:

            IPhysics();
            virtual ~IPhysics();
            
            virtual void InitPhysics() = 0;
            virtual void Update(const double &time, const unsigned &steps) = 0;
            virtual void EnableDebugDraw() = 0;
            virtual void RenderDebugDraw(Projection projection, GameObject* Camera) = 0;
            virtual void DisableDebugDraw() = 0;
            virtual void EndPhysics() = 0;

            virtual void RemovePhysicsComponent(IPhysicsComponent* pcomp) = 0;
            
            virtual void UpdateTransformations(IPhysicsComponent* pcomp) = 0;
            
            bool IsInitialized() { return physicsInitialized; }
            
            virtual void UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position) = 0;
            virtual void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation) = 0;
            virtual void CleanForces(IPhysicsComponent *pcomp) = 0;
            virtual void SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity) = 0;
            virtual void SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity) = 0;
            virtual void Activate(IPhysicsComponent *pcomp) = 0;
            
            // Create Physics Components
            virtual IComponent* CreateBox(const f32 &width, const f32 &height, const f32 &depth, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateCapsule(const f32 &radius, const f32 &height, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateCone(const f32 &radius, const f32 &height, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateConvexHull(const std::vector<Vec3> &points, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateConvexTriangleMesh(const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateCylinder(const f32 &radius, const f32 &height, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateSphere(const f32 &radius, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateStaticPlane(const Vec3 &Normal, const f32 &Constant, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateTriangleMesh(RenderingComponent* rcomp, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateTriangleMesh(const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass = 0.f) = 0;
            virtual IComponent* CreateVehicle(IPhysicsComponent* ChassisShape) = 0;
            virtual void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 &WheelRadius, const f32 &WheelWidth, const f32 &WheelFriction, const f32 &WheelRollInfluence, const Vec3 &Position, bool isFrontWheel) = 0;
        
    protected:
            
            virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp) = 0;
            
            // Save Physics Components List
            std::vector<IPhysicsComponent*> _PhysicsList;
            
            // Save Physics Initialization Flag
            bool physicsInitialized;
            
            // Timer
            f96 lastTime, nowTime, timeInterval;
    };
    
};

#endif /*PHYSICS_H*/