// The environment BRDF table, against what the integral must produce.
//
// A wrong LUT does not look broken - it makes metals slightly too dark
// or too bright at grazing angles, uniformly, which reads as "the
// material is off" rather than "the table is wrong". So none of these
// check recorded numbers; they check properties the split-sum integral
// has to satisfy whatever implementation produced it.
//
//   c++ -std=c++17 -I include tools/tests/brdf_lut.cpp -o /tmp/brdf_lut \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
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

int main()
{
	BRDFLut lut;
	check(!lut.Generate(1, 64), "a 1x1 table is refused");
	check(!lut.Generate(64, 0), "zero samples is refused");
	check(lut.Generate(64, 512), "64x64 table generates");
	check(lut.GetSize() == 64 && lut.GetData().size() == 64*64*2, "table is the right size");

	// ---- every entry must be a usable reflectance factor -------------
	//
	// scale+bias is the fraction of specular energy leaving the surface
	// for F0 = 1. Above 1 means the BRDF creates energy; below 0 is
	// meaningless. Either indicates a sign or normalisation error.
	{
		f32 worstHigh = 0.f, worstLow = 1.f;
		for (uint32 y = 0; y < 64; y++)
			for (uint32 x = 0; x < 64; x++)
			{
				const f32 s = lut.GetData()[(y*64+x)*2+0];
				const f32 b = lut.GetData()[(y*64+x)*2+1];
				if (s + b > worstHigh) worstHigh = s + b;
				if (s + b < worstLow) worstLow = s + b;
				if (s < -1e-4f || b < -1e-4f) { worstLow = -1.f; }
			}
		check(worstHigh <= 1.02f, "no entry reflects more energy than arrives",
			"max scale+bias = " + std::to_string(worstHigh));
		check(worstLow >= 0.f, "no entry is negative",
			"min scale+bias = " + std::to_string(worstLow));
	}

	// ---- a smooth mirror at normal incidence reflects nearly all ------
	//
	// roughness -> 0, N.V -> 1: the GGX lobe collapses to a mirror and
	// the geometry term goes to 1, so scale must approach 1 and bias 0.
	// This is the corner where a wrong Smith k shows up most clearly.
	{
		f32 s, b;
		lut.Sample(1.f, 0.02f, s, b);
		check(s > 0.92f, "a smooth surface head-on reflects nearly all of F0",
			"scale = " + std::to_string(s));
		check(b < 0.08f, "with almost no Fresnel bias there",
			"bias = " + std::to_string(b));
	}

	// ---- rough surfaces lose energy to shadowing ----------------------
	//
	// Monotonic in roughness at fixed N.V: single-scattering GGX does
	// not compensate for multiple bounces, so it must get dimmer. A
	// table that rose with roughness would have the geometry term
	// inverted.
	{
		f32 prev = 2.f; bool monotonic = true;
		for (uint32 i = 1; i <= 9; i++)
		{
			f32 s, b; lut.Sample(0.9f, (f32)i * 0.1f, s, b);
			if (s + b > prev + 1e-3f) monotonic = false;
			prev = s + b;
		}
		check(monotonic, "total reflectance falls as roughness rises");
	}

	// ---- grazing angles keep energy through the Fresnel bias ----------
	//
	// At grazing incidence Fresnel drives reflectance toward 1 whatever
	// F0 is, and in the split-sum that shows up in the BIAS channel. A
	// table where bias vanished at grazing would kill every rim
	// highlight in the renderer.
	{
		f32 sHead, bHead, sGraze, bGraze;
		lut.Sample(0.95f, 0.35f, sHead, bHead);
		lut.Sample(0.06f, 0.35f, sGraze, bGraze);
		check(bGraze > bHead, "the Fresnel bias grows toward grazing incidence",
			"grazing " + std::to_string(bGraze) + " vs head-on " + std::to_string(bHead));
	}

	// ---- determinism --------------------------------------------------
	//
	// Hammersley, not rand(): two tables generated independently must
	// be bit-identical, or the LUT would shimmer between runs and no
	// test of it could assert anything.
	{
		BRDFLut again;
		again.Generate(64, 512);
		f32 worst = 0.f;
		for (size_t i = 0; i < lut.GetData().size(); i++)
			worst = fmaxf(worst, fabsf(lut.GetData()[i] - again.GetData()[i]));
		check(worst == 0.f, "generation is deterministic", "worst |d| = " + std::to_string(worst));
	}

	// ---- more samples converges, rather than wandering ----------------
	{
		BRDFLut coarse, fine;
		coarse.Generate(32, 64);
		fine.Generate(32, 1024);
		f32 worst = 0.f;
		for (size_t i = 0; i < coarse.GetData().size(); i++)
			worst = fmaxf(worst, fabsf(coarse.GetData()[i] - fine.GetData()[i]));
		check(worst < 0.05f, "64 samples is already close to 1024",
			"worst |d| = " + std::to_string(worst));
	}

	// ---- why the table is a texture and not a polynomial --------------
	//
	// The shader could skip the sampler entirely by evaluating
	// Lazarov's well-known approximation to this table inline. It does
	// not, and this is the measurement behind that decision: the fit
	// is off by ~0.18 in scale and ~0.28 in bias against the real
	// integral, which on a metal is a visible error in how much it
	// reflects. (Fitting my own polynomial to this exact table got to
	// 0.13 and no further - the mirror corner is too steep.)
	//
	// So the assertion is the inverse of the usual one: the fit must
	// still be BAD ENOUGH to justify the sampler it replaced. If this
	// ever fails, the approximation has become good enough to ship and
	// uBRDFLut can go away.
	{
		BRDFLut ref;
		ref.Generate(128, 1024);
		f32 worstScale = 0.f, worstBias = 0.f, worstAt[2] = {0,0};
		for (uint32 y = 0; y < 128; y++)
			for (uint32 x = 0; x < 128; x++)
			{
				const f32 nDotV = ((f32)x + 0.5f) / 128.f;
				const f32 rough = ((f32)y + 0.5f) / 128.f;

				// EnvBRDFApprox, verbatim as the shader has it.
				const f32 c0[4] = { -1.f, -0.0275f, -0.572f, 0.022f };
				const f32 c1[4] = {  1.f,  0.0425f,  1.04f, -0.04f };
				const f32 r[4] = { rough*c0[0]+c1[0], rough*c0[1]+c1[1],
								   rough*c0[2]+c1[2], rough*c0[3]+c1[3] };
				const f32 a004 = std::min(r[0]*r[0], powf(2.f, -9.28f*nDotV)) * r[0] + r[1];
				const f32 scale = a004 * -1.04f + r[2];
				const f32 bias  = a004 *  1.04f + r[3];

				const f32 ds = fabsf(scale - ref.GetData()[(y*128+x)*2+0]);
				const f32 db = fabsf(bias  - ref.GetData()[(y*128+x)*2+1]);
				if (ds > worstScale) { worstScale = ds; worstAt[0]=nDotV; worstAt[1]=rough; }
				worstBias = fmaxf(worstBias, db);
			}
		printf("      analytic fit vs integrated: worst scale %.4f (at N.V=%.2f r=%.2f), worst bias %.4f\n",
			worstScale, worstAt[0], worstAt[1], worstBias);
		check(worstScale > 0.05f || worstBias > 0.05f,
			"the analytic fit is too far off to replace the sampled table",
			"worst scale " + std::to_string(worstScale) + ", worst bias " + std::to_string(worstBias));
		// And a floor on the same numbers: an error this large has to
		// come from the approximation, not from a broken reference.
		// k = r^2/2 was confirmed the right Smith k by brute force -
		// the direct-lighting k and the alpha-squared k are 0.42 and
		// 1.36 off respectively, against 0.18 for this one.
		check(worstScale < 0.5f && worstBias < 0.5f,
			"and not so far off that the table itself is suspect",
			"worst scale " + std::to_string(worstScale));
	}

	printf("\n%s  brdf_lut: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
