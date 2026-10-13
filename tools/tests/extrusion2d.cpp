// Extrusion2D's geometry, with no window: the curve through its points, the
// triangles of a closed shape, and the two surfaces of an outline given depth.
//
//   c++ -std=c++17 -I include tools/tests/extrusion2d.cpp -o /tmp/extrusion2d \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan && /tmp/extrusion2d
#include <Pyros3D/Rendering/Components/Extrusion2D/Extrusion2D.h>
#include <cmath>
#include <cstdio>

using namespace p3d;

static int failures = 0;
static void check(bool ok, const char* what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) failures++; }
static void nearly(f32 got, f32 want, f32 tol, const char* what)
{
	const bool ok = fabsf(got - want) <= tol;
	printf("%s  %s (got %.4f, want %.4f)\n", ok ? "PASS" : "FAIL", what, got, want);
	if (!ok) failures++;
}
static f32 AreaOf(const ExtrusionMesh2D &m, const Vec3 &facing)
{
	// the area of the triangles whose winding faces `facing`
	f32 total = 0.f;
	for (size_t i = 0; i + 2 < m.index.size(); i += 3)
	{
		const Vec3 &a = m.vertex[m.index[i]], &b = m.vertex[m.index[i + 1]], &c = m.vertex[m.index[i + 2]];
		const Vec3 n = (b - a).cross(c - a);
		if (n.dotProduct(facing) > 0.f) total += n.magnitude() * 0.5f;
	}
	return total;
}

int main()
{
	// --- the curve ---------------------------------------------------------------
	{
		std::vector<Vec2> pts, out;
		pts.push_back(Vec2(0, 0)); pts.push_back(Vec2(4, 2)); pts.push_back(Vec2(8, 0));
		Extrusion2D::SmoothPath(pts, false, 0, out);
		check(out.size() == 3, "not smoothed, the path is the points");
		Extrusion2D::SmoothPath(pts, false, 5, out);
		check(out.size() == 11, "smoothed, each stretch is that many pieces");
		bool through = false;
		for (size_t i = 0; i < out.size(); i++) if (fabsf(out[i].x - 4.f) < 1e-4f && fabsf(out[i].y - 2.f) < 1e-4f) through = true;
		check(through, "and the curve still goes through the points");
		nearly(out.front().x, 0.f, 1e-5f, "it starts where the first point is");
		nearly(out.back().x, 8.f, 1e-5f, "and ends where the last is");
		Extrusion2D::SmoothPath(pts, true, 4, out);
		check(out.size() == 12, "closed, the stretch back to the start is curved too");
	}
	// --- triangles of a shape ----------------------------------------------------
	{
		std::vector<Vec2> ell;      // an L: six corners, one of them a dent
		ell.push_back(Vec2(0, 0)); ell.push_back(Vec2(2, 0)); ell.push_back(Vec2(2, 1)); ell.push_back(Vec2(1, 1)); ell.push_back(Vec2(1, 2)); ell.push_back(Vec2(0, 2));
		std::vector<uint32> tri;
		check(Extrusion2D::Triangulate(ell, tri) && tri.size() == 12, "a six-cornered shape is four triangles");
		f32 area = 0.f;
		for (size_t i = 0; i + 2 < tri.size(); i += 3)
		{
			const Vec2 &a = ell[tri[i]], &b = ell[tri[i + 1]], &c = ell[tri[i + 2]];
			area += ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5f;
		}
		nearly(area, 3.f, 1e-4f, "that cover it exactly, all facing the same way");
		std::vector<Vec2> cw(ell.rbegin(), ell.rend());
		check(Extrusion2D::Triangulate(cw, tri) && tri.size() == 12, "drawn the other way round it is the same");
	}
	// --- ground: an open outline ---------------------------------------------------
	{
		std::vector<Vec2> path;
		path.push_back(Vec2(0, 1)); path.push_back(Vec2(10, 1));
		ExtrusionMesh2D skin, faces;
		check(Extrusion2D::BuildMesh(path, false, 4.f, -1.f, 2.f, skin, faces), "a flat stretch of ground is made");
		nearly(AreaOf(skin, Vec3(0, 1, 0)), 40.f, 1e-3f, "its top is its length by its depth, facing up");
		nearly(AreaOf(faces, Vec3(0, 0, 1)), 20.f, 1e-3f, "its near face is its length by its height, facing the camera");
		nearly(AreaOf(faces, Vec3(0, 0, -1)), 20.f, 1e-3f, "and so is its far face, facing away");
		nearly(AreaOf(faces, Vec3(-1, 0, 0)), 8.f, 1e-3f, "its left end faces left");
		nearly(AreaOf(faces, Vec3(1, 0, 0)), 8.f, 1e-3f, "its right end faces right");
		f32 uMax = 0.f;
		for (size_t i = 0; i < skin.texcoord.size(); i++) if (skin.texcoord[i].x > uMax) uMax = skin.texcoord[i].x;
		nearly(uMax, 5.f, 1e-4f, "the image repeats once every tile along it");
		f32 vMax = 0.f;
		for (size_t i = 0; i < skin.texcoord.size(); i++) if (skin.texcoord[i].y > vMax) vMax = skin.texcoord[i].y;
		nearly(vMax, 2.f, 1e-4f, "and across it");
		Extrusion2D::BuildMesh(path, false, 4.f, -1.f, 2.f, skin, faces, true);
		vMax = 0.f;
		for (size_t i = 0; i < skin.texcoord.size(); i++) if (skin.texcoord[i].y > vMax) vMax = skin.texcoord[i].y;
		nearly(vMax, 1.f, 1e-4f, "or, asked to, lies once across the whole depth");
		std::vector<Vec2> rtl(path.rbegin(), path.rend());
		Extrusion2D::BuildMesh(rtl, false, 4.f, -1.f, 2.f, skin, faces);
		nearly(AreaOf(skin, Vec3(0, 1, 0)), 40.f, 1e-3f, "drawn right to left, its top still faces up");
		check(!Extrusion2D::BuildMesh(std::vector<Vec2>(1, Vec2(0, 0)), false, 4.f, -1.f, 2.f, skin, faces), "one point is no shape");
	}
	// --- a block: a closed outline -----------------------------------------------------
	{
		std::vector<Vec2> box;
		box.push_back(Vec2(0, 0)); box.push_back(Vec2(3, 0)); box.push_back(Vec2(3, 2)); box.push_back(Vec2(0, 2));
		ExtrusionMesh2D skin, faces;
		check(Extrusion2D::BuildMesh(box, true, 2.f, 0.f, 1.f, skin, faces), "a closed outline is made");
		nearly(AreaOf(skin, Vec3(0, 1, 0)), 6.f, 1e-3f, "its top faces up");
		nearly(AreaOf(skin, Vec3(0, -1, 0)), 6.f, 1e-3f, "its underside faces down");
		nearly(AreaOf(skin, Vec3(1, 0, 0)), 4.f, 1e-3f, "its right side faces right");
		nearly(AreaOf(skin, Vec3(-1, 0, 0)), 4.f, 1e-3f, "its left side faces left");
		nearly(AreaOf(faces, Vec3(0, 0, 1)), 6.f, 1e-3f, "its near face is the shape, facing the camera");
		nearly(AreaOf(faces, Vec3(0, 0, -1)), 6.f, 1e-3f, "its far face is the shape, facing away");
	}
	printf(failures ? "\nFAILED (%d failure%s)\n" : "\nALL PASSED (%d failures)\n", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
