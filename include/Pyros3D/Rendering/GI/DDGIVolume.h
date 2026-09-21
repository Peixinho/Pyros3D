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
		// `radianceLevels` roughness slices of prefiltered radiance for
		// specular. Zero disables specular entirely and costs nothing -
		// the atlas is not allocated and SampleRadiance returns black.
		bool Allocate(const Vec3 &origin, const Vec3 &spacing,
			const uint32 nx, const uint32 ny, const uint32 nz,
			const uint32 irradianceRes = 8, const uint32 visibilityRes = 16,
			const uint32 radianceRes = 16, const uint32 radianceLevels = 4);

		// The roughness a given prefilter level was gathered at.
		//
		// Level 0 is NOT roughness zero. A probe traces on the order of
		// a hundred rays; a mirror lobe is narrower than the angle
		// between neighbouring rays, so prefiltering at roughness 0
		// gathers one ray and returns noise that looks like a mirror
		// only by accident. The floor is where the lobe is wide enough
		// that several rays land in it. Below that the shader has
		// nothing better to offer than this level - which is the honest
		// limit of probe-based specular, and why screen-space
		// reflections exist.
		static f32 LevelRoughness(const uint32 level, const uint32 levels);
		static f32 MinRoughness();

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
		// `probeBudget` caps how many probes this call re-traces,
		// resuming where the last call stopped; 0 means all of them.
		//
		// That budget is what makes this a live algorithm rather than a
		// bake. Tracing every probe every frame is not affordable on the
		// CPU, but tracing a slice of them is - each probe still gets
		// refreshed on a fixed cycle, so a light that moves is followed
		// with a lag of (probes / budget) frames instead of never.
		// Round-robin rather than nearest-first so staleness is bounded
		// rather than letting the probes behind the camera starve.
		void Update(const RayScene &scene, const std::vector<RayLight> &lights,
			const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis,
			const uint32 probeBudget = 0);

		// Irradiance arriving at `worldPosition` on a surface facing
		// `normal`, from the eight surrounding probes, weighted by
		// Chebyshev visibility.
		Vec3 SampleIrradiance(const Vec3 &worldPosition, const Vec3 &normal) const;

		// Prefiltered radiance along `reflection` for a surface of the
		// given roughness - the first factor of the split sum. Multiply
		// by BRDFLut's scale/bias to get the specular term.
		//
		// `normal` is still needed: it selects which probes may light
		// this point at all, and that rejection is the same one the
		// diffuse path does. Using the reflection vector for probe
		// selection instead would pick probes on the far side of the
		// surface whenever the view is grazing.
		Vec3 SampleRadiance(const Vec3 &worldPosition, const Vec3 &normal,
			const Vec3 &reflection, const f32 roughness) const;

		const ProbeAtlas &GetRadianceAtlas() const { return radiance; }
		ProbeAtlas &GetRadianceAtlasMutable() { return radiance; }
		uint32 GetRadianceLevels() const { return radianceLevels; }
		// Tile index of one probe at one prefilter level. Levels are
		// stacked: all probes at level 0, then all at level 1. The
		// shader has to reproduce this, so it is a named function and
		// not an expression repeated in three places.
		uint32 RadianceTile(const uint32 probe, const uint32 level) const
		{
			return level * ProbeCount() + probe;
		}

		const ProbeAtlas &GetIrradianceAtlas() const { return irradiance; }
		const ProbeAtlas &GetVisibilityAtlas() const { return visibility; }
		// Mutable access, for a GPU backend writing results back in.
		// Deliberately separate from the const accessors above so a
		// reader has to ask for write access explicitly.
		ProbeAtlas &GetIrradianceAtlasMutable() { return irradiance; }
		ProbeAtlas &GetVisibilityAtlasMutable() { return visibility; }
		void FillAtlasBorders() { irradiance.FillBorders(); visibility.FillBorders(); radiance.FillBorders(); }
		f32 GetMaxRayDistance() const { return maxRayDistance; }
		const Vec3 &GetSkyColor() const { return skyColor; }

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
		ProbeAtlas radiance;    // RGB, probeCount tiles per roughness level
		uint32 radianceLevels;
		Vec3 skyColor;
		f32 maxRayDistance;
		// Where the next budgeted Update() resumes.
		uint32 updateCursor;

		// Evenly distributed directions, rotated per frame - see Update.
		static Vec3 SphericalFibonacci(const uint32 index, const uint32 count, const f32 rotation);
		// The eight surrounding probes and their trilinear * backface *
		// Chebyshev weights. Shared by both samplers so specular and
		// diffuse cannot disagree about which probes can see a point -
		// if they did, a wall would leak in the reflection but not in
		// the diffuse, which is far harder to recognise than both
		// leaking.
		uint32 GatherProbes(const Vec3 &worldPosition, const Vec3 &normal,
			uint32 *outProbes, f32 *outWeights) const;

		Vec3 ShadeHit(const RayScene &scene, const RayHit &hit,
			const Vec3 &rayOrigin, const Vec3 &rayDir,
			const std::vector<RayLight> &lights) const;
	};

};

#endif /* DDGIVOLUME_H */
