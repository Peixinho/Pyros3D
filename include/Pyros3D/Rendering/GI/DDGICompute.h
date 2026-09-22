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
		// There is one readback per update and it is not the expense it
		// looks like. Measured on the Cornell demo (294 probes x 128
		// rays, every probe every frame): about a megabyte, 1.3 ms, ~10%
		// of a ~12 ms update. The rest is real GPU tracing.
		//
		// What DID cost 6-15 ms was reading the relocation statistics
		// after every dispatch batch - three full round trips per
		// update, each waiting on work that had only just been
		// submitted. The stall, not the bytes. Both readbacks now
		// happen once, at the end, sharing a single sync.
		//
		// So writing the atlases straight into the sampled textures
		// (imageStore, which IRenderDevice does not have) would buy
		// back that 1.3 ms at the price of a second sampling path -
		// WebGL2 has no compute and would still need this one. Worth
		// doing eventually; not worth doing first.
		//
		// `PYROS_GI_TIMING=1` prints the breakdown, which is how those
		// numbers were arrived at rather than guessed.
		bool Update(DDGIVolume &volume, const std::vector<RayLight> &lights,
			const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis,
			const uint32 probeBudget);

		// Re-uploads the triangles and the BVH after the scene has
		// moved. Indices are untouched - a refit changes bounds and
		// vertex positions, never which triangle sits in which leaf -
		// so only two of the three geometry buffers are rewritten.
		//
		// Returns false if the geometry no longer fits what was
		// allocated at Initialize time, which means the scene gained or
		// lost triangles and wants a full rebuild rather than a
		// refresh.
		bool UpdateGeometry(const RayScene &scene);

		void Shutdown();

		uint32 GetProbeCursor() const { return cursor; }

	private:

		bool CompileStage(const std::string &source, const std::string &stageDefine,
			DeviceHandle &outStage, DeviceHandle &outProgram, DeviceHandle &outPipeline);

		bool initialized;
		DeviceHandle traceStage, traceProgram, tracePipeline;
		DeviceHandle irrStage, irrProgram, irrPipeline;
		DeviceHandle visStage, visProgram, visPipeline;
		// Only compiled when the volume asked for specular. A volume
		// with no radiance levels must not pay for a fourth kernel.
		DeviceHandle radStage, radProgram, radPipeline;
		// Relocation statistics. Only compiled when the volume wants
		// relocation; the decision itself is made on the CPU.
		DeviceHandle statStage, statProgram, statPipeline;

		DeviceHandle bTris, bNodes, bIdx, bMats, bLights, bRays, bIrr, bVis, bParams, bRad, bStats;
		uint32 maxRaysPerProbe, maxBatch, radianceLevels;
		// Texels in one irradiance atlas - bIrr holds two of them.
		uint32 irrTexels;
		// Sizes the geometry buffers were created with, so a refresh
		// can tell "the scene moved" from "the scene changed".
		uint32 triBytes, nodeBytes;
		// Probes the params buffer has room for, past its eight fixed
		// vec4s: the per-probe relocation offsets ride there.
		uint32 paramProbes;
		bool relocation;
		uint32 cursor;
	};

};

#endif /* DDGICOMPUTE_H */
