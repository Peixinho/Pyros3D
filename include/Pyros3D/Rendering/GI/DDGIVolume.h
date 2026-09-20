//============================================================================
// Name        : DDGIVolume.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Dynamic diffuse global illumination - a probe grid that
//               knows what it can see.
//============================================================================

#ifndef DDGIVOLUME_H
#define DDGIVOLUME_H

#include <Pyros3D/Rendering/GI/Octahedral.h>
#include <Pyros3D/Rendering/GI/RayScene.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	// One light, as a ray tracer needs it: no shadow maps, no matrices,
	// just what is required to evaluate direct light at a hit point and
	// trace a shadow ray toward it.
	struct PYROS3D_API RayLight
	{
		// Directional when w == 0 (direction points FROM the light),
		// point when w == 1 and positionOrDirection is a position.
		Vec3 positionOrDirection;
		f32 isPoint;
		Vec3 color;
		f32 range;
		RayLight() : positionOrDirection(0.f, -1.f, 0.f), isPoint(0.f), color(1.f, 1.f, 1.f), range(0.f) {}
	};

	// A DDGI probe volume.
	//
	// The difference from IrradianceProbeGrid is not resolution, it is
	// that each probe also stores how far away the geometry around it
	// is. Sampling then runs a Chebyshev test - given the mean and mean
	// square of distance in a direction, how likely is it that a point
	// that far away is visible? - and weights each probe's contribution
	// by the answer. A probe inside a wall reports "nothing is visible
	// past ~0" and contributes nothing to the room on the other side.
	//
	// That single test is what makes a probe grid usable rather than a
	// thing you fight, and it is the defect IrradianceProbeGrid has by
	// construction.
	//
	// This class is the CPU reference. It is not the shipping path - the
	// point of it is to be correct, and to be the thing the compute
	// implementation is verified against, exactly as RayScene::Intersect
	// is for the traversal kernel.
	class PYROS3D_API DDGIVolume
	{
	public:

		DDGIVolume();

		// Irradiance tiles are small because irradiance is smooth;
		// visibility needs more, because resolving which side of a wall
		// edge a direction falls on is the whole job.
		bool Allocate(const Vec3 &origin, const Vec3 &spacing,
			const uint32 nx, const uint32 ny, const uint32 nz,
			const uint32 irradianceRes = 8, const uint32 visibilityRes = 16);

		bool IsValid() const;
		uint32 ProbeCount() const { return counts[0] * counts[1] * counts[2]; }
		uint32 Index(const uint32 x, const uint32 y, const uint32 z) const
		{
			return (z * counts[1] + y) * counts[0] + x;
		}
		Vec3 ProbePosition(const uint32 x, const uint32 y, const uint32 z) const;

		// Traces `raysPerProbe` rays from each probe, shades what they
		// hit, and blends the result into both atlases.
		//
		// `frame` rotates the ray set so successive frames sample
		// different directions - without it the same fixed directions
		// are resampled forever and the estimate never improves past
		// whatever those directions happened to see. Blending is
		// exponential with `hysteresis`: 0 replaces outright (a one-shot
		// bake), 0.97 converges over a few dozen frames while tolerating
		// a moving light.
		void Update(const RayScene &scene, const std::vector<RayLight> &lights,
			const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis);

		// Irradiance arriving at `worldPosition` on a surface facing
		// `normal`, from the eight surrounding probes, weighted by
		// Chebyshev visibility.
		Vec3 SampleIrradiance(const Vec3 &worldPosition, const Vec3 &normal) const;

		const ProbeAtlas &GetIrradianceAtlas() const { return irradiance; }
		const ProbeAtlas &GetVisibilityAtlas() const { return visibility; }

		// Sky colour for rays that hit nothing. Without one an enclosed
		// scene is correct and an open one is black.
		void SetSkyColor(const Vec3 &c) { skyColor = c; }

		// Rays that hit nothing travel this far before being treated as
		// sky, which is also what gets written into the visibility
		// moments. Defaults to the grid diagonal at Allocate() time.
		void SetMaxRayDistance(const f32 d) { maxRayDistance = d; }

		Vec3 origin, spacing;
		uint32 counts[3];

	private:

		ProbeAtlas irradiance;  // RGB
		ProbeAtlas visibility;  // R = mean distance, G = mean squared
		Vec3 skyColor;
		f32 maxRayDistance;

		// Evenly distributed directions, rotated per frame - see Update.
		static Vec3 SphericalFibonacci(const uint32 index, const uint32 count, const f32 rotation);
		Vec3 ShadeHit(const RayScene &scene, const RayHit &hit,
			const Vec3 &rayOrigin, const Vec3 &rayDir,
			const std::vector<RayLight> &lights) const;
	};

};

#endif /* DDGIVOLUME_H */
