//============================================================================
// Name        : Crowd.cpp
// Description : Agents on a plane steering round each other.
//============================================================================

#include <Pyros3D/Utils/Navigation/Crowd.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace p3d {

	namespace {
		// When would two discs, |p| apart and closing at v, first touch (within r)? Infinity if never.
		f32 TimeToCollision(const Vec2 &p, const Vec2 &v, const f32 r)
		{
			const f32 c = p.x * p.x + p.y * p.y - r * r;
			if (c < 0.f) return 0.f;                         // (already overlapping)
			const f32 a = v.x * v.x + v.y * v.y;
			const f32 b = p.x * v.x + p.y * v.y;
			if (b >= 0.f || a < 1e-8f) return 1e30f;         // (moving apart, or not at all)
			const f32 d = b * b - a * c;
			if (d < 0.f) return 1e30f;
			return (-b - sqrtf(d)) / a;
		}
		uint64 CellKey(const int32 x, const int32 z) { return ((uint64)(uint32)x << 32) | (uint64)(uint32)z; }
	}

	Crowd::Crowd(const f32 neighbourRadius) : neighbourRadius(neighbourRadius > 0.5f ? neighbourRadius : 0.5f) {}

	Crowd::Agent* Crowd::Find(const uint32 id)
	{
		std::unordered_map<uint32, uint32>::iterator it = index.find(id);
		return it == index.end() ? NULL : &agents[it->second];
	}
	const Crowd::Agent* Crowd::Find(const uint32 id) const
	{
		std::unordered_map<uint32, uint32>::const_iterator it = index.find(id);
		return it == index.end() ? NULL : &agents[it->second];
	}

	uint32 Crowd::Add(const Vec2 &position, const f32 radius, const f32 maxSpeed)
	{
		Agent a;
		a.id = nextId++;
		a.position = position; a.velocity = Vec2(0.f, 0.f); a.preferred = Vec2(0.f, 0.f);
		a.radius = radius > 0.01f ? radius : 0.01f; a.maxSpeed = maxSpeed > 0.f ? maxSpeed : 0.f; a.fixed = false;
		index[a.id] = (uint32)agents.size();
		agents.push_back(a);
		return a.id;
	}

	void Crowd::Remove(const uint32 id)
	{
		std::unordered_map<uint32, uint32>::iterator it = index.find(id);
		if (it == index.end()) return;
		const uint32 at = it->second, last = (uint32)agents.size() - 1;
		if (at != last) { agents[at] = agents[last]; index[agents[at].id] = at; }
		agents.pop_back();
		index.erase(id);
	}

	void Crowd::SetPreferredVelocity(const uint32 id, const Vec2 &v) { if (Agent* a = Find(id)) a->preferred = v; }
	void Crowd::SetPosition(const uint32 id, const Vec2 &p) { if (Agent* a = Find(id)) a->position = p; }
	void Crowd::SetMaxSpeed(const uint32 id, const f32 s) { if (Agent* a = Find(id)) a->maxSpeed = s > 0.f ? s : 0.f; }
	void Crowd::SetFixed(const uint32 id, const bool fixed) { if (Agent* a = Find(id)) a->fixed = fixed; }
	Vec2 Crowd::GetPosition(const uint32 id) const { const Agent* a = Find(id); return a ? a->position : Vec2(0.f, 0.f); }
	Vec2 Crowd::GetVelocity(const uint32 id) const { const Agent* a = Find(id); return a ? a->velocity : Vec2(0.f, 0.f); }

	void Crowd::Update(const f32 dt)
	{
		const uint32 n = (uint32)agents.size();
		if (n == 0 || dt <= 0.f) return;
		// Neighbours by a hash of cells a neighbour radius across: each agent looks in its own and the eight round it.
		const f32 cell = neighbourRadius;
		std::unordered_map<uint64, std::vector<uint32> > grid;
		grid.reserve(n);
		for (uint32 i = 0; i < n; i++)
			grid[CellKey((int32)floorf(agents[i].position.x / cell), (int32)floorf(agents[i].position.y / cell))].push_back(i);

		std::vector<Vec2> chosen(n);
		const std::vector<Agent> &A = agents;                  // (read-only below: every agent reads the step's start)
		const f32 hz = horizon, nr2 = neighbourRadius * neighbourRadius;
		JobSystem::Instance().ParallelFor(n, 16, [&](const uint32 begin, const uint32 end) {
			std::vector<uint32> near;
			for (uint32 i = begin; i < end; i++)
			{
				const Agent &me = A[i];
				if (me.fixed || me.maxSpeed <= 0.f) { chosen[i] = me.fixed ? me.preferred : Vec2(0.f, 0.f); continue; }
				near.clear();
				const int32 cx = (int32)floorf(me.position.x / cell), cz = (int32)floorf(me.position.y / cell);
				for (int32 dz = -1; dz <= 1; dz++)
					for (int32 dx = -1; dx <= 1; dx++)
					{
						std::unordered_map<uint64, std::vector<uint32> >::const_iterator g = grid.find(CellKey(cx + dx, cz + dz));
						if (g == grid.end()) continue;
						for (uint32 k : g->second)
						{
							if (k == i) continue;
							const Vec2 d = A[k].position - me.position;
							if (d.x * d.x + d.y * d.y < nr2) near.push_back(k);
						}
					}
				Vec2 pref = me.preferred;
				const f32 pl = sqrtf(pref.x * pref.x + pref.y * pref.y);
				if (pl > me.maxSpeed && pl > 0.f) pref = pref * (me.maxSpeed / pl);
				if (near.empty()) { chosen[i] = pref; continue; }
				// (the nearest ten: further ones are dodged when they are near - a packed crowd had
				// thirty-odd each, and every one of sixty-five candidates was tried against them all)
				const uint32 kMaxNeighbours = 10;
				if (near.size() > kMaxNeighbours)
				{
					std::nth_element(near.begin(), near.begin() + kMaxNeighbours, near.end(), [&](const uint32 a, const uint32 b) {
						const Vec2 da = A[a].position - me.position, db = A[b].position - me.position;
						return da.x * da.x + da.y * da.y < db.x * db.x + db.y * db.y; });
					near.resize(kMaxNeighbours);
				}
				// (the way it wants, if nothing is in it: most of a crowd most of the time)
				{
					f32 tPref = 1e30f;
					for (uint32 k : near)
					{
						const Agent &o = A[k];
						const Vec2 mine = o.fixed ? pref : (pref * 2.f - me.velocity);
						const f32 t = TimeToCollision(o.position - me.position, o.velocity - mine, me.radius + o.radius);
						if (t < tPref) tPref = t;
					}
					if (tPref >= hz) { chosen[i] = pref; continue; }
				}
				// Candidates: the wanted velocity, and rings of others round it up to the top speed.
				// Each scored by its distance from the wanted one plus a penalty for how soon it meets
				// someone (sharing the turn with another agent: each takes half the dodge - reciprocal).
				Vec2 best = pref; f32 bestScore = 1e30f;
				const int kRings = 4, kDirs = 16;
				for (int ring = 0; ring <= kRings; ring++)
				{
					const f32 speed = me.maxSpeed * (f32)ring / (f32)kRings;
					const int dirs = ring == 0 ? 1 : kDirs;
					for (int d = 0; d < dirs; d++)
					{
						Vec2 cand;
						if (ring == 0) cand = pref;
						else { const f32 a = 6.2831853f * (f32)d / (f32)dirs; cand = Vec2(cosf(a) * speed, sinf(a) * speed); }
						f32 tMin = 1e30f;
						for (uint32 k : near)
						{
							const Agent &o = A[k];
							// The gap closes at (other's velocity - mine). Against a steered agent this one
							// takes half the dodge (reciprocal: as if it moved at 2*cand - its velocity now);
							// against a fixed one, all of it.
							const Vec2 mine = o.fixed ? cand : (cand * 2.f - me.velocity);
							const f32 t = TimeToCollision(o.position - me.position, o.velocity - mine, me.radius + o.radius);
							if (t < tMin) tMin = t;
						}
						const Vec2 off = cand - pref;
						const f32 score = sqrtf(off.x * off.x + off.y * off.y)
							+ (tMin < hz ? 2.5f * (hz - tMin) / hz * me.maxSpeed + (tMin <= 0.f ? 10.f : 0.f) : 0.f);
						if (score < bestScore) { bestScore = score; best = cand; }
					}
				}
				chosen[i] = best;
			}
		});
		for (uint32 i = 0; i < n; i++)
		{
			agents[i].velocity = chosen[i];
			agents[i].position = agents[i].position + chosen[i] * dt;
		}
		// What steering left overlapping (a crowd packed tighter than it can dodge): pushed apart,
		// each of a pair half the overlap - a fixed agent not at all, its partner all of it.
		for (int pass = 0; pass < 2; pass++)
			for (uint32 i = 0; i < n; i++)
			{
				const int32 cx = (int32)floorf(agents[i].position.x / cell), cz = (int32)floorf(agents[i].position.y / cell);
				for (int32 dz = -1; dz <= 1; dz++)
					for (int32 dx = -1; dx <= 1; dx++)
					{
						std::unordered_map<uint64, std::vector<uint32> >::const_iterator g = grid.find(CellKey(cx + dx, cz + dz));
						if (g == grid.end()) continue;
						for (uint32 k : g->second)
						{
							if (k <= i) continue;
							Agent &a = agents[i], &b = agents[k];
							Vec2 d = b.position - a.position;
							const f32 l = sqrtf(d.x * d.x + d.y * d.y), r = a.radius + b.radius;
							if (l >= r) continue;
							if (l < 1e-5f) d = Vec2(1.f, 0.f); else d = d * (1.f / l);
							const f32 over = r - (l < 1e-5f ? 0.f : l);
							if (a.fixed && b.fixed) continue;
							const f32 wa = a.fixed ? 0.f : (b.fixed ? 1.f : 0.5f), wb = 1.f - wa;
							a.position = a.position - d * (over * wa);
							b.position = b.position + d * (over * wb);
						}
					}
			}
	}
}
