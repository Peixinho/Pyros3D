//============================================================================
// Name        : BRDFLut.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : The environment half of the split-sum approximation.
//============================================================================

#ifndef BRDFLUT_H
#define BRDFLUT_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	// Karis' split-sum: the specular integral is factored into
	// "prefiltered radiance" (which varies per probe and lives in the
	// radiance atlas) times "environment BRDF" (which does not vary at
	// all). This is the second factor.
	//
	// It depends on nothing but N.V and roughness - not the scene, not
	// the material, not the lighting - so it is a constant table. Two
	// channels: a scale and a bias applied to the surface's F0, which
	// is what lets one table serve every material.
	//
	// Generated rather than shipped as an asset because it is a few
	// milliseconds of arithmetic and a binary blob nobody can review.
	class PYROS3D_API BRDFLut
	{
	public:

		BRDFLut() : size(0) {}

		// `samples` is the Monte Carlo count per texel. 256 is visually
		// indistinguishable from 1024 here because the integrand is
		// smooth; the cost is linear and this runs once.
		bool Generate(const uint32 resolution = 64, const uint32 samples = 256);

		uint32 GetSize() const { return size; }
		// RG, row-major, size*size texels.
		const std::vector<f32> &GetData() const { return data; }

		// Bilinear lookup, for the CPU side and for tests. nDotV and
		// roughness are both clamped to [0,1].
		void Sample(const f32 nDotV, const f32 roughness, f32 &outScale, f32 &outBias) const;

	private:
		uint32 size;
		std::vector<f32> data;
	};

};

#endif /* BRDFLUT_H */
