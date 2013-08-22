//============================================================================
// Name        : Bullet Physics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bullet Physics Wrapper
//============================================================================

#ifndef BULLETPHYSICS_H
#define BULLETPHYSICS_H

#include "btBulletDynamicsCommon.h"
#include "LinearMath/btIDebugDraw.h"
#include "../IPhysics.h"
#include "../../../Core/Math/Math.h"
#include "DebugDraw/PhysicsDebugDraw.h"

namespace p3d {
    
    // Circular Dependency
    class PhysicsDebugDraw;
    
    class BulletPhysics : public IPhysics
    {
        
        friend class IPhysicsComponent;
        
        public:
            
            BulletPhysics();
            virtual ~BulletPhysics();
            
            virtual void InitPhysics();
            virtual void Update(const double &time, const unsigned &steps = 10);
            virtual void EnableDebugDraw();
            virtual void RenderDebugDraw(f32* Projection, f32* Camera);
            virtual void DisableDebugDraw();
            virtual void EndPhysics();
            
            virtual void RemovePhysicsComponent(IPhysicsComponent* pcomp);
            
            virtual void UpdateTransformations(IPhysicsComponent* pcomp);

            void UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position);
            void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation);
            void CleanForces(IPhysicsComponent *pcomp);
            void SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
            void SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
            void Activate(IPhysicsComponent *pcomp);
            
            // Create Physics Components
            virtual IComponent* CreateBox(const f32 &width, const f32 &height, const f32 &depth, const f32 &mass);
            virtual IComponent* CreateCapsule(const f32 &radius, const f32 &height, const f32 &mass);
            virtual IComponent* CreateCone(const f32 &radius, const f32 &height, const f32 &mass);
            virtual IComponent* CreateConvexHull(const std::vector<Vec3> &points, const f32 &mass);
            virtual IComponent* CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 &mass);
            virtual IComponent* CreateConvexTriangleMesh(const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass);
            virtual IComponent* CreateCylinder(const f32 &radius, const f32 &height, const f32 &mass);
            virtual IComponent* CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 &mass);
            virtual IComponent* CreateSphere(const f32 &radius, const f32 &mass);
            virtual IComponent* CreateStaticPlane(const Vec3 &Normal, const f32 &Constant, const f32 &mass);
            virtual IComponent* CreateTriangleMesh(RenderingComponent* rcomp, const f32 &mass);
            virtual IComponent* CreateTriangleMesh(const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass);
            virtual IComponent* CreateVehicle(IPhysicsComponent* ChassisShape);

            // Vehicle Add Wheel
            void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 &WheelRadius, const f32 &WheelWidth, const f32 &WheelFriction, const f32 &WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);

        private:
            
            // Bullet Physics Essentials
            btBroadphaseInterface* m_broadphase;
            btCollisionDispatcher* m_dispatcher;
            btConstraintSolver* m_solver;
            btDefaultCollisionConfiguration* m_collisionConfiguration;
            btDiscreteDynamicsWorld* m_dynamicsWorld;
            PhysicsDebugDraw* m_debugDraw;
 
            // Function to Create Rigid Bodys and Add them to the Physics World
            void CreateRigidBody(btCollisionShape* shape, IPhysicsComponent* pcomp);
            
            btRigidBody* LocalCreateRigidBody(f32 mass, const btTransform& startTransform,btCollisionShape* shape);
            
            btCollisionShape* GetCollisionShape(IPhysicsComponent* pcomp);
        
        protected: 
            
            virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp);
    };
    
}

#endif /*BULLETPHYSICS_H*/