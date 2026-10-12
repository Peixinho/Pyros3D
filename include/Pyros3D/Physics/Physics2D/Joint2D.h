//============================================================================
// Name        : Joint2D
// Description : Ties this object's 2D body to another one, or to the world.
//
//               A body on its own falls, slides and bounces. Most things in
//               a 2D game that feel mechanical are two bodies with a rule
//               between them: a door on a hinge, a platform on a rail, a
//               lantern on a rope, a bridge of planks, a wheel on an axle.
//               Each of those is one of these on one of the two objects.
//
//               It goes on the object that MOVES, next to its Physics2D, and
//               names the other object - or nothing, to be pinned to the
//               world. Like Physics2D it owns nothing: Physics2DWorld builds
//               the joint once both bodies exist and rebuilds it when a
//               setting changes.
//============================================================================

#ifndef JOINT2D_H
#define JOINT2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>

namespace p3d {

	namespace Joint2DType
	{
		enum {
			// A pin: the two bodies share one point and turn about it. A
			// door, a pendulum, a wheel's axle, a link of a chain.
			Hinge = 0,
			// Keeps two points a set distance apart - rigidly (a rod), up
			// to a maximum (a rope), or sprung (a bungee).
			Distance = 1,
			// Glues the two together as they are. With a spring it is a
			// stiff connection that gives: a diving board, a plank bridge.
			Weld = 2,
			// A rail: the body slides along one line and does not turn. A
			// lift, a piston, a sliding door.
			Slider = 3,
			// A slider that also turns freely: a wheel on its suspension.
			Wheel = 4,
			// Pulls its anchor towards a point in the world (or, connected,
			// towards its anchor on the other body), as hard as it is
			// allowed and no harder: dragging a thing with the mouse,
			// something that follows a marker, a magnet. SetTarget() says
			// where; the spring says how eagerly; the motor's force is the
			// most it may pull with.
			Target = 5,
			// Nothing but this: the two bodies do not collide with each
			// other. For parts of one machine that overlap by design.
			Ignore = 6,
			// A rope over two fixed wheels: this body hangs from one ground
			// point, the other body from the other, and what one gives the
			// other takes - times the ratio, for a block and tackle. It is a
			// rope, so it only ever pulls.
			Pulley = 7,
			// Two things that turn, turning together: when this one turns
			// one way the other turns the other, ratio times as far. Each
			// is usually hinged to the world as well - a gear by itself is
			// only the teeth.
			Gear = 8
		};
	}

	// A joint goes on the object that moves and ties that object's body. An
	// object holds one joint; for a second on the same body - a wheel that
	// is hinged and geared, a crate hung from two ropes - put it on a CHILD
	// with no body of its own: such a joint ties the nearest body above it,
	// and its anchor is in that body's space.
	class PYROS3D_API Joint2D : public IComponent {

	public:

		Joint2D(const uint32 type = Joint2DType::Hinge);
		virtual ~Joint2D();
		// How many exist anywhere: see Physics2D::LiveCount.
		static uint32 LiveCount();

		uint32 GetJointType() const { return type; }
		void SetJointType(const uint32 t) { if (t != type) { type = t; dirty = true; haveReference = false; haveSoft = false; softActive = false; } }

		// The NAME of the object whose body this one is tied to. Empty ties
		// it to the world: a point that never moves.
		const std::string &GetConnected() const { return connected; }
		void SetConnected(const std::string &name) { if (name != connected) { connected = name; dirty = true; haveReference = false; } }

		// Where on THIS object the joint acts, in its local space: the hinge
		// pin, the end of the rope.
		const Vec2 &GetAnchor() const { return anchor; }
		void SetAnchor(const Vec2 &a) { if (a.x != anchor.x || a.y != anchor.y) { anchor = a; dirty = true; haveReference = false; } }

		// Where on the OTHER body (its local space; world space when tied to
		// the world). Left automatic, it is wherever this object's anchor
		// happens to be when the joint is made - which is right for a hinge
		// and a weld, where the two points coincide. A rope names both ends.
		bool HasConnectedAnchor() const { return haveConnectedAnchor; }
		const Vec2 &GetConnectedAnchor() const { return connectedAnchor; }
		void SetConnectedAnchor(const Vec2 &a) { connectedAnchor = a; haveConnectedAnchor = true; dirty = true; haveReference = false; }
		void ClearConnectedAnchor() { if (haveConnectedAnchor) { haveConnectedAnchor = false; dirty = true; haveReference = false; } }

		// The line a Slider or a Wheel moves along, in world space as the
		// scene is authored.
		const Vec2 &GetAxis() const { return axis; }
		void SetAxis(const Vec2 &a) { if (a.x != axis.x || a.y != axis.y) { axis = a; dirty = true; haveReference = false; } }

		// Whether the two joined bodies still collide with each other. Off
		// by default: a door overlaps its frame at the hinge.
		bool GetCollideConnected() const { return collideConnected; }
		void SetCollideConnected(const bool c) { if (c != collideConnected) { collideConnected = c; dirty = true; } }

		// Limits. Radians for a Hinge (relative to how the two bodies stand
		// when the joint is made), distance along the axis for a Slider or a
		// Wheel, and the shortest and longest length for a Distance joint.
		bool IsLimitEnabled() const { return limit; }
		f32 GetLowerLimit() const { return lower; }
		f32 GetUpperLimit() const { return upper; }
		void SetLimits(const bool enabled, const f32 lowerLimit, const f32 upperLimit);

		// A motor drives the joint: it turns a Hinge or a Wheel at `speed`
		// radians a second, pushes a Slider at `speed` units a second, with
		// no more than `maxForce` (a torque for the turning ones).
		bool IsMotorEnabled() const { return motor; }
		f32 GetMotorSpeed() const { return motorSpeed; }
		f32 GetMaxMotorForce() const { return maxMotorForce; }
		void SetMotor(const bool enabled, const f32 speed, const f32 maxForce);
		// Speed alone, applied to the running joint without rebuilding it -
		// what a vehicle's throttle or a conveyor's switch calls every frame.
		void SetMotorSpeed(const f32 speed);

		// A spring pulls the joint towards rest rather than holding it
		// rigidly: `hertz` is how stiff (oscillations a second), `damping`
		// how quickly it settles (1 = no bounce).
		bool IsSpringEnabled() const { return spring; }
		f32 GetSpringHertz() const { return hertz; }
		f32 GetSpringDamping() const { return damping; }
		void SetSpring(const bool enabled, const f32 springHertz, const f32 dampingRatio);

		// A Distance joint's rest length. 0 measures it from where the two
		// anchors are when the joint is made.
		// Target: the world point pulled towards. Changing it does not
		// rebuild the joint - it is what a script sets every frame.
		const Vec2 &GetTarget() const { return target; }
		void SetTarget(const Vec2 &t);
		bool HaveTarget() const { return haveTarget; }
		// Pulley: the two fixed points the rope runs over, in the world -
		// this body's, and the other's. Gear and Pulley: how far the other
		// moves for each unit this one does.
		const Vec2 &GetGroundAnchor() const { return groundAnchor; }
		const Vec2 &GetConnectedGroundAnchor() const { return connectedGroundAnchor; }
		void SetGroundAnchors(const Vec2 &mine, const Vec2 &others)
		{ groundAnchor = mine; connectedGroundAnchor = others; dirty = true; haveReference = false; haveSoft = false; }
		f32 GetRatio() const { return ratio; }
		void SetRatio(const f32 r) { const f32 v = (r > 1e-4f || r < -1e-4f) ? r : 1.f; if (v != ratio) { ratio = v; dirty = true; haveSoft = false; } }
		// Pulley and Gear are not the solver's joints: the world holds them
		// itself, step by step. These are its notes - what "at rest" was
		// when it first looked, and whether it has both ends.
		bool IsSoft() const { return type == Joint2DType::Pulley || type == Joint2DType::Gear; }
		bool HaveSoft() const { return haveSoft; }
		void SetSoft(const f32 rest, const f32 angleA, const f32 angleB) { softRest = rest; softAngleA = angleA; softAngleB = angleB; haveSoft = true; }
		void ClearSoft() { haveSoft = false; softActive = false; }
		f32 GetSoftRest() const { return softRest; }
		f32 GetSoftAngleA() const { return softAngleA; }
		f32 GetSoftAngleB() const { return softAngleB; }
		bool IsSoftActive() const { return softActive; }
		void SetSoftActive(const bool a) { softActive = a; }
		// Whether the world has it working: a solver joint made, or - for a
		// pulley or a gear - both of its bodies found.
		bool IsBuilt() const { return IsSoft() ? softActive : haveJoint; }

		f32 GetLength() const { return length; }
		void SetLength(const f32 l) { if (l != length) { length = l; dirty = true; } }

		// --- for Physics2DWorld -------------------------------------------------
		// The pose the joint calls zero, taken when it was first built - see
		// Physics2DWorld::BuildJoint. Forgotten when what the joint ties, or
		// where, changes; ResetReference() forgets it on request, which
		// re-zeroes the joint at wherever the bodies are now.
		bool HaveReference() const { return haveReference; }
		void SetReference(const Vec2 &anchorOnOther, const f32 angleA, const f32 angleB)
		{ referenceAnchor = anchorOnOther; referenceAngleA = angleA; referenceAngleB = angleB; haveReference = true; }
		const Vec2 &GetReferenceAnchor() const { return referenceAnchor; }
		f32 GetReferenceAngleA() const { return referenceAngleA; }
		f32 GetReferenceAngleB() const { return referenceAngleB; }
		void ResetReference() { haveReference = false; dirty = true; }
		bool NeedsRebuild() const { return dirty; }
		void ClearRebuild() { dirty = false; }
		// Opaque b2JointId, split as Physics2D's body handle is.
		void SetJointHandle(const int32 index, const uint16 world, const uint16 generation);
		bool HaveJoint() const { return haveJoint; }
		int32 GetJointIndex() const { return jointIndex; }
		uint16 GetJointWorld() const { return jointWorld; }
		uint16 GetJointGeneration() const { return jointGeneration; }
		void ClearJointHandle() { haveJoint = false; }

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual bool NeedsUpdate() const { return false; }
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Joint2D; }

	private:

		Vec2 target = Vec2(0.f, 0.f);
		bool haveTarget = false;
		Vec2 groundAnchor = Vec2(0.f, 0.f);
		Vec2 connectedGroundAnchor = Vec2(0.f, 0.f);
		f32 ratio = 1.f;
		bool haveSoft = false;
		bool softActive = false;
		f32 softRest = 0.f, softAngleA = 0.f, softAngleB = 0.f;
		uint32 type;
		std::string connected;
		Vec2 anchor;
		Vec2 connectedAnchor;
		bool haveConnectedAnchor;
		Vec2 axis;
		bool collideConnected;
		bool limit;
		f32 lower, upper;
		bool motor;
		f32 motorSpeed, maxMotorForce;
		bool spring;
		f32 hertz, damping;
		f32 length;

		bool haveReference;
		Vec2 referenceAnchor;
		f32 referenceAngleA, referenceAngleB;

		bool dirty;
		bool haveJoint;
		int32 jointIndex;
		uint16 jointWorld;
		uint16 jointGeneration;
	};

};

#endif
