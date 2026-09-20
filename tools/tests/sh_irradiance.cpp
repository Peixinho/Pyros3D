// Spherical-harmonic irradiance, checked against things that are true
// independently of this implementation.
//
// SH is the kind of code that produces a smooth, plausible, completely
// wrong answer when a constant or an axis is off by a little - and it is
// invisible on screen, because the failure mode is "the ambient is a
// slightly different colour than it should be", which nobody can source.
// So none of these checks compare against a recorded golden value. They
// compare against closed-form results and against brute-force numerical
// integration of the same environment.
//
//   c++ -std=c++17 -I include tools/tests/sh_irradiance.cpp \
//       src/Pyros3D/Rendering/GI/SphericalHarmonics.cpp \
//       src/Pyros3D/Core/Math/Vec3.cpp -o /tmp/sh_irradiance
//   /tmp/sh_irradiance
#include <Pyros3D/Rendering/GI/SphericalHarmonics.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static const f32 kPi = 3.14159265358979323846f;

static void check(bool cond, const std::string &what, const std::string &extra = std::string())
{
	printf("%s  %s%s%s\n", cond ? "PASS" : "FAIL", what.c_str(),
		extra.empty() ? "" : " - ", extra.c_str());
	if (!cond) failures++;
}

static void checkNear(f32 got, f32 want, f32 tol, const std::string &what)
{
	char buf[160];
	snprintf(buf, sizeof(buf), "got %.6f want %.6f (tol %.4f)", got, want, tol);
	check(fabsf(got - want) <= tol, what, buf);
}

// Fills all 6 faces with one constant colour.
static void MakeUniformCubemap(std::vector<std::vector<f32> > &storage,
	std::vector<CubemapFacePixels> &faces, uint32 size, f32 r, f32 g, f32 b)
{
	storage.assign(6, std::vector<f32>(size * size * 3));
	faces.clear();
	for (uint32 f = 0; f < 6; f++)
	{
		for (uint32 i = 0; i < size * size; i++)
		{
			storage[f][i * 3 + 0] = r;
			storage[f][i * 3 + 1] = g;
			storage[f][i * 3 + 2] = b;
		}
		faces.push_back(CubemapFacePixels(storage[f].data(), size));
	}
}

// The direction a texel faces, duplicated here ON PURPOSE. If the test
// imported the projector's own mapping, a wrong mapping would agree with
// itself and every check below would pass. This is written from the
// cubemap convention independently; the brute-force integral uses it, and
// agreement between the two is the actual evidence.
static Vec3 FaceDirection(uint32 face, f32 u, f32 v)
{
	switch (face)
	{
	case 0: return Vec3( 1.f,   -v,   -u);
	case 1: return Vec3(-1.f,   -v,    u);
	case 2: return Vec3(   u,  1.f,    v);
	case 3: return Vec3(   u, -1.f,   -v);
	case 4: return Vec3(   u,   -v,  1.f);
	default:return Vec3(  -u,   -v, -1.f);
	}
}

// Irradiance by direct numerical integration: sum over every texel of
// radiance * max(0, N.L) * solidAngle. No SH anywhere. This is the
// ground truth an order-2 projection is trying to approximate.
static Vec3 BruteForceIrradiance(const std::vector<CubemapFacePixels> &faces, const Vec3 &normal)
{
	Vec3 total(0.f, 0.f, 0.f);
	const uint32 size = faces[0].size;
	const f32 invSize = 1.f / (f32)size;
	for (uint32 face = 0; face < 6; face++)
	{
		const f32 *pixels = faces[face].pixels;
		for (uint32 py = 0; py < size; py++)
		{
			const f32 v0 = 2.f * ((f32)py * invSize) - 1.f;
			const f32 v1 = 2.f * ((f32)(py + 1) * invSize) - 1.f;
			for (uint32 px = 0; px < size; px++)
			{
				const f32 u0 = 2.f * ((f32)px * invSize) - 1.f;
				const f32 u1 = 2.f * ((f32)(px + 1) * invSize) - 1.f;
				const f32 a = atanf((u1 * v1) / sqrtf(u1 * u1 + v1 * v1 + 1.f));
				const f32 b = atanf((u0 * v1) / sqrtf(u0 * u0 + v1 * v1 + 1.f));
				const f32 c = atanf((u1 * v0) / sqrtf(u1 * u1 + v0 * v0 + 1.f));
				const f32 d = atanf((u0 * v0) / sqrtf(u0 * u0 + v0 * v0 + 1.f));
				const f32 solidAngle = a - b - c + d;

				Vec3 dir = FaceDirection(face, 0.5f * (u0 + u1), 0.5f * (v0 + v1));
				dir.normalizeSelf();
				const f32 ndotl = dir.x * normal.x + dir.y * normal.y + dir.z * normal.z;
				if (ndotl <= 0.f) continue;

				const uint32 idx = (py * size + px) * 3;
				total += Vec3(pixels[idx + 0], pixels[idx + 1], pixels[idx + 2]) * (ndotl * solidAngle);
			}
		}
	}
	return total;
}

int main()
{
	// ---- 1. the basis itself ------------------------------------------
	//
	// The band-0 function is the constant 1/(2*sqrt(pi)); every other
	// band integrates to zero over the sphere. Checking the constant
	// catches a mistyped normalisation, which is the single easiest way
	// to get a projection that is uniformly too bright or too dark.
	{
		f32 basis[9];
		EvaluateSHBasis(Vec3(0.f, 1.f, 0.f), basis);
		checkNear(basis[0], 0.5f / sqrtf(kPi), 1e-5f, "Y00 is 1/(2*sqrt(pi))");
		checkNear(basis[1], 0.488603f, 1e-5f, "Y1-1 at +Y is its full magnitude");
		checkNear(basis[2], 0.f, 1e-5f, "Y10 vanishes at +Y");
		checkNear(basis[3], 0.f, 1e-5f, "Y11 vanishes at +Y");
	}

	// ---- 2. a uniform environment, which has a closed form -------------
	//
	// Radiance 1 in every direction projects to L00 = 2*sqrt(pi) and
	// nothing else, because every higher band integrates to zero against
	// a constant. Irradiance is then exactly pi, and irradiance/pi is
	// exactly 1 - meaning a white surface under a unit-radiance white
	// environment reflects exactly 1, for EVERY normal. That last part is
	// the strongest single check here: it pins the constants, the
	// solid-angle weighting and the cosine convolution all at once.
	{
		std::vector<std::vector<f32> > storage;
		std::vector<CubemapFacePixels> faces;
		MakeUniformCubemap(storage, faces, 32, 1.f, 1.f, 1.f);

		SphericalHarmonicsL2 sh;
		check(ProjectCubemapToSH(faces, sh), "ProjectCubemapToSH(uniform white)");

		checkNear(sh.coefficients[0].x, 2.f * sqrtf(kPi), 1e-3f, "L00 of a unit-radiance sphere is 2*sqrt(pi)");
		f32 maxHigher = 0.f;
		for (uint32 i = 1; i < 9; i++)
		{
			maxHigher = std::max(maxHigher, fabsf(sh.coefficients[i].x));
			maxHigher = std::max(maxHigher, fabsf(sh.coefficients[i].y));
			maxHigher = std::max(maxHigher, fabsf(sh.coefficients[i].z));
		}
		check(maxHigher < 1e-3f, "every band above 0 vanishes for a uniform environment",
			"max |coeff| = " + std::to_string(maxHigher));

		const Vec3 normals[6] = {
			Vec3(1,0,0), Vec3(-1,0,0), Vec3(0,1,0), Vec3(0,-1,0), Vec3(0,0,1), Vec3(0,0,-1)
		};
		f32 worst = 0.f;
		for (uint32 i = 0; i < 6; i++)
			worst = std::max(worst, fabsf(sh.AmbientIrradiance(normals[i]).x - 1.f));
		checkNear(1.f - worst, 1.f, 2e-3f, "AmbientIrradiance is 1.0 for every normal under a unit white environment");

		checkNear(sh.Irradiance(Vec3(0,1,0)).x, kPi, 5e-3f, "raw Irradiance of that environment is PI");
	}

	// ---- 3. scaling is linear ------------------------------------------
	{
		std::vector<std::vector<f32> > storage;
		std::vector<CubemapFacePixels> faces;
		MakeUniformCubemap(storage, faces, 16, 0.5f, 0.25f, 0.125f);
		SphericalHarmonicsL2 sh;
		ProjectCubemapToSH(faces, sh);
		const Vec3 before = sh.AmbientIrradiance(Vec3(0,1,0));
		sh.Scale(3.f);
		const Vec3 after = sh.AmbientIrradiance(Vec3(0,1,0));
		checkNear(after.x, before.x * 3.f, 1e-4f, "Scale(3) triples the reconstructed irradiance");
		// And the per-channel values must stay independent - a projection
		// that accidentally shared a channel would pass every grey test
		// above and fail here.
		checkNear(before.x, 0.5f, 3e-3f, "red channel of a 0.5/0.25/0.125 environment");
		checkNear(before.y, 0.25f, 3e-3f, "green channel");
		checkNear(before.z, 0.125f, 3e-3f, "blue channel");
	}

	// ---- 4. a directional environment, against brute force -------------
	//
	// One bright face, everything else black. This has real content in
	// bands 1 and 2, so it exercises the parts the uniform case cannot,
	// and it is checked against a direct cosine-weighted integral rather
	// than against itself. Order-2 SH is an approximation, so the
	// tolerance is generous - but the sign, the direction and the rough
	// magnitude all have to be right, and they are exactly what a wrong
	// axis or a wrong constant would break.
	{
		const uint32 size = 32;
		std::vector<std::vector<f32> > storage(6, std::vector<f32>(size * size * 3, 0.f));
		std::vector<CubemapFacePixels> faces;
		// Face 2 is +Y: a bright sky overhead.
		for (uint32 i = 0; i < size * size; i++)
		{
			storage[2][i * 3 + 0] = 1.f;
			storage[2][i * 3 + 1] = 1.f;
			storage[2][i * 3 + 2] = 1.f;
		}
		for (uint32 f = 0; f < 6; f++)
			faces.push_back(CubemapFacePixels(storage[f].data(), size));

		SphericalHarmonicsL2 sh;
		check(ProjectCubemapToSH(faces, sh), "ProjectCubemapToSH(bright +Y face)");

		const Vec3 up(0.f, 1.f, 0.f);
		const Vec3 down(0.f, -1.f, 0.f);

		const Vec3 shUp = sh.Irradiance(up);
		const Vec3 shDown = sh.Irradiance(down);
		const Vec3 bfUp = BruteForceIrradiance(faces, up);
		const Vec3 bfDown = BruteForceIrradiance(faces, down);

		printf("      up:   SH %.4f  brute %.4f\n", shUp.x, bfUp.x);
		printf("      down: SH %.4f  brute %.4f\n", shDown.x, bfDown.x);

		// A surface facing the bright face must receive far more than one
		// facing away. This is the check that catches a flipped axis.
		check(shUp.x > shDown.x * 4.f, "a normal facing the lit face receives much more than one facing away");

		// Within order-2's approximation error of the true integral.
		check(fabsf(shUp.x - bfUp.x) < 0.15f * bfUp.x,
			"SH irradiance matches brute force facing the light",
			"SH " + std::to_string(shUp.x) + " vs " + std::to_string(bfUp.x));

		// Order-2 SH famously rings slightly negative behind a sharp
		// source; requiring only that it stays small and non-negative-ish
		// is the honest bound, not zero.
		check(shDown.x < 0.25f * bfUp.x && shDown.x > -0.05f * bfUp.x,
			"the shadowed side stays near zero without ringing badly",
			"SH " + std::to_string(shDown.x));

		// Sideways normals should land between the two extremes.
		const Vec3 side = sh.Irradiance(Vec3(1.f, 0.f, 0.f));
		check(side.x > shDown.x && side.x < shUp.x, "a sideways normal lands between the lit and unlit extremes");
	}

	// ---- 5. malformed input is refused ---------------------------------
	{
		SphericalHarmonicsL2 sh;
		std::vector<CubemapFacePixels> tooFew(5);
		check(!ProjectCubemapToSH(tooFew, sh), "five faces is refused");
		check(sh.IsZero(), "a refused projection leaves the coefficients cleared");

		std::vector<std::vector<f32> > storage;
		std::vector<CubemapFacePixels> faces;
		MakeUniformCubemap(storage, faces, 8, 1.f, 1.f, 1.f);
		faces[3].size = 4; // mismatched face
		check(!ProjectCubemapToSH(faces, sh), "a face of a different size is refused");
	}

	printf("\n%s  sh_irradiance: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
	return failures == 0 ? 0 : 1;
}
