//============================================================================
// Name        : Box3DPhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box3D Physics Wrapper
//============================================================================

#ifndef BOX3DPHYSICS_H
#define BOX3DPHYSICS_H

#include <box3d/box3d.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/DebugDraw/PhysicsDebugDraw.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <set>
#include <utility>
#include <vector>
#include <cstdint>

namespace p3d {

	class PYROS3D_API PhysicsDebugDraw;

	// Backend handles stored via IPhysicsComponent::SaveRigidBodyPTR
	struct Box3DBodyHandles {
		b3BodyId body;
		std::vector<b3BodyId> wheelBodies;
		std::vector<b3JointId> wheelJoints;
		b3HullData* ownedHull;
		b3MeshData* ownedMesh;
		std::vector<b3Vec3> meshVerts;
		std::vector<int32_t> meshIndices;

		Box3DBodyHandles() : body(b3_nullBodyId), ownedHull(NULL), ownedMesh(NULL) {}
	};

	class PYROS3D_API Box3DPhysics : public IPhysics
	{

		friend class IPhysicsComponent;

	public:

		Box3DPhysics();
		virtual ~Box3DPhysics();

		virtual void InitPhysics();
		virtual void Update(const f64 &time, const uint32 steps = 10);
		virtual void EnableDebugDraw();
		virtual void RenderDebugDraw(Projection projection, GameObject* Camera);
		virtual void DisableDebugDraw();
		virtual void EndPhysics();

		virtual void RemovePhysicsComponent(IPhysicsComponent* pcomp);

		virtual RayCastHit RayCast(const Vec3 &from, const Vec3 &to);

		virtual void UpdateTransformations(IPhysicsComponent* pcomp);

		b3WorldId GetWorld() const { return m_world; }

		virtual void UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position);
		virtual void UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation);
		virtual void CleanForces(IPhysicsComponent *pcomp);
		virtual void SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
		virtual void SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity);
		virtual void Activate(IPhysicsComponent *pcomp);
		virtual Vec3 GetLinearVelocity(IPhysicsComponent *pcomp);
		virtual Vec3 GetAngularVelocity(IPhysicsComponent *pcomp);
		virtual void ApplyCentralForce(IPhysicsComponent *pcomp, const Vec3 &force);
		virtual void ApplyCentralImpulse(IPhysicsComponent *pcomp, const Vec3 &impulse);
		virtual void SetMass(IPhysicsComponent *pcomp, const f32 mass);

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

		virtual void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);

	private:

		b3WorldId m_world;
		std::unique_ptr<PhysicsDebugDraw> m_debugDraw;
		b3DebugDraw m_draw;

		void CreateBody(IPhysicsComponent* pcomp);
		void AttachShapes(IPhysicsComponent* pcomp, Box3DBodyHandles* handles, b3BodyId body, const b3ShapeDef &shapeDef);
		void AttachChassisShapes(IPhysicsComponent* chassis, Box3DBodyHandles* handles, b3BodyId body, const b3ShapeDef &shapeDef);
		b3ShapeDef MakeShapeDef(IPhysicsComponent* pcomp) const;
		b3BodyDef MakeBodyDef(IPhysicsComponent* pcomp) const;
		void ApplyVehicleMotors(IPhysicsComponent* pcomp);

		void ProcessCollisionEvents();
		std::set<std::pair<IPhysicsComponent*, IPhysicsComponent*> > m_touchingPairs;
		std::vector<IPhysicsComponent*> m_vehicles;

		static Box3DBodyHandles* GetHandles(IPhysicsComponent* pcomp);
		static IPhysicsComponent* ComponentFromShape(b3ShapeId shapeId);
		static void DestroyHandles(Box3DBodyHandles* handles);

	protected:

		virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp);
	};

}

#endif /* BOX3DPHYSICS_H */
