//============================================================================
// Name        : Bullet Physics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bullet Physics Wrapper
//============================================================================

#ifndef BULLETPHYSICS_H
#define BULLETPHYSICS_H

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btIDebugDraw.h>
#include <BulletCollision/CollisionShapes/btMultimaterialTriangleMeshShape.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Physics/PhysicsEngines/BulletPhysics/DebugDraw/PhysicsDebugDraw.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	// Circular Dependency
	class PYROS3D_API PhysicsDebugDraw;

	class PYROS3D_API BulletPhysics : public IPhysics
	{

		friend class IPhysicsComponent;

	public:

		BulletPhysics();
		virtual ~BulletPhysics();

		virtual void InitPhysics();
		virtual void Update(const f64 &time, const uint32 steps = 10);
		virtual void EnableDebugDraw();
		virtual void RenderDebugDraw(Projection projection, GameObject* Camera);
		virtual void DisableDebugDraw();
		virtual void EndPhysics();

		virtual void RemovePhysicsComponent(IPhysicsComponent* pcomp);

		virtual void UpdateTransformations(IPhysicsComponent* pcomp);

		btDiscreteDynamicsWorld* GetPhysicsWorld()
		{
			return m_dynamicsWorld;
		}

		void UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position);
		void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation);
		void CleanForces(IPhysicsComponent *pcomp);
		void SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
		void SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
		void Activate(IPhysicsComponent *pcomp);

		// Create Physics Components
		virtual IPhysicsComponent* CreateBox(const f32 width, const f32 height, const f32 depth, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateCapsule(const f32 radius, const f32 height, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateCone(const f32 radius, const f32 height, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateConvexHull(const std::vector<Vec3> &points, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateConvexTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateCylinder(const f32 radius, const f32 height, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateSphere(const f32 radius, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateStaticPlane(const Vec3 &Normal, const f32 Constant, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateTriangleMesh(RenderingComponent* rcomp, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost = false);
		virtual IPhysicsComponent* CreateVehicle(IPhysicsComponent* ChassisShape, bool ghost = false);

		// Vehicle Add Wheel
		void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);

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

		btRigidBody* LocalCreateRigidBody(f32 mass, const btTransform& startTransform, btCollisionShape* shape);
		void CreateGhostObject(btCollisionShape* shape, IPhysicsComponent* pcomp);

		btCollisionShape* GetCollisionShape(IPhysicsComponent* pcomp);

	protected:

		virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp);
	};

}

#endif /*BULLETPHYSICS_H*/
