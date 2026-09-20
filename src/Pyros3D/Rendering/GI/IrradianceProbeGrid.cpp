//============================================================================
// Name        : IrradianceProbeGrid.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See IrradianceProbeGrid.h.
//============================================================================

#include <Pyros3D/Rendering/GI/IrradianceProbeGrid.h>
#include <cmath>

namespace p3d {

	bool IrradianceProbeGrid::IsValid() const
	{
		if (counts[0] < 2 || counts[1] < 2 || counts[2] < 2)
			return false;
		if (spacing.x <= 0.f || spacing.y <= 0.f || spacing.z <= 0.f)
			return false;
		return probes.size() == (size_t)ProbeCount();
	}

	Vec3 IrradianceProbeGrid::ProbePosition(const uint32 x, const uint32 y, const uint32 z) const
	{
		return Vec3(origin.x + spacing.x * (f32)x,
					origin.y + spacing.y * (f32)y,
					origin.z + spacing.z * (f32)z);
	}

	bool IrradianceProbeGrid::Allocate(const Vec3 &gridOrigin, const Vec3 &gridSpacing,
		const uint32 nx, const uint32 ny, const uint32 nz)
	{
		if (nx < 2 || ny < 2 || nz < 2)
			return false;
		if (gridSpacing.x <= 0.f || gridSpacing.y <= 0.f || gridSpacing.z <= 0.f)
			return false;
		// Guards the multiplication below as much as the memory: a grid
		// this size is a mistake, and nx*ny*nz can wrap uint32 long before
		// the allocation would fail on its own.
		const uint64 total = (uint64)nx * (uint64)ny * (uint64)nz;
		if (total > 1000000ull)
			return false;

		origin = gridOrigin;
		spacing = gridSpacing;
		counts[0] = nx; counts[1] = ny; counts[2] = nz;
		probes.assign((size_t)total, SphericalHarmonicsL2());
		return true;
	}

	void IrradianceProbeGrid::Clear()
	{
		origin = Vec3(0.f, 0.f, 0.f);
		spacing = Vec3(1.f, 1.f, 1.f);
		counts[0] = counts[1] = counts[2] = 0;
		probes.clear();
	}

	SphericalHarmonicsL2 IrradianceProbeGrid::Sample(const Vec3 &worldPosition) const
	{
		SphericalHarmonicsL2 result;
		if (!IsValid())
			return result;

		// Continuous grid coordinates, clamped so a point outside the
		// volume lands on its boundary rather than extrapolating - an
		// extrapolated SH can go negative and paint black blotches.
		f32 g[3] = {
			(worldPosition.x - origin.x) / spacing.x,
			(worldPosition.y - origin.y) / spacing.y,
			(worldPosition.z - origin.z) / spacing.z
		};
		uint32 i0[3], i1[3];
		f32 t[3];
		for (uint32 a = 0; a < 3; a++)
		{
			const f32 maxCoord = (f32)(counts[a] - 1);
			if (g[a] < 0.f) g[a] = 0.f;
			if (g[a] > maxCoord) g[a] = maxCoord;
			const f32 base = floorf(g[a]);
			i0[a] = (uint32)base;
			// The upper corner of the last cell is the last probe, not one
			// past it: at exactly maxCoord, base == maxCoord and i0+1
			// would index out of the grid.
			i1[a] = (i0[a] + 1 < counts[a]) ? i0[a] + 1 : i0[a];
			t[a] = g[a] - base;
		}

		// Eight corners, weighted by the usual trilinear products. The
		// coefficients are summed, not the reconstructed irradiance - see
		// the comment on Sample() for why those are the same thing.
		for (uint32 corner = 0; corner < 8; corner++)
		{
			const uint32 cx = (corner & 1) ? i1[0] : i0[0];
			const uint32 cy = (corner & 2) ? i1[1] : i0[1];
			const uint32 cz = (corner & 4) ? i1[2] : i0[2];
			const f32 wx = (corner & 1) ? t[0] : (1.f - t[0]);
			const f32 wy = (corner & 2) ? t[1] : (1.f - t[1]);
			const f32 wz = (corner & 4) ? t[2] : (1.f - t[2]);
			const f32 w = wx * wy * wz;
			if (w <= 0.f)
				continue;
			const SphericalHarmonicsL2 &p = probes[Index(cx, cy, cz)];
			for (uint32 c = 0; c < SphericalHarmonicsL2::kCoefficientCount; c++)
				result.coefficients[c] += p.coefficients[c] * w;
		}
		return result;
	}

	Vec3 IrradianceProbeGrid::AmbientIrradianceAt(const Vec3 &worldPosition, const Vec3 &normal) const
	{
		return Sample(worldPosition).AmbientIrradiance(normal);
	}

};
