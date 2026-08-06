//============================================================================
// Name        : Box3DPhysics.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box3D Physics Wrapper
//============================================================================

#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Physics/Components/Box/PhysicsBox.h>
#include <Pyros3D/Physics/Components/Sphere/PhysicsSphere.h>
#include <Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.h>
#include <Pyros3D/Physics/Components/Capsule/PhysicsCapsule.h>
#include <Pyros3D/Physics/Components/Cone/PhysicsCone.h>
#include <Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.h>
#include <Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.h>
#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>
#include <Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.h>
#include <Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.h>
#include <Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {

		b3Vec3 ToB3(const Vec3 &v)
		{
			b3Vec3 r = { v.x, v.y, v.z };
			return r;
		}

		b3Pos ToB3Pos(const Vec3 &v)
		{
			b3Pos r = { v.x, v.y, v.z };
			return r;
		}

		Vec3 FromB3(b3Vec3 v)
		{
			return Vec3(v.x, v.y, v.z);
		}

		Vec3 FromB3Pos(b3Pos p)
		{
			return Vec3((f32)p.x, (f32)p.y, (f32)p.z);
		}

		b3Quat ToB3Quat(const Quaternion &q)
		{
			b3Quat r;
			r.v.x = q.x;
			r.v.y = q.y;
			r.v.z = q.z;
			r.s = q.w;
			return r;
		}

		Quaternion FromB3Quat(b3Quat q)
		{
			return Quaternion(q.s, q.v.x, q.v.y, q.v.z);
		}

		b3Quat EulerToB3Quat(const Vec3 &euler)
		{
			Quaternion q;
			q.SetRotationFromEuler(euler);
			return ToB3Quat(q);
		}

		Matrix BodyToMatrix(b3BodyId body)
		{
			b3Pos p = b3Body_GetPosition(body);
			b3Quat q = b3Body_GetRotation(body);
			Matrix m = FromB3Quat(q).ConvertToMatrix();
			m.m[12] = (f32)p.x;
			m.m[13] = (f32)p.y;
			m.m[14] = (f32)p.z;
			return m;
		}

		float EstimateVolume(IPhysicsComponent* pcomp)
		{
			if (!pcomp) return 1.f;
			switch (pcomp->GetShape())
			{
			case CollisionShapes::Box:
			{
				PhysicsBox* box = static_cast<PhysicsBox*>(pcomp);
				// width/height/depth are half-extents (same as Cube primitive)
				return 8.f * box->GetWidth() * box->GetHeight() * box->GetDepth();
			}
			case CollisionShapes::Sphere:
			{
				PhysicsSphere* sphere = static_cast<PhysicsSphere*>(pcomp);
				const f32 r = sphere->GetRadius();
				return (4.f / 3.f) * 3.14159265358979323846f * r * r * r;
			}
			case CollisionShapes::Capsule:
			{
				PhysicsCapsule* capsule = static_cast<PhysicsCapsule*>(pcomp);
				const f32 r = capsule->GetRadius();
				const f32 h = capsule->GetHeight();
				return 3.14159265358979323846f * r * r * h + (4.f / 3.f) * 3.14159265358979323846f * r * r * r;
			}
			case CollisionShapes::Cylinder:
			{
				PhysicsCylinder* cylinder = static_cast<PhysicsCylinder*>(pcomp);
				const f32 r = cylinder->GetRadius();
				return 3.14159265358979323846f * r * r * (2.f * cylinder->GetHeight());
			}
			case CollisionShapes::Cone:
			{
				PhysicsCone* cone = static_cast<PhysicsCone*>(pcomp);
				const f32 r = cone->GetRadius();
				return (1.f / 3.f) * 3.14159265358979323846f * r * r * cone->GetHeight();
			}
			default:
				return 1.f;
			}
		}

		// density so that density * volume ~= authored mass (was wrongly using mass as density)
		float ShapeDensity(IPhysicsComponent* pcomp)
		{
			const f32 mass = pcomp->GetMass();
			if (mass <= 0.f) return 0.f;
			const float vol = EstimateVolume(pcomp);
			if (vol < 1e-8f) return mass > 0.001f ? mass : 0.001f;
			const float density = mass / vol;
			return density > 1e-6f ? density : 1e-6f;
		}

		void FillMeshBuffers(const std::vector<Vec3> &vertex, const std::vector<unsigned> &index,
			std::vector<b3Vec3> &outVerts, std::vector<int32_t> &outIndices)
		{
			outVerts.clear();
			outIndices.clear();
			outVerts.reserve(vertex.size());
			for (size_t i = 0; i < vertex.size(); ++i)
				outVerts.push_back(ToB3(vertex[i]));

			if (!index.empty())
			{
				outIndices.reserve(index.size());
				for (size_t i = 0; i < index.size(); ++i)
					outIndices.push_back((int32_t)index[i]);
			}
			else
			{
				outIndices.reserve(vertex.size());
				for (size_t i = 0; i < vertex.size(); ++i)
					outIndices.push_back((int32_t)i);
			}
		}

		b3HullData* CreateHullFromVertices(const std::vector<Vec3> &vertex)
		{
			if (vertex.empty()) return NULL;
			std::vector<b3Vec3> points;
			points.reserve(vertex.size());
			for (size_t i = 0; i < vertex.size(); ++i)
				points.push_back(ToB3(vertex[i]));
			return b3CreateHull(points.data(), (int)points.size(), 0);
		}

	}

	Box3DPhysics::Box3DPhysics() : m_world(b3_nullWorldId)
	{
		m_draw = b3DefaultDebugDraw();
	}

	Box3DPhysics::~Box3DPhysics()
	{
		EndPhysics();
	}

	Box3DBodyHandles* Box3DPhysics::GetHandles(IPhysicsComponent* pcomp)
	{
		return static_cast<Box3DBodyHandles*>(pcomp->GetRigidBodyPTR());
	}

	IPhysicsComponent* Box3DPhysics::ComponentFromShape(b3ShapeId shapeId)
	{
		if (!b3Shape_IsValid(shapeId)) return NULL;
		b3BodyId body = b3Shape_GetBody(shapeId);
		return static_cast<IPhysicsComponent*>(b3Body_GetUserData(body));
	}

	void Box3DPhysics::DestroyHandles(Box3DBodyHandles* handles)
	{
		if (!handles) return;

		for (size_t i = 0; i < handles->wheelJoints.size(); ++i)
		{
			if (handles->wheelJoints[i].index1 != 0)
				b3DestroyJoint(handles->wheelJoints[i], true);
		}
		handles->wheelJoints.clear();

		for (size_t i = 0; i < handles->wheelBodies.size(); ++i)
		{
			if (handles->wheelBodies[i].index1 != 0)
				b3DestroyBody(handles->wheelBodies[i]);
		}
		handles->wheelBodies.clear();

		if (handles->body.index1 != 0)
			b3DestroyBody(handles->body);
		handles->body = b3_nullBodyId;

		if (handles->ownedHull)
		{
			b3DestroyHull(handles->ownedHull);
			handles->ownedHull = NULL;
		}
		if (handles->ownedMesh)
		{
			b3DestroyMesh(handles->ownedMesh);
			handles->ownedMesh = NULL;
		}
		handles->meshVerts.clear();
		handles->meshIndices.clear();

		delete handles;
	}

	void Box3DPhysics::InitPhysics()
	{
		b3WorldDef def = b3DefaultWorldDef();
		// Stress piles easily hit the default 400 m/s cap and tunnel through walls.
		def.maximumLinearSpeed = 40.f;
		def.contactHertz = 60.f;
		def.contactDampingRatio = 10.f;
		m_world = b3CreateWorld(&def);
		b3World_SetGravity(m_world, ToB3(Vec3(0.f, -9.8f, 0.f)));
		physicsInitialized = true;
		lastTime = 0;
		timeInterval = 0;
	}

	void Box3DPhysics::ApplyVehicleMotors(IPhysicsComponent* pcomp)
	{
		if (!pcomp || pcomp->GetShape() != CollisionShapes::Vehicle) return;
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;

		PhysicsVehicle* vcomp = static_cast<PhysicsVehicle*>(pcomp);
		std::vector<VehicleWheel> &wheels = vcomp->GetWheels();
		const f32 steer = vcomp->GetVehicleSteering();
		const size_t jointCount = std::min(wheels.size(), handles->wheelJoints.size());

		// Only push authored steering into the joints. Drive/brake are demo-side
		// (Lua applyCentralForce / etc.) — do not special-case propulsion here.
		for (size_t i = 0; i < jointCount; ++i)
		{
			b3JointId joint = handles->wheelJoints[i];
			if (joint.index1 == 0) continue;
			if (wheels[i].IsFrontWheel)
				b3WheelJoint_SetTargetSteeringAngle(joint, steer);
		}
	}

	void Box3DPhysics::Update(const f64 &time, const uint32 steps)
	{
		PYROS_PROFILE_SCOPE("Physics.Update");

		if (m_world.index1 == 0) return;

		// Fixed-step accumulator: variable frame dt (especially under load)
		// was detonating contact islands so containment looked like walls crumbled.
		timeInterval += time;
		const float fixed = 1.f / 60.f;
		int maxSteps = (int)steps;
		if (maxSteps < 1) maxSteps = 1;
		if (maxSteps > 8) maxSteps = 8;

		int taken = 0;
		while (timeInterval >= (f64)fixed && taken < maxSteps)
		{
			// Motors must be set before the solver step (DemoLauncher steps
			// physics before Scene/Lua UpdateTransformations).
			for (size_t i = 0; i < m_vehicles.size(); ++i)
				ApplyVehicleMotors(m_vehicles[i]);

			b3World_Step(m_world, fixed, 4);
			timeInterval -= (f64)fixed;
			++taken;
		}
		if (timeInterval > (f64)fixed * 2.0)
			timeInterval = 0.0;

		ProcessCollisionEvents();
	}

	void Box3DPhysics::ProcessCollisionEvents()
	{
		b3ContactEvents contacts = b3World_GetContactEvents(m_world);
		for (int i = 0; i < contacts.beginCount; ++i)
		{
			IPhysicsComponent* a = ComponentFromShape(contacts.beginEvents[i].shapeIdA);
			IPhysicsComponent* b = ComponentFromShape(contacts.beginEvents[i].shapeIdB);
			if (!a || !b || a == b) continue;
			std::pair<IPhysicsComponent*, IPhysicsComponent*> pair = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
			if (m_touchingPairs.insert(pair).second)
			{
				if (pair.first->OnCollisionEnter) pair.first->OnCollisionEnter(pair.second);
				if (pair.second->OnCollisionEnter) pair.second->OnCollisionEnter(pair.first);
			}
		}
		for (int i = 0; i < contacts.endCount; ++i)
		{
			IPhysicsComponent* a = ComponentFromShape(contacts.endEvents[i].shapeIdA);
			IPhysicsComponent* b = ComponentFromShape(contacts.endEvents[i].shapeIdB);
			if (!a || !b || a == b) continue;
			std::pair<IPhysicsComponent*, IPhysicsComponent*> pair = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
			if (m_touchingPairs.erase(pair) > 0)
			{
				if (pair.first->OnCollisionExit) pair.first->OnCollisionExit(pair.second);
				if (pair.second->OnCollisionExit) pair.second->OnCollisionExit(pair.first);
			}
		}

		b3SensorEvents sensors = b3World_GetSensorEvents(m_world);
		for (int i = 0; i < sensors.beginCount; ++i)
		{
			IPhysicsComponent* a = ComponentFromShape(sensors.beginEvents[i].sensorShapeId);
			IPhysicsComponent* b = ComponentFromShape(sensors.beginEvents[i].visitorShapeId);
			if (!a || !b || a == b) continue;
			std::pair<IPhysicsComponent*, IPhysicsComponent*> pair = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
			if (m_touchingPairs.insert(pair).second)
			{
				if (pair.first->OnCollisionEnter) pair.first->OnCollisionEnter(pair.second);
				if (pair.second->OnCollisionEnter) pair.second->OnCollisionEnter(pair.first);
			}
		}
		for (int i = 0; i < sensors.endCount; ++i)
		{
			IPhysicsComponent* a = ComponentFromShape(sensors.endEvents[i].sensorShapeId);
			IPhysicsComponent* b = ComponentFromShape(sensors.endEvents[i].visitorShapeId);
			if (!a || !b || a == b) continue;
			std::pair<IPhysicsComponent*, IPhysicsComponent*> pair = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
			if (m_touchingPairs.erase(pair) > 0)
			{
				if (pair.first->OnCollisionExit) pair.first->OnCollisionExit(pair.second);
				if (pair.second->OnCollisionExit) pair.second->OnCollisionExit(pair.first);
			}
		}
	}

	void Box3DPhysics::EnableDebugDraw()
	{
		m_debugDraw.reset(new PhysicsDebugDraw());
		m_draw = m_debugDraw->CreateDraw();
	}

	void Box3DPhysics::RenderDebugDraw(Projection projection, GameObject* Camera)
	{
		if (!m_debugDraw || m_world.index1 == 0) return;
		m_debugDraw->ClearBuffers();
		b3World_Draw(m_world, &m_draw, ~0ull);
		m_debugDraw->Render(Camera->GetWorldTransformation().Inverse(), projection.m);
	}

	void Box3DPhysics::DisableDebugDraw()
	{
		m_debugDraw.reset();
		m_draw = b3DefaultDebugDraw();
	}

	void Box3DPhysics::EndPhysics()
	{
		DisableDebugDraw();
		if (m_world.index1 != 0)
		{
			b3DestroyWorld(m_world);
			m_world = b3_nullWorldId;
		}
		m_touchingPairs.clear();
		m_vehicles.clear();
		physicsInitialized = false;
	}

	b3ShapeDef Box3DPhysics::MakeShapeDef(IPhysicsComponent* pcomp) const
	{
		b3ShapeDef def = b3DefaultShapeDef();
		def.density = ShapeDensity(pcomp);
		// Contact events on every stress-test sphere dominate CPU past a few
		// thousand bodies and feed the same frame-time spiral. Opt in via
		// callbacks if a demo needs OnCollisionEnter (main.lua etc. can set
		// enableContactEvents after creation later if required).
		def.enableContactEvents = false;
		def.baseMaterial.restitution = 0.f;
		def.baseMaterial.friction = 0.5f;
		if (pcomp->IsGhost())
		{
			def.isSensor = true;
			def.enableSensorEvents = true;
			def.density = 0.f;
		}
		def.userData = pcomp;
		return def;
	}

	b3BodyDef Box3DPhysics::MakeBodyDef(IPhysicsComponent* pcomp) const
	{
		b3BodyDef def = b3DefaultBodyDef();
		const f32 mass = pcomp->GetMass();
		def.type = (mass == 0.f || pcomp->IsGhost()) ? b3_staticBody : b3_dynamicBody;
		if (def.type == b3_staticBody)
		{
			def.motionLocks.linearX = true;
			def.motionLocks.linearY = true;
			def.motionLocks.linearZ = true;
			def.motionLocks.angularX = true;
			def.motionLocks.angularY = true;
			def.motionLocks.angularZ = true;
		}
		if (pcomp->GetOwner())
		{
			def.position = ToB3Pos(pcomp->GetOwner()->GetPosition());
			def.rotation = EulerToB3Quat(pcomp->GetOwner()->GetRotation());
		}
		def.userData = pcomp;
		return def;
	}

	void Box3DPhysics::AttachShapes(IPhysicsComponent* pcomp, Box3DBodyHandles* handles, b3BodyId body, const b3ShapeDef &shapeDef)
	{
		switch (pcomp->GetShape())
		{
		case CollisionShapes::Box:
		{
			PhysicsBox* box = static_cast<PhysicsBox*>(pcomp);
			b3BoxHull hull = b3MakeBoxHull(box->GetWidth(), box->GetHeight(), box->GetDepth());
			b3CreateHullShape(body, &shapeDef, &hull.base);
		}
		break;
		case CollisionShapes::Sphere:
		{
			PhysicsSphere* sphere = static_cast<PhysicsSphere*>(pcomp);
			b3Sphere s;
			s.center = b3Vec3_zero;
			s.radius = sphere->GetRadius();
			b3CreateSphereShape(body, &shapeDef, &s);
		}
		break;
		case CollisionShapes::Capsule:
		{
			PhysicsCapsule* capsule = static_cast<PhysicsCapsule*>(pcomp);
			const f32 half = capsule->GetHeight() * 0.5f;
			b3Capsule c;
			c.center1 = ToB3(Vec3(0.f, -half, 0.f));
			c.center2 = ToB3(Vec3(0.f, half, 0.f));
			c.radius = capsule->GetRadius();
			b3CreateCapsuleShape(body, &shapeDef, &c);
		}
		break;
		case CollisionShapes::Cone:
		{
			PhysicsCone* cone = static_cast<PhysicsCone*>(pcomp);
			handles->ownedHull = b3CreateCone(cone->GetHeight(), cone->GetRadius(), 0.f, 16);
			if (handles->ownedHull)
				b3CreateHullShape(body, &shapeDef, handles->ownedHull);
		}
		break;
		case CollisionShapes::Cylinder:
		{
			PhysicsCylinder* cylinder = static_cast<PhysicsCylinder*>(pcomp);
			handles->ownedHull = b3CreateCylinder(cylinder->GetHeight(), cylinder->GetRadius(), 0.f, 16);
			if (handles->ownedHull)
				b3CreateHullShape(body, &shapeDef, handles->ownedHull);
		}
		break;
		case CollisionShapes::StaticPlane:
		{
			PhysicsStaticPlane* plane = static_cast<PhysicsStaticPlane*>(pcomp);
			Vec3 n = plane->GetNormal();
			const f32 len = n.magnitude();
			if (len > 1e-6f) n = n * (1.f / len);
			else n = Vec3(0.f, 1.f, 0.f);

			b3BoxHull hull = b3MakeBoxHull(1000.f, 0.05f, 1000.f);
			b3CreateHullShape(body, &shapeDef, &hull.base);

			b3Quat rot = b3ComputeQuatBetweenUnitVectors(b3Vec3_axisY, ToB3(n));
			b3Pos pos = ToB3Pos(n * plane->GetConstant());
			b3Body_SetTransform(body, pos, rot);
		}
		break;
		case CollisionShapes::ConvexHull:
		{
			PhysicsConvexHull* hullComp = static_cast<PhysicsConvexHull*>(pcomp);
			handles->ownedHull = CreateHullFromVertices(hullComp->GetPoints());
			if (handles->ownedHull)
				b3CreateHullShape(body, &shapeDef, handles->ownedHull);
		}
		break;
		case CollisionShapes::ConvexTriangleMesh:
		{
			PhysicsConvexTriangleMesh* mesh = static_cast<PhysicsConvexTriangleMesh*>(pcomp);
			handles->ownedHull = CreateHullFromVertices(mesh->GetVertexData());
			if (handles->ownedHull)
				b3CreateHullShape(body, &shapeDef, handles->ownedHull);
		}
		break;
		case CollisionShapes::TriangleMesh:
		{
			PhysicsTriangleMesh* mesh = static_cast<PhysicsTriangleMesh*>(pcomp);
			FillMeshBuffers(mesh->GetVertexData(), mesh->GetIndexData(), handles->meshVerts, handles->meshIndices);

			const int triCount = (int)handles->meshIndices.size() / 3;
			if (triCount > 0 && !handles->meshVerts.empty())
			{
				b3MeshDef def = {};
				def.vertices = handles->meshVerts.data();
				def.indices = handles->meshIndices.data();
				def.vertexCount = (int)handles->meshVerts.size();
				def.triangleCount = triCount;
				handles->ownedMesh = b3CreateMesh(&def, NULL, 0);
				if (handles->ownedMesh)
					b3CreateMeshShape(body, &shapeDef, handles->ownedMesh, b3Vec3_one);
			}
		}
		break;
		case CollisionShapes::MultipleSphere:
		{
			PhysicsMultipleSphere* multi = static_cast<PhysicsMultipleSphere*>(pcomp);
			const std::vector<Vec3> &positions = multi->GetPositions();
			const std::vector<f32> &radii = multi->GetRadius();
			const size_t count = std::min(positions.size(), radii.size());
			for (size_t i = 0; i < count; ++i)
			{
				b3Sphere s;
				s.center = ToB3(positions[i]);
				s.radius = radii[i];
				b3CreateSphereShape(body, &shapeDef, &s);
			}
		}
		break;
		default:
			break;
		};

		if (!pcomp->IsGhost() && pcomp->GetMass() > 0.f)
			b3Body_ApplyMassFromShapes(body);
	}

	void Box3DPhysics::AttachChassisShapes(IPhysicsComponent* chassis, Box3DBodyHandles* handles, b3BodyId body, const b3ShapeDef &shapeDef)
	{
		if (!chassis) return;
		AttachShapes(chassis, handles, body, shapeDef);
	}

	void Box3DPhysics::CreateBody(IPhysicsComponent* pcomp)
	{
		Box3DBodyHandles* handles = new Box3DBodyHandles();
		b3BodyDef bodyDef = MakeBodyDef(pcomp);

		// Dynamic triangle meshes become convex hulls; static meshes stay meshes.
		if (pcomp->GetShape() == CollisionShapes::TriangleMesh && pcomp->GetMass() > 0.f && !pcomp->IsGhost())
		{
			bodyDef.type = b3_dynamicBody;
		}

		handles->body = b3CreateBody(m_world, &bodyDef);
		b3Body_SetUserData(handles->body, pcomp);

		b3ShapeDef shapeDef = MakeShapeDef(pcomp);

		if (pcomp->GetShape() == CollisionShapes::TriangleMesh && pcomp->GetMass() > 0.f && !pcomp->IsGhost())
		{
			// Contacts on triangle meshes are static-only — use a hull for dynamics.
			PhysicsTriangleMesh* mesh = static_cast<PhysicsTriangleMesh*>(pcomp);
			handles->ownedHull = CreateHullFromVertices(mesh->GetVertexData());
			if (handles->ownedHull)
				b3CreateHullShape(handles->body, &shapeDef, handles->ownedHull);
			b3Body_ApplyMassFromShapes(handles->body);
		}
		else
		{
			AttachShapes(pcomp, handles, handles->body, shapeDef);
		}

		pcomp->SaveRigidBodyPTR(handles);
	}

	void Box3DPhysics::CreatePhysicsComponent(IPhysicsComponent* pcomp)
	{
		if (pcomp->GetShape() == CollisionShapes::Vehicle)
		{
			PhysicsVehicle* vehicle = static_cast<PhysicsVehicle*>(pcomp);
			IPhysicsComponent* chassis = vehicle->GetChassis();

			Box3DBodyHandles* handles = new Box3DBodyHandles();
			b3BodyDef bodyDef = b3DefaultBodyDef();
			const f32 chassisMass = chassis ? chassis->GetMass() : 0.f;
			bodyDef.type = (chassisMass == 0.f) ? b3_staticBody : b3_dynamicBody;
			if (pcomp->GetOwner())
			{
				bodyDef.position = ToB3Pos(pcomp->GetOwner()->GetPosition());
				bodyDef.rotation = EulerToB3Quat(pcomp->GetOwner()->GetRotation());
			}
			bodyDef.userData = pcomp;
			bodyDef.enableSleep = false;

			handles->body = b3CreateBody(m_world, &bodyDef);
			b3Body_SetUserData(handles->body, pcomp);

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.density = chassis ? ShapeDensity(chassis) : 0.f;
			shapeDef.enableContactEvents = false;
			shapeDef.baseMaterial.restitution = 0.f;
			if (pcomp->IsGhost())
			{
				shapeDef.isSensor = true;
				shapeDef.enableSensorEvents = true;
				shapeDef.density = 0.f;
			}
			shapeDef.userData = pcomp;

			if (chassis)
				AttachChassisShapes(chassis, handles, handles->body, shapeDef);

			pcomp->SaveRigidBodyPTR(handles);
			m_vehicles.push_back(pcomp);

			std::vector<VehicleWheel> &wheels = vehicle->GetWheels();
			for (uint32 i = 0; i < wheels.size(); ++i)
			{
				AddWheel(pcomp, wheels[i].Direction, wheels[i].Axle, wheels[i].Radius, wheels[i].Width,
					wheels[i].Friction, wheels[i].RollInfluence, wheels[i].Position, wheels[i].IsFrontWheel);
			}
		}
		else
		{
			CreateBody(pcomp);
		}
	}

	void Box3DPhysics::AddWheel(IPhysicsComponent *pcomp, const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel)
	{
		(void)WheelDirection;
		(void)WheelWidth;
		(void)WheelRollInfluence;

		PhysicsVehicle* vehicle = static_cast<PhysicsVehicle*>(pcomp);
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;

		// Sphere wheels: keep body orientation = chassis so getWheelTransform
		// matches Bullet/mesh authorship. Put spin/suspension axes in the joint
		// frames (Driving tips the body for cylinders — wrong for mesh spheres).
		b3Pos chassisPos = b3Body_GetPosition(handles->body);
		b3Quat chassisRot = b3Body_GetRotation(handles->body);
		b3Vec3 axle = b3Normalize(ToB3(WheelAxle));
		if (b3LengthSquared(axle) < 0.01f)
			axle = b3Vec3_axisX;
		b3Vec3 worldOffset = b3RotateVector(chassisRot, ToB3(Position));

		b3BodyDef wheelDef = b3DefaultBodyDef();
		wheelDef.type = b3_dynamicBody;
		wheelDef.position.x = chassisPos.x + worldOffset.x;
		wheelDef.position.y = chassisPos.y + worldOffset.y;
		wheelDef.position.z = chassisPos.z + worldOffset.z;
		wheelDef.rotation = chassisRot;
		wheelDef.allowFastRotation = true;
		wheelDef.enableSleep = false;

		b3BodyId wheelBody = b3CreateBody(m_world, &wheelDef);

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 2.f;
		shapeDef.baseMaterial.friction = WheelFriction > 0.1f ? WheelFriction : 3.f;
		if (shapeDef.baseMaterial.friction < 1.5f)
			shapeDef.baseMaterial.friction = 3.f;
		shapeDef.enableContactEvents = true;

		b3Sphere sphere;
		sphere.center = b3Vec3_zero;
		sphere.radius = WheelRadius > 0.05f ? WheelRadius : 0.35f;
		b3CreateSphereShape(wheelBody, &shapeDef, &sphere);
		b3Body_ApplyMassFromShapes(wheelBody);

		b3WheelJointDef jointDef = b3DefaultWheelJointDef();
		jointDef.base.bodyIdA = handles->body;
		jointDef.base.bodyIdB = wheelBody;
		jointDef.base.localFrameA.p = ToB3(Position);
		// Suspension along chassis Y (joint X → Y).
		jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors(b3Vec3_axisX, b3Vec3_axisY);
		jointDef.base.localFrameB.p = b3Vec3_zero;
		// Spin around WheelAxle in body space (joint Z → axle).
		jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors(b3Vec3_axisZ, axle);
		jointDef.base.collideConnected = false;

		// Map legacy Bullet-ish suspension params into Box3D Hertz/ratio.
		f32 hertz = vehicle->GetSuspensionStiffness();
		if (hertz > 12.f) hertz = 4.f;
		if (hertz < 1.f) hertz = 4.f;
		f32 damp = vehicle->GetSuspensionDamping();
		if (damp > 1.2f || damp < 0.05f) damp = 0.7f;

		jointDef.enableSuspensionSpring = true;
		jointDef.suspensionHertz = hertz;
		jointDef.suspensionDampingRatio = damp;
		jointDef.enableSuspensionLimit = true;
		jointDef.lowerSuspensionLimit = -0.2f;
		jointDef.upperSuspensionLimit = 0.15f;

		jointDef.enableSpinMotor = !isFrontWheel;
		jointDef.spinSpeed = 0.f;
		jointDef.maxSpinTorque = 80.f;
		jointDef.enableSteering = isFrontWheel;
		jointDef.steeringHertz = 10.f;
		jointDef.steeringDampingRatio = 0.7f;
		jointDef.targetSteeringAngle = 0.f;
		jointDef.maxSteeringTorque = 40.f;
		jointDef.enableSteeringLimit = true;
		const f32 steerClamp = vehicle->GetSteeringClamp() > 0.05f ? vehicle->GetSteeringClamp() : 0.45f;
		jointDef.lowerSteeringLimit = -steerClamp;
		jointDef.upperSteeringLimit = steerClamp;

		b3JointId joint = b3CreateWheelJoint(m_world, &jointDef);

		handles->wheelBodies.push_back(wheelBody);
		handles->wheelJoints.push_back(joint);
	}

	void Box3DPhysics::UpdateTransformations(IPhysicsComponent* pcomp)
	{
		if (!pcomp->RigidBodyRegistered()) return;
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		if (!b3Body_IsValid(handles->body)) return;

		GameObject* owner = pcomp->GetOwner();
		if (!owner) return;

		// Vehicles report mass 0 on the component (mass lives on the chassis
		// orphan) but are dynamic bodies — must not hit the static path below
		// or the chassis is glued to the GameObject and never simulates.
		if (pcomp->GetShape() == CollisionShapes::Vehicle)
		{
			PhysicsVehicle* vcomp = static_cast<PhysicsVehicle*>(pcomp);
			std::vector<VehicleWheel> &wheels = vcomp->GetWheels();

			// Drive motors are applied in Update() before b3World_Step.
			b3Pos p = b3Body_GetPosition(handles->body);
			b3Quat q = b3Body_GetRotation(handles->body);
			owner->SetPosition(FromB3Pos(p));
			owner->SetRotation(FromB3Quat(q).GetEulerFromQuaternion());

			const size_t count = std::min(wheels.size(), handles->wheelBodies.size());
			for (size_t i = 0; i < count; ++i)
			{
				if (handles->wheelBodies[i].index1 != 0)
					wheels[i].Transformation = BodyToMatrix(handles->wheelBodies[i]);
			}
			return;
		}

		// Static / sensor: keep the Box3D body glued to the authored GameObject
		// pose (never the other way around — solver noise must not move walls).
		if (pcomp->GetMass() <= 0.f || pcomp->IsGhost())
		{
			b3Body_SetTransform(handles->body,
				ToB3Pos(owner->GetPosition()),
				EulerToB3Quat(owner->GetRotation()));
			return;
		}

		b3Pos p = b3Body_GetPosition(handles->body);
		b3Quat q = b3Body_GetRotation(handles->body);
		owner->SetPosition(FromB3Pos(p));
		owner->SetRotation(FromB3Quat(q).GetEulerFromQuaternion());
	}

	void Box3DPhysics::RemovePhysicsComponent(IPhysicsComponent* pcomp)
	{
		if (!pcomp->RigidBodyRegistered()) return;

		for (size_t i = 0; i < m_vehicles.size(); ++i)
		{
			if (m_vehicles[i] == pcomp)
			{
				m_vehicles.erase(m_vehicles.begin() + (std::ptrdiff_t)i);
				break;
			}
		}

		Box3DBodyHandles* handles = GetHandles(pcomp);
		DestroyHandles(handles);
		pcomp->ClearRigidBodyPTR();

		for (std::set<std::pair<IPhysicsComponent*, IPhysicsComponent*> >::iterator it = m_touchingPairs.begin(); it != m_touchingPairs.end(); )
		{
			if (it->first == pcomp || it->second == pcomp)
				it = m_touchingPairs.erase(it);
			else
				++it;
		}
	}

	RayCastHit Box3DPhysics::RayCast(const Vec3 &from, const Vec3 &to)
	{
		RayCastHit hit;
		if (m_world.index1 == 0) return hit;

		const Vec3 delta = to - from;
		b3RayResult result = b3World_CastRayClosest(m_world, ToB3Pos(from), ToB3(delta), b3DefaultQueryFilter());
		if (result.hit)
		{
			hit.hasHit = true;
			hit.point = FromB3Pos(result.point);
			hit.normal = FromB3(result.normal);
			hit.distance = delta.magnitude() * result.fraction;
			hit.component = ComponentFromShape(result.shapeId);
		}
		return hit;
	}

	void Box3DPhysics::UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Pos oldPos = b3Body_GetPosition(handles->body);
		b3Quat q = b3Body_GetRotation(handles->body);
		b3Pos newPos = ToB3Pos(position);
		const b3Vec3 delta = { newPos.x - oldPos.x, newPos.y - oldPos.y, newPos.z - oldPos.z };
		b3Body_SetTransform(handles->body, newPos, q);
		b3Body_SetLinearVelocity(handles->body, b3Vec3_zero);
		b3Body_SetAngularVelocity(handles->body, b3Vec3_zero);
		for (size_t i = 0; i < handles->wheelBodies.size(); ++i)
		{
			if (handles->wheelBodies[i].index1 == 0) continue;
			b3Pos wp = b3Body_GetPosition(handles->wheelBodies[i]);
			wp.x += delta.x; wp.y += delta.y; wp.z += delta.z;
			b3Quat wq = b3Body_GetRotation(handles->wheelBodies[i]);
			b3Body_SetTransform(handles->wheelBodies[i], wp, wq);
			b3Body_SetLinearVelocity(handles->wheelBodies[i], b3Vec3_zero);
			b3Body_SetAngularVelocity(handles->wheelBodies[i], b3Vec3_zero);
		}
	}

	void Box3DPhysics::UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Pos p = b3Body_GetPosition(handles->body);
		b3Quat q = EulerToB3Quat(rotation);
		b3Body_SetTransform(handles->body, p, q);
		b3Body_SetLinearVelocity(handles->body, b3Vec3_zero);
		b3Body_SetAngularVelocity(handles->body, b3Vec3_zero);

		if (pcomp->GetShape() == CollisionShapes::Vehicle)
		{
			PhysicsVehicle* vehicle = static_cast<PhysicsVehicle*>(pcomp);
			std::vector<VehicleWheel> &wheels = vehicle->GetWheels();
			const size_t count = std::min(wheels.size(), handles->wheelBodies.size());
			for (size_t i = 0; i < count; ++i)
			{
				if (handles->wheelBodies[i].index1 == 0) continue;
				b3Vec3 worldOffset = b3RotateVector(q, ToB3(wheels[i].Position));
				b3Pos wp = { p.x + worldOffset.x, p.y + worldOffset.y, p.z + worldOffset.z };
				// Same orientation as chassis — spin axis lives in the joint frame.
				b3Body_SetTransform(handles->wheelBodies[i], wp, q);
				b3Body_SetLinearVelocity(handles->wheelBodies[i], b3Vec3_zero);
				b3Body_SetAngularVelocity(handles->wheelBodies[i], b3Vec3_zero);
			}
		}
	}

	void Box3DPhysics::CleanForces(IPhysicsComponent *pcomp)
	{
		// Box3D does not accumulate forces across steps the way Bullet did.
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Body_SetLinearVelocity(handles->body, b3Vec3_zero);
		b3Body_SetAngularVelocity(handles->body, b3Vec3_zero);
		for (size_t i = 0; i < handles->wheelBodies.size(); ++i)
		{
			if (handles->wheelBodies[i].index1 == 0) continue;
			b3Body_SetLinearVelocity(handles->wheelBodies[i], b3Vec3_zero);
			b3Body_SetAngularVelocity(handles->wheelBodies[i], b3Vec3_zero);
		}
	}

	void Box3DPhysics::SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Body_SetAngularVelocity(handles->body, ToB3(velocity));
	}

	void Box3DPhysics::SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Body_SetLinearVelocity(handles->body, ToB3(velocity));
	}

	void Box3DPhysics::Activate(IPhysicsComponent *pcomp)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Body_SetAwake(handles->body, true);
	}

	Vec3 Box3DPhysics::GetLinearVelocity(IPhysicsComponent *pcomp)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return Vec3();
		return FromB3(b3Body_GetLinearVelocity(handles->body));
	}

	Vec3 Box3DPhysics::GetAngularVelocity(IPhysicsComponent *pcomp)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return Vec3();
		return FromB3(b3Body_GetAngularVelocity(handles->body));
	}

	void Box3DPhysics::ApplyCentralForce(IPhysicsComponent *pcomp, const Vec3 &force)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Body_ApplyForceToCenter(handles->body, ToB3(force), true);
	}

	void Box3DPhysics::ApplyCentralImpulse(IPhysicsComponent *pcomp, const Vec3 &impulse)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;
		b3Body_ApplyLinearImpulseToCenter(handles->body, ToB3(impulse), true);
	}

	void Box3DPhysics::SetMass(IPhysicsComponent *pcomp, const f32 mass)
	{
		Box3DBodyHandles* handles = GetHandles(pcomp);
		if (!handles || handles->body.index1 == 0) return;

		if (mass <= 0.f)
		{
			b3Body_SetType(handles->body, b3_staticBody);
			return;
		}

		b3Body_SetType(handles->body, b3_dynamicBody);
		b3MassData md = b3Body_GetMassData(handles->body);
		const float oldMass = md.mass > 1e-6f ? md.mass : 1.f;
		const float scale = mass / oldMass;
		md.mass = mass;
		md.inertia.cx.x *= scale; md.inertia.cx.y *= scale; md.inertia.cx.z *= scale;
		md.inertia.cy.x *= scale; md.inertia.cy.y *= scale; md.inertia.cy.z *= scale;
		md.inertia.cz.x *= scale; md.inertia.cz.y *= scale; md.inertia.cz.z *= scale;
		b3Body_SetMassData(handles->body, md);
	}

	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateBox(const f32 width, const f32 height, const f32 depth, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsBox>(this, width, height, depth, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateCapsule(const f32 radius, const f32 height, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsCapsule>(this, radius, height, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateCone(const f32 radius, const f32 height, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsCone>(this, radius, height, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateConvexHull(const std::vector<Vec3> &points, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsConvexHull>(this, points, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsConvexTriangleMesh>(this, rcomp, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateConvexTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsConvexTriangleMesh>(this, index, vertex, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateCylinder(const f32 radius, const f32 height, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsCylinder>(this, radius, height, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsMultipleSphere>(this, positions, radius, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateSphere(const f32 radius, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsSphere>(this, radius, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateStaticPlane(const Vec3 &Normal, const f32 Constant, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsStaticPlane>(this, Normal, Constant, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateTriangleMesh(RenderingComponent* rcomp, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsTriangleMesh>(this, rcomp, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost)
	{
		return std::make_shared<PhysicsTriangleMesh>(this, index, vertex, mass, ghost);
	}
	std::shared_ptr<IPhysicsComponent> Box3DPhysics::CreateVehicle(const std::shared_ptr<IPhysicsComponent> &ChassisShape, bool ghost)
	{
		return std::make_shared<PhysicsVehicle>(this, ChassisShape, ghost);
	}

}
