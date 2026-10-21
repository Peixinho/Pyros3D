//============================================================================
// Name        : NavAgentComponent.cpp
// Description : A character that walks itself to where it is told.
//============================================================================

#include <Pyros3D/Utils/Navigation/NavAgentComponent.h>
#include <Pyros3D/Utils/Navigation/NavMesh.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <cmath>

namespace p3d {

	namespace {
		f32 WrapPi(f32 a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
	}

	NavAgentComponent::NavAgentComponent()
		: registeredIn(NULL), joined(NULL), agentId(-1), maxSpeed(3.5f), radius(0.35f), acceleration(8.f), stoppingDistance(0.2f),
		  yawOffset(0.f), turnSpeed(8.f), currentSpeed(0.f), remaining(0.f),
		  moving(false), reachable(false), faceMovement(true), running(false), onLink(false), wantsTarget(false), lastTime(-1.0)
	{
	}

	NavAgentComponent::~NavAgentComponent() { Leave(); }

	void NavAgentComponent::Register(SceneGraph* Scene) { registeredIn = Scene; }

	NavMesh* NavAgentComponent::SceneNav() const { return registeredIn ? registeredIn->GetNavMesh() : NULL; }

	void NavAgentComponent::Leave()
	{
		// (only from the mesh it joined, and only while that is still the scene's: a rebaked mesh
		// took its old agents with it)
		if (agentId >= 0 && joined && joined == SceneNav()) joined->RemoveAgent(agentId);
		agentId = -1; joined = NULL;
		onLink = false;
	}

	void NavAgentComponent::SetRunning(const bool r)
	{
		if (r == running) return;
		running = r;
		// Out of the crowd when stopped; back in, where the object is, when it runs again (the
		// editor's Stop put the object back where it was).
		Leave();
		if (!r) { moving = false; currentSpeed = 0.f; }
	}

	bool NavAgentComponent::SetDestination(const Vec3 &p)
	{
		destination = p;
		moving = true;
		wantsTarget = true;
		// (the way there, asked now for the answer; the crowd plans its own as it walks)
		NavMesh* nav = SceneNav();
		GameObject* owner = GetOwner();
		std::vector<Vec3> pts;
		reachable = nav && owner && nav->FindPath(owner->GetWorldPosition(), p, pts);
		if (agentId >= 0 && joined) { wantsTarget = !joined->SetAgentTarget(agentId, p); }
		return reachable;
	}

	void NavAgentComponent::Stop()
	{
		moving = false; wantsTarget = false;
		if (agentId >= 0 && joined) joined->StopAgent(agentId);
	}

	f32 NavAgentComponent::GetRemainingDistance() const { return moving ? remaining : 0.f; }

	void NavAgentComponent::Update(const f64 time)
	{
		f32 dt = lastTime >= 0.0 ? (f32)(time - lastTime) : 0.f;
		lastTime = time;
		if (!running || dt <= 0.f) return;
		if (dt > 0.1f) dt = 0.1f;
		GameObject* owner = GetOwner();
		NavMesh* nav = SceneNav();
		if (!owner || !nav || !nav->IsBuilt()) return;
		if (joined != nav) { agentId = -1; joined = NULL; }            // (a new mesh: join it)
		if (agentId < 0)
		{
			agentId = nav->AddAgent(owner->GetWorldPosition(), radius, 1.8f, maxSpeed, acceleration);
			if (agentId < 0) return;
			joined = nav;
			if (moving) wantsTarget = true;
		}
		nav->SetAgentLimits(agentId, radius, maxSpeed, acceleration);
		if (wantsTarget && moving) wantsTarget = !nav->SetAgentTarget(agentId, destination);
		// The crowd moves everyone, once a frame, by whoever is first.
		if (registeredIn->TakeCrowdStep(time)) nav->UpdateAgents(dt);

		Vec3 pos, vel;
		bool planned = false;
		if (!nav->GetAgent(agentId, pos, vel, onLink, remaining, planned)) return;
		currentSpeed = sqrtf(vel.x * vel.x + vel.z * vel.z);
		if (moving && planned && !wantsTarget && !onLink && remaining <= stoppingDistance) Stop();
		owner->SetPosition(owner->GetParent() ? owner->GetParent()->GetWorldTransformation().Inverse() * pos : pos);
		if (faceMovement && currentSpeed > 0.15f)
		{
			// (forward is -Z; yawOffset turns a model made facing elsewhere)
			const f32 target = atan2f(-vel.x, -vel.z) + yawOffset;
			Vec3 rot = owner->GetRotation();
			const f32 diff = WrapPi(target - rot.y), step = turnSpeed * dt;
			rot.y = WrapPi(rot.y + (fabsf(diff) <= step ? diff : (diff > 0.f ? step : -step)));
			owner->SetRotation(rot);
		}
	}
}
