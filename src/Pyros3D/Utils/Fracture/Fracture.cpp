//============================================================================
// Name        : Fracture.cpp
// Description : See Fracture.h. A piece is clipped out of the mesh one cell
//               plane at a time: every face (a convex polygon) is cut by the
//               plane and the part inside kept; the points where faces crossed
//               the plane, sorted round their middle, are the cap over the cut
//               - itself a polygon the later planes cut in turn. What is left
//               is fanned into triangles.
//============================================================================

#include <Pyros3D/Utils/Fracture/Fracture.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <algorithm>
#include <cmath>
#include <random>

namespace p3d {

	namespace {
		struct V { Vec3 p, n; Vec2 uv; };
		struct Poly { std::vector<V> v; uint32 source; bool cap; };
		struct Plane { Vec3 n; f32 d; };     // inside: dot(n, p) <= d

		V Lerp(const V &a, const V &b, const f32 t)
		{
			V o;
			o.p = a.p + (b.p - a.p) * t;
			o.n = (a.n + (b.n - a.n) * t);
			const f32 l = o.n.magnitude(); if (l > 1e-6f) o.n = o.n / l;
			o.uv = Vec2(a.uv.x + (b.uv.x - a.uv.x) * t, a.uv.y + (b.uv.y - a.uv.y) * t);
			return o;
		}

		// The part of a convex polygon inside the plane; the points where it crossed, in `cut`.
		bool ClipPoly(const Poly &in, const Plane &pl, Poly &out, std::vector<Vec3> &cut)
		{
			out.v.clear(); out.source = in.source; out.cap = in.cap;
			const size_t n = in.v.size();
			for (size_t i = 0; i < n; i++)
			{
				const V &a = in.v[i], &b = in.v[(i + 1) % n];
				const f32 da = pl.n.dotProduct(a.p) - pl.d, db = pl.n.dotProduct(b.p) - pl.d;
				if (da <= 0.f) out.v.push_back(a);
				if ((da < 0.f && db > 0.f) || (da > 0.f && db < 0.f))
				{
					const V x = Lerp(a, b, da / (da - db));
					out.v.push_back(x);
					cut.push_back(x.p);
				}
				else if (da == 0.f) cut.push_back(a.p);
			}
			return out.v.size() >= 3;
		}

		// The cap over a cut: the crossing points, without repeats, round their middle.
		bool CapOf(std::vector<Vec3> &pts, const Plane &pl, const f32 eps, Poly &cap, const f32 uvScale)
		{
			std::vector<Vec3> u;
			for (size_t i = 0; i < pts.size(); i++)
			{
				bool dup = false;
				for (size_t j = 0; j < u.size() && !dup; j++) dup = pts[i].distanceSQR(u[j]) < eps * eps;
				if (!dup) u.push_back(pts[i]);
			}
			if (u.size() < 3) return false;
			Vec3 c(0.f, 0.f, 0.f);
			for (size_t i = 0; i < u.size(); i++) c = c + u[i];
			c = c / (f32)u.size();
			const Vec3 t = (fabsf(pl.n.y) < 0.9f ? Vec3(0.f, 1.f, 0.f) : Vec3(1.f, 0.f, 0.f)).cross(pl.n).normalize();
			const Vec3 b = pl.n.cross(t);
			std::sort(u.begin(), u.end(), [&](const Vec3 &x, const Vec3 &y) {
				return atan2f((x - c).dotProduct(b), (x - c).dotProduct(t)) < atan2f((y - c).dotProduct(b), (y - c).dotProduct(t)); });
			cap.v.clear(); cap.cap = true; cap.source = 0;
			for (size_t i = 0; i < u.size(); i++)
			{
				V v; v.p = u[i]; v.n = pl.n;
				v.uv = Vec2(u[i].dotProduct(t) * uvScale, u[i].dotProduct(b) * uvScale);
				cap.v.push_back(v);
			}
			// (t, b, n is right-handed: by rising angle is counter-clockwise seen from outside, the front)
			return true;
		}

		f32 HullVolume(const std::vector<Poly> &polys)
		{
			// (the divergence theorem over the closed surface: what the pieces' masses go by)
			f32 vol = 0.f;
			for (size_t i = 0; i < polys.size(); i++)
				for (size_t k = 1; k + 1 < polys[i].v.size(); k++)
					vol += polys[i].v[0].p.dotProduct(polys[i].v[k].p.cross(polys[i].v[k + 1].p)) / 6.f;
			return fabsf(vol);
		}
	}

	std::vector<Fracture::Piece> Fracture::Split(const Renderable &source, const Matrix &world, const uint32 countIn,
		const Vec3 &impact, const uint32 seed)
	{
		std::vector<Piece> out;
		const uint32 count = std::max(2u, std::min(64u, countIn));
		// the source's faces, in `world`
		std::vector<Poly> faces;
		Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
		Matrix normalM = world; normalM.m[12] = normalM.m[13] = normalM.m[14] = 0.f;
		for (size_t gi = 0; gi < source.Geometries.size(); gi++)
		{
			const IGeometry* g = source.Geometries[gi];
			const std::vector<__INDEX_C_TYPE__> &idx = g->GetIndexData();
			const std::vector<Vec3> &pos = g->GetVertexData();
			const std::vector<Vec3> &nrm = g->GetNormalData();
			const std::vector<Vec2>* uvs = NULL;
			if (const ModelGeometry* mg = dynamic_cast<const ModelGeometry*>(g)) uvs = &mg->tTexcoord;
			else if (const PrimitiveGeometry* pg = dynamic_cast<const PrimitiveGeometry*>(g)) uvs = &pg->tTexcoord;
			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				Poly f; f.source = (uint32)gi; f.cap = false;
				bool ok = true;
				for (int k = 0; k < 3; k++)
				{
					const uint32 j = idx[i + k];
					if (j >= pos.size()) { ok = false; break; }
					V v;
					v.p = world * pos[j];
					v.n = (j < nrm.size()) ? (normalM * nrm[j]).normalize() : Vec3(0.f, 1.f, 0.f);
					v.uv = (uvs != NULL && j < uvs->size()) ? (*uvs)[j] : Vec2(0.f, 0.f);
					f.v.push_back(v);
					lo = Vec3(std::min(lo.x, v.p.x), std::min(lo.y, v.p.y), std::min(lo.z, v.p.z));
					hi = Vec3(std::max(hi.x, v.p.x), std::max(hi.y, v.p.y), std::max(hi.z, v.p.z));
				}
				if (ok) faces.push_back(f);
			}
		}
		if (faces.empty()) return out;
		const Vec3 size = hi - lo;
		const f32 diag = std::max(1e-4f, size.magnitude());

		// The cells' sites: half packed round the impact, half anywhere in the bounds.
		std::mt19937 rng(seed);
		std::uniform_real_distribution<f32> u01(0.f, 1.f);
		std::normal_distribution<f32> g01(0.f, 1.f);
		std::vector<Vec3> sites;
		const Vec3 hit(std::max(lo.x, std::min(hi.x, impact.x)), std::max(lo.y, std::min(hi.y, impact.y)), std::max(lo.z, std::min(hi.z, impact.z)));
		for (uint32 i = 0; i < count; i++)
		{
			Vec3 s;
			if (i < count / 2) s = hit + Vec3(g01(rng), g01(rng), g01(rng)) * (diag * 0.18f);
			else s = lo + Vec3(size.x * u01(rng), size.y * u01(rng), size.z * u01(rng));
			s = Vec3(std::max(lo.x, std::min(hi.x, s.x)), std::max(lo.y, std::min(hi.y, s.y)), std::max(lo.z, std::min(hi.z, s.z)));
			sites.push_back(s);
		}

		const f32 eps = diag * 1e-4f;
		for (uint32 c = 0; c < count; c++)
		{
			std::vector<Poly> polys = faces;
			for (uint32 o = 0; o < count && !polys.empty(); o++)
			{
				if (o == c) continue;
				Vec3 dn = sites[o] - sites[c];
				const f32 l = dn.magnitude();
				if (l < 1e-6f) continue;
				Plane pl; pl.n = dn / l; pl.d = pl.n.dotProduct((sites[o] + sites[c]) * 0.5f);
				std::vector<Poly> kept;
				std::vector<Vec3> cut;
				for (size_t i = 0; i < polys.size(); i++)
				{
					Poly p;
					if (ClipPoly(polys[i], pl, p, cut)) kept.push_back(p);
				}
				Poly cap;
				if (!kept.empty() && CapOf(cut, pl, eps, cap, 1.f / std::max(0.25f, diag * 0.25f))) kept.push_back(cap);
				polys.swap(kept);
			}
			if (polys.size() < 4) continue;

			// about its own centre
			Piece pc;
			Vec3 centre(0.f, 0.f, 0.f); uint32 n = 0;
			for (size_t i = 0; i < polys.size(); i++) for (size_t k = 0; k < polys[i].v.size(); k++) { centre = centre + polys[i].v[k].p; n++; }
			centre = centre / (f32)n;
			for (size_t i = 0; i < polys.size(); i++) for (size_t k = 0; k < polys[i].v.size(); k++) polys[i].v[k].p = polys[i].v[k].p - centre;
			pc.centre = centre;
			pc.volume = HullVolume(polys);
			if (pc.volume < diag * diag * diag * 1e-6f) continue;

			pc.mesh = std::make_shared<FracturedPiece>();
			for (uint32 gi = 0; gi < (uint32)source.Geometries.size(); gi++)
			{
				ModelGeometry* geo = NULL;
				for (size_t i = 0; i < polys.size(); i++)
				{
					const Poly &p = polys[i];
					if ((p.cap ? 0u : p.source) != gi) continue;
					if (geo == NULL) { geo = new ModelGeometry(); geo->materialProperties = source.Geometries[gi]->materialProperties; }
					const uint32 base = (uint32)geo->tVertex.size();
					for (size_t k = 0; k < p.v.size(); k++)
					{
						geo->tVertex.push_back(p.v[k].p); geo->tNormal.push_back(p.v[k].n); geo->tTexcoord.push_back(p.v[k].uv);
						pc.hull.push_back(p.v[k].p);
					}
					for (size_t k = 1; k + 1 < p.v.size(); k++)
					{
						geo->index.push_back(base); geo->index.push_back(base + (uint32)k); geo->index.push_back(base + (uint32)k + 1);
					}
				}
				if (geo == NULL) continue;
				geo->materialProperties.haveBones = false;
				geo->CreateBuffers();
				geo->SendBuffers();
				pc.mesh->Geometries.push_back(geo);
				pc.mesh->SourceGeometry.push_back(gi);
			}
			if (pc.mesh->Geometries.empty()) continue;
			pc.mesh->Finish();
			out.push_back(pc);
		}
		return out;
	}
}
