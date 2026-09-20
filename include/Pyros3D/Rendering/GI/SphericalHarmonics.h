//============================================================================
// Name        : SphericalHarmonics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Order-2 spherical harmonics for diffuse environment
//               lighting - the "irradiance" half of image-based lighting.
//============================================================================

#ifndef SPHERICALHARMONICS_H
#define SPHERICALHARMONICS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Math/Vec3.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	// Nine RGB coefficients: the whole diffuse response of an environment,
	// to within a few percent.
	//
	// Why nine numbers and not a convolved cubemap, which is the other
	// standard way to do this: the cosine lobe a diffuse surface integrates
	// against is so smooth that everything above order 2 contributes under
	// 1% of the energy (Ramamoorthi & Hanrahan 2001). So the irradiance of
	// ANY environment is captured almost exactly by 9 coefficients per
	// channel - 108 bytes - which costs no texture unit, no sampler, no
	// cubemap render pass, and works unchanged on WebGL2 where a float
	// cubemap is awkward. It is also exactly the payload an irradiance
	// probe stores, so the probe grid this engine does not have yet is a
	// spatial array of these and nothing more.
	//
	// Coefficient order is the standard (l, m) scan: index = l*(l+1) + m,
	// giving 0:(0,0) 1:(1,-1) 2:(1,0) 3:(1,1) 4:(2,-2) 5:(2,-1) 6:(2,0)
	// 7:(2,1) 8:(2,2). The shader's uAmbientSH array uses the same order;
	// they are written and read by index, so the two must not drift.
	struct PYROS3D_API SphericalHarmonicsL2
	{
		static const uint32 kCoefficientCount = 9;
		Vec3 coefficients[kCoefficientCount];

		SphericalHarmonicsL2() { Clear(); }
		void Clear();

		// True when every coefficient is zero - i.e. nothing has been
		// projected into this yet. A caller uses it to decide whether SH
		// ambient is worth enabling at all.
		bool IsZero() const;

		// Radiance arriving from `direction`, reconstructed from the
		// coefficients. Mostly useful for testing the projection: the
		// thing rendering actually wants is Irradiance() below.
		Vec3 EvaluateRadiance(const Vec3 &direction) const;

		// Cosine-convolved irradiance E(n) for a surface whose normal is
		// `normal`, by Ramamoorthi & Hanrahan's closed form - the
		// per-band cosine-lobe factors are folded into the constants
		// rather than convolved at projection time, so the coefficients
		// stored here stay plain radiance projections and can also be
		// used for anything else.
		//
		// This is irradiance, so it carries the PI a Lambertian BRDF
		// divides back out. See AmbientIrradiance() for the form this
		// engine's shaders actually want.
		Vec3 Irradiance(const Vec3 &normal) const;

		// Irradiance / PI: what a white Lambertian surface reflects.
		//
		// That division is not cosmetic here. This engine defines a light
		// colour as "the radiance a white Lambertian surface reflects at
		// N.L == 1" - irradiance/PI - and the ambient term is multiplied
		// straight into albedo by both the forward and the deferred path.
		// Feeding raw irradiance into that slot makes an environment of
		// radiance 1 come back as PI, and every ambient-lit surface blows
		// out by 3.14x. Which is the same factor, in the same direction,
		// as the PBR light-colour bug already recorded in
		// CalculatePBRLighting's comment.
		Vec3 AmbientIrradiance(const Vec3 &normal) const;

		// Scales every coefficient - SH is linear, so this scales the
		// reconstructed irradiance by the same factor. Lets a caller
		// expose an "environment intensity" without re-projecting.
		void Scale(const f32 factor);
	};

	// One face of a cubemap, as linear RGB floats, row-major from the
	// face's top-left in the usual cubemap face orientation. `size` is the
	// edge length; `pixels` holds size*size*3 floats.
	struct PYROS3D_API CubemapFacePixels
	{
		const f32 *pixels;
		uint32 size;
		CubemapFacePixels() : pixels(NULL), size(0) {}
		CubemapFacePixels(const f32 *p, const uint32 s) : pixels(p), size(s) {}
	};

	// Projects an environment cubemap into SH.
	//
	// `faces` must be exactly 6, in TextureType's cubemap face order
	// (+X, -X, +Y, -Y, +Z, -Z) - the same order Texture uses, so a caller
	// reading faces off a Texture does not have to reorder them.
	//
	// Every texel is weighted by its true solid angle, not uniformly.
	// A cubemap's texels are wildly unequal in solid angle - a corner
	// texel subtends roughly a fifth of what a face-centre one does - and
	// ignoring that is the classic way to get a projection that looks
	// plausible, is smooth, and is quietly wrong: the error shows up as
	// the corners of the cube leaking into the result, which reads as a
	// faint directional tint nobody can source.
	//
	// Returns false (leaving `outSH` cleared) if `faces` is not 6 faces of
	// equal, non-zero size with non-null pixels.
	PYROS3D_API bool ProjectCubemapToSH(const std::vector<CubemapFacePixels> &faces, SphericalHarmonicsL2 &outSH);

	// The 9 basis function values for a direction, in the same index
	// order as SphericalHarmonicsL2::coefficients. Exposed because a test
	// wants to check the basis independently of the projection that uses
	// it. `direction` must be normalised.
	PYROS3D_API void EvaluateSHBasis(const Vec3 &direction, f32 outBasis[9]);

};

#endif /* SPHERICALHARMONICS_H */
