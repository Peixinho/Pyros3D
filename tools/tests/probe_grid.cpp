// The probe grid's interpolation, checked against values that are true by
// construction rather than recorded.
//
// Interpolation bugs here are invisible: the lighting is smooth and
// plausible whether or not the weights are right, whether or not the
// axes are transposed, and whether or not the volume is offset by one
// cell. So every check below builds a grid whose correct answer is known
// analytically.
//
//   c++ -std=c++17 -I include tools/tests/probe_grid.cpp \
//       src/Pyros3D/Rendering/GI/IrradianceProbeGrid.cpp \
//       src/Pyros3D/Rendering/GI/SphericalHarmonics.cpp \
//       src/Pyros3D/Core/Math/Vec3.cpp -o /tmp/probe_grid
//   /tmp/probe_grid
#include <Pyros3D/Rendering/GI/IrradianceProbeGrid.h>
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
static void near(f32 got, f32 want, f32 tol, const std::string &what)
{
	char b[160]; snprintf(b,sizeof(b),"got %.6f want %.6f", got, want);
	check(fabsf(got-want) <= tol, what, b);
}

int main()
{
	// ---- validity -------------------------------------------------------
	{
		IrradianceProbeGrid g;
		check(!g.IsValid(), "a default-constructed grid is not valid");
		check(!g.Allocate(Vec3(0,0,0), Vec3(1,1,1), 1, 4, 4), "a single-probe axis is refused");
		check(!g.Allocate(Vec3(0,0,0), Vec3(0,1,1), 4, 4, 4), "zero spacing is refused");
		check(g.Allocate(Vec3(0,0,0), Vec3(1,1,1), 2, 2, 2), "the smallest usable grid allocates");
		check(g.IsValid() && g.ProbeCount() == 8, "2x2x2 holds 8 probes");
	}

	// ---- indexing is a bijection ---------------------------------------
	//
	// A transposed index is the classic probe-grid bug: everything still
	// interpolates smoothly, just along the wrong axis.
	{
		IrradianceProbeGrid g;
		g.Allocate(Vec3(0,0,0), Vec3(1,1,1), 3, 4, 5);
		bool seen[60] = { false };
		bool dup = false, oob = false;
		for (uint32 z=0; z<5; z++) for (uint32 y=0; y<4; y++) for (uint32 x=0; x<3; x++)
		{
			const uint32 i = g.Index(x,y,z);
			if (i >= 60) { oob = true; continue; }
			if (seen[i]) dup = true;
			seen[i] = true;
		}
		check(!oob && !dup, "Index() maps every (x,y,z) to a distinct in-range slot");

		// And that position tracks the same axes the index does.
		g.origin = Vec3(10.f, 20.f, 30.f);
		g.spacing = Vec3(2.f, 3.f, 4.f);
		const Vec3 p = g.ProbePosition(2, 3, 4);
		near(p.x, 10.f + 2.f*2.f, 1e-5f, "ProbePosition x");
		near(p.y, 20.f + 3.f*3.f, 1e-5f, "ProbePosition y");
		near(p.z, 30.f + 4.f*4.f, 1e-5f, "ProbePosition z");
	}

	// ---- a uniform grid samples to that value everywhere ----------------
	{
		IrradianceProbeGrid g;
		g.Allocate(Vec3(0,0,0), Vec3(2,2,2), 3, 3, 3);
		for (size_t i = 0; i < g.probes.size(); i++)
			g.probes[i].coefficients[0] = Vec3(1.f, 2.f, 3.f);

		const Vec3 pts[5] = { Vec3(0,0,0), Vec3(4,4,4), Vec3(1.7f,0.3f,3.9f),
							  Vec3(-50,-50,-50), Vec3(99,99,99) };
		f32 worst = 0.f;
		for (uint32 i = 0; i < 5; i++)
		{
			const SphericalHarmonicsL2 s = g.Sample(pts[i]);
			worst = std::max(worst, fabsf(s.coefficients[0].x - 1.f));
			worst = std::max(worst, fabsf(s.coefficients[0].z - 3.f));
		}
		near(worst, 0.f, 1e-5f, "a uniform grid returns that value everywhere, inside and outside");
	}

	// ---- a linear gradient interpolates linearly ------------------------
	//
	// Trilinear interpolation reproduces a linear field exactly. So if the
	// weights, the axis order and the cell origin are all right, sampling
	// a grid whose probes vary linearly in X must return exactly the
	// analytic value - not approximately.
	{
		IrradianceProbeGrid g;
		g.Allocate(Vec3(-3.f, 0.f, 0.f), Vec3(1.5f, 2.f, 4.f), 5, 3, 2);
		for (uint32 z=0; z<2; z++) for (uint32 y=0; y<3; y++) for (uint32 x=0; x<5; x++)
		{
			const Vec3 p = g.ProbePosition(x,y,z);
			// f(x,y,z) = 1 + 2x  (channel r), 5 - y (g), 0.25z (b)
			g.probes[g.Index(x,y,z)].coefficients[0] = Vec3(1.f + 2.f*p.x, 5.f - p.y, 0.25f*p.z);
		}
		const Vec3 q(-0.7f, 3.1f, 2.5f); // inside, off-lattice on every axis
		const SphericalHarmonicsL2 s = g.Sample(q);
		near(s.coefficients[0].x, 1.f + 2.f*q.x, 1e-4f, "linear field in X reproduces exactly");
		near(s.coefficients[0].y, 5.f - q.y,     1e-4f, "linear field in Y reproduces exactly");
		near(s.coefficients[0].z, 0.25f*q.z,     1e-4f, "linear field in Z reproduces exactly");
	}

	// ---- the boundary is the last probe, not one past it ----------------
	{
		IrradianceProbeGrid g;
		g.Allocate(Vec3(0,0,0), Vec3(1,1,1), 2, 2, 2);
		for (size_t i = 0; i < g.probes.size(); i++)
			g.probes[i].coefficients[0] = Vec3(7.f, 7.f, 7.f);
		g.probes[g.Index(1,1,1)].coefficients[0] = Vec3(9.f, 9.f, 9.f);
		// Exactly on the far corner: must be the corner probe's own value,
		// which only holds if the upper index is clamped rather than
		// running off the end.
		near(g.Sample(Vec3(1,1,1)).coefficients[0].x, 9.f, 1e-5f, "sampling the far corner returns that probe");
		near(g.Sample(Vec3(0,0,0)).coefficients[0].x, 7.f, 1e-5f, "sampling the near corner returns that probe");
		near(g.Sample(Vec3(0.5f,1,1)).coefficients[0].x, 8.f, 1e-5f, "halfway along the top edge is the mean");
	}

	// ---- irradiance comes out the far end -------------------------------
	//
	// Guards the whole chain rather than the interpolation alone: a grid
	// of probes each holding a unit-white environment must light a
	// surface exactly as that environment does - 1.0 for every normal.
	{
		IrradianceProbeGrid g;
		g.Allocate(Vec3(0,0,0), Vec3(1,1,1), 2, 2, 2);
		SphericalHarmonicsL2 white;
		white.coefficients[0] = Vec3(2.f*sqrtf(3.14159265f), 2.f*sqrtf(3.14159265f), 2.f*sqrtf(3.14159265f));
		for (size_t i = 0; i < g.probes.size(); i++) g.probes[i] = white;
		const Vec3 e = g.AmbientIrradianceAt(Vec3(0.5f,0.5f,0.5f), Vec3(0,1,0));
		near(e.x, 1.f, 2e-3f, "a grid of unit-white probes reflects exactly 1.0");
	}

	printf("\n%s  probe_grid: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
