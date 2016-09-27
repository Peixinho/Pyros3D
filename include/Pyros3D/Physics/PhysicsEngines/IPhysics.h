//============================================================================
// Name        : IPhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Interface
//============================================================================

#ifndef IPHYSICS_H
#define IPHYSICS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	// Circular Dependency
	class PYROS3D_API IPhysicsComponent;

	class PYROS3D_API IPhysics {

		friend class IPhysicsComponent;

	public:

		IPhysics();
		virtual ~IPhysics();

		virtual void InitPhysics() = 0;
		virtual void Update(const f64 &time, const uint32 steps) = 0;
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
		virtual IPhysicsComponent* CreateBox(const f32 width, const f32 height, const f32 depth, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateCapsule(const f32 radius, const f32 height, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateCone(const f32 radius, const f32 height, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateConvexHull(const std::vector<Vec3> &points, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateConvexTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateCylinder(const f32 radius, const f32 height, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateSphere(const f32 radius, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateStaticPlane(const Vec3 &Normal, const f32 Constant, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateTriangleMesh(RenderingComponent* rcomp, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f) = 0;
		virtual IPhysicsComponent* CreateVehicle(IPhysicsComponent* ChassisShape) = 0;
		virtual void AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel) = 0;

	protected:

		virtual void CreatePhysicsComponent(IPhysicsComponent* pcomp) = 0;

		// Save Physics Components List
		std::vector<IPhysicsComponent*> _PhysicsList;

		// Save Physics Initialization Flag
		bool physicsInitialized;

		// Timer
		f64 lastTime, nowTime, timeInterval;
	};

};

#endif /*PHYSICS_H*/