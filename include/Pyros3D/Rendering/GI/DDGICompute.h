//============================================================================
// Name        : DDGICompute.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : DDGI probe tracing on the GPU - the reason the BVH and
//               the compute layer exist.
//============================================================================

#ifndef DDGICOMPUTE_H
#define DDGICOMPUTE_H

#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>

namespace p3d {

	// Traces, shades and blends a DDGIVolume's probes in compute.
	//
	// Same algorithm as DDGIVolume's CPU path, step for step, because
	// that class is the reference this is checked against
	// (tools/tests/ddgi_gpu.cpp). Where the two differ it is a bug in
	// one of them, and keeping them structurally identical is what makes
	// that statement useful.
	//
	// Why this matters: the CPU path costs tens of milliseconds for a
	// few hundred probes, which is why it has to be rationed to a
	// handful of probes per frame. On the GPU the whole volume fits in a
	// frame, which is the difference between indirect light that follows
	// a moving light with a visible lag and one that just follows it.
	//
	// Geometry is uploaded once and reused. Lights are re-uploaded every
	// update, because those are what actually change.
	class PYROS3D_API DDGICompute
	{
	public:

		DDGICompute();
		~DDGICompute();

		bool IsSupported() const;

		// Compiles the kernels and uploads the scene. `shaderRoot` is
		// the directory holding gi/*.glsl - the kernels are assembled
		// from those files at runtime rather than embedded, so the GPU
		// and CPU traversal cannot drift apart silently.
		bool Initialize(const RayScene &scene, const DDGIVolume &volume,
			const std::string &shaderRoot);

		// One update. Traces `probeBudget` probes (0 = all), blends into
		// the atlases, and reads them back into `volume` so the existing
		// texture upload path works unchanged.
		//
		// The readback is the honest weak point: about a megabyte per
		// frame, which is far cheaper than CPU tracing but is still a
		// round trip the GPU should not need. Writing the atlases
		// straight into the sampled textures needs image-store support
		// in IRenderDevice, which does not exist yet.
		bool Update(DDGIVolume &volume, const std::vector<RayLight> &lights,
			const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis,
			const uint32 probeBudget);

		void Shutdown();

		uint32 GetProbeCursor() const { return cursor; }

	private:

		bool CompileStage(const std::string &source, const std::string &stageDefine,
			DeviceHandle &outStage, DeviceHandle &outProgram, DeviceHandle &outPipeline);

		bool initialized;
		DeviceHandle traceStage, traceProgram, tracePipeline;
		DeviceHandle irrStage, irrProgram, irrPipeline;
		DeviceHandle visStage, visProgram, visPipeline;

		DeviceHandle bTris, bNodes, bIdx, bMats, bLights, bRays, bIrr, bVis, bParams;
		uint32 maxRaysPerProbe, maxBatch;
		uint32 cursor;
	};

};

#endif /* DDGICOMPUTE_H */
