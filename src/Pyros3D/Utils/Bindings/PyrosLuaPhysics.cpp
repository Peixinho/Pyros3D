//============================================================================
// Name        : PyrosLuaPhysics.cpp
// Description : Box3DPhysics / IPhysics / RayCastHit / PhysicsVehicle.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaPhysicsEarly(sol::state* lua)
	{
		// Real read/write physics API - was previously registered with
		// zero bound methods (only IPhysics::CreateBox/CreateSphere/...,
		// which return this type, were bound), meaning physics bodies
		// were write-only from Lua: creatable but unqueryable and
		// undrivable by force/impulse. OnCollisionEnter/OnCollisionExit
		// are plain fields here (like onUpdate/onInit elsewhere) - sol2
		// auto-converts an assigned Lua closure to std::function, same
		// proven pattern, no extra binding plumbing needed.
		lua->new_usertype<IPhysicsComponent>("IPhysicsComponent",
			"getMass", &IPhysicsComponent::GetMass,
			"getShape", &IPhysicsComponent::GetShape,
			"setPosition", &IPhysicsComponent::SetPosition,
			"setRotation", &IPhysicsComponent::SetRotation,
			"cleanForces", &IPhysicsComponent::CleanForces,
			"setAngularVelocity", &IPhysicsComponent::SetAngularVelocity,
			"setLinearVelocity", &IPhysicsComponent::SetLinearVelocity,
			"getLinearVelocity", &IPhysicsComponent::GetLinearVelocity,
			"getAngularVelocity", &IPhysicsComponent::GetAngularVelocity,
			"applyCentralForce", &IPhysicsComponent::ApplyCentralForce,
			"applyCentralImpulse", &IPhysicsComponent::ApplyCentralImpulse,
			"setMass", &IPhysicsComponent::SetMass,
			"activate", &IPhysicsComponent::Activate,
			"isGhost", &IPhysicsComponent::IsGhost,
			"onCollisionEnter", &IPhysicsComponent::OnCollisionEnter,
			"onCollisionExit", &IPhysicsComponent::OnCollisionExit,
			sol::base_classes, sol::bases<IComponent>()
			);


		// Drive API for Box3D vehicles (createVehicle returns this type).
		lua->new_usertype<PhysicsVehicle>("PhysicsVehicle",
			"setEngineForce", &PhysicsVehicle::SetEngineForce,
			"getEngineForce", &PhysicsVehicle::GetEngineForce,
			"setBreakingForce", &PhysicsVehicle::SetBreakingForce,
			"getBreakingForce", &PhysicsVehicle::GetBreakingForce,
			"setMaxEngineForce", &PhysicsVehicle::SetMaxEngineForce,
			"getMaxEngineForce", &PhysicsVehicle::GetMaxEngineForce,
			"setMaxBreakingForce", &PhysicsVehicle::SetMaxBreakingForce,
			"getMaxBreakingForce", &PhysicsVehicle::GetMaxBreakingForce,
			"setVehicleSteering", &PhysicsVehicle::SetVehicleSteering,
			"getVehicleSteering", &PhysicsVehicle::GetVehicleSteering,
			"setSteeringIncrement", &PhysicsVehicle::SetSteeringIncrement,
			"getSteeringIncrement", &PhysicsVehicle::GetSteeringIncrement,
			"setSteeringClamp", &PhysicsVehicle::SetSteeringClamp,
			"getSteeringClamp", &PhysicsVehicle::GetSteeringClamp,
			"setSuspensionStiffness", &PhysicsVehicle::SetSuspensionStiffness,
			"setSuspensionDamping", &PhysicsVehicle::SetSuspensionDamping,
			"setSuspensionCompression", &PhysicsVehicle::SetSuspensionCompression,
			"setSuspensionRestLength", &PhysicsVehicle::SetSuspensionRestLength,
			"addWheel", &PhysicsVehicle::AddWheel,
			"getWheelCount", [](PhysicsVehicle &v) { return (uint32)v.GetWheels().size(); },
			"getWheelTransform", [](PhysicsVehicle &v, uint32 i) -> Matrix {
				if (i >= v.GetWheels().size()) return Matrix();
				return v.GetWheels()[i].Transformation;
			},
			"isFrontWheel", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].IsFrontWheel : false;
			},
			sol::base_classes, sol::bases<IPhysicsComponent, IComponent>()
			);

		(*lua)["asPhysicsVehicle"] = [](const std::shared_ptr<IPhysicsComponent> &c) -> std::shared_ptr<PhysicsVehicle> {
			return std::dynamic_pointer_cast<PhysicsVehicle>(c);
		};


		{
			// RayCastHit / RayCast - real raycasting from Lua, e.g. for
			// click-picking or ground checks. hasHit gates whether the
			// rest of the fields are meaningful (mirrors the real C++
			// struct exactly - no Lua-side reinterpretation).
			sol::constructors<sol::types<>> con;
			lua->new_usertype<RayCastHit>("RayCastHit",
				con,
				"hasHit", &RayCastHit::hasHit,
				"point", &RayCastHit::point,
				"normal", &RayCastHit::normal,
				"distance", &RayCastHit::distance,
				"component", &RayCastHit::component
				);
		}

	}

	void RegisterLuaPhysicsLate(sol::state* lua)
	{
		{
			// IPhysics
			lua->new_usertype<IPhysics>("IPhysics",
				"initPhysics", &IPhysics::InitPhysics,
				"enableDebugDraw", &IPhysics::EnableDebugDraw,
				"renderDebugDraw", &IPhysics::RenderDebugDraw,
				"disableDebugDraw", &IPhysics::DisableDebugDraw,
				"update", &IPhysics::Update,
				"endPhysics", &IPhysics::EndPhysics,
				"RemovePhysicsComponent", &IPhysics::RemovePhysicsComponent,
				"UpdateTransformations", &IPhysics::UpdateTransformations,
				"UpdatePosition", &IPhysics::UpdatePosition,
				"UpdateRotation", &IPhysics::UpdateRotation,
				"CleanForces", &IPhysics::CleanForces,
				"SetAngularVelocity", &IPhysics::SetAngularVelocity,
				"SetLinearVelocity", &IPhysics::SetLinearVelocity,
				"Activate", &IPhysics::Activate,
				"rayCast", &IPhysics::RayCast,
				"createBox", &IPhysics::CreateBox,
				"createCapsule", &IPhysics::CreateCapsule,
				"createCone", &IPhysics::CreateCone,
				"createConvexHull", &IPhysics::CreateConvexHull,
				"createCylinder", &IPhysics::CreateCylinder,
				"createMultiplerSphere", &IPhysics::CreateMultipleSphere,
				"createSphere", &IPhysics::CreateSphere,
				"createStaticPlane", &IPhysics::CreateStaticPlane,
				"createVehicle", [](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &chassis) -> std::shared_ptr<IPhysicsComponent> {
					return p.CreateVehicle(chassis);
				},
				"addWheel", &IPhysics::AddWheel,
				"createTriangleMesh", sol::overload(
					&IPhysics_CreateTriangleMesh,
					&IPhysics_CreateTriangleMeshRCOMP
				),
				"createConvexTriangleMesh", sol::overload(
					&IPhysics_CreateConvexTriangleMesh,
					&IPhysics_CreateConvexTriangleMeshRCOMP
				)
				);
		}

		{
			// Box3D Physics (BulletPhysics kept as Lua alias for old scripts)
			sol::constructors<sol::types<>> con;
			lua->new_usertype<Box3DPhysics>("Box3DPhysics",
				con,
				sol::base_classes, sol::bases<IPhysics>()
				);
			(*lua)["BulletPhysics"] = (*lua)["Box3DPhysics"];
		}

	}

	void RegisterLuaPhysics(sol::state* lua)
	{
		RegisterLuaPhysicsEarly(lua);
		RegisterLuaPhysicsLate(lua);
	}

} // namespace p3d

#endif
