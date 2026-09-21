//============================================================================
// Name        : DDGICompute.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See DDGICompute.h.
//============================================================================

#include <Pyros3D/Rendering/GI/DDGICompute.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace p3d {

	namespace {
		std::string ReadFile(const std::string &path)
		{
			std::ifstream f(path.c_str());
			if (!f.good()) return std::string();
			std::stringstream ss; ss << f.rdbuf();
			return ss.str();
		}
		// Probes per dispatch. Bounded so the ray buffer stays a fixed,
		// modest allocation instead of scaling with the whole volume -
		// a 32x32x32 grid at 256 rays would otherwise need a 1GB
		// scratch buffer for data that is consumed immediately.
		const uint32 kMaxBatchProbes = 128;
	}

	DDGICompute::DDGICompute()
		: initialized(false),
		  traceStage(0), traceProgram(0), tracePipeline(0),
		  irrStage(0), irrProgram(0), irrPipeline(0),
		  visStage(0), visProgram(0), visPipeline(0),
		  bTris(0), bNodes(0), bIdx(0), bMats(0), bLights(0),
		  radStage(0), radProgram(0), radPipeline(0),
		  bRays(0), bIrr(0), bVis(0), bParams(0), bRad(0), radianceLevels(0),
		  maxRaysPerProbe(0), maxBatch(0), cursor(0)
	{
	}

	DDGICompute::~DDGICompute() { Shutdown(); }

	bool DDGICompute::IsSupported() const
	{
		return GetActiveRenderDevice().SupportsCompute();
	}

	bool DDGICompute::CompileStage(const std::string &source, const std::string &stageDefine,
		DeviceHandle &outStage, DeviceHandle &outProgram, DeviceHandle &outPipeline)
	{
		IRenderDevice &dev = GetActiveRenderDevice();
		outStage = dev.CreateShaderStage(ShaderType::ComputeShader);
		if (outStage == 0)
			return false;
		// The stage define has to precede the shared code, so it is
		// prepended to the body rather than passed as `definitions` -
		// BuildShaderSource puts definitions before the body anyway, but
		// being explicit here keeps the three variants obviously
		// identical apart from one line.
		const std::string body = "#define " + stageDefine + "\n" + source;
		std::string log;
		if (!dev.CompileShaderStage(outStage, dev.BuildShaderSource(std::string(), body), log))
		{
			echo("DDGICompute: " + stageDefine + " failed to compile: " + log);
			return false;
		}
		outProgram = dev.CreateProgram();
		dev.AttachShaderStage(outProgram, outStage);
		if (!dev.LinkProgram(outProgram, log))
		{
			echo("DDGICompute: " + stageDefine + " failed to link: " + log);
			return false;
		}
		outPipeline = dev.CreateComputePipeline(outProgram);
		return outPipeline != 0;
	}

	bool DDGICompute::Initialize(const RayScene &scene, const DDGIVolume &volume,
		const std::string &shaderRoot)
	{
		Shutdown();
		if (!IsSupported() || !volume.IsValid() || scene.TriangleCount() == 0)
			return false;

		// Assembled from the same files the CPU-verified traversal test
		// reads, in dependency order. Concatenated rather than #included
		// because GLSL has no include and the engine's shader pipeline
		// does not implement one.
		// Searched rather than assumed. Different hosts run from
		// different working directories - the demo launcher from
		// resources/, the editor from its build directory, a game from
		// its own root - and a single hardcoded root silently disables
		// GPU tracing for all but one of them. The failure is a quiet
		// fallback to the CPU path, which is correct and slow, so
		// nobody notices it for a while.
		const char *candidates[] = { "", "resources/", "../resources/", "../../resources/" };
		std::string traversal, octa, update, usedRoot;
		for (uint32 i = 0; i < 4 && traversal.empty(); i++)
		{
			const std::string root = (i == 0) ? shaderRoot : (std::string(candidates[i]) + "shaders");
			const std::string t = ReadFile(root + "/gi/raytrace.glsl");
			if (t.empty()) continue;
			traversal = t;
			octa = ReadFile(root + "/gi/octahedral.glsl");
			update = ReadFile(root + "/gi/ddgi_update.glsl");
			usedRoot = root;
		}
		if (traversal.empty() || octa.empty() || update.empty())
		{
			echo("DDGICompute: could not read gi/*.glsl - tried '" + shaderRoot
				+ "', 'shaders', '../resources/shaders'.");
			return false;
		}
		const std::string source = traversal + "\n" + octa + "\n" + update;

		echo("DDGICompute: kernels from " + usedRoot);
		radianceLevels = volume.GetRadianceLevels();
		if (!CompileStage(source, "DDGI_STAGE_TRACE", traceStage, traceProgram, tracePipeline)
		 || !CompileStage(source, "DDGI_STAGE_IRRADIANCE", irrStage, irrProgram, irrPipeline)
		 || !CompileStage(source, "DDGI_STAGE_VISIBILITY", visStage, visProgram, visPipeline)
		 || (radianceLevels > 0
			&& !CompileStage(source, "DDGI_STAGE_RADIANCE", radStage, radProgram, radPipeline)))
		{
			Shutdown();
			return false;
		}

		IRenderDevice &dev = GetActiveRenderDevice();

		std::vector<f32> triData, nodeData;
		std::vector<uint32> idxData;
		scene.PackForGPU(triData, nodeData, idxData);

		// Materials as 2 vec4 each: albedo, emissive.
		std::vector<f32> matData(std::max<size_t>(1, scene.materials.size()) * 8, 0.f);
		for (size_t i = 0; i < scene.materials.size(); i++)
		{
			matData[i*8+0] = scene.materials[i].albedo.x;
			matData[i*8+1] = scene.materials[i].albedo.y;
			matData[i*8+2] = scene.materials[i].albedo.z;
			matData[i*8+4] = scene.materials[i].emissive.x;
			matData[i*8+5] = scene.materials[i].emissive.y;
			matData[i*8+6] = scene.materials[i].emissive.z;
		}

		bTris  = dev.CreateStorageBuffer((uint32)(triData.size()*sizeof(f32)), 0, triData.data());
		bNodes = dev.CreateStorageBuffer((uint32)(nodeData.size()*sizeof(f32)), 1, nodeData.data());
		bIdx   = dev.CreateStorageBuffer((uint32)(idxData.size()*sizeof(uint32)), 2, idxData.data());
		bMats  = dev.CreateStorageBuffer((uint32)(matData.size()*sizeof(f32)), 3, matData.data());

		// Lights are re-uploaded per update; 64 is a generous ceiling
		// for a ray tracer that shadow-rays each one per hit.
		bLights = dev.CreateStorageBuffer(64 * 2 * 4 * (uint32)sizeof(f32), 4, NULL);

		maxRaysPerProbe = 256;
		maxBatch = std::min(kMaxBatchProbes, volume.ProbeCount());
		bRays = dev.CreateStorageBuffer(maxBatch * maxRaysPerProbe * 2 * 4 * (uint32)sizeof(f32), 5, NULL);

		const ProbeAtlas &irr = volume.GetIrradianceAtlas();
		const ProbeAtlas &vis = volume.GetVisibilityAtlas();
		// One vec4 per atlas texel on the GPU side even though the CPU
		// atlases are 4- and 2-channel: an SSBO indexed as vec4 keeps
		// the addressing arithmetic identical between the two stages,
		// and the conversion happens once on readback.
		bIrr = dev.CreateStorageBuffer(irr.GetWidth()*irr.GetHeight()*4*(uint32)sizeof(f32), 6, NULL);
		bVis = dev.CreateStorageBuffer(vis.GetWidth()*vis.GetHeight()*4*(uint32)sizeof(f32), 7, NULL);
		bParams = dev.CreateStorageBuffer(7 * 4 * (uint32)sizeof(f32), 8, NULL);

		const ProbeAtlas &rad = volume.GetRadianceAtlas();
		if (radianceLevels > 0)
		{
			bRad = dev.CreateStorageBuffer(rad.GetWidth()*rad.GetHeight()*4*(uint32)sizeof(f32), 9, NULL);
			if (!bRad)
			{
				// Binding 9 is past the eight storage blocks GL 4.3
				// guarantees per stage. Every driver this has run on
				// allows far more, but a device that does not should
				// say so here rather than render black reflections.
				echo("DDGICompute: radiance storage buffer (binding 9) failed - specular disabled.");
				radianceLevels = 0;
			}
		}

		if (!bTris || !bNodes || !bIdx || !bMats || !bLights || !bRays || !bIrr || !bVis || !bParams)
		{
			echo("DDGICompute: storage buffer allocation failed.");
			Shutdown();
			return false;
		}

		// Seed the GPU atlases from the volume's own initial state -
		// visibility starts at maxRayDistance, not zero, or every probe
		// fails its own Chebyshev test until it has been traced.
		{
			std::vector<f32> seed(irr.GetWidth()*irr.GetHeight()*4, 0.f);
			dev.UpdateStorageBuffer(bIrr, 0, (uint32)(seed.size()*sizeof(f32)), seed.data());
			std::vector<f32> vseed(vis.GetWidth()*vis.GetHeight()*4, 0.f);
			const std::vector<f32> &src = vis.GetData();
			for (size_t t = 0; t*2+1 < src.size(); t++)
			{
				vseed[t*4+0] = src[t*2+0];
				vseed[t*4+1] = src[t*2+1];
			}
			dev.UpdateStorageBuffer(bVis, 0, (uint32)(vseed.size()*sizeof(f32)), vseed.data());
			if (radianceLevels > 0)
			{
				std::vector<f32> rseed(rad.GetWidth()*rad.GetHeight()*4, 0.f);
				dev.UpdateStorageBuffer(bRad, 0, (uint32)(rseed.size()*sizeof(f32)), rseed.data());
			}
		}

		cursor = 0;
		initialized = true;
		return true;
	}

	bool DDGICompute::Update(DDGIVolume &volume, const std::vector<RayLight> &lights,
		const uint32 raysPerProbe, const uint32 frame, const f32 hysteresis,
		const uint32 probeBudget)
	{
		if (!initialized || !volume.IsValid())
			return false;
		IRenderDevice &dev = GetActiveRenderDevice();

		const uint32 rays = std::min(raysPerProbe, maxRaysPerProbe);
		const uint32 total = volume.ProbeCount();
		const uint32 wanted = (probeBudget == 0 || probeBudget > total) ? total : probeBudget;

		// Lights, 2 vec4 each: (posOrDir, isPoint), (color, range).
		{
			const uint32 count = std::min<uint32>((uint32)lights.size(), 64);
			std::vector<f32> ld(64 * 8, 0.f);
			for (uint32 i = 0; i < count; i++)
			{
				ld[i*8+0]=lights[i].positionOrDirection.x;
				ld[i*8+1]=lights[i].positionOrDirection.y;
				ld[i*8+2]=lights[i].positionOrDirection.z;
				ld[i*8+3]=lights[i].isPoint;
				ld[i*8+4]=lights[i].color.x;
				ld[i*8+5]=lights[i].color.y;
				ld[i*8+6]=lights[i].color.z;
				ld[i*8+7]=lights[i].range;
			}
			dev.UpdateStorageBuffer(bLights, 0, (uint32)(ld.size()*sizeof(f32)), ld.data());
		}

		const ProbeAtlas &irr = volume.GetIrradianceAtlas();
		const ProbeAtlas &vis = volume.GetVisibilityAtlas();
		const ProbeAtlas &rad = volume.GetRadianceAtlas();
		const f32 rotation = (f32)frame * 0.618033988749895f;
		const uint32 lightCount = std::min<uint32>((uint32)lights.size(), 64);

		// Dispatched in batches so the ray scratch buffer stays bounded.
		uint32 done = 0;
		while (done < wanted)
		{
			if (cursor >= total) cursor = 0;
			const uint32 batch = std::min(std::min(maxBatch, wanted - done), total - cursor);

			f32 params[28] = {0};
			params[0]=volume.origin.x; params[1]=volume.origin.y; params[2]=volume.origin.z; params[3]=(f32)total;
			params[4]=volume.spacing.x; params[5]=volume.spacing.y; params[6]=volume.spacing.z; params[7]=(f32)rays;
			params[8]=(f32)volume.counts[0]; params[9]=(f32)volume.counts[1]; params[10]=(f32)volume.counts[2];
			params[11]=volume.GetMaxRayDistance();
			params[12]=volume.GetSkyColor().x; params[13]=volume.GetSkyColor().y; params[14]=volume.GetSkyColor().z;
			params[15]=hysteresis;
			params[16]=(f32)irr.GetResolution(); params[17]=(f32)vis.GetResolution();
			params[18]=(f32)irr.GetProbesPerRow(); params[19]=(f32)vis.GetProbesPerRow();
			params[20]=rotation; params[21]=(f32)lightCount; params[22]=(f32)cursor; params[23]=(f32)batch;
			params[24]=(f32)rad.GetResolution(); params[25]=(f32)rad.GetProbesPerRow();
			params[26]=(f32)radianceLevels; params[27]=DDGIVolume::MinRoughness();
			dev.UpdateStorageBuffer(bParams, 0, sizeof(params), params);

			const uint32 bufCount = radianceLevels > 0 ? 10 : 9;
			const DeviceHandle bufs[10] = { bTris,bNodes,bIdx,bMats,bLights,bRays,bIrr,bVis,bParams,bRad };

			// Trace.
			dev.BindComputePipeline(0, tracePipeline);
			for (uint32 b = 0; b < bufCount; b++) dev.BindStorageBuffer(0, bufs[b], b);
			dev.Dispatch(0, (batch * rays + 63) / 64, 1, 1);
			// The gathers read what this wrote, in a later dispatch -
			// an in-encoder ordering barrier, not a submission.
			dev.ComputeBarrier(0, ComputeBarrierBit::StorageBuffer);

			// Gather irradiance.
			dev.BindComputePipeline(0, irrPipeline);
			for (uint32 b = 0; b < bufCount; b++) dev.BindStorageBuffer(0, bufs[b], b);
			dev.Dispatch(0, (batch * irr.GetResolution() * irr.GetResolution() + 63) / 64, 1, 1);

			// Gather visibility. Reads the same ray buffer, writes a
			// different atlas, so it needs no barrier against the one
			// above - only against the trace, which it already has.
			dev.BindComputePipeline(0, visPipeline);
			for (uint32 b = 0; b < bufCount; b++) dev.BindStorageBuffer(0, bufs[b], b);
			dev.Dispatch(0, (batch * vis.GetResolution() * vis.GetResolution() + 63) / 64, 1, 1);

			// Gather prefiltered radiance, one thread per (probe,
			// level, texel). Also reads only the ray buffer, so it is
			// ordered by the same barrier as the two above.
			if (radianceLevels > 0)
			{
				dev.BindComputePipeline(0, radPipeline);
				for (uint32 b = 0; b < bufCount; b++) dev.BindStorageBuffer(0, bufs[b], b);
				const uint32 threads = batch * rad.GetResolution() * rad.GetResolution() * radianceLevels;
				dev.Dispatch(0, (threads + 63) / 64, 1, 1);
			}
			dev.ComputeBarrier(0, ComputeBarrierBit::StorageBuffer);

			cursor += batch;
			done += batch;
		}

		// Read the atlases back into the volume so the existing texture
		// upload path is unchanged. See the header on why this round
		// trip is the weak point rather than the design.
		dev.ComputeBarrier(0, ComputeBarrierBit::HostRead);
		{
			std::vector<f32> tmp(irr.GetWidth()*irr.GetHeight()*4, 0.f);
			dev.ReadStorageBuffer(bIrr, 0, (uint32)(tmp.size()*sizeof(f32)), tmp.data());
			std::vector<f32> &dst = volume.GetIrradianceAtlasMutable().GetData();
			const uint32 ch = irr.GetChannels();
			for (size_t t = 0; t*ch < dst.size(); t++)
				for (uint32 c = 0; c < ch && c < 4; c++)
					dst[t*ch+c] = tmp[t*4+c];
		}
		{
			std::vector<f32> tmp(vis.GetWidth()*vis.GetHeight()*4, 0.f);
			dev.ReadStorageBuffer(bVis, 0, (uint32)(tmp.size()*sizeof(f32)), tmp.data());
			std::vector<f32> &dst = volume.GetVisibilityAtlasMutable().GetData();
			const uint32 ch = vis.GetChannels();
			for (size_t t = 0; t*ch < dst.size(); t++)
				for (uint32 c = 0; c < ch && c < 4; c++)
					dst[t*ch+c] = tmp[t*4+c];
		}
		if (radianceLevels > 0)
		{
			std::vector<f32> tmp(rad.GetWidth()*rad.GetHeight()*4, 0.f);
			dev.ReadStorageBuffer(bRad, 0, (uint32)(tmp.size()*sizeof(f32)), tmp.data());
			std::vector<f32> &dst = volume.GetRadianceAtlasMutable().GetData();
			const uint32 ch = rad.GetChannels();
			for (size_t t = 0; t*ch < dst.size(); t++)
				for (uint32 c = 0; c < ch && c < 4; c++)
					dst[t*ch+c] = tmp[t*4+c];
		}
		volume.FillAtlasBorders();
		return true;
	}

	void DDGICompute::Shutdown()
	{
		if (!initialized && tracePipeline == 0)
			return;
		IRenderDevice &dev = GetActiveRenderDevice();
		const DeviceHandle bufs[10] = { bTris,bNodes,bIdx,bMats,bLights,bRays,bIrr,bVis,bParams,bRad };
		for (uint32 i = 0; i < 10; i++) if (bufs[i] != 0) dev.DestroyStorageBuffer(bufs[i]);
		bTris=bNodes=bIdx=bMats=bLights=bRays=bIrr=bVis=bParams=bRad=0;

		const DeviceHandle pipes[4] = { tracePipeline, irrPipeline, visPipeline, radPipeline };
		const DeviceHandle progs[4] = { traceProgram, irrProgram, visProgram, radProgram };
		const DeviceHandle stages[4] = { traceStage, irrStage, visStage, radStage };
		for (uint32 i = 0; i < 4; i++)
		{
			if (pipes[i] != 0) dev.DestroyComputePipeline(pipes[i]);
			if (progs[i] != 0) dev.DeleteProgram(progs[i]);
			if (stages[i] != 0) dev.DeleteShaderStage(stages[i]);
		}
		tracePipeline=irrPipeline=visPipeline=radPipeline=0;
		traceProgram=irrProgram=visProgram=radProgram=0;
		traceStage=irrStage=visStage=radStage=0;
		initialized = false;
		cursor = 0;
		radianceLevels = 0;
	}

};
