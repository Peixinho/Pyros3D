//============================================================================
// Name        : Physics2DWorld
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Owns the Box2D world for a scene's Physics2D components
//============================================================================

#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <box2d/box2d.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace p3d {

	namespace {
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

	void Physics2DWorld::Sync(SceneGraph* Scene)
	{
		if (Scene == NULL) return;

		// Collect this frame's components first, so a scene with none never
		// creates a world at all.
		std::vector<Physics2D*> found;
		std::vector<std::shared_ptr<GameObject> > &all = Scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); i++)
		{
			if (!all[i]) continue;
			const std::vector<std::shared_ptr<IComponent> > &comps = all[i]->GetComponents();
			for (size_t c = 0; c < comps.size(); c++)
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::Physics2D)
					found.push_back(static_cast<Physics2D*>(comps[c].get()));
		}
		if (found.empty() && !live) return;

		if (!live)
		{
			b2WorldDef def = b2DefaultWorldDef();
			def.gravity.x = gravity.x;
			def.gravity.y = gravity.y;
			b2WorldId w = b2CreateWorld(&def);
			worldIndex = w.index1;
			worldGeneration = w.generation;
			live = true;
		}

		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);

		for (size_t i = 0; i < found.size(); i++)
		{
			Physics2D* p = found[i];
			if (p->HaveBody() || p->GetOwner() == NULL) continue;

			const Vec3 pos = p->GetOwner()->GetWorldPosition();
			b2BodyDef bd = b2DefaultBodyDef();
			bd.type = TranslateBodyType(p->GetBodyType());
			bd.position.x = pos.x;
			bd.position.y = pos.y;
			// v3 expresses this as a per-axis motion lock rather than one flag.
			bd.motionLocks.angularZ = p->IsFixedRotation();
			// What lets a contact event find its way back to a component:
			// events carry shape ids, a shape knows its body, and a body
			// carries this.
			bd.userData = p;
			b2BodyId body = b2CreateBody(world, &bd);

			b2ShapeDef sd = b2DefaultShapeDef();
			// Off by default in v3; without it the contact event buffers stay
			// empty and OnCollisionEnter/Exit simply never fire.
			sd.enableContactEvents = true;
			sd.density = p->GetDensity();
			sd.material.friction = p->GetFriction();
			sd.material.restitution = p->GetRestitution();

			if (p->GetShapeType() == Shape2DType::Circle)
			{
				b2Circle circle;
				circle.center.x = 0.f;
				circle.center.y = 0.f;
				circle.radius = p->GetSize().x;
				b2CreateCircleShape(body, &sd, &circle);
			}
			else
			{
				b2Polygon box = b2MakeBox(p->GetSize().x, p->GetSize().y);
				b2CreatePolygonShape(body, &sd, &box);
			}

			p->SetBodyHandle(body.index1, body.world0, body.generation);
		}

		// Anything tracked last frame but gone now takes its body with it.
		for (size_t i = 0; i < tracked.size(); i++)
		{
			bool stillHere = false;
			for (size_t j = 0; j < found.size() && !stillHere; j++)
				if (found[j] == tracked[i]) stillHere = true;
			if (!stillHere && tracked[i]) DestroyBodyFor(tracked[i]);
		}
		tracked = found;
	}

	void Physics2DWorld::DispatchContacts(const void* worldHandle)
	{
		const b2WorldId world = *(const b2WorldId*)worldHandle;
		const b2ContactEvents events = b2World_GetContactEvents(world);

		// Both sides get told, because "did I hit something" is the question
		// a script actually asks and neither body is privileged.
		for (int i = 0; i < events.beginCount; i++)
		{
			Physics2D* a = ComponentForShape(&events.beginEvents[i].shapeIdA);
			Physics2D* b = ComponentForShape(&events.beginEvents[i].shapeIdB);
			if (a && a->OnCollisionEnter) a->OnCollisionEnter(b);
			if (b && b->OnCollisionEnter) b->OnCollisionEnter(a);
		}
		for (int i = 0; i < events.endCount; i++)
		{
			// An end event can name a shape that has since been destroyed -
			// b2Shape_IsValid is the documented guard and skipping it turns a
			// body deleted mid-collision into a crash.
			Physics2D* a = b2Shape_IsValid(events.endEvents[i].shapeIdA)
				? ComponentForShape(&events.endEvents[i].shapeIdA) : NULL;
			Physics2D* b = b2Shape_IsValid(events.endEvents[i].shapeIdB)
				? ComponentForShape(&events.endEvents[i].shapeIdB) : NULL;
			if (a && a->OnCollisionExit) a->OnCollisionExit(b);
			if (b && b->OnCollisionExit) b->OnCollisionExit(a);
		}
	}

	Physics2D* Physics2DWorld::ComponentForShape(const void* shapeHandle)
	{
		const b2ShapeId shape = *(const b2ShapeId*)shapeHandle;
		const b2BodyId body = b2Shape_GetBody(shape);
		return (Physics2D*)b2Body_GetUserData(body);
	}

	void Physics2DWorld::Step(const f64 dt, SceneGraph* Scene)
	{
		if (!live) return;
		const b2WorldId world = MakeWorldId(worldIndex, worldGeneration);

		// Fixed timestep, accumulated. A solver fed the frame's own dt gives a
		// different jump arc on every machine and stutters whenever a frame is
		// long, which in a 2D game reads immediately as the physics being
		// broken. Capped so a long stall (a breakpoint, a window drag) does
		// not then run a hundred catch-up steps at once.
		static const f64 FIXED_STEP = 1.0 / 60.0;
		accumulator += (dt > 0.25 ? 0.25 : dt);
		int steps = 0;
		while (accumulator >= FIXED_STEP && steps < 8)
		{
			b2World_Step(world, (float)FIXED_STEP, 4);
			accumulator -= FIXED_STEP;
			steps++;
		}
		if (steps == 8) accumulator = 0.0;

		DispatchContacts(&world);

		// Write the solved transforms back. Only x/y and the z rotation: z
		// itself is draw order and belongs to whoever authored the scene.
		for (size_t i = 0; i < tracked.size(); i++)
		{
			Physics2D* p = tracked[i];
			if (!p || !p->HaveBody() || p->GetOwner() == NULL) continue;
			if (p->GetBodyType() == Body2DType::Static) continue;

			const b2BodyId body = MakeBodyId(p);
			const b2Pos bp = b2Body_GetPosition(body);
			const b2Rot br = b2Body_GetRotation(body);

			GameObject* go = p->GetOwner();
			const Vec3 cur = go->GetPosition();
			go->SetPosition(Vec3((f32)bp.x, (f32)bp.y, cur.z));
			const Vec3 rot = go->GetRotation();
			go->SetRotation(Vec3(rot.x, rot.y, atan2f(br.s, br.c)));
			go->RefreshTransformation();
		}
	}

};
