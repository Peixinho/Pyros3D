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

	// What a probe's own rays say about where it is sitting - the whole
	// input to the relocation decision.
	//
	// Separated from the tracing that produces it because the GPU
	// produces exactly these numbers in a compute stage and then hands
	// them back: the decision itself then runs in ONE place for both
	// paths, so they cannot drift. Everything else in this file is
	// duplicated between C++ and GLSL and kept honest by a parity test;
	// this is the one part that did not have to be.
	struct PYROS3D_API ProbeRayStats
	{
		// Fraction of rays that struck a surface from behind, which can
		// only happen if the probe is on the inside of it.
		f32 backfaceRatio;
		// Nearest front-facing hit, and which way it was.
		f32 closestFront;
		Vec3 closestFrontDir;
		// Where the open space is: the non-backface ray directions
		// summed, each weighted by how far it got, then normalised.
		//
		// NOT the single farthest ray, which is what this was first.
		// That is a discontinuous function of 128 nearly equal
		// distances, and the CPU and GPU intersectors differ in the
		// last bits - so the two picked different rays, the probe
		// walked off in different directions, and the difference
		// compounded over updates. Twelve of 125 probes had diverged
		// after eight. A weighted average is continuous in the
		// distances: last-bit noise moves it by last-bit amounts.
		f32 openLength;
		Vec3 openDir;
		ProbeRayStats() : backfaceRatio(0.f), closestFront(1e30f),
			closestFrontDir(0.f,0.f,0.f), openLength(0.f), openDir(0.f,0.f,0.f) {}
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
		// Where the probe actually is: its grid cell plus whatever
		// offset relocation has given it. This is the position rays
		// are traced from and the position a gather measures distance
		// to, so everything that cares about "where is probe N" must
		// use it rather than recomputing the grid position.
		Vec3 ProbePosition(const uint32 x, const uint32 y, const uint32 z) const;
		// The unrelocated cell position. Only relocation itself and the
		// clamp on its own offset need this.
		Vec3 ProbeGridPosition(const uint32 x, const uint32 y, const uint32 z) const;

		// Probes move, and probes switch off.
		//
		// A probe that lands inside a wall or a pillar is the last
		// structural weakness of a fixed grid, and it is not a corner
		// case: at any spacing, some probes land in geometry. Such a
		// probe sees the inside of the surface in every direction, so
		// it reports near-zero distances - and its Chebyshev test then
		// rejects it for every point it should have lit, while the
		// backface weight's 0.2 floor still leaks a fifth of it into
		// points on the far side. Measured earlier in this volume's
		// life: a sealed box with a probe exactly in the floor plane
		// leaked 4.99 against 5.15 inside.
		//
		// Two fixes, both driven by statistics the trace already has:
		//
		//   RELOCATION nudges a probe out of geometry, toward the open
		//   space its own rays found. Bounded to a fraction of the cell
		//   so the trilinear weights - which are computed from the grid,
		//   not from where the probe ended up - stay meaningful.
		//
		//   CLASSIFICATION switches off a probe that is still enclosed
		//   after relocation. Inside a solid pillar there is nowhere to
		//   move to, and the honest answer is that the probe has no
		//   lighting to contribute.
		void SetProbeRelocation(const bool on) { relocationEnabled = on; }
		// One probe's relocation decision. Public because the compute
		// backend calls it with statistics its own kernel produced -
		// see ProbeRayStats.
		// Writes into the PENDING offsets, not the live ones. An
		// update must trace and gather against the positions the
		// probes had when it started: the multi-bounce feedback
		// gather reads its neighbours' positions, so relocating probe
		// 0 mid-update would move the volume under probe 1. The CPU
		// did exactly that and the GPU - which uploads offsets once
		// per batch - could not reproduce it; 18 of 125 probes
		// disagreed after a single update.
		void ApplyRelocation(const uint32 probe, const ProbeRayStats &stats);
		// Makes this pass's relocations live. Update() calls it; a GPU
		// backend calls it after its last batch.
		void CommitRelocation();
		// The winding-is-unreliable check, over the whole volume. Run
		// by Update; the compute backend runs it too, after writing
		// back the probe data its own stats produced.
		void ValidateClassification();
		bool GetProbeRelocation() const { return relocationEnabled; }
		// xyz = the probe's offset from its cell, w = 1 active, 0 off.
		// One entry per probe, in Index() order. This is what the
		// sampling shader needs uploaded to agree with the trace.
		const std::vector<Vec4> &GetProbeData() const { return probeData; }
		std::vector<Vec4> &GetProbeDataMutable() { return probeData; }
		bool IsProbeActive(const uint32 probe) const
		{
			return probe >= probeData.size() || probeData[probe].w > 0.5f;
		}
		// Largest offset relocation may apply, per axis: just under half
		// the spacing, so a relocated probe stays inside its own cell
		// and the eight probes a point interpolates between are still
		// the eight around it.
		Vec3 GetMaxProbeOffset() const { return spacing * 0.45f; }

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
		// The irradiance as it stood when the current update began -
		// what multi-bounce feeds back from. See the member.
		const ProbeAtlas &GetFeedbackAtlas() const { return feedback; }
		// Refreshes the snapshot from the live atlas. Update() does
		// this itself; a GPU backend driving the same volume calls it
		// so that both are feeding back from the same thing.
		void SnapshotFeedback() { feedback = irradiance; }
		ProbeAtlas &GetVisibilityAtlasMutable() { return visibility; }
		void FillAtlasBorders() { irradiance.FillBorders(); visibility.FillBorders(); radiance.FillBorders(); }
		f32 GetMaxRayDistance() const { return maxRayDistance; }
		const Vec3 &GetSkyColor() const { return skyColor; }

		// How much of the light already in the volume a probe ray adds
		// back when it hits a surface. 0 is one bounce; 1 is the real
		// answer.
		//
		// This is the whole of multi-bounce, and it costs no rays. A
		// ray that hits a wall currently reports only the DIRECT light
		// on that wall, so a room lit by a lamp pointed at the ceiling
		// comes out almost black - the first bounce is the ceiling and
		// nothing carries it to the floor. Adding the irradiance the
		// volume already holds at the hit point makes each update fold
		// in one more bounce, and the series converges geometrically
		// because albedo < 1: with albedo 0.75 the fixed point is 4x
		// the single-bounce answer, which is what a real room does.
		//
		// The feedback reads the PREVIOUS update's atlas, so it lags by
		// one update per bounce. That is the standard DDGI arrangement
		// and the reason it is stable: a probe never reads a value it
		// is in the middle of writing.
		//
		// Clamped to [0,1]. Above 1 the series diverges and the scene
		// glows brighter every frame, which looks like a bug in the
		// lighting rather than a number someone chose.
		void SetMultiBounce(const f32 strength);
		f32 GetMultiBounce() const { return multiBounce; }

		// How far along the normal the feedback lookup moves off the
		// surface it just hit. A quarter of the tightest probe spacing.
		//
		// Without it multi-bounce leaks through thin walls, and it is
		// worth being precise about why, because the obvious
		// explanation is wrong. The Chebyshev test compares the
		// distance to a probe against the mean distance that probe
		// sees in that direction. For a point ON a wall and a probe one
		// spacing away on the OTHER side, those two are the same
		// number - the probe's nearest surface in that direction IS
		// this wall - so the test does not fire. The backface term is
		// what should reject it, and it cannot: it bottoms out at 0.2
		// rather than 0, deliberately, so that a surface rotating past
		// the threshold does not pop.
		//
		// So 20% of a fully lit probe crosses the wall, and with
		// feedback that 20% is re-injected every update and compounds.
		// Measured in the sealed-box test: 0.0% leak on one bounce,
		// 13.9% with multi-bounce and no bias.
		//
		// Moving the lookup off the surface fixes it at the root: the
		// distance to the far-side probe grows past what that probe
		// reports seeing, and Chebyshev fires the way it was meant to.
		f32 GetFeedbackNormalBias() const;

		// A wall-clock ceiling on one Update(), in milliseconds. 0 (the
		// default) means no ceiling and `probeBudget` alone decides.
		//
		// A probe budget is the wrong unit for the CPU path, and the
		// measurement says so: tracing one probe at 128 rays costs
		// about 0.7 ms on a fast desktop, so the budget of 12 that the
		// editor and the player default to is 8.6 ms - over half a
		// 60Hz frame, before anything else in the engine runs. On a
		// phone or a Raspberry Pi, which are exactly the machines with
		// no compute to fall back from, it is the whole frame and
		// more.
		//
		// The number of probes a machine can afford is not knowable in
		// advance; the time it can afford is. With a budget set,
		// Update traces until the budget is spent and stops - one
		// probe minimum, so a slow machine still converges, just
		// slower. The cursor means the rest are simply next in line.
		void SetUpdateTimeBudget(const f32 milliseconds) { updateTimeBudgetMs = milliseconds; }
		f32 GetUpdateTimeBudget() const { return updateTimeBudgetMs; }
		// Probes traced by the last Update(). With a time budget this
		// varies with the machine and with what else it is doing.
		uint32 GetLastUpdatedProbeCount() const { return lastUpdatedProbes; }

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
		// The irradiance atlas as it was when this update started.
		//
		// Multi-bounce feeds back from this rather than from the live
		// atlas so that the answer does not depend on the order probes
		// happen to be visited in: the CPU walks them one at a time, so
		// probe 40 would otherwise see probe 39's brand-new value and
		// probe 41's stale one. Worse, it would make the CPU
		// unmatchable by the GPU, which traces 128 probes at once and
		// cannot reproduce a sequential dependency - and the whole
		// value of this class is being the thing the GPU is checked
		// against.
		// Per-probe offset (xyz) and active flag (w) - see
		// SetProbeRelocation.
		std::vector<Vec4> probeData;
		// Where ApplyRelocation writes until CommitRelocation runs.
		std::vector<Vec4> pendingProbeData;
		bool relocationEnabled;
		f32 updateTimeBudgetMs;
		uint32 lastUpdatedProbes;
		// Classification gives up when a scene's winding says nearly
		// every probe is buried - see the check at the end of Update.
		bool classificationTrusted;
		ProbeAtlas feedback;
		ProbeAtlas radiance;    // RGB, probeCount tiles per roughness level
		uint32 radianceLevels;
		f32 multiBounce;
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

		// SampleIrradiance against a given atlas - the live one for the
		// public sampler, the snapshot for multi-bounce.
		Vec3 SampleIrradianceIn(const ProbeAtlas &atlas, const Vec3 &worldPosition,
			const Vec3 &normal) const;


		Vec3 ShadeHit(const RayScene &scene, const RayHit &hit,
			const Vec3 &rayOrigin, const Vec3 &rayDir,
			const std::vector<RayLight> &lights) const;
	};

};

#endif /* DDGIVOLUME_H */
