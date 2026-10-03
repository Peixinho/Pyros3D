// CaveVolume: a dug sphere meshes into a round wall that faces the air,
// filling it back leaves nothing, the field survives a save, the walls stop
// at the terrain's surface, and two tiles mesh their shared border alike.
// Pure data: no render device.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/cave_volume.cpp -o /tmp/cave_volume \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/cave_volume
#include <Pyros3D/Assets/Renderable/Terrains/CaveVolume.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>

#include <map>
#include <tuple>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}

int main()
{
	// A 64 m tile of 1 m voxels.
	CaveVolume v(1.f, 64);
	check(v.Empty(), "a new volume is all rock and stores nothing");
	const Vec3 c(32.f, -20.f, 32.f);
	check(v.Carve(c, 6.f, true), "digging changes it");
	check(!v.Carve(c, 6.f, true), "digging the same sphere again does not");
	check(v.AirAt(c) > 0.99f && v.AirAt(c + Vec3(10.f, 0.f, 0.f)) < 0.01f, "air inside, rock outside");

	CaveMeshData m;
	v.BuildMesh(CaveVolume::GroundFn(), true, true, m);
	check(!m.vertex.empty() && m.index.size() % 3 == 0, "the sphere has walls");
	f32 worst = 0.f;
	bool inward = true, frontToAir = true;
	for (size_t i = 0; i < m.vertex.size(); i++)
	{
		worst = std::max(worst, std::fabs(m.vertex[i].distance(c) - 6.f));
		inward = inward && m.normal[i].dotProduct(c - m.vertex[i]) > 0.f;
	}
	for (size_t i = 0; i + 2 < m.index.size(); i += 3)
	{
		const Vec3 &a = m.vertex[m.index[i]], &b = m.vertex[m.index[i + 1]], &d = m.vertex[m.index[i + 2]];
		const Vec3 n = (b - a).cross(d - a);
		if (n.magnitude() < 1e-6f) continue;
		frontToAir = frontToAir && n.dotProduct(c - (a + b + d) * (1.f / 3.f)) > 0.f;
	}
	printf("sphere: %zu vertices, %zu triangles, worst distance from the 6 m sphere %.3f m\n", m.vertex.size(), m.index.size() / 3, worst);
	check(worst < 0.6f, "the wall is the sphere, within a voxel");
	check(inward, "normals point into the cave");
	check(frontToAir, "every triangle faces the air");
	// Closed: every edge is shared by exactly two triangles.
	{
		std::multiset<std::pair<uint32, uint32> > edges;
		for (size_t i = 0; i + 2 < m.index.size(); i += 3)
			for (int k = 0; k < 3; k++)
			{
				const uint32 a = m.index[i + k], b = m.index[i + (k + 1) % 3];
				edges.insert(std::make_pair(std::min(a, b), std::max(a, b)));
			}
		bool closed = true;
		for (std::multiset<std::pair<uint32, uint32> >::const_iterator it = edges.begin(); it != edges.end(); ++it)
			closed = closed && edges.count(*it) == 2;
		check(closed, "the wall is closed");
	}

	// Save and load.
	const std::vector<uchar> blob = v.ToBlob();
	CaveVolume w(1.f, 64);
	check(w.FromBlob(blob), "the blob reads back");
	CaveMeshData m2;
	w.BuildMesh(CaveVolume::GroundFn(), true, true, m2);
	check(m2.vertex.size() == m.vertex.size() && m2.index == m.index, "and meshes the same");
	printf("blob: %zu bytes for %zu chunks\n", blob.size(), v.ChunkCount());
	CaveVolume other(2.f, 64);
	check(!other.FromBlob(blob), "a blob for another lattice is refused");

	// Fill it back.
	check(v.Carve(c, 8.f, false), "filling changes it");
	check(v.Empty(), "and a filled cave stores nothing again");

	// A cave through the ground: flat ground at y = 0, a sphere half out of
	// it. One mesh: a ring of ground, the rim, the wall.
	{
		CaveVolume g(1.f, 64);
		const Vec3 c(32.3f, -1.f, 31.6f);
		g.Carve(c, 6.f, true);
		const CaveVolume::GroundFn flat = [](const f32, const f32) { return 0.f; };
		CaveMeshData gm;
		g.BuildMesh(flat, true, true, gm);
		f32 top = -1e9f;
		for (size_t i = 0; i < gm.index.size(); i++) top = std::max(top, gm.vertex[gm.index[i]].y);
		check(!gm.index.empty() && top <= 1e-4f, "nothing above the ground");
		check(std::fabs(top + CaveVolume::GroundDrop()) < 0.02f, "its ground lies just under the terrain's");
		// Its only open edge is the outside of the ground ring: at ground
		// level, clear of the dug air, and under terrain that is still there.
		std::map<std::pair<uint32, uint32>, int> uses;
		for (size_t i = 0; i + 2 < gm.index.size(); i += 3)
			for (int k = 0; k < 3; k++)
			{
				const uint32 a = gm.index[i + k], b = gm.index[i + (k + 1) % 3];
				uses[std::make_pair(std::min(a, b), std::max(a, b))]++;
			}
		size_t open = 0;
		bool onGround = true, clear = true, covered = true, manifold = true;
		f32 nearest = 1e9f;
		for (std::map<std::pair<uint32, uint32>, int>::const_iterator it = uses.begin(); it != uses.end(); ++it)
		{
			manifold = manifold && it->second <= 2;
			if (it->second != 1) continue;
			open++;
			const uint32 ends[2] = { it->first.first, it->first.second };
			for (int k = 0; k < 2; k++)
			{
				const Vec3 &p = gm.vertex[ends[k]];
				onGround = onGround && std::fabs(p.y + CaveVolume::GroundDrop()) < 0.02f;
				clear = clear && g.AirAt(Vec3(p.x, 0.f, p.z)) == 0.f;
				covered = covered && g.OpeningAt(Vec3(p.x, 0.f, p.z)) < 0.5f;
				nearest = std::min(nearest, Vec3(p.x, 0.f, p.z).distance(Vec3(c.x, 0.f, c.z)));
			}
		}
		printf("opening: %zu triangles, %zu open edges, the nearest %.2f m from the centre of a %.2f m wide mouth\n",
			gm.index.size() / 3, open, nearest, std::sqrt(36.f - 1.f));
		check(manifold, "no edge has more than two triangles");
		check(open > 0 && onGround, "the mesh's only edge is at ground level");
		check(clear, "clear of everything that was dug");
		check(covered, "and under terrain that is still drawn");
		// Where the terrain IS cut away, the mesh is under it: the terrain's
		// cut reaches less far from the mouth than the mesh's ring does.
		f32 cut = 0.f;
		for (f32 z = 16.f; z <= 48.f; z += 0.125f)
			for (f32 x = 16.f; x <= 48.f; x += 0.125f)
				if (g.OpeningAt(Vec3(x, 0.f, z)) >= 0.5f) cut = std::max(cut, Vec3(x - c.x, 0.f, z - c.z).magnitude());
		printf("the terrain is cut out to %.2f m; the ring of ground reaches %.2f m\n", cut, nearest);
		check(cut > std::sqrt(35.f) && cut + 0.3f < nearest, "the terrain's cut is outside the rim and inside the ring of ground");
		// The wall under the rim is still the sphere.
		f32 worst = 0.f;
		for (size_t i = 0; i < gm.vertex.size(); i++)
			if (gm.vertex[i].y < -1.5f) worst = std::max(worst, std::fabs(gm.vertex[i].distance(c) - 6.f));
		check(worst < 0.2f, "and below the rim the wall is the sphere");
	}

	// Two tiles side by side, a sphere across their border: each meshes its
	// own side, and the vertices along the border are the same points.
	{
		CaveVolume a(1.f, 64), b(1.f, 64);
		const Vec3 inA(64.f, -20.f, 32.f);		// on a's far edge...
		const Vec3 inB(0.f, -20.f, 32.f);		// ...which is b's near edge
		a.Carve(inA, 6.f, true);
		b.Carve(inB, 6.f, true);
		CaveMeshData ma, mb;
		a.BuildMesh(CaveVolume::GroundFn(), false, true, ma);
		b.BuildMesh(CaveVolume::GroundFn(), true, true, mb);
		check(!ma.index.empty() && !mb.index.empty(), "both tiles have a part of it");
		// Border vertices of a (x near 64) each have a twin in b (x near 0).
		size_t border = 0, matched = 0;
		for (size_t i = 0; i < ma.index.size(); i++)
		{
			const Vec3 &p = ma.vertex[ma.index[i]];
			if (p.x < 63.f) continue;
			border++;
			for (size_t k = 0; k < mb.vertex.size(); k++)
				if (mb.vertex[k].distance(p - Vec3(64.f, 0.f, 0.f)) < 1e-3f) { matched++; break; }
		}
		check(border > 0 && matched == border, "the walls meet at the border");
		// Together they are the whole sphere: as many triangles as one tile makes of it.
		CaveVolume whole(1.f, 128);
		whole.Carve(Vec3(64.f, -20.f, 32.f), 6.f, true);
		CaveMeshData mw;
		whole.BuildMesh(CaveVolume::GroundFn(), true, true, mw);
		check(ma.index.size() + mb.index.size() == mw.index.size(), "and nothing is drawn twice or left out");
	}

	// Noise tunnels.
	{
		HeightfieldData ground;
		ground.samples = 2;
		ground.size = 64.f;
		ground.heights.assign(4, 100.f);
		CaveVolume n(1.f, 64);
		CaveVolume::Noise noise;
		check(n.Generate(ground, Vec3(), Vec3(32.f, 0.f, 32.f), 0.f, noise), "noise digs something");
		CaveMeshData nm;
		n.BuildMesh([](const f32, const f32) { return 100.f; }, true, true, nm);
		f32 lo = 1e9f, hi = -1e9f;
		for (size_t i = 0; i < nm.vertex.size(); i++) { lo = std::min(lo, nm.vertex[i].y); hi = std::max(hi, nm.vertex[i].y); }
		printf("tunnels: %zu triangles between y = %.1f and %.1f (ground at 100)\n", nm.index.size() / 3, lo, hi);
		check(!nm.index.empty() && hi <= 100.f - noise.minDepth + 1.f && lo >= 100.f - noise.maxDepth - 1.f, "tunnels stay inside their depth band");
	}

	// Holes in a cave's own wall: cut along a smooth line, and saved.
	{
		CaveVolume v2(1.f, 64);
		const Vec3 c2(32.f, -20.f, 32.f);
		v2.Carve(c2, 8.f, true);
		CaveMeshData whole;
		v2.BuildMesh(CaveVolume::GroundFn(), true, true, whole);
		// An opening in the wall on the +x side.
		const Vec3 at = c2 + Vec3(8.f, 0.f, 0.f);
		check(v2.CutHole(at, 3.f, true) && v2.HasHoles(), "a hole is cut");
		CaveMeshData holed;
		v2.BuildMesh(CaveVolume::GroundFn(), true, true, holed);
		check(holed.index.size() < whole.index.size() && !holed.index.empty(), "the wall has fewer triangles");
		bool noneInside = true;
		for (size_t i = 0; i + 2 < holed.index.size(); i += 3)
		{
			const Vec3 mid = (holed.vertex[holed.index[i]] + holed.vertex[holed.index[i + 1]] + holed.vertex[holed.index[i + 2]]) * (1.f / 3.f);
			noneInside = noneInside && mid.distance(at) > 2.6f;
		}
		check(noneInside, "nothing is left inside the opening");
		// Its edge: the open edges of the mesh all lie on the cut, 3 m from its centre.
		std::map<std::pair<uint32, uint32>, int> uses;
		for (size_t i = 0; i + 2 < holed.index.size(); i += 3)
			for (int k = 0; k < 3; k++)
			{
				const uint32 a = holed.index[i + k], b = holed.index[i + (k + 1) % 3];
				uses[std::make_pair(std::min(a, b), std::max(a, b))]++;
			}
		size_t open = 0;
		f32 worst = 0.f;
		for (std::map<std::pair<uint32, uint32>, int>::const_iterator it = uses.begin(); it != uses.end(); ++it)
		{
			if (it->second != 1) continue;
			open++;
			worst = std::max(worst, std::max(std::fabs(holed.vertex[it->first.first].distance(at) - 3.f), std::fabs(holed.vertex[it->first.second].distance(at) - 3.f)));
		}
		printf("cave hole: %zu edge segments, furthest from the 3 m cut %.3f m\n", open, worst);
		check(open > 8 && worst < 0.15f, "the opening's edge is the brush's circle");
		CaveVolume back(1.f, 64);
		check(back.FromBlob(v2.ToBlob()) && back.HasHoles(), "holes survive a save");
		CaveMeshData again;
		back.BuildMesh(CaveVolume::GroundFn(), true, true, again);
		check(again.index == holed.index, "and mesh the same");
		check(v2.CutHole(at, 5.f, false) && !v2.HasHoles(), "closing it leaves no hole");
		CaveMeshData closed;
		v2.BuildMesh(CaveVolume::GroundFn(), true, true, closed);
		check(closed.index.size() == whole.index.size(), "and the wall is whole again");
	}

	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
