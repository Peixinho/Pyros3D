//============================================================================
// Name        : CharacterController.h
// Description : A character walking through the physics world, not pushed by it.
//============================================================================
#ifndef CHARACTERCONTROLLER_H
#define CHARACTERCONTROLLER_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	class IPhysics;

	// Moves its object as a player's or an NPC's body moves: an upright capsule, standing on the
	// object's position, that walks where it is told and is stopped (and slid along) by what is
	// solid - not shoved about by the simulation as a rigid body would be. Up a step as high as
	// it can step, up slopes as steep as it can climb (and not steeper - it slides off), down
	// both staying on the ground rather than walking off into the air, gravity and jumping.
	// Box3D's mover: collide the capsule with the world, solve the planes it touches, cast it the
	// way that leaves, a few times over.
	//
	// The game says how fast it wants to go (SetMoveVelocity, horizontally, every frame) and when
	// to jump; it says whether it is on the ground and how fast it really went. Runs once the
	// game does. Sensors (trigger volumes) are walked through.
	class PYROS3D_API CharacterController : public IComponent {
	public:
		explicit CharacterController(IPhysics* physics = NULL);
		virtual ~CharacterController() {}

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}
		virtual uint32 GetComponentType() const { return ComponentType::CharacterController; }

		void SetPhysics(IPhysics* p) { physics = p; }

		// The capsule: its radius and its whole height (feet to head), standing on the object's position.
		void SetShape(const f32 radius, const f32 height) { this->radius = radius > 0.05f ? radius : 0.05f; this->height = height > this->radius * 2.f ? height : this->radius * 2.f; }
		f32 GetRadius() const { return radius; }
		f32 GetHeight() const { return height; }
		void SetStepHeight(const f32 h) { stepHeight = h > 0.f ? h : 0.f; }
		f32 GetStepHeight() const { return stepHeight; }
		// Degrees: steeper than this is a wall to slide off, not ground to stand on.
		void SetMaxSlope(const f32 degrees) { maxSlope = degrees; }
		f32 GetMaxSlope() const { return maxSlope; }
		void SetGravity(const f32 g) { gravity = g; }
		f32 GetGravity() const { return gravity; }

		// Metres a second along the ground (y ignored), from now on until told otherwise.
		void SetMoveVelocity(const Vec3 &v) { move = Vec3(v.x, 0.f, v.z); }
		// Off the ground at this speed (up), if it is on it.
		void Jump(const f32 speed) { jumpWanted = speed; }
		// Put somewhere (a spawn, a teleport): no collision on the way.
		void Teleport(const Vec3 &p);

		bool IsGrounded() const { return grounded; }
		const Vec3 &GetVelocity() const { return velocity; }       // what it really did, last step
		const Vec3 &GetGroundNormal() const { return groundNormal; }

		void SetRunning(const bool r) { running = r; if (!r) { verticalSpeed = 0.f; } }
		bool IsRunning() const { return running; }

	private:
		bool Step(Vec3 &pos, const Vec3 &delta, const bool walking = false);   // collide-solve-cast round trips; true when it got there
		bool Probe(const Vec3 &pos, const f32 down, f32 &dropped, Vec3 &normal) const;

		IPhysics* physics;
		f32 radius, height, stepHeight, maxSlope, gravity, verticalSpeed, jumpWanted;
		Vec3 move, velocity, groundNormal;
		bool grounded, running;
		f64 lastTime;
	};
}

#endif /* CHARACTERCONTROLLER_H */
