// Prefiltered radiance: does a probe actually reflect the room?
//
// Diffuse GI is easy to check - the floor next to a red wall goes red.
// Specular is easier to get subtly wrong and harder to see: a
// reflection that is too blurry, or that ignores roughness, or that
// points the wrong way, all still look like "a reflection". So this
// measures the three things that distinguish a prefiltered radiance
// atlas from a blurry copy of the irradiance one:
//
//   1. It is DIRECTIONAL. Looking at the red wall reflects red;
//      turning to the green wall reflects green. Irradiance cannot do
//      this - it is already integrated over the hemisphere.
//   2. It RESPONDS TO ROUGHNESS. The same direction desaturates as
//      roughness rises, converging on the room average.
//   3. It obeys the SAME visibility as diffuse. A reflection is not
//      allowed to see through a wall the diffuse path is blocked by.
//
//   c++ -std=c++17 -I include tools/tests/ddgi_specular.cpp \
//       -o /tmp/ddgi_specular -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <Pyros3D/Rendering/GI/BRDFLut.h>
#include <cmath>
#include <cstdio>
#include <string>

using namespace p3d;
static int failures = 0;
static void check(bool c, const std::string &w, const std::string &e = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", w.c_str(), e.empty()?"":" - ", e.c_str());
	if(!c) failures++;
}
static void AddQuad(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d, uint32 mat)
{
	// Inward-facing - see the note in tools/tests/ddgi.cpp's AddQuad.
	RayTriangle t1, t2;
	t1.v0=a; t1.v1=c; t1.v2=b; t1.materialIndex=mat;
	t2.v0=a; t2.v1=d; t2.v2=c; t2.materialIndex=mat;
	Vec3 n = (c-a).cross(b-a).normalize();
	t1.n0=t1.n1=t1.n2=n; t2.n0=t2.n1=t2.n2=n;
	s.triangles.push_back(t1); s.triangles.push_back(t2);
}
static uint32 AddMaterial(RayScene &s, const Vec3 &albedo)
{
	RayMaterial m; m.albedo = albedo;
	s.materials.push_back(m);
	return (uint32)s.materials.size() - 1;
}
static f32 Saturation(const Vec3 &c, const int channel)
{
	const f32 v[3] = { c.x, c.y, c.z };
	const f32 other = (v[(channel+1)%3] + v[(channel+2)%3]) * 0.5f;
	return v[channel] / (other + 1e-6f);
}

int main()
{
	// ---- the same Cornell box the diffuse test uses ------------------
	RayScene scene;
	const uint32 white = AddMaterial(scene, Vec3(0.75f, 0.75f, 0.75f));
	const uint32 red   = AddMaterial(scene, Vec3(0.75f, 0.06f, 0.06f));
	const uint32 green = AddMaterial(scene, Vec3(0.06f, 0.75f, 0.06f));
	const f32 H = 5.f;
	AddQuad(scene, Vec3(-H,-H,-H), Vec3( H,-H,-H), Vec3( H,-H, H), Vec3(-H,-H, H), white);
	AddQuad(scene, Vec3(-H, H, H), Vec3( H, H, H), Vec3( H, H,-H), Vec3(-H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H, H), Vec3( H,-H, H), Vec3( H, H, H), Vec3(-H, H, H), white);
	AddQuad(scene, Vec3( H,-H,-H), Vec3(-H,-H,-H), Vec3(-H, H,-H), Vec3( H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H,-H), Vec3(-H,-H, H), Vec3(-H, H, H), Vec3(-H, H,-H), red);
	AddQuad(scene, Vec3( H,-H, H), Vec3( H,-H,-H), Vec3( H, H,-H), Vec3( H, H, H), green);
	scene.Build(4);

	std::vector<RayLight> lights(1);
	lights[0].isPoint = 1.f;
	lights[0].positionOrDirection = Vec3(0.f, 4.f, 0.f);
	lights[0].color = Vec3(4.f, 4.f, 4.f);
	lights[0].range = 30.f;

	// ---- allocation ---------------------------------------------------
	{
		DDGIVolume off;
		check(off.Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16, 16, 0),
			"a volume with zero radiance levels allocates");
		check(off.GetRadianceAtlas().GetResolution() == 0,
			"and does not pay for a radiance atlas it will not use");
		check(off.SampleRadiance(Vec3(0,0,0), Vec3(0,1,0), Vec3(1,0,0), 0.3f).magnitude() == 0.f,
			"sampling it returns black rather than reading unallocated memory");
	}

	// ---- the level roughness ladder ------------------------------------
	{
		check(DDGIVolume::LevelRoughness(0, 4) == DDGIVolume::MinRoughness(),
			"level 0 sits at the minimum resolvable roughness, not at zero",
			std::to_string(DDGIVolume::LevelRoughness(0, 4)));
		check(DDGIVolume::LevelRoughness(3, 4) == 1.f, "the top level is fully rough");
		bool rising = true;
		for (uint32 i = 1; i < 4; i++)
			if (DDGIVolume::LevelRoughness(i,4) <= DDGIVolume::LevelRoughness(i-1,4)) rising = false;
		check(rising, "the ladder is strictly increasing");
	}

	DDGIVolume v;
	check(v.Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16, 16, 4), "a 5x5x5 volume allocates");
	check(v.GetRadianceAtlas().GetResolution() == 16 && v.GetRadianceLevels() == 4,
		"with four prefiltered roughness levels");
	check(v.GetRadianceAtlas().GetData().size() ==
			(size_t)v.GetRadianceAtlas().GetWidth() * v.GetRadianceAtlas().GetHeight() * 4,
		"the atlas is sized for probes x levels");

	// One full trace, replacing outright - this is a measurement, not a
	// convergence test, so no hysteresis to reason about.
	v.Update(scene, lights, 256, 0, 0.f, 0);

	// A point in the middle of the floor, facing up. Its reflection
	// vector is what changes between the measurements below; the
	// surface and its normal do not, so any difference is the atlas
	// being directional and nothing else.
	const Vec3 p(0.f, -4.f, 0.f);
	const Vec3 n(0.f, 1.f, 0.f);

	// ---- 1. directionality ---------------------------------------------
	{
		// Grazing reflections toward each side wall.
		Vec3 toRed(-0.94f, 0.34f, 0.f); toRed.normalizeSelf();
		Vec3 toGreen(0.94f, 0.34f, 0.f); toGreen.normalizeSelf();
		const f32 rough = 0.25f;
		const Vec3 r = v.SampleRadiance(p, n, toRed, rough);
		const Vec3 g = v.SampleRadiance(p, n, toGreen, rough);
		printf("      reflecting the red wall    R=%.4f G=%.4f B=%.4f\n", r.x, r.y, r.z);
		printf("      reflecting the green wall  R=%.4f G=%.4f B=%.4f\n", g.x, g.y, g.z);
		check(r.magnitude() > 0.f, "the reflection is not black");
		check(r.x > r.y * 1.3f, "looking at the red wall reflects red",
			"R/G = " + std::to_string(r.x / (r.y + 1e-6f)));
		check(g.y > g.x * 1.3f, "looking at the green wall reflects green",
			"G/R = " + std::to_string(g.y / (g.x + 1e-6f)));
	}

	// ---- 2. roughness blurs ----------------------------------------------
	{
		Vec3 toRed(-0.94f, 0.34f, 0.f); toRed.normalizeSelf();
		const Vec3 sharp = v.SampleRadiance(p, n, toRed, DDGIVolume::MinRoughness());
		const Vec3 rough = v.SampleRadiance(p, n, toRed, 1.f);
		const f32 sSharp = Saturation(sharp, 0), sRough = Saturation(rough, 0);
		printf("      red-wall reflection: sharp R/avg=%.3f  rough R/avg=%.3f\n", sSharp, sRough);
		check(sSharp > sRough * 1.15f,
			"a rough surface reflects a blurrier, less saturated wall than a smooth one",
			"sharp " + std::to_string(sSharp) + " vs rough " + std::to_string(sRough));

		// And the blur must be gradual, not a switch between two
		// levels: interpolating across the ladder is what stops a
		// roughness gradient from banding.
		f32 prev = sSharp; bool monotonic = true;
		for (uint32 i = 1; i <= 8; i++)
		{
			const f32 s = Saturation(v.SampleRadiance(p, n, toRed, (f32)i / 8.f), 0);
			if (s > prev + 0.02f) monotonic = false;
			prev = s;
		}
		check(monotonic, "saturation falls smoothly across the roughness ladder");
	}

	// ---- 3. a rough reflection agrees with the diffuse gather ------------
	//
	// At roughness 1 the GGX lobe is nearly the cosine lobe, so the
	// prefiltered radiance along the normal and the irradiance along
	// the normal are estimates of almost the same integral. They are
	// not identical - the lobes differ - but an implementation that had
	// the two filters confused, or the atlas indexed wrongly, would not
	// land anywhere near.
	{
		const Vec3 diffuse = v.SampleIrradiance(p, n);
		const Vec3 spec = v.SampleRadiance(p, n, n, 1.f);
		const f32 ratio = spec.magnitude() / (diffuse.magnitude() + 1e-6f);
		printf("      along the normal: irradiance %.4f, rough radiance %.4f\n",
			diffuse.magnitude(), spec.magnitude());
		check(ratio > 0.5f && ratio < 2.0f,
			"at roughness 1 the specular gather lands near the diffuse one",
			"ratio = " + std::to_string(ratio));
	}

	// ---- 4. reflections do not see through walls --------------------------
	//
	// The one property that a blurred cubemap would fail. Outside the
	// sealed box, facing away, a reflection pointing back INTO the box
	// must still come back black: the probes inside are rejected by the
	// same Chebyshev test the diffuse path uses.
	{
		RayScene sealed;
		const uint32 bright = AddMaterial(sealed, Vec3(0.9f, 0.9f, 0.9f));
		const f32 B = 3.f;
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
		inner[0].color = Vec3(6.f, 6.f, 6.f);
		inner[0].range = 20.f;

		DDGIVolume s;
		// Spacing 2 over a box of half-extent 3, so no probe lands
		// exactly ON a wall. A probe embedded in geometry reports
		// nonsense distances in every direction, survives its own
		// Chebyshev test, and - being the only survivor - gets
		// renormalised back to full strength. Measured at spacing 3,
		// where a probe sits exactly in the floor plane, the DIFFUSE
		// path leaks 4.99 against 5.15 inside; this is not a specular
		// defect and not something to hide inside a specular test. It
		// is what probe relocation exists to fix, and the volume does
		// not do that yet.
		s.Allocate(Vec3(-6,-6,-6), Vec3(2,2,2), 7,7,7, 8, 16, 16, 4);
		for (uint32 f = 0; f < 8; f++)
			s.Update(sealed, inner, 256, f, 0.f, 0);

		const Vec3 in = s.SampleRadiance(Vec3(0.f, 0.f, 0.f), Vec3(0,1,0), Vec3(0,1,0), 0.3f);
		// Just past the +X wall, facing away from the box, reflecting
		// back toward the wall it cannot see through.
		const Vec3 out = s.SampleRadiance(Vec3(4.2f, 0.f, 0.f), Vec3(1,0,0), Vec3(-1,0,0), 0.3f);
		printf("      sealed box: reflection inside %.4f, outside %.4f\n",
			in.magnitude(), out.magnitude());
		check(in.magnitude() > 0.1f, "the reflection inside the sealed box is lit");
		check(out.magnitude() < in.magnitude() * 0.05f,
			"and does not leak out through the wall",
			"outside/inside = " + std::to_string(out.magnitude()/(in.magnitude()+1e-6f)));
	}

	// ---- 5. the two halves of the split sum combine sanely ----------------
	//
	// Prefiltered radiance times environment BRDF is the whole specular
	// term. For a metal (F0 = 1) it must not exceed the radiance it is
	// reflecting - a mirror cannot be brighter than what it reflects.
	{
		BRDFLut lut;
		lut.Generate(64, 256);
		Vec3 toRed(-0.94f, 0.34f, 0.f); toRed.normalizeSelf();
		bool conserved = true;
		f32 worst = 0.f;
		for (uint32 i = 0; i <= 8; i++)
		{
			const f32 rough = 0.05f + (f32)i * 0.115f;
			const Vec3 pre = v.SampleRadiance(p, n, toRed, rough);
			for (uint32 j = 0; j <= 4; j++)
			{
				f32 sc, bi;
				lut.Sample(0.1f + (f32)j * 0.22f, rough, sc, bi);
				const f32 factor = sc + bi;   // F0 = 1, the brightest case
				if (factor > 1.001f) conserved = false;
				worst = fmaxf(worst, (pre * factor).magnitude() / (pre.magnitude() + 1e-6f));
			}
		}
		check(conserved && worst <= 1.001f,
			"radiance x environment BRDF never brightens the reflection",
			"worst gain = " + std::to_string(worst));
	}

	printf("\n%s  ddgi_specular: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
