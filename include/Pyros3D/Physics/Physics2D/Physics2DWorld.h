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

		void SetGravity(const Vec2 &g);
		const Vec2 &GetGravity() const { return gravity; }

		// Whether a world was actually created. False until the first Sync()
		// that finds something to simulate.
		bool IsLive() const { return live; }

	private:

		void DestroyBodyFor(Physics2D* p);

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
