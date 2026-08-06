//============================================================================
// Name        : PyrosEmbindPhysics.cpp
// Description : Embind Box3DPhysics / IPhysics / RayCastHit / PhysicsVehicle.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Projection/Projection.h>

#include <memory>
#include <vector>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;

namespace {

	std::shared_ptr<IPhysicsComponent> IPhysics_CreateBox(IPhysics &p, float w, float h, float d, float mass)
	{
		return p.CreateBox(w, h, d, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateBoxGhost(IPhysics &p, float w, float h, float d, float mass, bool ghost)
	{
		return p.CreateBox(w, h, d, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateSphere(IPhysics &p, float radius, float mass)
	{
		return p.CreateSphere(radius, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateSphereGhost(IPhysics &p, float radius, float mass, bool ghost)
	{
		return p.CreateSphere(radius, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateCapsule(IPhysics &p, float radius, float height, float mass)
	{
		return p.CreateCapsule(radius, height, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateCone(IPhysics &p, float radius, float height, float mass)
	{
		return p.CreateCone(radius, height, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateCylinder(IPhysics &p, float radius, float height, float mass)
	{
		return p.CreateCylinder(radius, height, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateStaticPlane(IPhysics &p, const Vec3 &n, float c, float mass)
	{
		return p.CreateStaticPlane(n, c, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMeshRcomp(IPhysics &p, RenderingComponent *rc, float mass)
	{
		return p.CreateTriangleMesh(rc, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMeshRcomp(IPhysics &p, RenderingComponent *rc, float mass)
	{
		return p.CreateConvexTriangleMesh(rc, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateVehicle(IPhysics &p, std::shared_ptr<IPhysicsComponent> chassis)
	{
		return p.CreateVehicle(chassis);
	}

	void IPhysics_RenderDebugDraw(IPhysics &p, const Projection &proj, GameObject *cam)
	{
		p.RenderDebugDraw(proj, cam);
	}
	void IPhysics_Update(IPhysics &p, f64 time, uint32 steps)
	{
		p.Update(time, steps);
	}

	uint32 PhysicsVehicle_GetWheelCount(PhysicsVehicle &v)
	{
		return (uint32)v.GetWheels().size();
	}
	Matrix PhysicsVehicle_GetWheelTransform(PhysicsVehicle &v, uint32 i)
	{
		if (i >= v.GetWheels().size()) return Matrix();
		return v.GetWheels()[i].Transformation;
	}
	bool PhysicsVehicle_IsFrontWheel(PhysicsVehicle &v, uint32 i)
	{
		return i < v.GetWheels().size() ? v.GetWheels()[i].IsFrontWheel : false;
	}
	std::shared_ptr<PhysicsVehicle> AsPhysicsVehicle(std::shared_ptr<IPhysicsComponent> c)
	{
		return std::dynamic_pointer_cast<PhysicsVehicle>(c);
	}

} // namespace

namespace p3d {
	void PyrosEmbindPhysicsForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_physics)
{
	class_<RayCastHit>("RayCastHit")
		.constructor<>()
		.property("hasHit", &RayCastHit::hasHit)
		.property("point", &RayCastHit::point)
		.property("normal", &RayCastHit::normal)
		.property("distance", &RayCastHit::distance)
		.function("getComponent", optional_override([](RayCastHit &h) { return h.component; }), allow_raw_pointers());

	class_<IPhysicsComponent, base<IComponent>>("IPhysicsComponent")
		.smart_ptr<std::shared_ptr<IPhysicsComponent>>("IPhysicsComponentPtr")
		.function("getMass", &IPhysicsComponent::GetMass)
		.function("getShape", &IPhysicsComponent::GetShape)
		.function("setPosition", &IPhysicsComponent::SetPosition)
		.function("setRotation", &IPhysicsComponent::SetRotation)
		.function("cleanForces", &IPhysicsComponent::CleanForces)
		.function("setAngularVelocity", &IPhysicsComponent::SetAngularVelocity)
		.function("setLinearVelocity", &IPhysicsComponent::SetLinearVelocity)
		.function("getLinearVelocity", &IPhysicsComponent::GetLinearVelocity)
		.function("getAngularVelocity", &IPhysicsComponent::GetAngularVelocity)
		.function("applyCentralForce", &IPhysicsComponent::ApplyCentralForce)
		.function("applyCentralImpulse", &IPhysicsComponent::ApplyCentralImpulse)
		.function("setMass", &IPhysicsComponent::SetMass)
		.function("activate", &IPhysicsComponent::Activate)
		.function("isGhost", &IPhysicsComponent::IsGhost);
		// onCollisionEnter/Exit — std::function Lua callbacks; bind JS-friendly later

	class_<PhysicsVehicle, base<IPhysicsComponent>>("PhysicsVehicle")
		.smart_ptr<std::shared_ptr<PhysicsVehicle>>("PhysicsVehiclePtr")
		.function("setEngineForce", &PhysicsVehicle::SetEngineForce)
		.function("getEngineForce", &PhysicsVehicle::GetEngineForce)
		.function("setBreakingForce", &PhysicsVehicle::SetBreakingForce)
		.function("getBreakingForce", &PhysicsVehicle::GetBreakingForce)
		.function("setMaxEngineForce", &PhysicsVehicle::SetMaxEngineForce)
		.function("getMaxEngineForce", &PhysicsVehicle::GetMaxEngineForce)
		.function("setMaxBreakingForce", &PhysicsVehicle::SetMaxBreakingForce)
		.function("getMaxBreakingForce", &PhysicsVehicle::GetMaxBreakingForce)
		.function("setVehicleSteering", &PhysicsVehicle::SetVehicleSteering)
		.function("getVehicleSteering", &PhysicsVehicle::GetVehicleSteering)
		.function("setSteeringIncrement", &PhysicsVehicle::SetSteeringIncrement)
		.function("getSteeringIncrement", &PhysicsVehicle::GetSteeringIncrement)
		.function("setSteeringClamp", &PhysicsVehicle::SetSteeringClamp)
		.function("getSteeringClamp", &PhysicsVehicle::GetSteeringClamp)
		.function("setSuspensionStiffness", &PhysicsVehicle::SetSuspensionStiffness)
		.function("setSuspensionDamping", &PhysicsVehicle::SetSuspensionDamping)
		.function("setSuspensionCompression", &PhysicsVehicle::SetSuspensionCompression)
		.function("setSuspensionRestLength", &PhysicsVehicle::SetSuspensionRestLength)
		.function("addWheel", &PhysicsVehicle::AddWheel)
		.function("getWheelCount", &PhysicsVehicle_GetWheelCount)
		.function("getWheelTransform", &PhysicsVehicle_GetWheelTransform)
		.function("isFrontWheel", &PhysicsVehicle_IsFrontWheel);

	emscripten::function("asPhysicsVehicle", &AsPhysicsVehicle);

	class_<IPhysics>("IPhysics")
		.function("initPhysics", &IPhysics::InitPhysics)
		.function("enableDebugDraw", &IPhysics::EnableDebugDraw)
		.function("renderDebugDraw", &IPhysics_RenderDebugDraw, allow_raw_pointers())
		.function("disableDebugDraw", &IPhysics::DisableDebugDraw)
		.function("update", &IPhysics_Update)
		.function("endPhysics", &IPhysics::EndPhysics)
		.function("RemovePhysicsComponent", &IPhysics::RemovePhysicsComponent, allow_raw_pointers())
		.function("UpdateTransformations", &IPhysics::UpdateTransformations, allow_raw_pointers())
		.function("UpdatePosition", &IPhysics::UpdatePosition, allow_raw_pointers())
		.function("UpdateRotation", &IPhysics::UpdateRotation, allow_raw_pointers())
		.function("CleanForces", &IPhysics::CleanForces, allow_raw_pointers())
		.function("SetAngularVelocity", &IPhysics::SetAngularVelocity, allow_raw_pointers())
		.function("SetLinearVelocity", &IPhysics::SetLinearVelocity, allow_raw_pointers())
		.function("Activate", &IPhysics::Activate, allow_raw_pointers())
		.function("rayCast", &IPhysics::RayCast)
		.function("createBox", &IPhysics_CreateBox)
		.function("createBoxGhost", &IPhysics_CreateBoxGhost)
		.function("createSphere", &IPhysics_CreateSphere)
		.function("createSphereGhost", &IPhysics_CreateSphereGhost)
		.function("createCapsule", &IPhysics_CreateCapsule)
		.function("createCone", &IPhysics_CreateCone)
		.function("createCylinder", &IPhysics_CreateCylinder)
		.function("createStaticPlane", &IPhysics_CreateStaticPlane)
		.function("createTriangleMesh", &IPhysics_CreateTriangleMeshRcomp, allow_raw_pointers())
		.function("createConvexTriangleMesh", &IPhysics_CreateConvexTriangleMeshRcomp, allow_raw_pointers())
		.function("createVehicle", &IPhysics_CreateVehicle)
		.function("addWheel", &IPhysics::AddWheel, allow_raw_pointers());

	class_<Box3DPhysics, base<IPhysics>>("Box3DPhysics")
		.constructor<>();
}

#endif /* EMSCRIPTEN */
