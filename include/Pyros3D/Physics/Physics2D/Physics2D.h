//============================================================================
// Name        : Physics2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box2D-backed rigid body for 2D scenes
//============================================================================

#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <vector>
#include <memory>

namespace p3d {

	class SceneGraph;
	class GameObject;

	namespace Body2DType
	{
		enum {
			Static = 0,     // never moves; the ground
			Kinematic = 1,  // moved by script, pushes others, ignores forces
			Dynamic = 2     // moved by the solver
		};
	}

	namespace Shape2DType
	{
		enum {
			Box = 0,
			Circle = 1
		};
	}

	// A rigid body in the plane, backed by Box2D.
	//
	// Deliberately *not* an IPhysicsComponent behind IPhysics: that interface
	// is 3D throughout - Vec3 positions, Euler rotations, 3D raycasts - and
	// driving a 2D solver through it would mean projecting every call in and
	// back out again, for a solver whose whole advantage is that it does not
	// work in three dimensions. Box3D stays what it is; this sits beside it.
	//
	// The component owns no world. Physics2DWorld finds every Physics2D in a
	// scene and builds bodies for them, so a scene with none pays nothing and
	// no world is created at all.
	class PYROS3D_API Physics2D : public IComponent {

	public:

		Physics2D(const uint32 bodyType = Body2DType::Dynamic,
			const uint32 shape = Shape2DType::Box,
			const Vec2 &size = Vec2(0.5f, 0.5f));
		virtual ~Physics2D();

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Physics2D; }

		uint32 GetBodyType() const { return bodyType; }
		void SetBodyType(const uint32 t) { bodyType = t; }
		uint32 GetShapeType() const { return shapeType; }
		void SetShapeType(const uint32 t) { shapeType = t; }
		// Half-extents for a box; .x is the radius for a circle. Half-extents
		// because that is what b2MakeBox takes, and converting at the boundary
		// would leave the editor showing one number and the solver using
		// another.
		const Vec2 &GetSize() const { return size; }
		void SetSize(const Vec2 &s) { size = s; }

		f32 GetDensity() const { return density; }
		void SetDensity(const f32 d) { density = d; }
		f32 GetFriction() const { return friction; }
		void SetFriction(const f32 f) { friction = f; }
		f32 GetRestitution() const { return restitution; }
		void SetRestitution(const f32 r) { restitution = r; }
		// A body that cannot tip over - what almost every platformer
		// character wants, and fiddly to discover if it is not offered.
		bool IsFixedRotation() const { return fixedRotation; }
		void SetFixedRotation(const bool f) { fixedRotation = f; }

		// --- Runtime, once a body exists -----------------------------------
		// These reach Box2D directly from the .cpp by rebuilding the body id
		// from the fields below, so they need no pointer back to the world.
		// All are no-ops before the first Sync() has built the body, which is
		// what a script calling one in init() will hit.
		void SetLinearVelocity(const Vec2 &v);
		Vec2 GetLinearVelocity() const;
		void ApplyForce(const Vec2 &force);
		void ApplyImpulse(const Vec2 &impulse);
		void SetAngularVelocity(const f32 radiansPerSecond);
		f32 GetAngularVelocity() const;
		// Teleports. Not what you want for ordinary movement - it ignores
		// collision on the way - but it is what a respawn is.
		void SetTransform(const Vec2 &position, const f32 angle);
		void Wake();

		// Opaque b2BodyId, owned by whichever Physics2DWorld built it. Stored
		// as two ints so this header does not drag box2d in - the world casts
		// it back. Zeroed when no body exists.
		void SetBodyHandle(const int32 index, const uint16 world, const uint16 generation);
		bool HaveBody() const { return haveBody; }
		int32 GetBodyIndex() const { return bodyIndex; }
		uint16 GetBodyWorld() const { return bodyWorld; }
		uint16 GetBodyGeneration() const { return bodyGeneration; }
		void ClearBodyHandle() { haveBody = false; }

	private:

		uint32 bodyType;
		uint32 shapeType;
		Vec2 size;
		f32 density;
		f32 friction;
		f32 restitution;
		bool fixedRotation;

		bool haveBody;
		int32 bodyIndex;
		uint16 bodyWorld;
		uint16 bodyGeneration;
	};

};

#endif
