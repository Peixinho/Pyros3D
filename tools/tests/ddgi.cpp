// The two claims that separate global illumination from ambient light,
// measured rather than looked at.
//
//   1. COLOUR BLEEDS. A red wall tints a white floor that receives no
//      direct light of its own. If this fails there is no indirect
//      light, whatever the screenshot suggests.
//   2. LIGHT DOES NOT LEAK. A probe on the far side of a wall does not
//      contribute through it. This is the defect that makes a naive
//      probe grid unusable, and the Chebyshev visibility test is the
//      only reason it passes.
//
// A Cornell box is used because its answer is known in advance: the
// left wall is red, the right is green, and the white floor between
// them must come out reddish on the left and greenish on the right.
// Nobody has to interpret that.
//
//   c++ -std=c++17 -I include tools/tests/ddgi.cpp -o /tmp/ddgi \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/ddgi
#include <Pyros3D/Rendering/GI/DDGIVolume.h>
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

static void AddQuad(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d, uint32 mat)
{
	RayTriangle t1, t2;
	t1.v0=a; t1.v1=b; t1.v2=c; t1.materialIndex=mat;
	t2.v0=a; t2.v1=c; t2.v2=d; t2.materialIndex=mat;
	Vec3 n = (b-a).cross(c-a).normalize();
	t1.n0=t1.n1=t1.n2=n;
	t2.n0=t2.n1=t2.n2=n;
	s.triangles.push_back(t1);
	s.triangles.push_back(t2);
}

static uint32 AddMaterial(RayScene &s, const Vec3 &albedo)
{
	RayMaterial m; m.albedo = albedo;
	s.materials.push_back(m);
	return (uint32)s.materials.size() - 1;
}

int main()
{
	// ---- a Cornell box, 10 units on a side, centred on the origin -----
	RayScene scene;
	const uint32 white = AddMaterial(scene, Vec3(0.75f, 0.75f, 0.75f));
	const uint32 red   = AddMaterial(scene, Vec3(0.75f, 0.06f, 0.06f));
	const uint32 green = AddMaterial(scene, Vec3(0.06f, 0.75f, 0.06f));
	const f32 H = 5.f;

	// floor, ceiling, back wall - white
	AddQuad(scene, Vec3(-H,-H,-H), Vec3( H,-H,-H), Vec3( H,-H, H), Vec3(-H,-H, H), white);
	AddQuad(scene, Vec3(-H, H, H), Vec3( H, H, H), Vec3( H, H,-H), Vec3(-H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H, H), Vec3( H,-H, H), Vec3( H, H, H), Vec3(-H, H, H), white);
	// left wall red, right wall green
	AddQuad(scene, Vec3(-H,-H,-H), Vec3(-H,-H, H), Vec3(-H, H, H), Vec3(-H, H,-H), red);
	AddQuad(scene, Vec3( H,-H, H), Vec3( H,-H,-H), Vec3( H, H,-H), Vec3( H, H, H), green);
	scene.Build(4);
	check(scene.TriangleCount() == 10, "the box has 10 triangles",
		std::to_string(scene.TriangleCount()));

	// A point light INSIDE the box, near the ceiling.
	//
	// Not a directional light: the box is sealed, so a light outside it
	// illuminates nothing at all - every shadow ray from the floor hits
	// the ceiling. That is physically right and it is what the first
	// version of this test got wrong, measuring a correct zero.
	//
	// White, so the light itself contributes no colour. Any red or
	// green reaching the floor has bounced off a wall.
	std::vector<RayLight> lights(1);
	lights[0].isPoint = 1.f;
	lights[0].positionOrDirection = Vec3(0.f, 4.f, 0.f);
	lights[0].color = Vec3(6.f, 6.f, 6.f);
	lights[0].range = 40.f;

	DDGIVolume volume;
	check(volume.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 5, 5, 5, 8, 16),
		"a 5x5x5 volume allocates");
	volume.SetSkyColor(Vec3(0.f,0.f,0.f)); // closed box - nothing outside

	// hysteresis 0: converge immediately, so the test measures the
	// estimator and not the blend schedule.
	for (uint32 f = 0; f < 8; f++)
		volume.Update(scene, lights, 256, f, 0.f);

	// ---- 1. colour bleeding ---------------------------------------------
	//
	// Sample the floor near the red wall and near the green wall, facing
	// up. Direct light is white and identical at both, so any difference
	// between them is indirect.
	const Vec3 up(0.f, 1.f, 0.f);
	const Vec3 nearRed   = volume.SampleIrradiance(Vec3(-3.5f, -4.4f, 0.f), up);
	const Vec3 nearGreen = volume.SampleIrradiance(Vec3( 3.5f, -4.4f, 0.f), up);
	const Vec3 middle    = volume.SampleIrradiance(Vec3( 0.0f, -4.4f, 0.f), up);

	printf("      floor near red wall   R=%.4f G=%.4f B=%.4f\n", nearRed.x, nearRed.y, nearRed.z);
	printf("      floor centre          R=%.4f G=%.4f B=%.4f\n", middle.x, middle.y, middle.z);
	printf("      floor near green wall R=%.4f G=%.4f B=%.4f\n", nearGreen.x, nearGreen.y, nearGreen.z);

	check(nearRed.x > 1e-4f, "the floor receives indirect light at all",
		"R = " + std::to_string(nearRed.x));
	check(nearRed.x > nearRed.y * 1.25f, "next to the red wall the floor is REDDER than it is green",
		"R/G = " + std::to_string(nearRed.x / std::max(1e-6f, nearRed.y)));
	check(nearGreen.y > nearGreen.x * 1.25f, "next to the green wall the floor is GREENER than it is red",
		"G/R = " + std::to_string(nearGreen.y / std::max(1e-6f, nearGreen.x)));
	// And the effect is local - the centre should be far more neutral
	// than either edge, or the "bleed" is just a global tint.
	{
		const f32 edgeSkew = nearRed.x / std::max(1e-6f, nearRed.y);
		const f32 midSkew  = middle.x / std::max(1e-6f, middle.y);
		check(midSkew < edgeSkew * 0.85f, "the bleed is local, not a uniform tint over the whole floor",
			"centre R/G = " + std::to_string(midSkew) + " vs edge " + std::to_string(edgeSkew));
	}

	// ---- 2. no leaking through a wall ------------------------------------
	//
	// A sealed box with a bright interior, and a sample point just
	// OUTSIDE it. Probes inside the box are metres away and full of
	// light; without the Chebyshev test they interpolate straight
	// through the wall and light the outside.
	{
		RayScene sealed;
		const uint32 bright = AddMaterial(sealed, Vec3(0.9f, 0.9f, 0.9f));
		const f32 B = 3.f;
		// A fully closed cube.
		AddQuad(sealed, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), bright);
		AddQuad(sealed, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), bright);
		AddQuad(sealed, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), bright);
		AddQuad(sealed, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), bright);
		AddQuad(sealed, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), bright);
		AddQuad(sealed, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), bright);
		sealed.Build(4);

		std::vector<RayLight> inner(1);
		inner[0].isPoint = 1.f;
		inner[0].positionOrDirection = Vec3(0.f, 0.f, 0.f);
		inner[0].color = Vec3(8.f, 8.f, 8.f);
		inner[0].range = 20.f;

		DDGIVolume v2;
		// The volume spans well beyond the box, so some probes are
		// inside it and some outside - exactly the configuration that
		// leaks.
		v2.Allocate(Vec3(-6.f,-6.f,-6.f), Vec3(2.f,2.f,2.f), 7, 7, 7, 8, 16);
		v2.SetSkyColor(Vec3(0.f,0.f,0.f));
		for (uint32 f = 0; f < 8; f++)
			v2.Update(sealed, inner, 256, f, 0.f);

		// Inside: should be brightly lit.
		const Vec3 insideL = v2.SampleIrradiance(Vec3(0.f, 0.f, 0.f), up);
		// Outside, just past the +X wall, facing away from the box.
		const Vec3 outsideL = v2.SampleIrradiance(Vec3(4.2f, 0.f, 0.f), Vec3(1.f,0.f,0.f));

		printf("      inside the sealed box  %.4f\n", insideL.x);
		printf("      outside, facing away   %.4f\n", outsideL.x);

		check(insideL.x > 0.05f, "the sealed box is lit inside",
			"L = " + std::to_string(insideL.x));
		check(outsideL.x < insideL.x * 0.10f,
			"and that light does NOT leak through the wall to the outside",
			"outside/inside = " + std::to_string(outsideL.x / std::max(1e-6f, insideL.x)));
	}

	// ---- 3. an unlit scene stays dark -------------------------------------
	//
	// Guards against the whole thing returning a constant, which would
	// pass a surprising number of the checks above.
	{
		DDGIVolume dark;
		dark.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 3, 3, 3, 8, 16);
		dark.SetSkyColor(Vec3(0.f,0.f,0.f));
		std::vector<RayLight> none;
		for (uint32 f = 0; f < 4; f++)
			dark.Update(scene, none, 128, f, 0.f);
		const Vec3 l = dark.SampleIrradiance(Vec3(0.f,-4.4f,0.f), up);
		check(l.x < 1e-3f && l.y < 1e-3f && l.z < 1e-3f, "a scene with no lights produces no indirect light",
			"L = " + std::to_string(l.x));
	}

	// ---- multi-bounce ----------------------------------------------------
	//
	// A probe ray reports the light on whatever it hits. With one
	// bounce that is the DIRECT light there and nothing else, so a
	// room lit indirectly - a lamp aimed at the ceiling - comes out
	// nearly black however many rays are traced. Feeding the volume's
	// own irradiance back at each hit adds a bounce per update, for no
	// extra rays.
	//
	// The two things that can go wrong are opposite and both are
	// checked: it does nothing, or it never stops.
	{
		RayScene box;
		const uint32 grey = AddMaterial(box, Vec3(0.75f, 0.75f, 0.75f));
		const f32 B = 5.f;
		AddQuad(box, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(box, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), grey);
		AddQuad(box, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(box, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), grey);
		AddQuad(box, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), grey);
		AddQuad(box, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), grey);
		box.Build(4);

		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(0.f, 3.f, 0.f);
		lamp[0].color = Vec3(3.f, 3.f, 3.f);
		lamp[0].range = 25.f;

		const Vec3 probePoint(0.f, -4.f, 0.f);

		DDGIVolume single, multi;
		single.Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16);
		multi .Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16);
		single.SetMultiBounce(0.f);

		f32 prev = 0.f, growth = 0.f;
		for (uint32 f = 0; f < 40; f++)
		{
			single.Update(box, lamp, 128, f, 0.f, 0);
			multi.Update(box, lamp, 128, f, 0.f, 0);
			const f32 e = multi.SampleIrradiance(probePoint, up).x;
			// The growth over the LAST few updates, once it should
			// have settled.
			if (f >= 36) growth = fmaxf(growth, e - prev);
			prev = e;
		}
		const f32 one = single.SampleIrradiance(probePoint, up).x;
		const f32 many = multi.SampleIrradiance(probePoint, up).x;
		printf("      one bounce %.4f, converged multi-bounce %.4f (x%.2f)\n",
			one, many, many / fmaxf(one, 1e-6f));

		check(many > one * 1.2f, "multi-bounce puts more light in the room than one bounce",
			"x" + std::to_string(many / fmaxf(one, 1e-6f)));

		// The series is sum of albedo^n, so a grey room of albedo 0.75
		// lands near 1/(1-0.75) = 4x. Bounded on both sides: too low
		// means the feedback is being swallowed, too high means it is
		// being counted more than once per update.
		check(many < one * 6.f, "and not more than the geometric series allows",
			"x" + std::to_string(many / fmaxf(one, 1e-6f)));

		// The failure that matters. Feedback is a loop; if its gain is
		// wrong it does not look wrong for a few frames, it grows
		// without bound and the scene whites out minutes later.
		check(growth < many * 0.01f, "and it converges rather than running away",
			"still growing by " + std::to_string(growth) + " per update at update 40");

		// Leak-through-a-wall is the other thing feedback amplifies -
		// whatever crosses a wall is re-injected every update - and it
		// is the sealed-box check above that covers it, now that
		// multi-bounce is on by default there. Not repeated here: this
		// volume lies entirely INSIDE its box, so a sample beyond the
		// floor is clamped back into the lit room and would measure a
		// leak of exactly 1.0 whatever the code did.
	}

	printf("\n%s  ddgi: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
