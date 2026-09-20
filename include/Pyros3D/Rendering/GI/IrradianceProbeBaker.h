//============================================================================
// Name        : IrradianceProbeBaker.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Fills an IrradianceProbeGrid by rendering the scene from
//               each probe - once, or continuously.
//============================================================================

#ifndef IRRADIANCEPROBEBAKER_H
#define IRRADIANCEPROBEBAKER_H

#include <Pyros3D/Rendering/GI/IrradianceProbeGrid.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	class SceneGraph;
	class GameObject;
	class CubemapRenderer;

	// Captures probes by rasterizing the scene into a small cubemap from
	// each probe's position and projecting that into SH.
	//
	// "Bake" is a cadence, not an architecture. Update() refreshes a
	// bounded number of probes per call and remembers where it stopped,
	// so the same code is a one-shot bake (budget 0, run once) or
	// progressive relighting that follows a moving sun (a few probes per
	// frame, forever). Nothing downstream can tell the difference: both
	// just write coefficients into the grid.
	//
	// The cost is real and worth stating plainly, because it is what
	// decides the budget: one probe is SIX scene renders. There is no ray
	// tracing here to avoid that. At 16x16 faces the passes are tiny and
	// mostly draw-call bound, so the budget should be chosen by how many
	// extra draw calls per frame the scene can afford, not by pixels.
	//
	// What a probe sees is the scene lit by DIRECT light, so this is one
	// bounce. Running it again with ambient mode 2 already enabled feeds
	// the previous result back in and gives a second - the loop converges
	// and needs no new code, just another pass.
	class PYROS3D_API IrradianceProbeBaker
	{
	public:

		// faceSize is the cubemap edge in texels. 16 is ample: the result
		// is convolved down to nine coefficients, so detail here buys
		// nothing and costs six render passes at that resolution.
		IrradianceProbeBaker(const uint32 faceSize = 16);
		~IrradianceProbeBaker();

		// True when a cubemap renderer could be created at all. False on
		// a build that cannot read a texture back (GLES3/WebGL2), where
		// probes have to arrive pre-baked in the scene file.
		bool IsSupported() const;

		// One probe. `outSH` is untouched on failure.
		bool CaptureProbe(SceneGraph *scene, const Vec3 &position, SphericalHarmonicsL2 &outSH);

		// Refreshes up to `budget` probes, resuming where the last call
		// stopped; 0 means every probe in one go. Returns how many were
		// actually captured.
		//
		// Round-robin rather than "nearest to the camera first" on
		// purpose: a fixed cycle gives every probe a bounded staleness,
		// where a priority scheme can starve the ones behind you
		// indefinitely and then pop when you turn around.
		uint32 Update(SceneGraph *scene, IrradianceProbeGrid &grid, const uint32 budget);

		// How far through the grid Update() currently is - 0 right after
		// a full sweep completes. Lets a caller show progress, or wait
		// for convergence before saving.
		uint32 GetCursor() const { return cursor; }
		void ResetCursor() { cursor = 0; }

		void SetNearFar(const f32 nearPlane, const f32 farPlane) { probeNear = nearPlane; probeFar = farPlane; }

	private:

		CubemapRenderer *renderer;
		// The camera the capture renders from. Owned here and moved to
		// each probe in turn rather than created per probe - a GameObject
		// per capture would churn the scene graph for nothing.
		GameObject *eye;
		uint32 faceSize;
		uint32 cursor;
		f32 probeNear, probeFar;

		// Reused across probes so a full sweep does not allocate six
		// buffers per probe.
		std::vector<f32> faceScratch[6];
	};

};

#endif /* IRRADIANCEPROBEBAKER_H */
