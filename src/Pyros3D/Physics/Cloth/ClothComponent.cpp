//============================================================================
// Name        : ClothComponent.cpp
// Description : Cloth: the object's own mesh, simulated.
//============================================================================

#include <Pyros3D/Physics/Cloth/ClothComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <tuple>
#include <unordered_map>

namespace p3d {

	namespace {
		// A stiffness 0..1 as XPBD compliance (metres per newton for unit-mass particles), on a log
		// scale: 1 rigid, 0.5 about 1e-4, 0 about 1.
		f32 Compliance(const f32 s) { return s >= 1.f ? 0.f : powf(10.f, -8.f * s); }
		// Wind that gusts and swirls: a few sines of time and place (cheap, smooth, never repeats soon).
		Vec3 Gust(const Vec3 &w, const f32 turbulence, const f32 t, const Vec3 &p)
		{
			if (turbulence <= 0.f) return w;
			const f32 g = 1.f + turbulence * 0.5f * (sinf(t * 1.3f + p.x * 0.35f) * 0.6f + sinf(t * 3.1f + p.z * 0.5f + p.y * 0.7f) * 0.4f);
			const f32 l = w.magnitude();
			const Vec3 swirl(sinf(t * 2.3f + p.y * 1.7f), sinf(t * 1.7f + p.x * 1.3f) * 0.5f, sinf(t * 2.9f + p.z * 1.9f));
			return w * g + swirl * (l * turbulence * 0.35f);
		}
		Vec3 ClosestOnSegment(const Vec3 &a, const Vec3 &b, const Vec3 &p)
		{
			const Vec3 ab = b - a;
			const f32 l2 = ab.dotProduct(ab);
			if (l2 < 1e-12f) return a;
			f32 t = (p - a).dotProduct(ab) / l2;
			t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			return a + ab * t;
		}
	}

	ClothComponent::ClothComponent()
		: rc(NULL), built(false), running(false), hasLast(false), lastTime(0.0), simTime(0.0),
		  stretch(1.f), bend(0.3f), density(0.2f), damping(0.15f), thickness(0.02f), friction(0.4f), drag(1.f), turbulence(0.5f), groundY(0.f),
		  substeps(12), gravity(0.f, -9.81f, 0.f), wind(0.f, 0.f, 0.f), ground(true)
	{
	}

	void ClothComponent::PinSide(const std::string &side, const f32 band)
	{
		if (side.size() != 2 || (side[0] != '+' && side[0] != '-') || side[1] < 'x' || side[1] > 'z') return;
		pinSides.push_back(std::make_pair(side, band < 0.f ? 0.f : band));
		if (built) ApplyPins();
	}
	void ClothComponent::PinVertex(const uint32 meshVertex) { pinVertices.push_back(meshVertex); if (built) ApplyPins(); }
	void ClothComponent::ClearPins() { pinSides.clear(); pinVertices.clear(); if (built) ApplyPins(); }

	uint32 ClothComponent::GetPinnedCount() const
	{
		uint32 n = 0;
		for (size_t i = 0; i < pinned.size(); i++) n += pinned[i] ? 1 : 0;
		return n;
	}

	int32 ClothComponent::GetParticleOf(const uint32 mesh, const uint32 vertex) const
	{
		if (mesh >= pieces.size() || vertex >= pieces[mesh].particleOf.size()) return -1;
		return (int32)pieces[mesh].particleOf[vertex];
	}

	uint32 ClothComponent::AddSphere(const Vec3 &centre, const f32 radius, const uint32 space)
	{
		Collider c; c.capsule = false; c.a = c.b = centre; c.radius = radius; c.space = space > Parent ? World : space;
		colliders.push_back(c);
		return (uint32)colliders.size() - 1;
	}
	uint32 ClothComponent::AddCapsule(const Vec3 &a, const Vec3 &b, const f32 radius, const uint32 space)
	{
		Collider c; c.capsule = true; c.a = a; c.b = b; c.radius = radius; c.space = space > Parent ? World : space;
		colliders.push_back(c);
		return (uint32)colliders.size() - 1;
	}
	void ClothComponent::SetSphere(const uint32 i, const Vec3 &centre, const f32 radius)
	{
		if (i >= colliders.size()) return;
		colliders[i].capsule = false; colliders[i].a = colliders[i].b = centre; colliders[i].radius = radius;
	}
	void ClothComponent::SetCapsule(const uint32 i, const Vec3 &a, const Vec3 &b, const f32 radius)
	{
		if (i >= colliders.size()) return;
		colliders[i].capsule = true; colliders[i].a = a; colliders[i].b = b; colliders[i].radius = radius;
	}

	Matrix ClothComponent::SpaceMatrix(const uint32 space) const
	{
		GameObject* owner = const_cast<ClothComponent*>(this)->GetOwner();
		if (space == Owner && owner) return owner->GetWorldTransformation();
		if (space == Parent && owner && owner->GetParent()) return owner->GetParent()->GetWorldTransformation();
		return Matrix();
	}

	bool ClothComponent::Build()
	{
		GameObject* owner = GetOwner();
		if (!owner) return false;
		rc = NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = owner->GetComponents();
		for (size_t i = 0; i < comps.size() && !rc; i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
				rc = static_cast<RenderingComponent*>(comps[i].get());
		if (!rc) return false;

		pieces.clear(); rest.clear(); tris.clear(); stretchC.clear(); bendC.clear();
		// Welding: vertices at one place (a seam, a hard edge's split normals) are one particle,
		// or the cloth would come apart along them.
		std::map<std::tuple<int64, int64, int64>, uint32> weld;
		for (RenderingMesh* m : rc->GetMeshes(0))
		{
			// (a skinned mesh is moved by its bones after this; cloth on a character is its own
			// mesh, a child of the character, with capsules on its parent's bones)
			ModelGeometry* skinned = m ? dynamic_cast<ModelGeometry*>(m->Geometry) : NULL;
			if (skinned && !skinned->tBonesID.empty())
			{
				if (!warnedSkinned) { fprintf(stderr, "WARNING: Cloth on %s: a skinned mesh cannot be cloth; make the cloth a separate mesh\n", owner->GetName().c_str()); warnedSkinned = true; }
				continue;
			}
			ModelGeometry* g = rc->OwnGeometry(m);
			if (!g || g->tVertex.empty() || g->index.size() < 3) continue;
			Piece piece; piece.geometry = g;
			piece.particleOf.resize(g->tVertex.size());
			for (size_t vtx = 0; vtx < g->tVertex.size(); vtx++)
			{
				const Vec3 &p = g->tVertex[vtx];
				const std::tuple<int64, int64, int64> key((int64)llroundf(p.x * 1e4f), (int64)llroundf(p.y * 1e4f), (int64)llroundf(p.z * 1e4f));
				std::map<std::tuple<int64, int64, int64>, uint32>::iterator it = weld.find(key);
				if (it == weld.end()) { it = weld.insert(std::make_pair(key, (uint32)rest.size())).first; rest.push_back(p); }
				piece.particleOf[vtx] = it->second;
			}
			for (size_t k = 0; k + 2 < g->index.size(); k += 3)
			{
				const uint32 a = piece.particleOf[g->index[k]], b = piece.particleOf[g->index[k + 1]], c = piece.particleOf[g->index[k + 2]];
				if (a == b || b == c || a == c) continue;
				tris.push_back(a); tris.push_back(b); tris.push_back(c);
			}
			pieces.push_back(std::move(piece));
		}
		if (rest.empty() || tris.empty()) { pieces.clear(); unusable = true; return false; }

		// Edges (stretch) and, for each edge two triangles share, their far corners (bend).
		std::unordered_map<uint64, uint32> edgeFirstOpposite;
		for (size_t t = 0; t < tris.size(); t += 3)
			for (int e = 0; e < 3; e++)
			{
				uint32 a = tris[t + e], b = tris[t + (e + 1) % 3];
				const uint32 opp = tris[t + (e + 2) % 3];
				if (a > b) std::swap(a, b);
				const uint64 key = ((uint64)a << 32) | b;
				std::unordered_map<uint64, uint32>::iterator it = edgeFirstOpposite.find(key);
				if (it == edgeFirstOpposite.end())
				{
					edgeFirstOpposite.insert(std::make_pair(key, opp));
					Distance d; d.a = a; d.b = b; d.rest = rest[a].distance(rest[b]);
					stretchC.push_back(d);
				}
				else if (it->second != opp && it->second != 0xffffffffu)
				{
					Distance d; d.a = it->second; d.b = opp; d.rest = rest[d.a].distance(rest[d.b]);
					bendC.push_back(d);
					it->second = 0xffffffffu;             // (a third triangle on one edge adds nothing)
				}
			}

		const size_t n = rest.size();
		x.assign(n, Vec3()); prev.assign(n, Vec3()); v.assign(n, Vec3()); normal.assign(n, Vec3(0.f, 1.f, 0.f)); force.assign(n, Vec3());
		pinned.assign(n, 0);
		// Each particle weighs a third of the triangles round it (at the cloth's weight a square
		// metre): a particle a kilo made a flag of 600 that no wind could lift.
		mass.assign(n, 0.f);
		for (size_t t = 0; t < tris.size(); t += 3)
		{
			const f32 area = (rest[tris[t + 1]] - rest[tris[t]]).cross(rest[tris[t + 2]] - rest[tris[t]]).magnitude() * 0.5f;
			for (int k = 0; k < 3; k++) mass[tris[t + k]] += area * density / 3.f;
		}
		for (size_t i = 0; i < n; i++) if (mass[i] < 1e-6f) mass[i] = 1e-6f;
		invMass.assign(n, 0.f);
		built = true;
		ApplyPins();
		Reset();
		if (getenv("PYROS_CLOTH_TRACE"))
			fprintf(stderr, "[cloth] %s: %u particles, %u stretch, %u bend, %u pinned, %u pieces\n", owner->GetName().c_str(),
				(unsigned)n, (unsigned)stretchC.size(), (unsigned)bendC.size(), GetPinnedCount(), (unsigned)pieces.size());
		return true;
	}

	void ClothComponent::ApplyPins()
	{
		const size_t n = rest.size();
		pinned.assign(n, 0);
		if (n == 0) return;
		Vec3 lo = rest[0], hi = rest[0];
		for (size_t i = 1; i < n; i++)
		{
			lo.x = fminf(lo.x, rest[i].x); lo.y = fminf(lo.y, rest[i].y); lo.z = fminf(lo.z, rest[i].z);
			hi.x = fmaxf(hi.x, rest[i].x); hi.y = fmaxf(hi.y, rest[i].y); hi.z = fmaxf(hi.z, rest[i].z);
		}
		for (const std::pair<std::string, f32> &s : pinSides)
		{
			const int axis = s.first[1] - 'x';
			const f32 l = axis == 0 ? lo.x : (axis == 1 ? lo.y : lo.z), h = axis == 0 ? hi.x : (axis == 1 ? hi.y : hi.z);
			const f32 tol = (h - l) * s.second + 1e-5f;
			for (size_t i = 0; i < n; i++)
			{
				const f32 c = axis == 0 ? rest[i].x : (axis == 1 ? rest[i].y : rest[i].z);
				if (s.first[0] == '+' ? c >= h - tol : c <= l + tol) pinned[i] = 1;
			}
		}
		for (uint32 mv : pinVertices)
			for (const Piece &p : pieces) if (mv < p.particleOf.size()) { pinned[p.particleOf[mv]] = 1; break; }
		for (size_t i = 0; i < n; i++) invMass[i] = pinned[i] || i >= mass.size() ? 0.f : 1.f / mass[i];
	}

	void ClothComponent::Reset()
	{
		GameObject* owner = GetOwner();
		if (!built || !owner) return;
		const Matrix &w = owner->GetWorldTransformation();
		for (size_t i = 0; i < rest.size(); i++) { x[i] = prev[i] = w * rest[i]; v[i] = Vec3(); }
		lastWorld = w; hasLast = true;
	}

	void ClothComponent::Step(const f32 h, const f32 t, const f32 alongFrame)
	{
		GameObject* owner = GetOwner();
		const size_t n = x.size();
		const Matrix &w = owner->GetWorldTransformation();

		// Wind: each triangle pushed by how square-on it meets the air moving past it, the force
		// shared by its corners (a sail's drag; the same push lifts a flag into the wind).
		for (size_t i = 0; i < n; i++) force[i] = Vec3();
		if (drag > 0.f)
			for (size_t k = 0; k < tris.size(); k += 3)
			{
				const uint32 a = tris[k], b = tris[k + 1], c = tris[k + 2];
				const Vec3 cr = (x[b] - x[a]).cross(x[c] - x[a]);
				const f32 twiceArea = cr.magnitude();
				if (twiceArea < 1e-10f) continue;
				const Vec3 nrm = cr / twiceArea;
				const Vec3 centre = (x[a] + x[b] + x[c]) / 3.f;
				const Vec3 air = Gust(wind, turbulence, t, centre);
				const Vec3 rel = (v[a] + v[b] + v[c]) / 3.f - air;
				const f32 vn = rel.dotProduct(nrm);
				// (F = 1/2 rho Cd A vn|vn| along the normal, air at 1.2 kg/m3, shared by three corners)
				const Vec3 f = nrm * (-0.5f * 1.2f * drag * vn * fabsf(vn) * (twiceArea * 0.5f) / 3.f);
				force[a] += f; force[b] += f; force[c] += f;
			}

		// Predict.
		for (size_t i = 0; i < n; i++)
		{
			prev[i] = x[i];
			if (pinned[i])
			{
				// (pins move with the object, eased across the frame's steps: a car's flag would
				// otherwise jerk once a frame and ring)
				const Vec3 from = lastWorld * rest[i], to = w * rest[i];
				x[i] = from + (to - from) * alongFrame;
				continue;
			}
			v[i] += (gravity + force[i] * invMass[i]) * h;
			x[i] += v[i] * h;
		}

		// Constraints (one Gauss-Seidel pass a step: many small steps beat many passes of one big one).
		const f32 aS = Compliance(stretch) / (h * h), aB = Compliance(bend) / (h * h);
		for (int pass = 0; pass < 2; pass++)
		{
			const std::vector<Distance> &cs = pass == 0 ? stretchC : bendC;
			const f32 alpha = pass == 0 ? aS : aB;
			for (const Distance &d : cs)
			{
				const f32 wa = invMass[d.a], wb = invMass[d.b], ws = wa + wb;
				if (ws <= 0.f) continue;
				Vec3 delta = x[d.b] - x[d.a];
				const f32 len = delta.magnitude();
				if (len < 1e-9f) continue;
				const f32 c = len - d.rest;
				const f32 lambda = -c / (ws + alpha);
				delta = delta / len;
				x[d.a] -= delta * (lambda * wa);
				x[d.b] += delta * (lambda * wb);
			}
		}

		// Collisions: pushed out to the surface plus the cloth's thickness, the slide along it slowed.
		std::vector<Collider> world(colliders);
		for (Collider &c : world) { if (c.space != World) { const Matrix m = SpaceMatrix(c.space); c.a = m * c.a; c.b = m * c.b; } }
		for (size_t i = 0; i < n; i++)
		{
			if (pinned[i]) continue;
			bool touched = false; Vec3 nrm; f32 depth = 0.f;
			for (const Collider &c : world)
			{
				const Vec3 centre = c.capsule ? ClosestOnSegment(c.a, c.b, x[i]) : c.a;
				Vec3 d = x[i] - centre;
				const f32 l = d.magnitude(), r = c.radius + thickness;
				if (l >= r) continue;
				d = l > 1e-7f ? d / l : Vec3(0.f, 1.f, 0.f);
				x[i] = centre + d * r;
				touched = true; nrm = d; depth = r - l;
			}
			if (ground && x[i].y < groundY + thickness) { depth = groundY + thickness - x[i].y; x[i].y = groundY + thickness; touched = true; nrm = Vec3(0.f, 1.f, 0.f); }
			if (touched && friction > 0.f)
			{
				// (position-based friction: a slide shorter than friction times how far it was pushed
				// out is undone - it sticks; a longer one is shortened by that much. Taking a fixed
				// share off instead never stopped one, and a draped sheet crept and shimmered.)
				const Vec3 move = x[i] - prev[i];
				const Vec3 tangential = move - nrm * move.dotProduct(nrm);
				const f32 tl = tangential.magnitude();
				if (tl < friction * depth) x[i] -= tangential;
				else if (tl > 1e-9f) x[i] -= tangential * fminf(friction * 0.8f * depth / tl, 1.f);
			}
		}

		// Velocities from what the step did, with damping.
		const f32 keep = damping * h < 1.f ? 1.f - damping * h : 0.f;
		for (size_t i = 0; i < n; i++)
			v[i] = pinned[i] ? (x[i] - prev[i]) / h : (x[i] - prev[i]) * (keep / h);
	}

	void ClothComponent::WriteBack()
	{
		GameObject* owner = GetOwner();
		const Matrix &w = owner->GetWorldTransformation();
		const Matrix inv = w.Inverse();
		// Normals: each particle's, from the triangles round it (area-weighted), in world space.
		for (size_t i = 0; i < normal.size(); i++) normal[i] = Vec3();
		for (size_t k = 0; k < tris.size(); k += 3)
		{
			const Vec3 cr = (x[tris[k + 1]] - x[tris[k]]).cross(x[tris[k + 2]] - x[tris[k]]);
			normal[tris[k]] += cr; normal[tris[k + 1]] += cr; normal[tris[k + 2]] += cr;
		}
		Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
		for (Piece &p : pieces)
		{
			ModelGeometry* g = p.geometry;
			const size_t nv = g->tVertex.size();
			const bool hasN = g->tNormal.size() == nv;
			for (size_t vtx = 0; vtx < nv; vtx++)
			{
				const uint32 i = p.particleOf[vtx];
				const Vec3 l = inv * x[i];
				g->tVertex[vtx] = l;
				lo.x = fminf(lo.x, l.x); lo.y = fminf(lo.y, l.y); lo.z = fminf(lo.z, l.z);
				hi.x = fmaxf(hi.x, l.x); hi.y = fmaxf(hi.y, l.y); hi.z = fmaxf(hi.z, l.z);
				if (!hasN) continue;
				// (to the mesh's space by the transpose of the world's rotation part: the inverse
				// transpose of a normal's transform, undone)
				const Vec3 &nw = normal[i];
				Vec3 nl(w.m[0] * nw.x + w.m[1] * nw.y + w.m[2] * nw.z,
					w.m[4] * nw.x + w.m[5] * nw.y + w.m[6] * nw.z,
					w.m[8] * nw.x + w.m[9] * nw.y + w.m[10] * nw.z);
				const f32 ln = nl.magnitude();
				if (ln > 1e-12f) g->tNormal[vtx] = nl / ln;
			}
			g->RecalculateBounding();
			g->ResendVertices();
		}
		// (what it covers now, for culling and the shadow pass: a flag blown out sideways is not
		// where the plane was)
		if (lo.x <= hi.x)
		{
			const Vec3 pad(thickness, thickness, thickness);
			rc->SetBounds(lo - pad, hi + pad);
			owner->RefreshComponentBounds();
		}
	}

	void ClothComponent::Update(const f64 time)
	{
		if (!running || !GetOwner()) { lastTime = time; return; }
		if (!built && (unusable || !Build())) { lastTime = time; return; }
		f32 dt = hasLast && lastTime > 0.0 ? (f32)(time - lastTime) : 0.f;
		lastTime = time;
		if (dt <= 0.f) return;
		if (dt > 1.f / 20.f) dt = 1.f / 20.f;                // (a stall is not simulated as one huge step)
		const Matrix &w = GetOwner()->GetWorldTransformation();
		// A teleport (or a respawn): start again where it is, rather than fling the cloth across.
		const Vec3 moved(w.m[12] - lastWorld.m[12], w.m[13] - lastWorld.m[13], w.m[14] - lastWorld.m[14]);
		if (moved.magnitude() > 5.f) Reset();
		// Steps of one fixed length (substeps of a 60th of a second), as many as the frame took,
		// the rest carried over. Split each frame into `substeps` instead and the step length went
		// up and down with the frame time, and with it how far each step's constraints converge:
		// a draped sheet changed shape a little every uneven frame, and never came to rest.
		const f32 h = 1.f / (60.f * (f32)substeps);
		carry += dt;
		uint32 steps = (uint32)(carry / h);
		carry -= (f32)steps * h;
		const uint32 maxSteps = substeps * 4;               // (below 15 fps it runs slow rather than slower still)
		if (steps > maxSteps) { steps = maxSteps; carry = 0.f; }
		for (uint32 s = 0; s < steps; s++)
		{
			simTime += h;
			Step(h, (f32)simTime, (f32)(s + 1) / (f32)steps);
		}
		if (steps == 0) return;
		lastWorld = w;
		WriteBack();
	}
}
