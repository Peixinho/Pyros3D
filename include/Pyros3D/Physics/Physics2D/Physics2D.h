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
#include <functional>

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
			Circle = 1,
			// A box with round ends, upright: size.x is the radius, size.y
			// the half-height INCLUDING the caps (so it fills the same
			// extents a box of that size would). What a character wants - it
			// does not catch on the seam between two floor pieces the way a
			// box's corners do, and it slides up a small step.
			Capsule = 2,
			// A solid outline, given with SetPolygon(): any simple polygon,
			// convex or not. One that is convex and has 8 points or fewer
			// is one shape to the solver; anything else is cut into convex
			// pieces that together are it.
			Polygon = 3,
			// A line of edges with no inside, given with SetChain(): the
			// ground of a hand-drawn level, a ramp, a wall. Open, it stops
			// things from both sides; closed into a loop it is the skin of
			// something solid, and stops what is outside it. It has no
			// mass, so it belongs on a Static or Kinematic body.
			Chain = 4
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
		// How many of these exist, anywhere. A scene with none - every 3D
		// game - is not searched for them: that search is the whole scene,
		// every frame.
		static uint32 LiveCount();

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Physics2D; }

		// Every setter below works before the body exists and after. One
		// that the solver can take live (friction, damping, gravity scale...)
		// is applied to the running body; one that changes what the body IS
		// (its shape, its size, whether it is a sensor) rebuilds it at the
		// next step, keeping its velocity.
		uint32 GetBodyType() const { return bodyType; }
		void SetBodyType(const uint32 t);
		uint32 GetShapeType() const { return shapeType; }
		void SetShapeType(const uint32 t);
		// Half-extents for a box and a capsule; .x is the radius for a
		// circle. Half-extents because that is what b2MakeBox takes, and
		// converting at the boundary would leave the editor showing one
		// number and the solver using another.
		const Vec2 &GetSize() const { return size; }
		void SetSize(const Vec2 &s);
		// Where the shape sits relative to the object's origin. A sprite
		// pivoted at its feet has its origin on the ground; its collider
		// belongs half its height above that.
		const Vec2 &GetOffset() const { return offset; }
		void SetOffset(const Vec2 &o);
		// The points of a Polygon shape, in the object's local space (the
		// offset is added): 3 to MaxOutlinePoints of them, in either
		// direction, making an outline that does not cross itself. Anything
		// else is refused and the previous points are kept.
		bool SetPolygon(const std::vector<Vec2> &points);
		// The points of a Polygon OR a Chain - whichever the shape is.
		const std::vector<Vec2> &GetPolygon() const { return polygon; }
		// The points of a Chain shape: 2 or more, no two in a row the same.
		// `loop` joins the last back to the first.
		bool SetChain(const std::vector<Vec2> &points, const bool loop);
		bool IsChainLoop() const { return chainLoop; }
		enum { MaxOutlinePoints = 256 };
		// A simple polygon as convex pieces of at most 8 points each, which
		// is what the solver can collide: one piece for an outline already
		// convex and small enough, triangles joined back together where the
		// join is still convex otherwise. Empty for an outline with no area.
		static void DecomposePolygon(const std::vector<Vec2> &outline, std::vector<std::vector<Vec2> > &piecesOut);

		f32 GetDensity() const { return density; }
		void SetDensity(const f32 d);
		f32 GetFriction() const { return friction; }
		void SetFriction(const f32 f);
		f32 GetRestitution() const { return restitution; }
		void SetRestitution(const f32 r);
		// A body that cannot tip over - what almost every platformer
		// character wants, and fiddly to discover if it is not offered.
		bool IsFixedRotation() const { return fixedRotation; }
		void SetFixedRotation(const bool f);
		// Whether this body's shape blocks 2D light. On by default: a solid
		// thing that does not cast is the surprising case, and a trigger or
		// sensor is the one that has to say so.
		bool CastsShadow() const { return castsShadow; }
		void SetCastsShadow(const bool c) { castsShadow = c; }

		// A sensor (a trigger): it collides with nothing and pushes nothing,
		// and says who is inside it - OnTriggerEnter / OnTriggerExit, on the
		// sensor AND on whoever walked in. A coin, a checkpoint, a kill
		// zone, the reach of an enemy's sight.
		bool IsSensor() const { return sensor; }
		void SetSensor(const bool s);

		// A platform that is solid from above and open from below and the
		// sides: jumped up through, stood on. Meant for a static or
		// kinematic body.
		bool IsOneWay() const { return oneWay; }
		void SetOneWay(const bool o);

		// Collision layers. A body is ON one layer (0-31) and collides with
		// the layers in its mask; two bodies collide when each has the
		// other's layer in its mask. Everything starts on layer 0 colliding
		// with everything, so nothing needs these until something must NOT
		// collide - the player and its own bullets, pickups and enemies.
		uint32 GetLayer() const { return layer; }
		void SetLayer(const uint32 l);
		uint32 GetMask() const { return mask; }
		void SetMask(const uint32 m);
		// The mask, a layer at a time.
		void SetCollidesWith(const uint32 otherLayer, const bool collides);
		bool CollidesWith(const uint32 otherLayer) const { return otherLayer < 32 && (mask & (1u << otherLayer)) != 0; }

		// How much of the world's gravity it feels: 0 floats, 2 falls twice
		// as hard, a negative one rises.
		f32 GetGravityScale() const { return gravityScale; }
		void SetGravityScale(const f32 g);
		// Slows it down over time, in the air as well as on the ground.
		f32 GetLinearDamping() const { return linearDamping; }
		void SetLinearDamping(const f32 d);
		f32 GetAngularDamping() const { return angularDamping; }
		void SetAngularDamping(const f32 d);
		// Continuous collision against other moving bodies as well as
		// static ones - for something small and fast that must not pass
		// through a thin moving thing. Costs more; off by default.
		bool IsBullet() const { return bullet; }
		void SetBullet(const bool b);

		// --- Compound shapes ------------------------------------------------
		// Several boxes on ONE body, as (centreX, centreY, halfW, halfH) in
		// body-local space. When this is non-empty it REPLACES the single
		// shape above; `shapeType`/`size` are then ignored.
		//
		// Exists for TileMap2D: a tiled floor is thousands of cells, and one
		// body per cell would be thousands of bodies for what is physically a
		// handful of rectangles. Box2D carries many shapes on one body
		// natively, so the merge happens before it ever gets here and the
		// solver sees roughly what a hand-placed level would have had.
		//
		// Deliberately not its own component type: everything else a body
		// needs - density, friction, contact events, the tracked/destroy
		// reconciliation in Physics2DWorld - is identical, and a second
		// component would have to duplicate all of it.
		// A convex polygon in the body's local space. Tile slopes are
		// triangles; nothing below assumes three points.
		typedef std::vector<Vec2> Poly2D;

		void SetCompoundBoxes(const std::vector<Vec4> &boxes);
		// Boxes AND polygons together. Both have to be set in one call: they
		// are two halves of one collider set, and applying them separately
		// would rebuild the body twice and briefly leave it with half its
		// geometry.
		void SetCompoundShapes(const std::vector<Vec4> &boxes,
			const std::vector<Poly2D> &polys);
		// Boxes, polygons AND chain loops in one call. Terrain uses the
		// chains; the other two stay for everything that is genuinely a set
		// of convex pieces. All three together because they are one collider
		// set - applying them separately rebuilds the body more than once and
		// leaves it briefly holding a fraction of its geometry.
		void SetCompoundShapes(const std::vector<Vec4> &boxes,
			const std::vector<Poly2D> &polys,
			const std::vector<Poly2D> &chains);
		const std::vector<Poly2D> &GetCompoundChains() const { return compoundChains; }
		const std::vector<Vec4> &GetCompoundBoxes() const { return compoundBoxes; }
		const std::vector<Poly2D> &GetCompoundPolys() const { return compoundPolys; }
		bool IsCompound() const
		{ return !compoundBoxes.empty() || !compoundPolys.empty() || !compoundChains.empty(); }
		// True when the shape list has changed since the body was built, so
		// Physics2DWorld knows to rebuild it. Cleared when it does.
		bool NeedsShapeRebuild() const { return shapesDirty; }
		void ClearShapeRebuild() { shapesDirty = false; }

		// --- Runtime, once a body exists -----------------------------------
		// These reach Box2D directly from the .cpp by rebuilding the body id
		// from the fields below, so they need no pointer back to the world.
		// All are no-ops before the first Sync() has built the body, which is
		// what a script calling one in init() will hit.
		void SetLinearVelocity(const Vec2 &v);
		Vec2 GetLinearVelocity() const;
		void ApplyForce(const Vec2 &force);
		void ApplyImpulse(const Vec2 &impulse);
		// The same at a world point, which also turns the body.
		void ApplyForceAt(const Vec2 &force, const Vec2 &worldPoint);
		void ApplyImpulseAt(const Vec2 &impulse, const Vec2 &worldPoint);
		void ApplyTorque(const f32 torque);
		void ApplyAngularImpulse(const f32 impulse);
		void SetAngularVelocity(const f32 radiansPerSecond);
		f32 GetAngularVelocity() const;
		// Teleports. Not what you want for ordinary movement - it ignores
		// collision on the way - but it is what a respawn is.
		void SetTransform(const Vec2 &position, const f32 angle);
		// Where the BODY is, in the world: its position and its angle in
		// radians. The object's own transform while no body exists.
		Vec2 GetPosition() const;
		f32 GetAngle() const;
		// Moves a kinematic body so that it ARRIVES at the target over `dt`
		// seconds, by giving it the velocity that gets it there - so what it
		// touches on the way is pushed and carried rather than teleported
		// through. This is how a moving platform or a door is driven.
		void MoveTo(const Vec2 &position, const f32 angle, const f32 dt);
		f32 GetMass() const;
		void Wake();
		bool IsAwake() const;

		// Standing on something: touching a surface whose normal points up
		// at least this much (0.7 is about 45 degrees). The question every
		// character controller asks before allowing a jump. False until the
		// body exists.
		bool IsGrounded(const f32 minNormalY = 0.7f) const;
		// The steepest-up surface normal it is touching, or (0, 0) for none.
		Vec2 GetGroundNormal() const;
		// The body it is standing on, or NULL. What a character on a moving
		// platform needs: a controller that sets its own velocity every
		// frame throws away the push friction would have given it, so it
		// has to add the platform's - and to do that it has to know which
		// platform, and how fast that one is going.
		Physics2D* GetGroundBody(const f32 minNormalY = 0.7f) const;
		// That body's velocity at the point this one stands on it - which
		// for a turning platform is not its centre's. (0, 0) in the air.
		Vec2 GetGroundVelocity(const f32 minNormalY = 0.7f) const;
		// How many bodies it is touching right now.
		uint32 GetContactCount() const;

		// --- Collision ------------------------------------------------------
		// Plain std::function fields, not virtuals or a registration call:
		// sol2 auto-converts an assigned Lua closure straight into one, which
		// is the same pattern IPhysicsComponent uses for its 3D equivalents
		// and needs no extra binding plumbing. The argument is the other body.
		//
		// Dispatched by Physics2DWorld::Step after the solver has run, so a
		// handler sees the positions the collision resolved to rather than
		// the ones that caused it, and can safely apply forces - the next
		// step has not started.
		std::function<void(Physics2D*)> OnCollisionEnter;
		std::function<void(Physics2D*)> OnCollisionExit;
		// The same moment as OnCollisionEnter, with where and how: the
		// contact point and the surface normal pointing AWAY from the other
		// body towards this one (so normal.y > 0 means "I landed on it"),
		// and how fast the two were closing. What a landing dust puff, a
		// damage-by-impact rule or a bounce sound needs.
		std::function<void(Physics2D*, Vec2, Vec2, f32)> OnContact;
		// A sensor and its visitor. On the sensor the argument is who came
		// in; on the visitor it is the sensor that was entered.
		std::function<void(Physics2D*)> OnTriggerEnter;
		std::function<void(Physics2D*)> OnTriggerExit;

		// Opaque b2BodyId, owned by whichever Physics2DWorld built it. Stored
		// as two ints so this header does not drag box2d in - the world casts
		// it back. Zeroed when no body exists.
		void SetBodyHandle(const int32 index, const uint16 world, const uint16 generation);
		bool HaveBody() const { return haveBody; }
		int32 GetBodyIndex() const { return bodyIndex; }
		uint16 GetBodyWorld() const { return bodyWorld; }
		uint16 GetBodyGeneration() const { return bodyGeneration; }
		void ClearBodyHandle() { haveBody = false; }

		// Changed something that needs the body made again - see the setters.
		void MarkShapeRebuild() { shapesDirty = true; }

		// What the solver last wrote onto the owner (or the body was created
		// at). When the owner's transform no longer matches, somebody moved
		// the OBJECT - a script calling setPosition, a tween - and
		// Physics2DWorld carries the body to it rather than snapping the
		// object back. Kept by the world.
		Vec3 syncedPosition;
		f32 syncedAngle = 0.f;
		bool haveSynced = false;

	private:

		void ApplyFilter();
		void ApplyMaterial();

		uint32 bodyType;
		uint32 shapeType;
		Vec2 size;
		Vec2 offset;
		std::vector<Vec2> polygon;
		bool chainLoop = false;
		f32 density;
		f32 friction;
		f32 restitution;
		bool fixedRotation;
		bool castsShadow;
		bool sensor;
		bool oneWay;
		uint32 layer;
		uint32 mask;
		f32 gravityScale;
		f32 linearDamping;
		f32 angularDamping;
		bool bullet;

		std::vector<Vec4> compoundBoxes;
		std::vector<Poly2D> compoundPolys;
		std::vector<Poly2D> compoundChains;
		bool shapesDirty;

		bool haveBody;
		int32 bodyIndex;
		uint16 bodyWorld;
		uint16 bodyGeneration;
	};

};

#endif
