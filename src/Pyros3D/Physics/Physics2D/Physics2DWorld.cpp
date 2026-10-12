//============================================================================
// Name        : Physics2DWorld
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Owns the Box2D world for a scene's Physics2D components
//============================================================================

#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Physics/Physics2D/Joint2D.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Physics/PhysicsStepping.h>
#include <box2d/box2d.h>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

namespace p3d {

	namespace {

		// Box2D's task hooks, backed by the engine's JobSystem so physics
		// shares one pool with everything else instead of spinning up its own
		// threads. b2World_Step runs on the main thread, not inside a job,
		// and FinishTask's Wait() runs other queued jobs while it blocks -
		// the two conditions Box2D's header gives for this not deadlocking.
		void* EnqueuePhysicsTask(b2TaskCallback* task, void* taskContext, void*)
		{
			JobSystem &jobs = JobSystem::Instance();
			if (jobs.WorkerCount() == 0)
			{
				task(taskContext);
				return NULL; // ran inline: Box2D will not call FinishTask
			}
			JobCounter* counter = new JobCounter();
			jobs.Run(task, taskContext, *counter);
			return counter;
		}

		void FinishPhysicsTask(void* userTask, void*)
		{
			JobCounter* counter = static_cast<JobCounter*>(userTask);
			JobSystem::Instance().Wait(*counter);
			delete counter;
		}
		// The split-handle dance both classes do so their headers stay
		// box2d-free. See Physics2D::SetBodyHandle.
		b2WorldId MakeWorldId(const uint16 index, const uint16 generation)
		{
			b2WorldId id;
			id.index1 = index;
			id.generation = generation;
			return id;
		}
		b2BodyId MakeBodyId(const Physics2D* p)
		{
			b2BodyId id;
			id.index1 = p->GetBodyIndex();
			id.world0 = p->GetBodyWorld();
			id.generation = p->GetBodyGeneration();
			return id;
		}
		b2BodyType TranslateBodyType(const uint32 t)
		{
			switch (t)
			{
			case Body2DType::Static: return b2_staticBody;
			case Body2DType::Kinematic: return b2_kinematicBody;
			default: return b2_dynamicBody;
			}
		}
	}

	Physics2DWorld::Physics2DWorld(const Vec2 &gravity)
	{
		this->gravity = gravity;
		worldIndex = 0;
		worldGeneration = 0;
		live = false;
		accumulator = 0.0;
		subSteps = 4;
		haveGround = false;
		groundIndex = 0;
		groundWorld = 0;
		groundGeneration = 0;
	}

	void Physics2DWorld::Clear()
	{
		if (live)
		{
			// The components may already be gone - this is called precisely
			// when the scene is being replaced - so the handles are not
			// cleared through them here. Destroying the world is enough; a
			// component that outlives this has its handle cleared by Sync()
			// when it fails to find a body.
			b2DestroyWorld(MakeWorldId(worldIndex, worldGeneration));
			live = false;
		}
		tracked.clear();
		trackedBodyIds.clear();
		// The joints went with the world. The components may be gone too, so
		// they are not told - Sync() asks the solver whether a handle is
		// still good before it trusts one.
		trackedJoints.clear();
		trackedJointIds.clear();
		haveGround = false;
		accumulator = 0.0;
	}

	Physics2DWorld::~Physics2DWorld()
	{
		if (live)
		{
			// Destroying the world destroys every body in it, so the tracked
			// components' handles are stale from here - clear them rather
			// than leave something that looks valid.
			for (size_t i = 0; i < tracked.size(); i++)
				if (tracked[i]) tracked[i]->ClearBodyHandle();
			b2DestroyWorld(MakeWorldId(worldIndex, worldGeneration));
			live = false;
		}
	}

	namespace {
		// Box2D hands colours as packed 0xRRGGBB.
		Vec4 FromHex(const unsigned int hex)
		{
			return Vec4(((hex >> 16) & 0xFF) / 255.f,
				((hex >> 8) & 0xFF) / 255.f,
				(hex & 0xFF) / 255.f, 1.f);
		}

		// Everything Box2D draws arrives through these C callbacks, which get
		// the renderer and the z plane through `context`.
		struct DebugCtx { DebugRenderer* debug; f32 z; };

		Vec3 At(const DebugCtx* c, const float x, const float y) { return Vec3(x, y, c->z); }

		// A shape's vertices are in body space; the transform puts them in the
		// world. Applying it here rather than asking Box2D for world-space
		// vertices keeps this the same shape as the callbacks it implements.
		Vec3 Xf(const DebugCtx* c, const b2WorldTransform &t, const b2Vec2 &v)
		{
			const float x = (float)t.p.x + (t.q.c * v.x - t.q.s * v.y);
			const float y = (float)t.p.y + (t.q.s * v.x + t.q.c * v.y);
			return At(c, x, y);
		}

		void DrawPoly(b2WorldTransform t, const b2Vec2* verts, int count, b2HexColor color, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			const Vec4 col = FromHex((unsigned int)color);
			for (int i = 0; i < count; i++)
				c->debug->drawLine(Xf(c, t, verts[i]), Xf(c, t, verts[(i + 1) % count]), col);
		}
		void DrawSolidPoly(b2WorldTransform t, const b2Vec2* verts, int count, float, b2HexColor color, void* ctx)
		{
			// Outline only - a filled collider would hide the sprite it is
			// meant to be checked against, which is the entire use for this.
			DrawPoly(t, verts, count, color, ctx);
		}
		void DrawCircleAt(DebugCtx* c, const float cx, const float cy, const float r, const Vec4 &col)
		{
			static const int SEGMENTS = 16;
			for (int i = 0; i < SEGMENTS; i++)
			{
				const float a0 = (float)i / SEGMENTS * 6.2831853f;
				const float a1 = (float)(i + 1) / SEGMENTS * 6.2831853f;
				c->debug->drawLine(At(c, cx + cosf(a0) * r, cy + sinf(a0) * r),
					At(c, cx + cosf(a1) * r, cy + sinf(a1) * r), col);
			}
		}
		void DrawCircle(b2Pos center, float radius, b2HexColor color, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			DrawCircleAt(c, (float)center.x, (float)center.y, radius, FromHex((unsigned int)color));
		}
		void DrawSolidCircle(b2WorldTransform t, b2Vec2 center, float radius, b2HexColor color, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			const Vec3 p = Xf(c, t, center);
			DrawCircleAt(c, p.x, p.y, radius, FromHex((unsigned int)color));
		}
		void DrawSolidCapsule(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			const Vec4 col = FromHex((unsigned int)color);
			DrawCircleAt(c, (float)p1.x, (float)p1.y, radius, col);
			DrawCircleAt(c, (float)p2.x, (float)p2.y, radius, col);
			c->debug->drawLine(At(c, (float)p1.x, (float)p1.y), At(c, (float)p2.x, (float)p2.y), col);
		}
		void DrawLine2D(b2Pos p1, b2Pos p2, b2HexColor color, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			c->debug->drawLine(At(c, (float)p1.x, (float)p1.y), At(c, (float)p2.x, (float)p2.y),
				FromHex((unsigned int)color));
		}
		void DrawTransform2D(b2WorldTransform t, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			const float L = 0.25f;
			const Vec3 o = Xf(c, t, b2Vec2{ 0.f, 0.f });
			c->debug->drawLine(o, Xf(c, t, b2Vec2{ L, 0.f }), Vec4(1.f, 0.f, 0.f, 1.f));
			c->debug->drawLine(o, Xf(c, t, b2Vec2{ 0.f, L }), Vec4(0.f, 1.f, 0.f, 1.f));
		}
		void DrawPoint2D(b2Pos p, float size, b2HexColor color, void* ctx)
		{
			DebugCtx* c = (DebugCtx*)ctx;
			const float r = size * 0.02f;
			DrawCircleAt(c, (float)p.x, (float)p.y, r, FromHex((unsigned int)color));
		}
		void DrawString2D(b2Pos, const char*, b2HexColor, void*) {}
	}

	namespace {
		// See WorldAngleOf, which this is, declared further down beside the
		// code that grew up needing it.
		f32 WorldAngleFor(GameObject* go)
		{
			const Matrix &w = go->GetWorldTransformation();
			return atan2f(w.m[1], w.m[0]);
		}
	}

	void Physics2DWorld::PullTransforms()
	{
		if (!live) return;
		for (size_t i = 0; i < tracked.size(); i++)
		{
			Physics2D* p = tracked[i];
			if (!p || !p->HaveBody() || p->GetOwner() == NULL) continue;
			const Vec3 pos = p->GetOwner()->GetWorldPosition();
			const f32 angle = WorldAngleFor(p->GetOwner());
			p->SetTransform(Vec2(pos.x, pos.y), angle);
			p->syncedPosition = pos;
			p->syncedAngle = angle;
			p->haveSynced = true;
			// Outside play a body has to match the authored scene
			// *completely*, not just in position. Placing it and leaving the
			// rest of its state alone is what made the second Play do
			// nothing: b2Body_SetTransform neither clears velocity nor wakes
			// the body, so anything that had come to rest during the first
			// run - a stack of boxes that settled - was still asleep when
			// Play started again, and a sleeping body is not simulated at
			// all. The leftover velocity is the same bug seen from the other
			// side: stop while something is moving and the next run began
			// with that speed already on it. Static bodies are excluded:
			// they have no velocity and never sleep.
			if (p->GetBodyType() != Body2DType::Static)
			{
				p->SetLinearVelocity(Vec2(0.f, 0.f));
				p->SetAngularVelocity(0.f);
				p->Wake();
			}
		}
	}

	namespace { Physics2D* BodyOf(GameObject* go); }

	void Physics2DWorld::DebugDraw(DebugRenderer* debug, const f32 z)
	{
		if (!live || debug == NULL) return;

		DebugCtx ctx;
		ctx.debug = debug;
		ctx.z = z;

		b2DebugDraw d = b2DefaultDebugDraw();
		d.DrawPolygonFcn = &DrawPoly;
		d.DrawSolidPolygonFcn = &DrawSolidPoly;
		d.DrawCircleFcn = &DrawCircle;
		d.DrawSolidCircleFcn = &DrawSolidCircle;
		d.DrawSolidCapsuleFcn = &DrawSolidCapsule;
		d.DrawLineFcn = &DrawLine2D;
		d.DrawTransformFcn = &DrawTransform2D;
		d.DrawPointFcn = &DrawPoint2D;
		d.DrawStringFcn = &DrawString2D;
		d.drawShapes = true;
		// A rope or a hinge is as invisible as a collider, and as easy to
		// have in the wrong place.
		d.drawJoints = true;
		d.context = &ctx;

		b2World_Draw(MakeWorldId(worldIndex, worldGeneration), &d);

		// The joints the solver does not know about, in its joints' colour.
		const Vec4 rope(0.5f, 0.8f, 0.8f, 1.f);
		for (size_t i = 0; i < trackedJoints.size(); i++)
		{
			Joint2D* j = trackedJoints[i];
			if (!j || j->GetJointType() != Joint2DType::Pulley || j->GetOwner() == NULL) continue;
			Physics2D* mine = BodyOf(j->GetOwner());
			Physics2D* other = NULL;
			for (size_t k = 0; k < tracked.size() && !other; k++)
				if (tracked[k] && tracked[k] != mine && tracked[k]->GetOwner() && tracked[k]->GetOwner()->GetName() == j->GetConnected())
					other = tracked[k];
			const Vec2 gA = j->GetGroundAnchor(), gB = j->GetConnectedGroundAnchor();
			debug->drawLine(At(&ctx, gA.x, gA.y), At(&ctx, gB.x, gB.y), rope);
			if (mine && mine->HaveBody())
			{
				b2Vec2 l; l.x = j->GetAnchor().x; l.y = j->GetAnchor().y;
				const b2Pos p = b2Body_GetWorldPoint(MakeBodyId(mine), l);
				debug->drawLine(At(&ctx, gA.x, gA.y), At(&ctx, (float)p.x, (float)p.y), rope);
			}
			if (other && other->HaveBody())
			{
				b2Vec2 l; l.x = j->HasConnectedAnchor() ? j->GetConnectedAnchor().x : 0.f; l.y = j->HasConnectedAnchor() ? j->GetConnectedAnchor().y : 0.f;
				const b2Pos p = b2Body_GetWorldPoint(MakeBodyId(other), l);
				debug->drawLine(At(&ctx, gB.x, gB.y), At(&ctx, (float)p.x, (float)p.y), rope);
			}
		}
	}

	void Physics2DWorld::SetGravity(const Vec2 &g)
	{
		gravity = g;
		if (live)
		{
			b2Vec2 v; v.x = g.x; v.y = g.y;
			b2World_SetGravity(MakeWorldId(worldIndex, worldGeneration), v);
		}
	}

	void Physics2DWorld::DestroyBodyFor(Physics2D* p)
	{
		if (!p || !p->HaveBody()) return;
		b2DestroyBody(MakeBodyId(p));
		p->ClearBodyHandle();
	}

	namespace {

		// An object's angle about z IN THE WORLD. Its own rotation.z is
		// relative to its parent, and a body lives in the world: a platform
		// parented under a turned object was built unturned.
		f32 WorldAngleOf(GameObject* go)
		{
			const Matrix &w = go->GetWorldTransformation();
			return atan2f(w.m[1], w.m[0]);
		}

		b2JointId MakeJointId(const Joint2D* j)
		{
			b2JointId id;
			id.index1 = j->GetJointIndex();
			id.world0 = j->GetJointWorld();
			id.generation = j->GetJointGeneration();
			return id;
		}

		b2Filter FilterOf(const Physics2D* p)
		{
			b2Filter f = b2DefaultFilter();
			f.categoryBits = (uint64_t)1 << (p->GetLayer() & 31u);
			f.maskBits = (uint64_t)p->GetMask();
			return f;
		}

		Physics2D* OwnerOfShape(const b2ShapeId shape)
		{
			return (Physics2D*)b2Body_GetUserData(b2Shape_GetBody(shape));
		}

		// One-way platforms. Called by the solver for a contact either of
		// whose shapes asked for it, possibly from several threads at once -
		// so it only reads, and only things that do not change during a step.
		//
		// The manifold normal points from shape A to shape B. A platform is
		// solid to a body that is ABOVE it; anything else passes. "Above" is
		// the normal pointing up out of the platform - and not already deep
		// inside it, which is where a body rising through from below is at
		// the moment the normal first turns upward. Without the depth test
		// the platform grabs it there and pops it up onto the top.
		void PreSolveOneWay(b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Manifold* manifold, void*)
		{
			const Physics2D* a = OwnerOfShape(shapeIdA);
			const Physics2D* b = OwnerOfShape(shapeIdB);
			const bool oneA = a && a->IsOneWay();
			const bool oneB = b && b->IsOneWay();
			if (!oneA && !oneB) return;
			const float up = oneA ? manifold->normal.y : -manifold->normal.y;
			bool solid = up > 0.5f;
			for (int k = 0; solid && k < manifold->pointCount; k++)
				if (manifold->points[k].separation < -0.06f) solid = false;
			if (!solid) manifold->pointCount = 0;
		}

		bool PreContinuousOneWay(b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Pos, b2Vec2 normal, void*)
		{
			const Physics2D* a = OwnerOfShape(shapeIdA);
			const Physics2D* b = OwnerOfShape(shapeIdB);
			const bool oneA = a && a->IsOneWay();
			const bool oneB = b && b->IsOneWay();
			if (!oneA && !oneB) return true;
			return (oneA ? normal.y : -normal.y) > 0.5f;
		}

		void CollectBodiesAndJoints(GameObject* go, std::vector<Physics2D*> &bodies, std::vector<Joint2D*> &joints)
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t c = 0; c < comps.size(); c++)
			{
				if (!comps[c]) continue;
				const uint32 t = comps[c]->GetComponentType();
				if (t == ComponentType::Physics2D) bodies.push_back(static_cast<Physics2D*>(comps[c].get()));
				else if (t == ComponentType::Joint2D) joints.push_back(static_cast<Joint2D*>(comps[c].get()));
			}
		}

		// The body a joint on `go` ties: go's own, or - go having none - the
		// nearest above it. An object holds one joint, so a wheel that is
		// hinged AND geared, or a crate on two ropes, carries the second on
		// a child; the child has no body of its own and means its parent's.
		Physics2D* BodyOf(GameObject* go)
		{
			for (GameObject* at = go; at != NULL; at = at->GetParent())
			{
				const std::vector<std::shared_ptr<IComponent> > &comps = at->GetComponents();
				for (size_t c = 0; c < comps.size(); c++)
					if (comps[c] && comps[c]->GetComponentType() == ComponentType::Physics2D)
						return static_cast<Physics2D*>(comps[c].get());
			}
			return NULL;
		}

	}

	void Physics2DWorld::BuildBody(Physics2D* p, const void* worldHandle)
	{
		const b2WorldId world = *(const b2WorldId*)worldHandle;
		GameObject* owner = p->GetOwner();

		const Vec3 pos = owner->GetWorldPosition();
		const f32 angle = WorldAngleOf(owner);
		b2BodyDef bd = b2DefaultBodyDef();
		bd.type = TranslateBodyType(p->GetBodyType());
		bd.position.x = pos.x;
		bd.position.y = pos.y;
		// The owner's rotation, at CREATION. Only the write-back in
		// PullTransforms carried this, and that is the editor's authoring
		// path - it does not run in Play. So a rotated body was built
		// axis-aligned and, if static, stayed that way for the whole run:
		// an angled platform collided as a flat one, in the game but not
		// in the editor. Radians, as everywhere else in the engine.
		bd.rotation = b2MakeRot(angle);
		// v3 expresses this as a per-axis motion lock rather than one flag.
		bd.motionLocks.angularZ = p->IsFixedRotation();
		bd.gravityScale = p->GetGravityScale();
		bd.linearDamping = p->GetLinearDamping();
		bd.angularDamping = p->GetAngularDamping();
		bd.isBullet = p->IsBullet();
		// What lets a contact event find its way back to a component:
		// events carry shape ids, a shape knows its body, and a body
		// carries this.
		bd.userData = p;
		b2BodyId body = b2CreateBody(world, &bd);

		b2ShapeDef sd = b2DefaultShapeDef();
		// Off by default in v3; without it the contact event buffers stay
		// empty and OnCollisionEnter/Exit simply never fire.
		sd.enableContactEvents = true;
		// For OnContact's closing speed - reported only above the world's
		// hit threshold, so resting contact does not chatter.
		sd.enableHitEvents = true;
		// So a sensor notices it. (A sensor does not notice other sensors.)
		sd.enableSensorEvents = true;
		sd.isSensor = p->IsSensor();
		// The solver asks PreSolveOneWay about every contact with this body.
		sd.enablePreSolveEvents = p->IsOneWay();
		sd.filter = FilterOf(p);
		sd.density = p->GetDensity();
		sd.material.friction = p->GetFriction();
		sd.material.restitution = p->GetRestitution();

		const Vec2 off = p->GetOffset();
		b2Vec2 center;
		center.x = off.x;
		center.y = off.y;

		if (p->IsCompound())
		{
			// Many shapes on one body. A tiled floor merges to a handful
			// of rectangles, so this is single digits for a level that
			// would otherwise be thousands of bodies - see
			// Physics2D::SetCompoundBoxes.
			const std::vector<Vec4> &boxes = p->GetCompoundBoxes();
			for (size_t b = 0; b < boxes.size(); b++)
			{
				b2Vec2 c;
				c.x = boxes[b].x;
				c.y = boxes[b].y;
				b2Polygon box = b2MakeOffsetBox(boxes[b].z, boxes[b].w, c, b2MakeRot(0.f));
				b2CreatePolygonShape(body, &sd, &box);
			}
			// Arbitrary convex pieces on the same body - tile slopes are
			// triangles. Kept separate from the boxes because the merge
			// above only produces rectangles, and a slope has to survive
			// it as its own shape.
			const std::vector<Physics2D::Poly2D> &polys = p->GetCompoundPolys();
			for (size_t q = 0; q < polys.size(); q++)
			{
				const Physics2D::Poly2D &poly = polys[q];
				// B2_MAX_POLYGON_VERTICES is 8; anything longer is an
				// authoring mistake, and silently truncating would build a
				// collider that does not match the art.
				if (poly.size() < 3 || poly.size() > 8) continue;
				b2Vec2 pts[8];
				for (size_t k = 0; k < poly.size(); k++)
				{ pts[k].x = poly[k].x; pts[k].y = poly[k].y; }
				// b2ComputeHull both orders the points and rejects
				// degenerate input - a zero-area triangle here would be an
				// assert inside Box2D rather than a missing collider.
				b2Hull hull = b2ComputeHull(pts, (int)poly.size());
				if (hull.count < 3) continue;
				b2Polygon pg = b2MakePolygon(&hull, 0.f);
				b2CreatePolygonShape(body, &sd, &pg);
			}
			// Chain loops: a continuous one-sided surface with no internal
			// faces. Box2D needs 4+ points and a static body, both of
			// which terrain is.
			const std::vector<Physics2D::Poly2D> &chains = p->GetCompoundChains();
			for (size_t q = 0; q < chains.size(); q++)
			{
				const Physics2D::Poly2D &loop = chains[q];
				if (loop.size() < 4) continue;
				// Winding is load-bearing and the convention is exact:
				// b2CollideChainSegment* accepts a contact only when
				// dot(b2RightPerp(p2 - p1), pB - p1) > 0, and
				// b2RightPerp(v) is (v.y, -v.x). For a loop wound
				// counter-clockwise around solid material that perpendicular
				// points OUTWARD on every edge - bottom edge left-to-right
				// gives (0,-1), top edge right-to-left gives (0,+1) - which
				// is exactly "solid on the inside, collide from outside".
				// BuildColliderChains already winds CCW, so it is fed
				// through unchanged. Reversing it collides from the inside
				// instead, which reads as bodies falling through the floor
				// and landing on the underside of the world.
				std::vector<b2Vec2> pts(loop.size());
				for (size_t k = 0; k < loop.size(); k++)
				{ pts[k].x = loop[k].x; pts[k].y = loop[k].y; }
				b2ChainDef cd = b2DefaultChainDef();
				cd.points = &pts[0];
				cd.count = (int)pts.size();
				cd.isLoop = true;
				cd.filter = sd.filter;
				cd.enableSensorEvents = true;
				b2SurfaceMaterial mat = b2DefaultSurfaceMaterial();
				mat.friction = p->GetFriction();
				mat.restitution = p->GetRestitution();
				cd.materials = &mat;
				cd.materialCount = 1;
				b2CreateChain(body, &cd);
			}
		}
		else if (p->GetShapeType() == Shape2DType::Circle)
		{
			b2Circle circle;
			circle.center = center;
			circle.radius = p->GetSize().x;
			b2CreateCircleShape(body, &sd, &circle);
		}
		else if (p->GetShapeType() == Shape2DType::Capsule)
		{
			// size.y is the half-height including the caps, so the segment
			// between the two cap centres is what is left after the radius.
			// A capsule no taller than it is wide is a circle.
			const f32 r = p->GetSize().x;
			const f32 half = p->GetSize().y - r;
			if (half <= 1e-4f)
			{
				b2Circle circle;
				circle.center = center;
				circle.radius = r;
				b2CreateCircleShape(body, &sd, &circle);
			}
			else
			{
				b2Capsule cap;
				cap.center1.x = center.x; cap.center1.y = center.y - half;
				cap.center2.x = center.x; cap.center2.y = center.y + half;
				cap.radius = r;
				b2CreateCapsuleShape(body, &sd, &cap);
			}
		}
		else if (p->GetShapeType() == Shape2DType::Polygon && p->GetPolygon().size() >= 3)
		{
			// One shape for an outline the solver can take whole, convex
			// pieces for one it cannot (see Physics2D::DecomposePolygon).
			std::vector<std::vector<Vec2> > pieces;
			Physics2D::DecomposePolygon(p->GetPolygon(), pieces);
			for (size_t q = 0; q < pieces.size(); q++)
			{
				b2Vec2 pts[8];
				const int n = (int)(pieces[q].size() > 8 ? 8 : pieces[q].size());
				for (int k = 0; k < n; k++) { pts[k].x = pieces[q][(size_t)k].x + off.x; pts[k].y = pieces[q][(size_t)k].y + off.y; }
				b2Hull hull = b2ComputeHull(pts, n);
				if (hull.count >= 3)
				{
					b2Polygon pg = b2MakePolygon(&hull, 0.f);
					b2CreatePolygonShape(body, &sd, &pg);
				}
			}
		}
		else if (p->GetShapeType() == Shape2DType::Chain && p->GetPolygon().size() >= 2)
		{
			std::vector<b2Vec2> pts(p->GetPolygon().size());
			for (size_t k = 0; k < pts.size(); k++) { pts[k].x = p->GetPolygon()[k].x + off.x; pts[k].y = p->GetPolygon()[k].y + off.y; }
			b2SurfaceMaterial mat = b2DefaultSurfaceMaterial();
			mat.friction = p->GetFriction();
			mat.restitution = p->GetRestitution();
			const bool loop = p->IsChainLoop() && pts.size() >= 3;
			// A chain is smooth where its edges meet - nothing catches on a
			// joint - and one-sided: it stops what comes at the right of its
			// direction (see the tile colliders above).
			b2ChainDef cd = b2DefaultChainDef();
			cd.filter = sd.filter;
			cd.enableSensorEvents = true;
			cd.materials = &mat;
			cd.materialCount = 1;
			if (loop && pts.size() >= 4)
			{
				// A loop is the skin of something solid, so it is wound
				// counter-clockwise, whichever way it was drawn, and stops
				// what is outside.
				f32 area = 0.f;
				for (size_t k = 0; k < pts.size(); k++)
				{
					const b2Vec2 &u = pts[k], &v = pts[(k + 1) % pts.size()];
					area += u.x * v.y - v.x * u.y;
				}
				if (area < 0.f) std::reverse(pts.begin(), pts.end());
				cd.points = &pts[0];
				cd.count = (int)pts.size();
				cd.isLoop = true;
				b2CreateChain(body, &cd);
			}
			else if (loop)
			{
				// A triangle: too few points for the solver's loop. Plain
				// edges, which stop things from both sides.
				for (size_t k = 0; k < pts.size(); k++)
				{
					b2Segment seg;
					seg.point1 = pts[k];
					seg.point2 = pts[(k + 1) % pts.size()];
					b2CreateSegmentShape(body, &sd, &seg);
				}
			}
			else
			{
				// An open line. The solver takes a chain's first and last
				// points as guides for the smoothing and makes no edge to
				// them, so one is added past each end, in line with it:
				// every edge that was drawn is then one that collides.
				// And it has no inside to be on - it is laid both ways, and
				// stops things from either side.
				std::vector<b2Vec2> padded;
				padded.reserve(pts.size() + 2);
				b2Vec2 lead; lead.x = 2.f * pts[0].x - pts[1].x; lead.y = 2.f * pts[0].y - pts[1].y;
				const size_t n = pts.size();
				b2Vec2 tail; tail.x = 2.f * pts[n - 1].x - pts[n - 2].x; tail.y = 2.f * pts[n - 1].y - pts[n - 2].y;
				padded.push_back(lead);
				padded.insert(padded.end(), pts.begin(), pts.end());
				padded.push_back(tail);
				cd.isLoop = false;
				cd.count = (int)padded.size();
				cd.points = &padded[0];
				b2CreateChain(body, &cd);
				std::vector<b2Vec2> back(padded.rbegin(), padded.rend());
				cd.points = &back[0];
				b2CreateChain(body, &cd);
			}
		}
		else
		{
			b2Polygon box = b2MakeOffsetBox(p->GetSize().x, p->GetSize().y, center, b2MakeRot(0.f));
			b2CreatePolygonShape(body, &sd, &box);
		}

		p->ClearShapeRebuild();
		p->SetBodyHandle(body.index1, body.world0, body.generation);
		p->syncedPosition = pos;
		p->syncedAngle = angle;
		p->haveSynced = true;
	}

	void Physics2DWorld::Sync(SceneGraph* Scene)
	{
		if (Scene == NULL) return;

		// Collect this frame's components first, so a scene with none never
		// creates a world at all.
		std::vector<Physics2D*> found;
		std::vector<Joint2D*> foundJoints;
		// Recursive, not GetAllGameObjectList(): a body parented under a
		// Layer2D root is a child, and the flat list does not contain it.
		std::vector<GameObject*> all;
		// (with no 2D body anywhere there is nothing to find: the scene is
		// not gone through - see Physics2D::LiveCount)
		if (Physics2D::LiveCount() > 0) Scene->CollectGameObjectsRecursive(all);
		for (size_t i = 0; i < all.size(); i++)
			if (all[i]) CollectBodiesAndJoints(all[i], found, foundJoints);
		if (found.empty() && !live) return;

		if (!live)
		{
			b2WorldDef def = b2DefaultWorldDef();
			def.gravity.x = gravity.x;
			def.gravity.y = gravity.y;
			def.workerCount = JobSystem::Instance().WorkerCount() + 1;
			def.enqueueTask = EnqueuePhysicsTask;
			def.finishTask = FinishPhysicsTask;
			def.userTaskContext = NULL;
			b2WorldId w = b2CreateWorld(&def);
			worldIndex = w.index1;
			worldGeneration = w.generation;
			live = true;
			haveGround = false;
			b2World_SetPreSolveCallback(w, PreSolveOneWay, PreContinuousOneWay, NULL);
		}

		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);

		for (size_t i = 0; i < found.size(); i++)
		{
			Physics2D* p = found[i];
			if (p->GetOwner() == NULL) continue;
			// A body whose shape changed is destroyed and rebuilt below
			// rather than edited in place: Box2D has no "replace every
			// shape" call, and removing them one at a time costs the same
			// as a fresh body. Done HERE, at the frame boundary, never
			// mid-step. What it was doing is carried across, so resizing a
			// moving thing does not stop it dead.
			b2Vec2 keepLinear = { 0.f, 0.f };
			float keepAngular = 0.f;
			bool keep = false;
			if (p->HaveBody() && p->NeedsShapeRebuild())
			{
				const b2BodyId old = MakeBodyId(p);
				if (b2Body_IsValid(old))
				{
					keepLinear = b2Body_GetLinearVelocity(old);
					keepAngular = b2Body_GetAngularVelocity(old);
					keep = true;
				}
				DestroyBodyFor(p);
			}
			if (p->HaveBody()) continue;

			BuildBody(p, &world);
			if (keep && p->GetBodyType() != Body2DType::Static)
			{
				b2Body_SetLinearVelocity(MakeBodyId(p), keepLinear);
				b2Body_SetAngularVelocity(MakeBodyId(p), keepAngular);
			}
		}

		// Anything tracked last frame but gone now takes its body with it.
		// Asked of the handle remembered here and never of the component,
		// which may have been freed - and "gone" includes a component that
		// is at the same address but is not the one that owned this body
		// (see trackedBodyIds): its handle is then a different one, or none.
		{
			std::unordered_map<const Physics2D*, const Physics2D*> now;
			now.reserve(found.size());
			for (size_t j = 0; j < found.size(); j++) now[found[j]] = found[j];
			for (size_t i = 0; i < tracked.size() && i < trackedBodyIds.size(); i++)
			{
				const JointHandle &h = trackedBodyIds[i];
				if (h.index == 0) continue;
				b2BodyId id;
				id.index1 = h.index;
				id.world0 = h.world;
				id.generation = h.generation;
				if (!b2Body_IsValid(id)) continue;          // already destroyed (a rebuild)
				std::unordered_map<const Physics2D*, const Physics2D*>::const_iterator at = now.find(tracked[i]);
				const bool owned = at != now.end() && at->second->HaveBody()
					&& at->second->GetBodyIndex() == h.index && at->second->GetBodyWorld() == h.world
					&& at->second->GetBodyGeneration() == h.generation;
				if (!owned) b2DestroyBody(id);
			}
		}
		tracked = found;
		trackedBodyIds.resize(found.size());
		for (size_t i = 0; i < found.size(); i++)
		{
			trackedBodyIds[i].index = found[i]->HaveBody() ? found[i]->GetBodyIndex() : 0;
			trackedBodyIds[i].world = found[i]->GetBodyWorld();
			trackedBodyIds[i].generation = found[i]->GetBodyGeneration();
		}

		// --- joints -----------------------------------------------------------
		// After the bodies: a joint needs both of its to exist. One whose
		// body was just rebuilt went with it (destroying a body destroys
		// what was attached), so validity is asked of the solver rather
		// than assumed from the handle.
		for (size_t i = 0; i < trackedJoints.size(); i++)
		{
			bool stillHere = false;
			for (size_t j = 0; j < foundJoints.size() && !stillHere; j++)
				if (foundJoints[j] == trackedJoints[i]) stillHere = true;
			if (stillHere || !trackedJoints[i]) continue;
			// The component is gone - and may have been freed. Only the
			// solver's side is touched, through the handle remembered here.
			b2JointId id;
			id.index1 = trackedJointIds[i].index;
			id.world0 = trackedJointIds[i].world;
			id.generation = trackedJointIds[i].generation;
			if (b2Joint_IsValid(id)) b2DestroyJoint(id);
		}
		trackedJoints = foundJoints;
		trackedJointIds.resize(foundJoints.size());

		for (size_t i = 0; i < foundJoints.size(); i++)
		{
			Joint2D* j = foundJoints[i];
			trackedJointIds[i].index = 0; trackedJointIds[i].world = 0; trackedJointIds[i].generation = 0;
			if (j->GetOwner() == NULL) continue;
			if (j->IsSoft())
			{
				// Not the solver's: see SolveSoftJoints. One that was a
				// solver joint until its kind was changed gives that up.
				if (j->HaveJoint())
				{
					if (b2Joint_IsValid(MakeJointId(j))) b2DestroyJoint(MakeJointId(j));
					j->ClearJointHandle();
				}
				if (j->NeedsRebuild()) { j->ClearSoft(); j->ClearRebuild(); }
				continue;
			}
			if (j->HaveJoint() && !b2Joint_IsValid(MakeJointId(j))) j->ClearJointHandle();
			if (j->HaveJoint() && j->NeedsRebuild())
			{
				b2DestroyJoint(MakeJointId(j));
				j->ClearJointHandle();
			}
			if (!j->HaveJoint()) BuildJoint(j, found, &world);
			if (j->HaveJoint())
			{
				trackedJointIds[i].index = j->GetJointIndex();
				trackedJointIds[i].world = j->GetJointWorld();
				trackedJointIds[i].generation = j->GetJointGeneration();
			}
		}
	}

	void Physics2DWorld::BuildJoint(Joint2D* j, const std::vector<Physics2D*> &bodies, const void* worldHandle)
	{
		const b2WorldId world = *(const b2WorldId*)worldHandle;
		Physics2D* mine = BodyOf(j->GetOwner());
		// No body of its own yet: nothing to tie. Tried again next frame.
		if (!mine || !mine->HaveBody()) return;
		const b2BodyId bodyB = MakeBodyId(mine);

		b2BodyId bodyA;
		if (j->GetConnected().empty())
		{
			// Tied to the world: a static body at the origin that exists
			// only to be the other end.
			if (!haveGround)
			{
				b2BodyDef gd = b2DefaultBodyDef();
				b2BodyId g = b2CreateBody(world, &gd);
				groundIndex = g.index1;
				groundWorld = g.world0;
				groundGeneration = g.generation;
				haveGround = true;
			}
			bodyA.index1 = groundIndex;
			bodyA.world0 = groundWorld;
			bodyA.generation = groundGeneration;
		}
		else
		{
			Physics2D* other = NULL;
			for (size_t i = 0; i < bodies.size() && !other; i++)
				if (bodies[i]->GetOwner() && bodies[i] != mine && bodies[i]->GetOwner()->GetName() == j->GetConnected())
					other = bodies[i];
			// Named something that is not there, or has no body. Left
			// unbuilt and dirty: it is tried again, so an object spawned
			// later is picked up, and a wrong name stays visibly unjoined.
			if (!other || !other->HaveBody()) return;
			bodyA = MakeBodyId(other);
		}

		// The pin, in the world as things stand now.
		b2Vec2 localB;
		localB.x = j->GetAnchor().x;
		localB.y = j->GetAnchor().y;
		const b2Pos worldAnchor = b2Body_GetWorldPoint(bodyB, localB);
		// What "zero" means for this joint - where the other end is and how
		// the two bodies were turned - is measured ONCE, when it is first
		// made. A joint is rebuilt whenever a setting changes (turning its
		// motor on, say), and measuring again then would move its limits to
		// wherever the bodies happened to have got to: a lift that had sunk
		// to its lower stop would take that as its new middle.
		if (!j->HaveReference())
		{
			Vec2 refA;
			if (j->HasConnectedAnchor()) refA = j->GetConnectedAnchor();
			// A rod or a rope runs BETWEEN two things: with no far end
			// given it is the other body's own origin. (The pin every
			// other joint defaults to is this body's anchor, which for a
			// distance is a rope of no length tied to where it already is.)
			else if (j->GetJointType() == Joint2DType::Distance && !j->GetConnected().empty()) refA = Vec2(0.f, 0.f);
			else
			{
				const b2Vec2 la = b2Body_GetLocalPoint(bodyA, worldAnchor);
				refA = Vec2(la.x, la.y);
			}
			j->SetReference(refA, b2Rot_GetAngle(b2Body_GetRotation(bodyA)), b2Rot_GetAngle(b2Body_GetRotation(bodyB)));
		}
		b2Vec2 localA;
		localA.x = j->GetReferenceAnchor().x;
		localA.y = j->GetReferenceAnchor().y;
		const float angleA = j->GetReferenceAngleA();
		const float angleB = j->GetReferenceAngleB();

		// Frames set so the joint reads ZERO as it is made: a hinge's limits
		// and a slider's travel are then measured from the authored pose,
		// which is the only meaning they can have to whoever typed them.
		b2JointDef base = b2DefaultRevoluteJointDef().base;
		base.bodyIdA = bodyA;
		base.bodyIdB = bodyB;
		base.localFrameA.p = localA;
		base.localFrameB.p = localB;
		base.localFrameA.q = b2MakeRot(angleB - angleA);
		base.localFrameB.q = b2MakeRot(0.f);
		base.collideConnected = j->GetCollideConnected();
		base.userData = j;

		// A Slider's and a Wheel's line, authored in the world: each body's
		// frame is turned so its x axis lies along it.
		const float axisAngle = atan2f(j->GetAxis().y, j->GetAxis().x);

		b2JointId id;
		switch (j->GetJointType())
		{
		case Joint2DType::Target:
		{
			// A motor joint used only as a spring on position: frame B (the
			// anchor on this body) is drawn to frame A, which is the target
			// point on the ground body - or, connected, the anchor on the
			// other body. Rotation is left alone.
			b2MotorJointDef d = b2DefaultMotorJointDef();
			d.base = base;
			if (j->GetConnected().empty())
			{
				// Until a target is given, it holds the body where it is.
				const Vec2 t = j->HaveTarget() ? j->GetTarget() : Vec2((f32)worldAnchor.x, (f32)worldAnchor.y);
				d.base.localFrameA.p.x = t.x;
				d.base.localFrameA.p.y = t.y;
			}
			d.base.localFrameA.q = b2MakeRot(0.f);
			d.linearHertz = j->IsSpringEnabled() ? j->GetSpringHertz() : 5.f;
			d.linearDampingRatio = j->IsSpringEnabled() ? j->GetSpringDamping() : 0.7f;
			d.maxSpringForce = j->GetMaxMotorForce() > 0.f ? j->GetMaxMotorForce() : 100.f;
			d.maxVelocityForce = 0.f;
			d.maxVelocityTorque = 0.f;
			d.angularHertz = 0.f;
			d.maxSpringTorque = 0.f;
			id = b2CreateMotorJoint(world, &d);
			break;
		}
		case Joint2DType::Ignore:
		{
			b2FilterJointDef d = b2DefaultFilterJointDef();
			d.base = base;
			d.base.collideConnected = false;
			id = b2CreateFilterJoint(world, &d);
			break;
		}
		case Joint2DType::Distance:
		{
			b2DistanceJointDef d = b2DefaultDistanceJointDef();
			d.base = base;
			const b2Pos wa = b2Body_GetWorldPoint(bodyA, localA);
			const float dx = (float)(worldAnchor.x - wa.x), dy = (float)(worldAnchor.y - wa.y);
			const float now = sqrtf(dx * dx + dy * dy);
			d.length = j->GetLength() > 0.f ? j->GetLength() : (now > 0.005f ? now : 0.005f);
			d.enableSpring = j->IsSpringEnabled();
			d.hertz = j->GetSpringHertz();
			d.dampingRatio = j->GetSpringDamping();
			d.enableLimit = j->IsLimitEnabled();
			d.minLength = j->GetLowerLimit() > 0.f ? j->GetLowerLimit() : 0.f;
			d.maxLength = j->GetUpperLimit() > d.minLength ? j->GetUpperLimit() : d.length;
			// A rope: free to go slack, never longer than its limit. That is
			// a spring with no stiffness under a maximum length.
			if (d.enableLimit && !d.enableSpring) { d.enableSpring = true; d.hertz = 0.f; d.dampingRatio = 0.f; }
			d.enableMotor = j->IsMotorEnabled();
			d.motorSpeed = j->GetMotorSpeed();
			d.maxMotorForce = j->GetMaxMotorForce();
			id = b2CreateDistanceJoint(world, &d);
			break;
		}
		case Joint2DType::Weld:
		{
			b2WeldJointDef d = b2DefaultWeldJointDef();
			d.base = base;
			if (j->IsSpringEnabled())
			{
				d.linearHertz = j->GetSpringHertz();
				d.angularHertz = j->GetSpringHertz();
				d.linearDampingRatio = j->GetSpringDamping();
				d.angularDampingRatio = j->GetSpringDamping();
			}
			id = b2CreateWeldJoint(world, &d);
			break;
		}
		case Joint2DType::Slider:
		{
			b2PrismaticJointDef d = b2DefaultPrismaticJointDef();
			d.base = base;
			d.base.localFrameA.q = b2MakeRot(axisAngle - angleA);
			d.base.localFrameB.q = b2MakeRot(axisAngle - angleB);
			d.enableSpring = j->IsSpringEnabled();
			d.hertz = j->GetSpringHertz();
			d.dampingRatio = j->GetSpringDamping();
			d.enableLimit = j->IsLimitEnabled();
			d.lowerTranslation = j->GetLowerLimit();
			d.upperTranslation = j->GetUpperLimit();
			d.enableMotor = j->IsMotorEnabled();
			d.motorSpeed = j->GetMotorSpeed();
			d.maxMotorForce = j->GetMaxMotorForce();
			id = b2CreatePrismaticJoint(world, &d);
			break;
		}
		case Joint2DType::Wheel:
		{
			b2WheelJointDef d = b2DefaultWheelJointDef();
			d.base = base;
			d.base.localFrameA.q = b2MakeRot(axisAngle - angleA);
			d.base.localFrameB.q = b2MakeRot(axisAngle - angleB);
			d.enableSpring = j->IsSpringEnabled();
			d.hertz = j->GetSpringHertz();
			d.dampingRatio = j->GetSpringDamping();
			d.enableLimit = j->IsLimitEnabled();
			d.lowerTranslation = j->GetLowerLimit();
			d.upperTranslation = j->GetUpperLimit();
			d.enableMotor = j->IsMotorEnabled();
			d.motorSpeed = j->GetMotorSpeed();
			d.maxMotorTorque = j->GetMaxMotorForce();
			id = b2CreateWheelJoint(world, &d);
			break;
		}
		default:
		{
			b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
			d.base = base;
			d.enableSpring = j->IsSpringEnabled();
			d.hertz = j->GetSpringHertz();
			d.dampingRatio = j->GetSpringDamping();
			d.enableLimit = j->IsLimitEnabled();
			d.lowerAngle = j->GetLowerLimit();
			d.upperAngle = j->GetUpperLimit();
			d.enableMotor = j->IsMotorEnabled();
			d.motorSpeed = j->GetMotorSpeed();
			d.maxMotorTorque = j->GetMaxMotorForce();
			id = b2CreateRevoluteJoint(world, &d);
			break;
		}
		}

		j->SetJointHandle(id.index1, id.world0, id.generation);
		j->ClearRebuild();
	}

	void Physics2DWorld::DispatchContacts(const void* worldHandle)
	{
		const b2WorldId world = *(const b2WorldId*)worldHandle;
		const b2ContactEvents events = b2World_GetContactEvents(world);

		// Everything is gathered before anything is called. A handler is
		// free to destroy a body, and the event arrays belong to the solver:
		// walking them while a handler edits the world reads freed shapes.
		struct Touch { Physics2D* a; Physics2D* b; bool have; Vec2 point; Vec2 normal; f32 speed; };
		std::vector<Touch> begins, ends;
		std::vector<std::pair<Physics2D*, Physics2D*> > enters, exits;

		for (int i = 0; i < events.beginCount; i++)
		{
			// Guarded like the end events below: a begin event can also name
			// a shape destroyed later in the same step.
			Touch t;
			t.a = b2Shape_IsValid(events.beginEvents[i].shapeIdA) ? OwnerOfShape(events.beginEvents[i].shapeIdA) : NULL;
			t.b = b2Shape_IsValid(events.beginEvents[i].shapeIdB) ? OwnerOfShape(events.beginEvents[i].shapeIdB) : NULL;
			t.have = false;
			t.speed = 0.f;
			if (t.a && t.b && b2Contact_IsValid(events.beginEvents[i].contactId))
			{
				const b2ContactData data = b2Contact_GetData(events.beginEvents[i].contactId);
				if (data.manifold.pointCount > 0)
				{
					const b2Pos wp = b2OffsetPos(b2Body_GetWorldCenter(b2Shape_GetBody(data.shapeIdA)),
						data.manifold.points[0].anchorA);
					t.point = Vec2((f32)wp.x, (f32)wp.y);
					// From A towards B, as the solver has it.
					t.normal = Vec2(data.manifold.normal.x, data.manifold.normal.y);
					for (int k = 0; k < data.manifold.pointCount; k++)
						if (-data.manifold.points[k].normalVelocity > t.speed) t.speed = -data.manifold.points[k].normalVelocity;
					t.have = true;
				}
			}
			begins.push_back(t);
		}
		for (int i = 0; i < events.endCount; i++)
		{
			// An end event can name a shape that has since been destroyed -
			// b2Shape_IsValid is the documented guard and skipping it turns a
			// body deleted mid-collision into a crash.
			Touch t;
			t.a = b2Shape_IsValid(events.endEvents[i].shapeIdA) ? OwnerOfShape(events.endEvents[i].shapeIdA) : NULL;
			t.b = b2Shape_IsValid(events.endEvents[i].shapeIdB) ? OwnerOfShape(events.endEvents[i].shapeIdB) : NULL;
			t.have = false;
			t.speed = 0.f;
			ends.push_back(t);
		}

		const b2SensorEvents sensors = b2World_GetSensorEvents(world);
		for (int i = 0; i < sensors.beginCount; i++)
		{
			if (!b2Shape_IsValid(sensors.beginEvents[i].sensorShapeId) || !b2Shape_IsValid(sensors.beginEvents[i].visitorShapeId)) continue;
			enters.push_back(std::make_pair(OwnerOfShape(sensors.beginEvents[i].sensorShapeId), OwnerOfShape(sensors.beginEvents[i].visitorShapeId)));
		}
		for (int i = 0; i < sensors.endCount; i++)
		{
			Physics2D* s = b2Shape_IsValid(sensors.endEvents[i].sensorShapeId) ? OwnerOfShape(sensors.endEvents[i].sensorShapeId) : NULL;
			Physics2D* v = b2Shape_IsValid(sensors.endEvents[i].visitorShapeId) ? OwnerOfShape(sensors.endEvents[i].visitorShapeId) : NULL;
			exits.push_back(std::make_pair(s, v));
		}

		// A handler may remove a component that a later event names. Each is
		// checked against what is still tracked before it is called.
		auto alive = [this](Physics2D* p) {
			if (!p) return false;
			for (size_t i = 0; i < tracked.size(); i++) if (tracked[i] == p) return true;
			return false;
		};

		// Both sides get told, because "did I hit something" is the question
		// a script actually asks and neither body is privileged.
		for (size_t i = 0; i < begins.size(); i++)
		{
			const Touch &t = begins[i];
			if (alive(t.a) && t.a->OnCollisionEnter) t.a->OnCollisionEnter(alive(t.b) ? t.b : NULL);
			if (alive(t.b) && t.b->OnCollisionEnter) t.b->OnCollisionEnter(alive(t.a) ? t.a : NULL);
			if (!t.have) continue;
			// The normal each side is given points from the other body to it.
			if (alive(t.a) && t.a->OnContact) t.a->OnContact(alive(t.b) ? t.b : NULL, t.point, Vec2(-t.normal.x, -t.normal.y), t.speed);
			if (alive(t.b) && t.b->OnContact) t.b->OnContact(alive(t.a) ? t.a : NULL, t.point, t.normal, t.speed);
		}
		for (size_t i = 0; i < ends.size(); i++)
		{
			const Touch &t = ends[i];
			if (alive(t.a) && t.a->OnCollisionExit) t.a->OnCollisionExit(alive(t.b) ? t.b : NULL);
			if (alive(t.b) && t.b->OnCollisionExit) t.b->OnCollisionExit(alive(t.a) ? t.a : NULL);
		}
		for (size_t i = 0; i < enters.size(); i++)
		{
			Physics2D* s = enters[i].first; Physics2D* v = enters[i].second;
			if (alive(s) && s->OnTriggerEnter) s->OnTriggerEnter(alive(v) ? v : NULL);
			if (alive(v) && v->OnTriggerEnter) v->OnTriggerEnter(alive(s) ? s : NULL);
		}
		for (size_t i = 0; i < exits.size(); i++)
		{
			Physics2D* s = exits[i].first; Physics2D* v = exits[i].second;
			if (alive(s) && s->OnTriggerExit) s->OnTriggerExit(alive(v) ? v : NULL);
			if (alive(v) && v->OnTriggerExit) v->OnTriggerExit(alive(s) ? s : NULL);
		}
	}

	// --- queries ------------------------------------------------------------------

	namespace {
		b2QueryFilter QueryFilterFor(const uint32 mask)
		{
			b2QueryFilter f = b2DefaultQueryFilter();
			// A query is on every layer and sees the ones asked for.
			f.categoryBits = UINT64_MAX;
			f.maskBits = (uint64_t)mask;
			return f;
		}

		struct RayFilterCtx {
			const Physics2D* ignore;
			const Physics2D* ignore2;
			b2ShapeId shape;
			b2Pos point;
			b2Vec2 normal;
			float fraction;
			bool hit;
		};

		// Returning the fraction clips the ray to the closest hit so far;
		// returning -1 tells Box2D to ignore this shape and keep going, which
		// is how `ignore` is honoured without a second query.
		float RayFilterFcn(b2ShapeId shapeId, b2Pos point, b2Vec2 normal,
			float fraction, void* context)
		{
			RayFilterCtx* c = (RayFilterCtx*)context;
			if (!b2Shape_IsValid(shapeId)) return -1.f;
			// A sensor is not a surface: a ground probe that stopped on a
			// coin's trigger would have the character standing in mid-air.
			if (b2Shape_IsSensor(shapeId)) return -1.f;
			if (c->ignore != NULL || c->ignore2 != NULL)
			{
				const Physics2D* owner = OwnerOfShape(shapeId);
				if (owner == c->ignore || owner == c->ignore2) return -1.f;
			}
			c->hit = true;
			c->shape = shapeId;
			c->point = point;
			c->normal = normal;
			c->fraction = fraction;
			return fraction;
		}

		struct RayAllCtx {
			const Physics2D* ignore;
			std::vector<Physics2DWorld::RayHit2D>* out;
		};

		// Returning 1 keeps the ray at full length, so every shape along it
		// is reported.
		float RayAllFcn(b2ShapeId shapeId, b2Pos point, b2Vec2 normal, float fraction, void* context)
		{
			RayAllCtx* c = (RayAllCtx*)context;
			if (!b2Shape_IsValid(shapeId) || b2Shape_IsSensor(shapeId)) return -1.f;
			Physics2D* owner = OwnerOfShape(shapeId);
			if (owner == NULL || owner == c->ignore) return -1.f;
			Physics2DWorld::RayHit2D h;
			h.hit = true;
			h.point = Vec2((f32)point.x, (f32)point.y);
			h.normal = Vec2(normal.x, normal.y);
			h.fraction = fraction;
			h.component = owner;
			c->out->push_back(h);
			return 1.f;
		}

		struct OverlapCtx {
			std::vector<Physics2D*>* out;
			bool sensors;
		};

		bool OverlapFcn(b2ShapeId shapeId, void* context)
		{
			OverlapCtx* c = (OverlapCtx*)context;
			if (!b2Shape_IsValid(shapeId)) return true;
			if (!c->sensors && b2Shape_IsSensor(shapeId)) return true;
			Physics2D* owner = OwnerOfShape(shapeId);
			// A body with several shapes is one answer, not several.
			if (owner && std::find(c->out->begin(), c->out->end(), owner) == c->out->end())
				c->out->push_back(owner);
			return true;
		}
	}

	Physics2DWorld::RayHit2D Physics2DWorld::RayCast(const Vec2 &from, const Vec2 &to,
		const Physics2D* ignore, const Physics2D* ignore2, const uint32 mask) const
	{
		RayHit2D out;
		// `live` is what says a world has been created at all; MakeWorldId on
		// an uninitialised index would hand Box2D a plausible-looking handle
		// to nothing.
		if (!live) return out;
		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);
		if (!b2World_IsValid(world)) return out;

		// Box2D takes an origin and a TRANSLATION, not two endpoints. Passing
		// `to` here casts a ray to twice the intended distance, which for a
		// ground sensor means finding floors that are not under you.
		b2Pos origin; origin.x = from.x; origin.y = from.y;
		b2Vec2 delta; delta.x = to.x - from.x; delta.y = to.y - from.y;
		if (delta.x == 0.f && delta.y == 0.f) return out;

		RayFilterCtx ctx;
		ctx.ignore = ignore;
		ctx.ignore2 = ignore2;
		ctx.fraction = 0.f;
		ctx.hit = false;
		b2World_CastRay(world, origin, delta, QueryFilterFor(mask), RayFilterFcn, &ctx);
		if (!ctx.hit) return out;

		out.hit = true;
		out.point = Vec2((f32)ctx.point.x, (f32)ctx.point.y);
		out.normal = Vec2(ctx.normal.x, ctx.normal.y);
		out.fraction = ctx.fraction;
		if (b2Shape_IsValid(ctx.shape)) out.component = OwnerOfShape(ctx.shape);
		return out;
	}

	std::vector<Physics2DWorld::RayHit2D> Physics2DWorld::RayCastAll(const Vec2 &from, const Vec2 &to,
		const Physics2D* ignore, const uint32 mask) const
	{
		std::vector<RayHit2D> out;
		if (!live) return out;
		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);
		if (!b2World_IsValid(world)) return out;
		b2Pos origin; origin.x = from.x; origin.y = from.y;
		b2Vec2 delta; delta.x = to.x - from.x; delta.y = to.y - from.y;
		if (delta.x == 0.f && delta.y == 0.f) return out;
		RayAllCtx ctx;
		ctx.ignore = ignore;
		ctx.out = &out;
		b2World_CastRay(world, origin, delta, QueryFilterFor(mask), RayAllFcn, &ctx);
		// Nearest first; the solver reports them in whatever order its tree
		// is walked.
		std::sort(out.begin(), out.end(), [](const RayHit2D &a, const RayHit2D &b) { return a.fraction < b.fraction; });
		return out;
	}

	Physics2DWorld::RayHit2D Physics2DWorld::ShapeCast(const Vec2* points, const int count, const f32 radius,
		const Vec2 &from, const Vec2 &to, const Physics2D* ignore, const uint32 mask) const
	{
		RayHit2D out;
		if (!live || count < 1 || count > 8) return out;
		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);
		if (!b2World_IsValid(world)) return out;
		b2Vec2 pts[8];
		for (int i = 0; i < count; i++) { pts[i].x = points[i].x; pts[i].y = points[i].y; }
		const b2ShapeProxy proxy = b2MakeProxy(pts, count, radius);
		b2Pos origin; origin.x = from.x; origin.y = from.y;
		b2Vec2 delta; delta.x = to.x - from.x; delta.y = to.y - from.y;
		RayFilterCtx ctx;
		ctx.ignore = ignore;
		ctx.ignore2 = NULL;
		ctx.fraction = 0.f;
		ctx.hit = false;
		b2World_CastShape(world, origin, &proxy, delta, QueryFilterFor(mask), RayFilterFcn, &ctx);
		if (!ctx.hit) return out;
		out.hit = true;
		out.point = Vec2((f32)ctx.point.x, (f32)ctx.point.y);
		out.normal = Vec2(ctx.normal.x, ctx.normal.y);
		out.fraction = ctx.fraction;
		if (b2Shape_IsValid(ctx.shape)) out.component = OwnerOfShape(ctx.shape);
		return out;
	}

	Physics2DWorld::RayHit2D Physics2DWorld::CircleCast(const Vec2 &from, const Vec2 &to, const f32 radius,
		const Physics2D* ignore, const uint32 mask) const
	{
		const Vec2 centre(0.f, 0.f);
		return ShapeCast(&centre, 1, radius, from, to, ignore, mask);
	}

	Physics2DWorld::RayHit2D Physics2DWorld::BoxCast(const Vec2 &from, const Vec2 &to, const Vec2 &halfExtents,
		const f32 angle, const Physics2D* ignore, const uint32 mask) const
	{
		const f32 ca = cosf(angle), sa = sinf(angle);
		const f32 xs[4] = { -halfExtents.x, halfExtents.x, halfExtents.x, -halfExtents.x };
		const f32 ys[4] = { -halfExtents.y, -halfExtents.y, halfExtents.y, halfExtents.y };
		Vec2 pts[4];
		for (int i = 0; i < 4; i++) pts[i] = Vec2(ca * xs[i] - sa * ys[i], sa * xs[i] + ca * ys[i]);
		return ShapeCast(pts, 4, 0.f, from, to, ignore, mask);
	}

	std::vector<Physics2D*> Physics2DWorld::OverlapShape(const Vec2* points, const int count, const f32 radius,
		const Vec2 &at, const uint32 mask, const bool includeSensors) const
	{
		std::vector<Physics2D*> out;
		if (!live || count < 1 || count > 8) return out;
		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);
		if (!b2World_IsValid(world)) return out;
		b2Vec2 pts[8];
		for (int i = 0; i < count; i++) { pts[i].x = points[i].x; pts[i].y = points[i].y; }
		const b2ShapeProxy proxy = b2MakeProxy(pts, count, radius);
		b2Pos origin; origin.x = at.x; origin.y = at.y;
		OverlapCtx ctx;
		ctx.out = &out;
		ctx.sensors = includeSensors;
		b2World_OverlapShape(world, origin, &proxy, QueryFilterFor(mask), OverlapFcn, &ctx);
		return out;
	}

	std::vector<Physics2D*> Physics2DWorld::OverlapPoint(const Vec2 &p, const uint32 mask, const bool includeSensors) const
	{
		const Vec2 centre(0.f, 0.f);
		// A point as a very small circle: the solver has no point query, and
		// a radius this size is below anything a scene authors.
		return OverlapShape(&centre, 1, 0.001f, p, mask, includeSensors);
	}

	std::vector<Physics2D*> Physics2DWorld::OverlapCircle(const Vec2 &center, const f32 radius,
		const uint32 mask, const bool includeSensors) const
	{
		const Vec2 centre(0.f, 0.f);
		return OverlapShape(&centre, 1, radius, center, mask, includeSensors);
	}

	std::vector<Physics2D*> Physics2DWorld::OverlapBox(const Vec2 &center, const Vec2 &halfExtents,
		const f32 angle, const uint32 mask, const bool includeSensors) const
	{
		const f32 ca = cosf(angle), sa = sinf(angle);
		const f32 xs[4] = { -halfExtents.x, halfExtents.x, halfExtents.x, -halfExtents.x };
		const f32 ys[4] = { -halfExtents.y, -halfExtents.y, halfExtents.y, halfExtents.y };
		Vec2 pts[4];
		for (int i = 0; i < 4; i++) pts[i] = Vec2(ca * xs[i] - sa * ys[i], sa * xs[i] + ca * ys[i]);
		return OverlapShape(pts, 4, 0.f, center, mask, includeSensors);
	}

	Physics2D* Physics2DWorld::ComponentForShape(const void* shapeHandle)
	{
		const b2ShapeId shape = *(const b2ShapeId*)shapeHandle;
		const b2BodyId body = b2Shape_GetBody(shape);
		return (Physics2D*)b2Body_GetUserData(body);
	}

	void Physics2DWorld::CarryMovedObjects()
	{
		// An object moved by something other than the solver - a script's
		// setPosition, a tween, the editor's gizmo - takes its body along.
		// Before this a static body ignored it entirely (the platform drew
		// in its new place and collided in its old one) and a dynamic one
		// was snapped back at the next step.
		for (size_t i = 0; i < tracked.size(); i++)
		{
			Physics2D* p = tracked[i];
			if (!p || !p->HaveBody() || p->GetOwner() == NULL || !p->haveSynced) continue;
			GameObject* go = p->GetOwner();
			const Vec3 pos = go->GetWorldPosition();
			const f32 angle = WorldAngleOf(go);
			if (fabsf(pos.x - p->syncedPosition.x) < 1e-5f && fabsf(pos.y - p->syncedPosition.y) < 1e-5f
				&& fabsf(angle - p->syncedAngle) < 1e-5f) continue;
			const b2BodyId body = MakeBodyId(p);
			// Whatever is resting on it has to hear about it. A sleeping box
			// on a platform that is taken away stays asleep in mid-air: the
			// solver does not wake an island because a static body moved.
			{
				b2ContactData touching[32];
				const int n = b2Body_GetContactData(body, touching, 32);
				for (int k = 0; k < n; k++)
				{
					b2Body_SetAwake(b2Shape_GetBody(touching[k].shapeIdA), true);
					b2Body_SetAwake(b2Shape_GetBody(touching[k].shapeIdB), true);
				}
			}
			b2Pos bp; bp.x = pos.x; bp.y = pos.y;
			b2Body_SetTransform(body, bp, b2MakeRot(angle));
			b2Body_SetAwake(body, true);
			p->syncedPosition = pos;
			p->syncedAngle = angle;
		}
	}

	void Physics2DWorld::Step(const f64 dt, SceneGraph* Scene)
	{
		if (!live) return;
		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);

		CarryMovedObjects();

		// Fixed timestep, accumulated. A solver fed the frame's own dt gives a
		// different jump arc on every machine and stutters whenever a frame is
		// long, which in a 2D game reads immediately as the physics being
		// broken. Capped so a long stall (a breakpoint, a window drag) does
		// not then run a hundred catch-up steps at once.
		static const f64 FIXED_STEP = 1.0 / 60.0;
		accumulator += (dt > 0.25 ? 0.25 : dt);
		const int maxSteps = (int)MaxPhysicsCatchUpSteps(); // see PhysicsStepping.h
		int steps = 0;
		while (accumulator >= FIXED_STEP && steps < maxSteps)
		{
			SolveSoftJoints(&world, (f32)FIXED_STEP);
			b2World_Step(world, (float)FIXED_STEP, subSteps);
			accumulator -= FIXED_STEP;
			steps++;
			// Events are per step: several steps in one frame would
			// otherwise report only the last one's, and a body that touched
			// and left inside the frame would never be heard from.
			DispatchContacts(&world);
			if (!live) return;
		}
		if (steps == maxSteps) accumulator = 0.0;

		// Write the solved transforms back. Only x/y and the z rotation: z
		// itself is draw order and belongs to whoever authored the scene.
		for (size_t i = 0; i < tracked.size(); i++)
		{
			Physics2D* p = tracked[i];
			if (!p || !p->HaveBody() || p->GetOwner() == NULL) continue;
			if (p->GetBodyType() == Body2DType::Static) continue;

			const b2BodyId body = MakeBodyId(p);
			if (!b2Body_IsValid(body)) continue;
			const b2Pos bp = b2Body_GetPosition(body);
			const f32 worldAngle = b2Rot_GetAngle(b2Body_GetRotation(body));

			GameObject* go = p->GetOwner();
			const Vec3 cur = go->GetPosition();
			GameObject* parent = go->GetParent();
			if (parent == NULL)
			{
				go->SetPosition(Vec3((f32)bp.x, (f32)bp.y, cur.z));
				const Vec3 rot = go->GetRotation();
				go->SetRotation(Vec3(rot.x, rot.y, worldAngle));
			}
			else
			{
				// The body is in the world and the object's transform is its
				// parent's: a child under a layer that is offset, turned or
				// scrolling was being written a world position as if it were
				// a local one.
				const Vec3 world3 = go->GetWorldPosition();
				const Vec3 local = parent->GetWorldTransformation().Inverse() * Vec3((f32)bp.x, (f32)bp.y, world3.z);
				go->SetPosition(Vec3(local.x, local.y, cur.z));
				const Vec3 rot = go->GetRotation();
				go->SetRotation(Vec3(rot.x, rot.y, worldAngle - WorldAngleOf(parent)));
			}
			go->RefreshTransformation();
			p->syncedPosition = go->GetWorldPosition();
			p->syncedAngle = WorldAngleOf(go);
		}
	}

	uint32 Physics2DWorld::JointCount() const
	{
		uint32 n = 0;
		for (size_t i = 0; i < trackedJoints.size(); i++)
			if (trackedJoints[i] && trackedJoints[i]->IsBuilt()) n++;
		return n;
	}

	// A pulley and a gear, held by hand.
	//
	// The solver has neither, and a rope over two wheels is not something
	// its other joints can be put together into. So each is one equation -
	// a pulley's "this rope's two lengths add up to what they were", a
	// gear's "these two angles turn together" - kept the way a solver keeps
	// one: look at how fast it is being broken, and push the two bodies
	// with exactly the impulse that stops it, shared out by how heavy each
	// is. It is done before the solver's own step, a few passes each, with
	// this step's gravity counted in beforehand so a hanging weight is
	// caught as it starts to fall and not a step later.
	//
	// What that cannot do is take part in the solver's own passes: a pulley
	// holding up a stack of boxes that are also wedged against a wall is
	// settled between the two a step at a time, and is softer for it. For
	// lifts, counterweights, cranes and clockwork it holds.
	void Physics2DWorld::SolveSoftJoints(const void* worldHandle, const f32 dt)
	{
		const b2WorldId world = *(const b2WorldId*)worldHandle;
		bool any = false;
		for (size_t i = 0; i < trackedJoints.size() && !any; i++)
			if (trackedJoints[i] && trackedJoints[i]->IsSoft()) any = true;
		if (!any) return;

		const b2Vec2 gravity = b2World_GetGravity(world);
		struct Side {
			b2BodyId id;
			float invMass, invInertia;
			b2Vec2 gdv;          // what gravity will add to its velocity this step
		};
		struct L {
			static bool Make(Physics2D* p, const b2Vec2 &gravity, const f32 dt, Side &s)
			{
				if (!p || !p->HaveBody()) return false;
				s.id = MakeBodyId(p);
				if (!b2Body_IsValid(s.id)) return false;
				const bool dynamic = b2Body_GetType(s.id) == b2_dynamicBody;
				const float m = dynamic ? b2Body_GetMass(s.id) : 0.f;
				const float I = dynamic ? b2Body_GetRotationalInertia(s.id) : 0.f;
				s.invMass = m > 0.f ? 1.f / m : 0.f;
				s.invInertia = (I > 0.f && !p->IsFixedRotation()) ? 1.f / I : 0.f;
				const float gs = dynamic ? b2Body_GetGravityScale(s.id) : 0.f;
				s.gdv.x = gravity.x * gs * dt;
				s.gdv.y = gravity.y * gs * dt;
				return true;
			}
		};

		for (size_t i = 0; i < trackedJoints.size(); i++)
		{
			Joint2D* j = trackedJoints[i];
			if (!j || !j->IsSoft() || j->GetOwner() == NULL) continue;
			j->SetSoftActive(false);

			Physics2D* mine = BodyOf(j->GetOwner());
			Physics2D* other = NULL;
			for (size_t k = 0; k < tracked.size() && !other; k++)
				if (tracked[k] && tracked[k] != mine && tracked[k]->GetOwner()
					&& tracked[k]->GetOwner()->GetName() == j->GetConnected())
					other = tracked[k];
			Side A, B;
			if (!L::Make(mine, gravity, dt, A) || !L::Make(other, gravity, dt, B)) continue;
			j->SetSoftActive(true);
			const float r = j->GetRatio();

			if (j->GetJointType() == Joint2DType::Gear)
			{
				const float aA = b2Rot_GetAngle(b2Body_GetRotation(A.id)), aB = b2Rot_GetAngle(b2Body_GetRotation(B.id));
				if (!j->HaveSoft()) j->SetSoft(0.f, aA, aB);
				const float K = A.invInertia + r * r * B.invInertia;
				if (K <= 0.f) continue;
				// How far out of step they have got, kept as a running sum
				// of what each turned since the last look: an angle is read
				// back between -pi and pi however many turns were made, so
				// the difference of two readings is only good for one step.
				float dA = aA - j->GetSoftAngleA(), dB = aB - j->GetSoftAngleB();
				while (dA > 3.14159265f) dA -= 6.2831853f;
				while (dA < -3.14159265f) dA += 6.2831853f;
				while (dB > 3.14159265f) dB -= 6.2831853f;
				while (dB < -3.14159265f) dB += 6.2831853f;
				const float C = j->GetSoftRest() + dA + r * dB;
				j->SetSoft(C, aA, aB);
				// The speed that pays a fifth of the error off this step is
				// what every pass aims at - the same in each. (Aiming at it
				// once and at zero afterwards has the later passes take the
				// correction straight back out.)
				const float bias = 0.2f * C / dt;
				for (int it = 0; it < 4; it++)
				{
					const float Cdot = b2Body_GetAngularVelocity(A.id) + r * b2Body_GetAngularVelocity(B.id);
					const float lambda = -(Cdot + bias) / K;
					b2Body_ApplyAngularImpulse(A.id, lambda, true);
					b2Body_ApplyAngularImpulse(B.id, r * lambda, true);
				}
				continue;
			}

			// Pulley.
			b2Vec2 la; la.x = j->GetAnchor().x; la.y = j->GetAnchor().y;
			b2Vec2 lb; lb.x = j->HasConnectedAnchor() ? j->GetConnectedAnchor().x : 0.f; lb.y = j->HasConnectedAnchor() ? j->GetConnectedAnchor().y : 0.f;
			const b2Pos pA = b2Body_GetWorldPoint(A.id, la), pB = b2Body_GetWorldPoint(B.id, lb);
			const Vec2 gA = j->GetGroundAnchor(), gB = j->GetConnectedGroundAnchor();
			b2Vec2 uA; uA.x = (float)(pA.x - gA.x); uA.y = (float)(pA.y - gA.y);
			b2Vec2 uB; uB.x = (float)(pB.x - gB.x); uB.y = (float)(pB.y - gB.y);
			const float lenA = sqrtf(uA.x * uA.x + uA.y * uA.y), lenB = sqrtf(uB.x * uB.x + uB.y * uB.y);
			if (lenA < 1e-4f || lenB < 1e-4f) continue;
			uA.x /= lenA; uA.y /= lenA; uB.x /= lenB; uB.y /= lenB;
			if (!j->HaveSoft()) j->SetSoft(j->GetLength() > 0.f ? j->GetLength() : lenA + r * lenB, 0.f, 0.f);
			const float C = lenA + r * lenB - j->GetSoftRest();
			const float bias = 0.2f * (C > 0.f ? C : 0.f) / dt;

			const b2Pos cA = b2Body_GetWorldCenter(A.id), cB = b2Body_GetWorldCenter(B.id);
			const float rAx = (float)(pA.x - cA.x), rAy = (float)(pA.y - cA.y), rBx = (float)(pB.x - cB.x), rBy = (float)(pB.y - cB.y);
			const float crA = rAx * uA.y - rAy * uA.x, crB = rBx * uB.y - rBy * uB.x;
			const float K = A.invMass + A.invInertia * crA * crA + r * r * (B.invMass + B.invInertia * crB * crB);
			if (K <= 0.f) continue;
			float total = 0.f;
			for (int it = 0; it < 4; it++)
			{
				b2Vec2 vA = b2Body_GetWorldPointVelocity(A.id, pA), vB = b2Body_GetWorldPointVelocity(B.id, pB);
				vA.x += A.gdv.x; vA.y += A.gdv.y; vB.x += B.gdv.x; vB.y += B.gdv.y;
				const float Cdot = uA.x * vA.x + uA.y * vA.y + r * (uB.x * vB.x + uB.y * vB.y);
				// Slack and not about to be taut: a rope does nothing.
				if (C < -0.005f && C + Cdot * dt < 0.f && total == 0.f) break;
				float lambda = -(Cdot + bias) / K;
				// It only pulls: the sum of what it has done may not push.
				const float was = total;
				total = total + lambda < 0.f ? total + lambda : 0.f;
				lambda = total - was;
				if (lambda == 0.f) break;
				b2Vec2 iA; iA.x = uA.x * lambda; iA.y = uA.y * lambda;
				b2Vec2 iB; iB.x = uB.x * lambda * r; iB.y = uB.y * lambda * r;
				b2Body_ApplyLinearImpulse(A.id, iA, pA, true);
				b2Body_ApplyLinearImpulse(B.id, iB, pB, true);
			}
		}
	}

};
