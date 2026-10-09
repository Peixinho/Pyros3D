#include <tuple>
//============================================================================
// Name        : PyrosLuaPhysics.cpp
// Description : Box3DPhysics / IPhysics / RayCastHit / PhysicsVehicle,
//               plus Box2D's Physics2D.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>
#include <Pyros3D/Rendering/Components/TileMap2D/TileMap2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>

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
			"getPosition", &IPhysicsComponent::GetPosition,
			"getRotation", &IPhysicsComponent::GetRotationQuat,
			"setLinearDamping", &IPhysicsComponent::SetLinearDamping,
			"setAngularDamping", &IPhysicsComponent::SetAngularDamping,
			"setGravityScale", &IPhysicsComponent::SetGravityScale,
			"setRotation", &IPhysicsComponent::SetRotation,
			// Quaternion in, to match getRotation() coming back out. A
			// ragdoll lays every limb capsule along the bone it drives, and
			// that direction is not expressible as three Euler angles
			// without a lossy round trip.
			"setRotationQuat", &IPhysicsComponent::SetRotationQuat,
			"cleanForces", &IPhysicsComponent::CleanForces,
			"setAngularVelocity", &IPhysicsComponent::SetAngularVelocity,
			"setLinearVelocity", &IPhysicsComponent::SetLinearVelocity,
			"getLinearVelocity", &IPhysicsComponent::GetLinearVelocity,
			"getAngularVelocity", &IPhysicsComponent::GetAngularVelocity,
			"applyCentralForce", &IPhysicsComponent::ApplyCentralForce,
			"applyCentralImpulse", &IPhysicsComponent::ApplyCentralImpulse,
			// At a point, so the body turns about the hit as well as moving.
			"applyImpulseAtPoint", &IPhysicsComponent::ApplyImpulseAtPoint,
			"applyForceAtPoint", &IPhysicsComponent::ApplyForceAtPoint,
			"setMass", &IPhysicsComponent::SetMass,
			"activate", &IPhysicsComponent::Activate,
			"isGhost", &IPhysicsComponent::IsGhost,
			"onCollisionEnter", &IPhysicsComponent::OnCollisionEnter,
			"onCollisionExit", &IPhysicsComponent::OnCollisionExit,
			sol::base_classes, sol::bases<IComponent>()
			);


		// A 2D layer. Bound because parallax is a game decision, not an engine
		// one: the engine owes a scene grouping, draw order and a place to
		// author the factor, and a script does the rest in three lines. It
		// used to be driven inside PyrosPlayer, which meant it only worked in
		// a built game and not in the editor's play mode - and that the
		// engine and a script would have been writing the same transform.
		lua->new_usertype<Layer2D>("Layer2D",
			"getParallax", &Layer2D::GetParallax,
			"setParallax", &Layer2D::SetParallax,
			"isVisible", &Layer2D::IsVisible,
			"setVisible", &Layer2D::SetVisible,
			"getBasePosition", &Layer2D::GetBasePosition,
			sol::base_classes, sol::bases<IComponent>()
			);

		// A tilemap. What a script needs from one is the grid, not the mesh:
		// reading a cell to ask what the ground under the player is made of,
		// and writing one to dig, destroy or build. The rebuild that follows a
		// write is the component's own business and happens at the end of the
		// frame, so a script may write as many cells as it likes without
		// thinking about cost.
		//
		// worldToTile returns two values rather than taking out-params: Lua
		// has multiple returns and `local tx, ty = map:worldToTile(p)` is what
		// a caller expects.
		// setBodyType has been callable for as long as Physics2D has existed,
		// and the constants to pass it were never exposed - so the only way to
		// use it was to hardcode 0/1/2 and hope. A kinematic character is the
		// normal way to write a sensor-driven controller, so this matters.
		lua->create_named_table("BodyType2D",
			"Static", (int)Body2DType::Static,
			"Kinematic", (int)Body2DType::Kinematic,
			"Dynamic", (int)Body2DType::Dynamic);

		// The 2D world itself. It existed in both hosts and was reachable from
		// neither - so a 2D game had no way to ask a question about the world,
		// only to react to contacts after the fact. A ground sensor, a
		// line-of-sight check and a hitscan shot are all this one call.
		lua->new_usertype<Physics2DWorld>("Physics2DWorld",
			// Returns nil on a miss rather than a table with hit=false, so
			// `if hit then` is the whole test.
			// The third argument is the body to SKIP - normally your own, since
			// a sensor ray starts inside the character that casts it.
			"rayCast", [](Physics2DWorld &w, const Vec2 &from, const Vec2 &to,
				sol::optional<Physics2D*> ignore, sol::optional<Physics2D*> ignore2,
				sol::this_state ts) -> sol::object {
				const Physics2DWorld::RayHit2D r = w.RayCast(from, to,
					ignore ? *ignore : NULL, ignore2 ? *ignore2 : NULL);
				sol::state_view lv(ts);
				if (!r.hit) return sol::object(sol::lua_nil);
				return sol::object(lv, sol::in_place, lv.create_table_with(
					"point", Vec2(r.point.x, r.point.y),
					"normal", Vec2(r.normal.x, r.normal.y),
					"fraction", r.fraction,
					"component", r.component));
			}
			);

		lua->new_usertype<TileMap2D>("TileMap2D",
			"getTile", &TileMap2D::GetTile,
			"setTile", &TileMap2D::SetTile,
			"fill", &TileMap2D::Fill,
			"clearTiles", &TileMap2D::ClearTiles,
			"tileToWorld", &TileMap2D::TileToWorld,
			"worldToTile", [](TileMap2D &m, const Vec2 &p) {
				int32 x = 0, y = 0;
				m.WorldToTile(p, x, y);
				return std::make_tuple(x, y);
			},
			"getTileSize", &TileMap2D::GetTileSize,
			"setTileSize", &TileMap2D::SetTileSize,
			"paintedCount", &TileMap2D::PaintedCount,
			// nil when nothing is painted, rather than four zeroes that read
			// as a real one-cell map at the origin.
			"getTileBounds", [](TileMap2D &m, sol::this_state ts) {
				int32 a = 0, b = 0, c = 0, d = 0;
				if (!m.GetTileBounds(a, b, c, d))
					return sol::object(sol::lua_nil);
				sol::state_view lv(ts);
				return sol::object(lv, sol::in_place, lv.create_table_with(
					"minX", a, "minY", b, "maxX", c, "maxY", d));
			},
			// Whether the cell at these tile coordinates collides - the
			// question a script actually has, rather than "what index is it
			// and is that index solid".
			// Honours the per-cell override, so a script and the collider
			// builder cannot disagree about what is solid.
			"isSolidAt", [](TileMap2D &m, const int32 x, const int32 y) {
				return m.IsSolidCell(x, y);
			},
			// 0 = follow the tileset, 1 = force solid, 2 = force passable.
			"setSolidAt", [](TileMap2D &m, const int32 x, const int32 y, const int32 mode) {
				m.SetSolidOverride(x, y, (uint8)(mode < 0 ? 0 : (mode > 2 ? 0 : mode)));
			},
			"getSolidOverrideAt", [](TileMap2D &m, const int32 x, const int32 y) {
				return (int32)m.GetSolidOverride(x, y);
			},
			// Tags are the documented way for a game to give a tile meaning
			// the engine has no opinion about - "hazard", "ice", "ladder".
			// The tileset editor has written them since it existed and the
			// .p3dt has always carried them, but NOTHING exposed them to Lua,
			// so the only way to act on a tile's kind was to hardcode its
			// atlas INDEX - which silently means a different tile the moment
			// anyone re-cuts the sheet. These three close that.
			"hasTagAt", [](TileMap2D &m, const int32 x, const int32 y, const std::string &tag) {
				const int32 t = m.GetTile(x, y);
				return t >= 0 && m.GetTileSet().HasTag(t, tag);
			},
			// The tags at a cell, as a 1-based Lua array; empty for an empty
			// cell or an untagged one.
			"getTagsAt", [](TileMap2D &m, const int32 x, const int32 y, sol::this_state ts) {
				sol::state_view lv(ts);
				sol::table out = lv.create_table();
				const int32 t = m.GetTile(x, y);
				if (t >= 0)
				{
					const std::vector<std::string> &tags = m.GetTileSet().Tags(t);
					for (size_t i = 0; i < tags.size(); i++) out[i + 1] = tags[i];
				}
				return out;
			},
			// By tile INDEX rather than by cell, for code that already has one
			// (e.g. straight out of getTile).
			"tileHasTag", [](TileMap2D &m, const int32 index, const std::string &tag) {
				return m.GetTileSet().HasTag(index, tag);
			},
			"tileIsSolid", [](TileMap2D &m, const int32 index) {
				return m.GetTileSet().IsSolid(index);
			},
			"getTileSetPath", &TileMap2D::GetTileSetPath,
			sol::base_classes, sol::bases<IComponent>()
			);

		// Box2D. A separate type from IPhysicsComponent on purpose - see
		// Physics2D.h - so its API is in the plane: Vec2 everywhere, and one
		// scalar for rotation about z rather than a Vec3 of Euler angles two
		// thirds of which mean nothing here.
		lua->new_usertype<Physics2D>("Physics2D",
			"setLinearVelocity", &Physics2D::SetLinearVelocity,
			"getLinearVelocity", &Physics2D::GetLinearVelocity,
			"applyForce", &Physics2D::ApplyForce,
			"applyImpulse", &Physics2D::ApplyImpulse,
			"setAngularVelocity", &Physics2D::SetAngularVelocity,
			"getAngularVelocity", &Physics2D::GetAngularVelocity,
			"setTransform", &Physics2D::SetTransform,
			"wake", &Physics2D::Wake,
			"getBodyType", &Physics2D::GetBodyType,
			"setBodyType", &Physics2D::SetBodyType,
			"getDensity", &Physics2D::GetDensity,
			"getFriction", &Physics2D::GetFriction,
			"getRestitution", &Physics2D::GetRestitution,
			// Half-extents, the same convention the body was built with.
			// Everything else about the shape was readable and this was not,
			// so a script could not do its own overlap test against a
			// platform it did not author itself.
			"getSize", &Physics2D::GetSize,
			"getShapeType", &Physics2D::GetShapeType,
			"isFixedRotation", &Physics2D::IsFixedRotation,
			"haveBody", &Physics2D::HaveBody,
			// Plain fields, like IPhysicsComponent's above - assign a Lua
			// closure and sol2 converts it. The argument is the other body,
			// which may be nil if it was destroyed before the end event.
			"onCollisionEnter", &Physics2D::OnCollisionEnter,
			"onCollisionExit", &Physics2D::OnCollisionExit,
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
			// Engine and brakes act through the wheels: see PhysicsVehicle.h
			// for what the forces mean and which way is forward.
			"setDriveWheels", &PhysicsVehicle::SetDriveWheels,
			"setHandBrakeForce", &PhysicsVehicle::SetHandBrakeForce,
			"getHandBrakeForce", &PhysicsVehicle::GetHandBrakeForce,
			"setSuspensionLimits", &PhysicsVehicle::SetSuspensionLimits,
			"setCenterOfMass", &PhysicsVehicle::SetCenterOfMass,
			"addWheel", &PhysicsVehicle::AddWheel,
			"getWheelCount", [](PhysicsVehicle &v) { return (uint32)v.GetWheels().size(); },
			// One wheel's grip and size, changed while it drives: a tyre that
			// goes flat, ice under one side, a wheel that is lost.
			"setWheelFriction", [](PhysicsVehicle &v, uint32 i, f32 friction) {
				std::vector<VehicleWheel> &w = v.GetWheels();
				if (i < w.size()) w[i].Friction = friction;
			},
			"setWheelRadius", [](PhysicsVehicle &v, uint32 i, f32 radius) {
				std::vector<VehicleWheel> &w = v.GetWheels();
				if (i < w.size() && radius > 0.f) w[i].Radius = radius;
			},
			// false: the wheel is gone - see VehicleWheel::Disabled
			"setWheelEnabled", [](PhysicsVehicle &v, uint32 i, bool on) {
				std::vector<VehicleWheel> &w = v.GetWheels();
				if (i < w.size()) w[i].Disabled = !on;
			},
			"isWheelEnabled", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() && !v.GetWheels()[i].Disabled;
			},
			"getWheelTransform", [](PhysicsVehicle &v, uint32 i) -> Matrix {
				if (i >= v.GetWheels().size()) return Matrix();
				return v.GetWheels()[i].Transformation;
			},
			"isWheelInContact", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].InContact : false;
			},
			// Where a wheel is relative to the chassis, for placing its model
			// as a child of the vehicle: metres up its suspension from where
			// it was added, and radians about its steering axis and its axle.
			"getWheelTravel", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].Travel : 0.f;
			},
			"getWheelSteer", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].Steer : 0.f;
			},
			"getWheelSpin", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].Spin : 0.f;
			},
			// Radians a second on the axle; positive rolls forward.
			"getWheelSpinSpeed", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].SpinSpeed : 0.f;
			},
			"isFrontWheel", [](PhysicsVehicle &v, uint32 i) {
				return i < v.GetWheels().size() ? v.GetWheels()[i].IsFrontWheel : false;
			},
			sol::base_classes, sol::bases<IPhysicsComponent, IComponent>()
			);

		lua->create_named_table("VehicleDrive",
			"Rear", (int)VehicleDrive::Rear,
			"Front", (int)VehicleDrive::Front,
			"All", (int)VehicleDrive::All);

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
				// The same ray, asked and answered in numbers:
				//   local hit, distance, px, py, pz, nx, ny, nz = physics:rayCastNumbers(x0, y0, z0, x1, y1, z1)
				// For what is asked many times a frame - a body feeling for the
				// ground and the walls round it - where two vectors made for the
				// asking and three objects read for the answer are most of the cost.
				"rayCastNumbers", [](IPhysics &world, const f32 x0, const f32 y0, const f32 z0, const f32 x1, const f32 y1, const f32 z1) {
					const RayCastHit h = world.RayCast(Vec3(x0, y0, z0), Vec3(x1, y1, z1));
					return std::make_tuple(h.hasHit, h.distance, h.point.x, h.point.y, h.point.z, h.normal.x, h.normal.y, h.normal.z);
				},
				"createBox", &IPhysics::CreateBox,
				"createCapsule", &IPhysics::CreateCapsule,
				"createCone", &IPhysics::CreateCone,
				// From a Lua array of Vec3. Bound to the C++ signature alone it
				// wanted a std::vector userdata, which no script can make.
				"createConvexHull", [](IPhysics &p, const sol::table &points, const f32 mass, const bool ghost) {
					std::vector<Vec3> list;
					list.reserve(points.size());
					for (size_t i = 1; i <= points.size(); i++)
						list.push_back(points.get<Vec3>(i));
					return p.CreateConvexHull(list, mass, ghost);
				},
				"createCylinder", &IPhysics::CreateCylinder,
				"createMultiplerSphere", &IPhysics::CreateMultipleSphere,
				"createSphere", &IPhysics::CreateSphere,
				"createStaticPlane", &IPhysics::CreateStaticPlane,
				// Joints. sol binds a function's full arity, so the C++
				// default arguments are spelled out as overloads here - the
				// same reason every other defaulted call in these bindings is.
				"createSphericalJoint", sol::overload(
					[](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &a,
					   const std::shared_ptr<IPhysicsComponent> &b, const Vec3 &anchor) {
						return p.CreateSphericalJoint(a.get(), b.get(), anchor);
					},
					[](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &a,
					   const std::shared_ptr<IPhysicsComponent> &b, const Vec3 &anchor, const f32 cone) {
						return p.CreateSphericalJoint(a.get(), b.get(), anchor, cone);
					},
					// With an axis the cone means something anatomical - see
					// IPhysics::CreateSphericalJoint.
					[](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &a,
					   const std::shared_ptr<IPhysicsComponent> &b, const Vec3 &anchor, const f32 cone,
					   const Vec3 &axis) {
						return p.CreateSphericalJoint(a.get(), b.get(), anchor, cone, axis);
					},
					[](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &a,
					   const std::shared_ptr<IPhysicsComponent> &b, const Vec3 &anchor, const f32 cone,
					   const Vec3 &axis, const f32 twistLo, const f32 twistHi) {
						return p.CreateSphericalJoint(a.get(), b.get(), anchor, cone, axis, twistLo, twistHi);
					}
				),
				"createRevoluteJoint", sol::overload(
					[](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &a,
					   const std::shared_ptr<IPhysicsComponent> &b, const Vec3 &anchor, const Vec3 &axis) {
						return p.CreateRevoluteJoint(a.get(), b.get(), anchor, axis);
					},
					[](IPhysics &p, const std::shared_ptr<IPhysicsComponent> &a,
					   const std::shared_ptr<IPhysicsComponent> &b, const Vec3 &anchor, const Vec3 &axis,
					   const f32 lower, const f32 upper) {
						return p.CreateRevoluteJoint(a.get(), b.get(), anchor, axis, lower, upper);
					}
				),
				"setJointSpring", &IPhysics::SetJointSpring,
				"destroyJoint", &IPhysics::DestroyJoint,
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
