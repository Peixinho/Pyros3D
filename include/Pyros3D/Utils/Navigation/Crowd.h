//============================================================================
// Name        : Crowd.h
// Description : Agents on a plane steering round each other.
//============================================================================
#ifndef CROWD_H
#define CROWD_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <unordered_map>
#include <vector>

namespace p3d {

	// Local avoidance for a crowd on the ground plane (x, z): every agent says
	// which way and how fast it would like to go (SetPreferredVelocity, from a
	// path, a target, a script), and Update() gives each one the velocity
	// nearest that which keeps it clear of the others for a few seconds - the
	// sampled velocity-obstacle method: candidate velocities scored by how far
	// they are from the wanted one plus how soon they would meet a neighbour.
	// Each agent only reads the others' state from the start of the step, so
	// the agents are steered in parallel on the job system.
	//
	// Paths are not its business: PathGrid (or anything) says where to head,
	// this says how to get through the people on the way.
	class PYROS3D_API Crowd {
	public:
		explicit Crowd(const f32 neighbourRadius = 6.f);

		uint32 Add(const Vec2 &position, const f32 radius = 0.3f, const f32 maxSpeed = 1.5f);
		void Remove(const uint32 id);
		bool Has(const uint32 id) const { return index.count(id) != 0; }
		uint32 Count() const { return (uint32)agents.size(); }

		void SetPreferredVelocity(const uint32 id, const Vec2 &v);
		// Where it is now (a script moved it, it was teleported): its velocity is kept.
		void SetPosition(const uint32 id, const Vec2 &p);
		void SetMaxSpeed(const uint32 id, const f32 s);
		// One that others steer round but which is not steered itself (a car, a pillar
		// that moves): it goes at its preferred velocity, whatever is in its way.
		void SetFixed(const uint32 id, const bool fixed);

		Vec2 GetPosition(const uint32 id) const;
		Vec2 GetVelocity(const uint32 id) const;

		// Steer every agent and move it by its new velocity over dt.
		void Update(const f32 dt);

		// How far ahead a meeting counts (seconds).
		void SetTimeHorizon(const f32 seconds) { horizon = seconds > 0.1f ? seconds : 0.1f; }

	private:
		struct Agent {
			uint32 id;
			Vec2 position, velocity, preferred;
			f32 radius, maxSpeed;
			bool fixed;
		};
		std::vector<Agent> agents;
		std::unordered_map<uint32, uint32> index;      // id -> place in agents
		uint32 nextId = 1;
		f32 neighbourRadius, horizon = 2.5f;
		Agent* Find(const uint32 id);
		const Agent* Find(const uint32 id) const;
	};
}

#endif /* CROWD_H */
