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
#include <memory>
#include <set>
#include <utility>

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

		virtual RayCastHit RayCast(const Vec3 &from, const Vec3 &to);

		virtual void UpdateTransformations(IPhysicsComponent* pcomp);

		btDiscreteDynamicsWorld* GetPhysicsWorld()
		{
			return m_dynamicsWorld.get();
		}

		void UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position);
		void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation);
		void CleanForces(IPhysicsComponent *pcomp);
		void SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
		void SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
		void Activate(IPhysicsComponent *pcomp);
		Vec3 GetLinearVelocity(IPhysicsComponent *pcomp);
		Vec3 GetAngularVelocity(IPhysicsComponent *pcomp);
		void ApplyCentralForce(IPhysicsComponent *pcomp, const Vec3 &force);
		void ApplyCentralImpulse(IPhysicsComponent *pcomp, const Vec3 &impulse);
		void SetMass(IPhysicsComponent *pcomp, const f32 mass);

		// Create Physics Components
		virtual std::shared_ptr<IPhysicsComponent> CreateBox(const f32 width, const f32 height, const f32 depth, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateCapsule(const f32 radius, const f32 height, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateCone(const f32 radius, const f32 height, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateConvexHull(const std::vector<Vec3> &points, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateConvexTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateCylinder(const f32 radius, const f32 height, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateSphere(const f32 radius, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateStaticPlane(const Vec3 &Normal, const f32 Constant, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateTriangleMesh(RenderingComponent* rcomp, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost = false);
		virtual std::shared_ptr<IPhysicsComponent> CreateVehicle(const std::shared_ptr<IPhysicsComponent> &ChassisShape, bool ghost = false);

		// Vehicle Add Wheel
		void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);

	private:

		// Bullet Physics Essentials. Declaration order matters: member
		// destruction runs in reverse declaration order, which must destroy
		// m_dynamicsWorld before the objects it depends on (m_solver,
		// m_broadphase, m_dispatcher, m_collisionConfiguration).
		std::unique_ptr<btBroadphaseInterface> m_broadphase;
		std::unique_ptr<btCollisionDispatcher> m_dispatcher;
		std::unique_ptr<btConstraintSolver> m_solver;
		std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfiguration;
		std::unique_ptr<btDiscreteDynamicsWorld> m_dynamicsWorld;
		std::unique_ptr<PhysicsDebugDraw> m_debugDraw;

		// Function to Create Rigid Bodys and Add them to the Physics World
		void CreateRigidBody(btCollisionShape* shape, IPhysicsComponent* pcomp);

		btRigidBody* LocalCreateRigidBody(f32 mass, const btTransform& startTransform, btCollisionShape* shape);
		void CreateGhostObject(btCollisionShape* shape, IPhysicsComponent* pcomp);

		btCollisionShape* GetCollisionShape(IPhysicsComponent* pcomp);

		// Real collision-notification scan - see IPhysicsComponent.h's
		// OnCollisionEnter/OnCollisionExit comment. Run once per Update()
		// after stepSimulation(), diffed against the previous step's set
		// so enter/exit fire exactly once per real state change, not once
		// per frame two bodies happen to still be touching.
		void ProcessCollisionEvents();
		std::set<std::pair<IPhysicsComponent*, IPhysicsComponent*> > m_touchingPairs;

	protected:

		virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp);
	};

}

#endif /*BULLETPHYSICS_H*/
