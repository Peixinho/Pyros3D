// Octahedral encoding and the bordered probe atlas.
//
// Both halves fail quietly. A wrong encoding still round-trips
// *something* and lights the scene from subtly rotated directions. A
// wrong border still filters, just across a seam into unrelated
// directions - which shows up as a faint cross through every probe's
// contribution and gets blamed on probe density.
//
//   c++ -std=c++17 -I include tools/tests/octahedral.cpp \
//       -o /tmp/octahedral -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/octahedral
#include <Pyros3D/Rendering/GI/Octahedral.h>
#include <cmath>
#include <cstdio>
#include <string>

using namespace p3d;
static int failures = 0;

static void check(bool c, const std::string &what, const std::string &extra = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", what.c_str(), extra.empty()?"":" - ", extra.c_str());
	if(!c) failures++;
}

static uint32 rngState = 987654u;
static f32 Rand01() { rngState = rngState*1664525u+1013904223u; return (f32)((rngState>>8)&0xFFFFFF)/(f32)0xFFFFFF; }

int main()
{
	// ---- the six axes land where the octahedron says they should ------
	{
		check(fabsf(OctEncode(Vec3(0,0,1)).x) < 1e-5f && fabsf(OctEncode(Vec3(0,0,1)).y) < 1e-5f,
			"+Z maps to the centre of the square");
		const Vec2 e = OctEncode(Vec3(0,0,-1));
		// -Z unfolds to a CORNER of the square, not to the |x|+|y| = 1
		// diamond - that diamond is the equator. Any of the four corners
		// is correct, so check both components are at the extreme.
		check(fabsf(fabsf(e.x) - 1.f) < 1e-4f && fabsf(fabsf(e.y) - 1.f) < 1e-4f,
			"-Z maps to a corner of the square",
			"(" + std::to_string(e.x) + ", " + std::to_string(e.y) + ")");
		// And the equator really is the diamond.
		const Vec2 eq = OctEncode(Vec3(0.7071f, 0.7071f, 0.f));
		check(fabsf(fabsf(eq.x) + fabsf(eq.y) - 1.f) < 1e-4f, "an equator direction lands on |x|+|y| = 1",
			"|x|+|y| = " + std::to_string(fabsf(eq.x)+fabsf(eq.y)));
		check(fabsf(OctEncode(Vec3(1,0,0)).x - 1.f) < 1e-4f, "+X maps to x = 1");
		check(fabsf(OctEncode(Vec3(0,1,0)).y - 1.f) < 1e-4f, "+Y maps to y = 1");
	}

	// ---- round-trip over the whole sphere ------------------------------
	//
	// The real check. Not "it returns a unit vector" - it has to return
	// the SAME direction.
	{
		f32 worst = 0.f;
		Vec3 worstDir(0,0,0);
		const uint32 kN = 20000;
		for (uint32 i = 0; i < kN; i++)
		{
			// Uniform on the sphere, so the corners of the octahedron
			// (where the folding happens, and where a bug would live)
			// get sampled as often as the middle of a face.
			const f32 z = Rand01() * 2.f - 1.f;
			const f32 a = Rand01() * 6.2831853f;
			const f32 r = sqrtf(fmaxf(0.f, 1.f - z*z));
			const Vec3 d(r*cosf(a), r*sinf(a), z);
			const Vec3 back = OctDecode(OctEncode(d));
			const f32 err = (back - d).magnitude();
			if (err > worst) { worst = err; worstDir = d; }
		}
		char buf[200];
		snprintf(buf,sizeof(buf),"worst error %.3e at (%.3f, %.3f, %.3f) over %u directions",
			worst, worstDir.x, worstDir.y, worstDir.z, kN);
		check(worst < 1e-4f, "encode/decode round-trips every direction on the sphere", buf);
	}

	// ---- decoded texel directions cover the sphere evenly --------------
	//
	// Guards the +0.5 texel-centre offset: without it the set of
	// directions is biased toward one corner, which is a systematic
	// lighting rotation rather than an obvious error. If the directions
	// are unbiased their vector sum is near zero.
	{
		const uint32 R = 16;
		Vec3 sum(0,0,0);
		for (uint32 y = 0; y < R; y++)
			for (uint32 x = 0; x < R; x++)
				sum += ProbeAtlas::TexelDirection(x, y, R);
		const f32 bias = sum.magnitude() / (f32)(R*R);
		check(bias < 0.02f, "texel directions are unbiased over the sphere",
			"mean |sum| = " + std::to_string(bias));
	}

	// ---- atlas geometry -------------------------------------------------
	{
		ProbeAtlas a;
		check(!a.Allocate(0, 8, 3), "zero probes is refused");
		check(!a.Allocate(4, 8, 5), "more than 4 channels is refused");
		check(a.Allocate(9, 8, 3), "a 9-probe, 8x8, RGB atlas allocates");
		check(a.GetTileSize() == 10, "tiles are resolution + 2 for the border");
		check(a.GetWidth() == 30 && a.GetHeight() == 30, "9 probes pack 3x3",
			std::to_string(a.GetWidth()) + "x" + std::to_string(a.GetHeight()));
		// Distinct probes must not alias onto the same memory.
		*a.At(0,0,0) = 1.f;
		*a.At(8,7,7) = 2.f;
		check(fabsf(*a.At(0,0,0) - 1.f) < 1e-6f && fabsf(*a.At(8,7,7) - 2.f) < 1e-6f,
			"probes address distinct texels");
	}

	// ---- the border carries the mirrored wrap --------------------------
	//
	// This is the part that no texture addressing mode implements and
	// the part that is worth a test. Fill every interior texel with a
	// value derived from its own coordinates, fill the borders, then
	// require each border texel to equal the interior texel the
	// octahedral wrap says it continues into.
	{
		const uint32 R = 8;
		ProbeAtlas a;
		a.Allocate(4, R, 1);
		for (uint32 p = 0; p < 4; p++)
			for (uint32 y = 0; y < R; y++)
				for (uint32 x = 0; x < R; x++)
					*a.At(p, x, y) = (f32)(p * 1000 + y * R + x);
		a.FillBorders();

		// Re-derive the expected values from the atlas's own interior,
		// through the rule stated independently here.
		uint32 bad = 0;
		const std::vector<f32> &d = a.GetData();
		const uint32 tile = a.GetTileSize(), W = a.GetWidth();
		for (uint32 p = 0; p < 4; p++)
		{
			const uint32 perRow = a.GetProbesPerRow();
			const uint32 px = (p % perRow) * tile, py = (p / perRow) * tile;
			for (uint32 i = 0; i < R; i++)
			{
				const uint32 m = R - 1 - i;
				// left border == interior column 0, flipped vertically
				if (fabsf(d[(py + i + 1) * W + px + 0] - *a.At(p, 0, m)) > 1e-6f) bad++;
				// right border == interior column R-1, flipped vertically
				if (fabsf(d[(py + i + 1) * W + px + R + 1] - *a.At(p, R-1, m)) > 1e-6f) bad++;
				// top border == interior row 0, flipped horizontally
				if (fabsf(d[(py + 0) * W + px + i + 1] - *a.At(p, m, 0)) > 1e-6f) bad++;
				// bottom border == interior row R-1, flipped horizontally
				if (fabsf(d[(py + R + 1) * W + px + i + 1] - *a.At(p, m, R-1)) > 1e-6f) bad++;
			}
			// corners wrap diagonally
			if (fabsf(d[(py+0)*W + px+0]         - *a.At(p, R-1, R-1)) > 1e-6f) bad++;
			if (fabsf(d[(py+0)*W + px+R+1]       - *a.At(p, 0,   R-1)) > 1e-6f) bad++;
			if (fabsf(d[(py+R+1)*W + px+0]       - *a.At(p, R-1, 0  )) > 1e-6f) bad++;
			if (fabsf(d[(py+R+1)*W + px+R+1]     - *a.At(p, 0,   0  )) > 1e-6f) bad++;
		}
		check(bad == 0, "every border texel mirrors the interior the octahedral wrap continues into",
			std::to_string(bad) + " wrong");

		// And that the border is not simply a copy of the adjacent
		// interior texel, which would pass a sloppier test and is the
		// most likely wrong implementation.
		const uint32 px = 0, py = 0;
		bool differs = false;
		for (uint32 i = 0; i < R && !differs; i++)
			if (fabsf(d[(py+i+1)*W + px+0] - *a.At(0, 0, i)) > 1e-6f) differs = true;
		check(differs, "the border is mirrored, not a plain edge clamp");
	}

	// ---- borders never reach outside their own tile ---------------------
	{
		ProbeAtlas a;
		a.Allocate(2, 4, 1);
		for (uint32 i = 0; i < a.GetData().size(); i++) a.GetData()[i] = -1.f;
		for (uint32 y = 0; y < 4; y++) for (uint32 x = 0; x < 4; x++) *a.At(0,x,y) = 5.f;
		a.FillBorders();
		// Probe 1's interior was never written, so if probe 0's border
		// fill bled into it the -1 sentinel would be gone.
		bool clean = true;
		for (uint32 y = 0; y < 4; y++) for (uint32 x = 0; x < 4; x++)
			if (fabsf(*a.At(1,x,y) + 1.f) > 1e-6f) clean = false;
		check(clean, "filling one tile's border does not write into the next tile");
	}

	printf("\n%s  octahedral: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
