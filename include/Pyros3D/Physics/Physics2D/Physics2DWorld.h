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
		RayHit2D RayCast(const Vec2 &from, const Vec2 &to,
			const Physics2D* ignore = NULL, const Physics2D* ignore2 = NULL) const;

		// Throws the world away: every body, and every tracked component
		// pointer. MUST be called when the scene those components belonged to
		// is replaced - a body keeps a raw Physics2D* as its user data, and
		// contact dispatch calls a std::function through it, so surviving a
		// scene swap means calling a callback on freed memory. Play mode
		// reloads the scene, which is exactly that.
		void Clear();

		void SetGravity(const Vec2 &g);
		const Vec2 &GetGravity() const { return gravity; }

		// Whether a world was actually created. False until the first Sync()
		// that finds something to simulate.
		bool IsLive() const { return live; }

	private:

		void DestroyBodyFor(Physics2D* p);
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
	};

};

#endif
