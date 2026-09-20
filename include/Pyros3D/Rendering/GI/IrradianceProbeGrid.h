//============================================================================
// Name        : IrradianceProbeGrid.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A regular grid of SH irradiance probes - indirect light
//               that varies with position.
//============================================================================

#ifndef IRRADIANCEPROBEGRID_H
#define IRRADIANCEPROBEGRID_H

#include <Pyros3D/Rendering/GI/SphericalHarmonics.h>
#include <Pyros3D/Core/Math/Vec3.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	// An axis-aligned lattice of SphericalHarmonicsL2 probes.
	//
	// What this adds over the single environment projection in
	// SphericalHarmonics.h is the two things that one cannot do, and they
	// are the difference between environment lighting and indirect light:
	//
	//   - It varies with position. A character under an overhang and one
	//     in the open no longer receive identical ambient.
	//   - Each probe is captured by rendering the actual SCENE from its
	//     position, not by projecting the sky. So a probe next to a red
	//     wall carries red, and that red lands on whatever stands beside
	//     it. That is a real bounce, once.
	//
	// It is still one bounce, still static, and still has no visibility
	// term: a probe on the far side of a wall is happily interpolated
	// into a point on this side (classic light leaking). Both are worth
	// fixing and neither is fixed here.
	//
	// Storage is one SphericalHarmonicsL2 per probe - 36 floats - so a
	// 16x8x16 grid is 2048 probes and about 288KB. Coarse grids are the
	// norm: irradiance is smooth, which is the same property that makes
	// nine coefficients enough per probe.
	struct PYROS3D_API IrradianceProbeGrid
	{
		// World position of probe (0,0,0). Probe (i,j,k) sits at
		// origin + spacing * (i,j,k), so the volume covered is
		// origin .. origin + spacing * (counts - 1).
		Vec3 origin;
		Vec3 spacing;
		uint32 counts[3];
		std::vector<SphericalHarmonicsL2> probes;

		IrradianceProbeGrid() : origin(0.f, 0.f, 0.f), spacing(1.f, 1.f, 1.f)
		{
			counts[0] = counts[1] = counts[2] = 0;
		}

		uint32 ProbeCount() const { return counts[0] * counts[1] * counts[2]; }

		// A grid is usable when it has at least 2 probes on every axis
		// (one probe cannot interpolate), positive spacing, and exactly
		// as many probes as its counts claim. Anything else is either
		// un-baked or corrupt, and Sample() would read out of bounds.
		bool IsValid() const;

		// Row-major with X fastest - the order Bake() fills and the
		// serializer writes. They are only consistent because they all
		// call this.
		uint32 Index(const uint32 x, const uint32 y, const uint32 z) const
		{
			return (z * counts[1] + y) * counts[0] + x;
		}

		Vec3 ProbePosition(const uint32 x, const uint32 y, const uint32 z) const;

		// Resizes to counts and clears every probe. Returns false for a
		// degenerate request rather than allocating something unusable.
		bool Allocate(const Vec3 &gridOrigin, const Vec3 &gridSpacing,
			const uint32 nx, const uint32 ny, const uint32 nz);

		// Trilinearly interpolated probe at a world position, clamped to
		// the volume - a point outside the grid gets the nearest face
		// rather than nothing, because an object that strays past the
		// baked volume should dim gracefully, not go black.
		//
		// Interpolating the COEFFICIENTS rather than the evaluated
		// irradiance, which is the same thing: SH evaluation is linear in
		// its coefficients, so lerping before or after is identical. This
		// order costs one evaluation instead of eight.
		SphericalHarmonicsL2 Sample(const Vec3 &worldPosition) const;

		// Sample() followed by AmbientIrradiance() - the value a shader's
		// ambient term wants, already divided by PI.
		Vec3 AmbientIrradianceAt(const Vec3 &worldPosition, const Vec3 &normal) const;

		void Clear();
	};

};

#endif /* IRRADIANCEPROBEGRID_H */
