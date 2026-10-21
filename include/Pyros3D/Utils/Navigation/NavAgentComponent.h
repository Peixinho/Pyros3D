//============================================================================
// Name        : NavAgentComponent.h
// Description : A character that walks itself to where it is told.
//============================================================================
#ifndef NAVAGENTCOMPONENT_H
#define NAVAGENTCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <vector>

namespace p3d {

	class NavMesh;

	// Give it a destination and the object walks there: on the scene's navigation mesh
	// (SceneGraph::GetNavMesh - the one the editor baked), as one of its crowd (NavMesh's agents,
	// Detour's crowd: round walls and each other, a corridor kept and shortened as it goes), on
	// the mesh at the ground's height, turned to face where it goes. A link on the way (a jump, a
	// ladder) is crossed by the crowd; IsOnLink() says so, for the animation. GetSpeed() is for
	// the walk/run blend (an Animator's "speed").
	//
	// Runs once the game does, like an Animator. Moves the object itself: what else moves it (a
	// script, physics) should not, while it has somewhere to go.
	class PYROS3D_API NavAgentComponent : public IComponent {
	public:
		NavAgentComponent();
		virtual ~NavAgentComponent();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() { Leave(); }
		virtual void Unregister(SceneGraph* Scene) { Leave(); registeredIn = NULL; }
		virtual uint32 GetComponentType() const { return ComponentType::NavAgent; }

		// Where to go (false: it has no way there - it walks as near as the mesh allows).
		bool SetDestination(const Vec3 &p);
		void Stop();                                   // stands where it is
		bool HasDestination() const { return moving; }
		const Vec3 &GetDestination() const { return destination; }
		bool IsMoving() const { return moving; }
		// True if the last path reaches the destination (not only as near as it gets).
		bool IsReachable() const { return reachable; }
		f32 GetRemainingDistance() const;
		f32 GetSpeed() const { return currentSpeed; }
		bool IsOnLink() const { return onLink; }

		// --- How it moves ---
		void SetMaxSpeed(const f32 s) { maxSpeed = s > 0.f ? s : 0.f; }
		void SetRadius(const f32 r) { radius = r > 0.05f ? r : 0.05f; }
		void SetAcceleration(const f32 a) { acceleration = a > 0.1f ? a : 0.1f; }
		// How near the destination counts as there.
		void SetStoppingDistance(const f32 d) { stoppingDistance = d > 0.f ? d : 0.f; }
		// Turn to face the way it walks; offset (radians) for a model not made facing -Z.
		void SetFaceMovement(const bool on, const f32 yawOffset = 0.f) { faceMovement = on; this->yawOffset = yawOffset; }
		void SetTurnSpeed(const f32 radiansPerSecond) { turnSpeed = radiansPerSecond > 0.f ? radiansPerSecond : 0.f; }
		f32 GetMaxSpeed() const { return maxSpeed; }
		f32 GetRadius() const { return radius; }
		f32 GetAcceleration() const { return acceleration; }
		f32 GetStoppingDistance() const { return stoppingDistance; }
		bool GetFaceMovement() const { return faceMovement; }
		f32 GetYawOffset() const { return yawOffset; }
		f32 GetTurnSpeed() const { return turnSpeed; }

		void SetRunning(const bool r);
		bool IsRunning() const { return running; }

	private:
		void Leave();
		NavMesh* SceneNav() const;

		SceneGraph* registeredIn;
		NavMesh* joined;                                // (the mesh its agent is on: a rebake is a new one)
		int32 agentId;
		Vec3 destination;
		f32 maxSpeed, radius, acceleration, stoppingDistance, yawOffset, turnSpeed, currentSpeed, remaining;
		bool moving, reachable, faceMovement, running, onLink, wantsTarget;
		f64 lastTime;
	};
}

#endif /* NAVAGENTCOMPONENT_H */
