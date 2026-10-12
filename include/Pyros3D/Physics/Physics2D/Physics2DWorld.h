//============================================================================
// Name        : Physics2DWorld
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Owns the Box2D world for a scene's Physics2D components
//============================================================================

#ifndef PHYSICS2DWORLD_H
#define PHYSICS2DWORLD_H

#include <Pyros3D/Core/Math/Math.h>
#include <vector>

namespace p3d {

	class SceneGraph;
	class GameObject;
	class Physics2D;
	class Joint2D;
	class DebugRenderer;

	// The Box2D world for one scene.
	//
	// Created lazily and only when a scene actually has Physics2D components,
	// so a 3D scene - or a 2D one that is just sprites - pays nothing for
	// this existing.
	//
	// The mapping to the engine is the plane z is drawn on: a body's (x, y) is
	// the GameObject's (x, y), its angle is rotation about z, and z itself is
	// left alone because that is draw order (see Layer2D) and the solver has
	// no opinion about it.
	class PYROS3D_API Physics2DWorld {

	public:

		Physics2DWorld(const Vec2 &gravity = Vec2(0.f, -10.f));
		~Physics2DWorld();

		// Builds bodies for any Physics2D in the scene that does not have one
		// yet, and drops bodies whose component has gone. Cheap to call every
		// frame - it does nothing when the set has not changed.
		void Sync(SceneGraph* Scene);

		// Steps the solver, then writes each body's transform back onto its
		// GameObject. Fixed sub-steps rather than the frame's own dt: a
		// variable step makes a 2D solver visibly jittery, and the whole point
		// of a 2D game is that a jump arc is the same every time.
		void Step(const f64 dt, SceneGraph* Scene);

		// Draws every collider through the engine's DebugRenderer, at the z
		// the layer sits on. Colliders are otherwise invisible, so a collider
		// that does not match the sprite it belongs to is silent - which is
		// the single most common way a 2D scene "does not land where it looks
		// like it should".
		//
		// Takes the renderer rather than owning one: the editor viewport and
		// the player each already have theirs, and a second would draw into
		// the wrong target.
		void DebugDraw(DebugRenderer* debug, const f32 z = 0.f);

		// The inverse of Step()'s write-back: pushes each GameObject's authored
		// transform onto its body. For an editor, where the scene is being
		// laid out rather than simulated - Sync() only builds bodies for
		// components that lack one, so without this a collider outline would
		// stay wherever its object was when the body was first created and
		// drift away as the object is dragged.
		void PullTransforms();

		// What a ray hit, if anything. `component` is the Physics2D the shape
		// belonged to - NULL only when `hit` is false.
		struct RayHit2D {
			bool hit = false;
			Vec2 point;
			Vec2 normal;
			f32 fraction = 0.f;          // along from->to
			Physics2D* component = NULL;
		};

		// Closest hit along the segment. This is the primitive a character
		// controller is built on: "how far below me is the ground, and which
		// way is its surface facing". Nothing could ask that before - the only
		// RayCast in the engine is IPhysics', which is the 3D world - so a 2D
		// game could only infer the ground from contact callbacks, which say
		// *that* you touched something and never *where* or *at what angle*.
		// `ignore` is skipped entirely - pass the caster's own body. A ground
		// sensor starts INSIDE the character, so without this every downward
		// ray reports the character itself at fraction ~0 and the controller
		// concludes it is standing on its own head.
		// TWO bodies can be skipped, and the second is not a convenience - it
		// is how a loop works. A character running a loop has to stop seeing
		// the ground the moment it commits, and stop seeing the loop while it
		// is merely running past the outside of it, because in a single
		// collision layer the loop's own material fills the space the entry
		// runs through. Sonic switched collision layers at the loop mouth for
		// exactly this reason; with a sensor-driven character, choosing what
		// the rays may see IS the layer switch.
		// `mask` is which collision layers the ray can see, a bit a layer -
		// everything unless told otherwise. Sensors are never hit: a ground
		// probe that stopped on a coin's trigger would be standing on air.
		RayHit2D RayCast(const Vec2 &from, const Vec2 &to,
			const Physics2D* ignore = NULL, const Physics2D* ignore2 = NULL,
			const uint32 mask = 0xFFFFFFFFu) const;
		// Every body along the segment, nearest first: a piercing shot, or
		// "what is between me and the player".
		std::vector<RayHit2D> RayCastAll(const Vec2 &from, const Vec2 &to,
			const Physics2D* ignore = NULL, const uint32 mask = 0xFFFFFFFFu) const;

		// A shape swept along the segment, and where it first touches: a
		// thrown ball, a dash that must stop at a wall. `fraction` is how
		// far along it got. A ray is infinitely thin and slips through gaps
		// the thing it stands for would not.
		RayHit2D CircleCast(const Vec2 &from, const Vec2 &to, const f32 radius,
			const Physics2D* ignore = NULL, const uint32 mask = 0xFFFFFFFFu) const;
		RayHit2D BoxCast(const Vec2 &from, const Vec2 &to, const Vec2 &halfExtents,
			const f32 angle = 0.f, const Physics2D* ignore = NULL, const uint32 mask = 0xFFFFFFFFu) const;

		// Which bodies are at a place, without anything having to collide
		// with them first: what an explosion reaches, what the cursor is
		// over, whether a spawn point is clear. Sensors are left out unless
		// asked for.
		std::vector<Physics2D*> OverlapPoint(const Vec2 &p, const uint32 mask = 0xFFFFFFFFu,
			const bool includeSensors = false) const;
		std::vector<Physics2D*> OverlapCircle(const Vec2 &center, const f32 radius,
			const uint32 mask = 0xFFFFFFFFu, const bool includeSensors = false) const;
		std::vector<Physics2D*> OverlapBox(const Vec2 &center, const Vec2 &halfExtents, const f32 angle = 0.f,
			const uint32 mask = 0xFFFFFFFFu, const bool includeSensors = false) const;

		// Throws the world away: every body, and every tracked component
		// pointer. MUST be called when the scene those components belonged to
		// is replaced - a body keeps a raw Physics2D* as its user data, and
		// contact dispatch calls a std::function through it, so surviving a
		// scene swap means calling a callback on freed memory. Play mode
		// reloads the scene, which is exactly that.
		void Clear();

		void SetGravity(const Vec2 &g);
		const Vec2 &GetGravity() const { return gravity; }

		// How many solver passes a step is cut into. More makes stacks and
		// chains of joints stiffer and costs proportionally; 4 is the
		// solver's own recommendation.
		void SetSubSteps(const int n) { subSteps = n < 1 ? 1 : (n > 16 ? 16 : n); }
		int GetSubSteps() const { return subSteps; }

		uint32 BodyCount() const { return (uint32)tracked.size(); }
		uint32 JointCount() const;

		// Whether a world was actually created. False until the first Sync()
		// that finds something to simulate.
		bool IsLive() const { return live; }

	private:

		void DestroyBodyFor(Physics2D* p);
		void BuildBody(Physics2D* p, const void* worldHandle);
		void BuildJoint(Joint2D* j, const std::vector<Physics2D*> &bodies, const void* worldHandle);
		void CarryMovedObjects();
		// The joints the solver has no kind for - a pulley, a gear - held
		// here: once per fixed step, before the solver runs, each is
		// brought back to what it allows by pushing its two bodies.
		void SolveSoftJoints(const void* worldHandle, const f32 dt);
		RayHit2D ShapeCast(const Vec2* points, const int count, const f32 radius,
			const Vec2 &from, const Vec2 &to, const Physics2D* ignore, const uint32 mask) const;
		std::vector<Physics2D*> OverlapShape(const Vec2* points, const int count, const f32 radius,
			const Vec2 &at, const uint32 mask, const bool includeSensors) const;
		// void* rather than b2WorldId/b2ShapeId so this header stays
		// box2d-free, same as the split handles below.
		void DispatchContacts(const void* worldHandle);
		static Physics2D* ComponentForShape(const void* shapeHandle);

		// b2WorldId, split so this header stays box2d-free (same reasoning as
		// Physics2D's body handle).
		uint16 worldIndex;
		uint16 worldGeneration;
		bool live;

		Vec2 gravity;
		f64 accumulator;
		std::vector<Physics2D*> tracked;
		int subSteps;

		// Joints, and the solver handle each had when last seen - kept apart
		// from the component because the component may be freed by the time
		// its joint has to be destroyed.
		struct JointHandle { int32 index; uint16 world; uint16 generation; };
		// The same for bodies, and for the same reason - plus one more. An
		// undo deletes an object and makes it again, and the new component
		// is often given the very address the old one had: by its pointer
		// it is "still here", with no body, so it is given one, and the old
		// one's is left in the solver for good - a collider with nothing
		// attached to it, exactly where the object used to be.
		std::vector<JointHandle> trackedBodyIds;
		std::vector<Joint2D*> trackedJoints;
		std::vector<JointHandle> trackedJointIds;

		// The static body joints "to the world" are tied to.
		bool haveGround;
		int32 groundIndex;
		uint16 groundWorld;
		uint16 groundGeneration;
	};

};

#endif
