// The compute port against the CPU volume it replaces.
//
// Same Cornell box, same lights, same ray count, same frame index - so
// the two trace the SAME directions and must produce the same
// irradiance. A port that is merely "also plausible" is worthless here:
// the whole reason the CPU path still exists is to be the thing this is
// measured against.
//
//   c++ -std=c++17 -DMETAL_BACKEND -DDDGIGPU_METAL -I include \
//       $(pkg-config --cflags freetype2) tools/tests/ddgi_gpu.cpp \
//       -o /tmp/ddgi_gpu -L build_metal -lPyrosEngine \
//       -framework Foundation -framework Metal -Wl,-rpath,$PWD/build_metal
//   /tmp/ddgi_gpu            # run from the repo root
#if defined(DDGIGPU_METAL)
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#elif defined(DDGIGPU_VULKAN)
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#else
#error "Define DDGIGPU_METAL or DDGIGPU_VULKAN"
#endif

#include <Pyros3D/Rendering/GI/DDGICompute.h>
#include <cmath>
#include <cstdio>
#include <string>

using namespace p3d;
static int failures = 0;

static void check(bool c, const std::string &what, const std::string &extra = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", what.c_str(), extra.empty()?"":" - ", extra.c_str());
	if(!c) failures++;
}

static void AddQuad(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d, uint32 m)
{
	RayTriangle t1, t2;
	t1.v0=a; t1.v1=b; t1.v2=c; t1.materialIndex=m;
	t2.v0=a; t2.v1=c; t2.v2=d; t2.materialIndex=m;
	Vec3 n=(b-a).cross(c-a).normalize();
	t1.n0=t1.n1=t1.n2=n; t2.n0=t2.n1=t2.n2=n;
	s.triangles.push_back(t1); s.triangles.push_back(t2);
}
static uint32 AddMat(RayScene &s, const Vec3 &a)
{ RayMaterial m; m.albedo=a; s.materials.push_back(m); return (uint32)s.materials.size()-1; }

int main()
{
#if defined(DDGIGPU_METAL)
	MetalRenderDevice device;
	SetActiveRenderDevice(&device);
	printf("      backend     Metal\n");
#else
	VulkanRenderDevice device;
	if (device.GetInstance()==VK_NULL_HANDLE || !device.InitializeHeadless())
	{ printf("SKIP  no usable Vulkan device\n"); return 0; }
	SetActiveRenderDevice(&device);
	printf("      backend     Vulkan (headless)\n");
#endif
	if (!device.SupportsCompute())
	{ printf("SKIP  SupportsCompute() is false\n"); return 0; }

	// ---- the same Cornell box the CPU test uses ------------------------
	RayScene scene;
	const uint32 white=AddMat(scene, Vec3(0.75f,0.75f,0.75f));
	const uint32 red  =AddMat(scene, Vec3(0.75f,0.06f,0.06f));
	const uint32 green=AddMat(scene, Vec3(0.06f,0.75f,0.06f));
	const f32 H=5.f;
	AddQuad(scene, Vec3(-H,-H,-H), Vec3( H,-H,-H), Vec3( H,-H, H), Vec3(-H,-H, H), white);
	AddQuad(scene, Vec3(-H, H, H), Vec3( H, H, H), Vec3( H, H,-H), Vec3(-H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H, H), Vec3( H,-H, H), Vec3( H, H, H), Vec3(-H, H, H), white);
	AddQuad(scene, Vec3(-H,-H,-H), Vec3(-H,-H, H), Vec3(-H, H, H), Vec3(-H, H,-H), red);
	AddQuad(scene, Vec3( H,-H, H), Vec3( H,-H,-H), Vec3( H, H,-H), Vec3( H, H, H), green);
	scene.Build(4);

	std::vector<RayLight> lights(1);
	lights[0].isPoint = 1.f;
	lights[0].positionOrDirection = Vec3(0.f, 4.f, 0.f);
	lights[0].color = Vec3(6.f, 6.f, 6.f);
	lights[0].range = 40.f;

	const uint32 kRays = 128;
	const uint32 kFrame = 3;

	// ---- CPU reference --------------------------------------------------
	DDGIVolume cpu;
	check(cpu.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 4, 4, 4, 8, 16, 16, 4), "CPU volume allocates");
	cpu.SetSkyColor(Vec3(0.f,0.f,0.f));
	cpu.Update(scene, lights, kRays, kFrame, 0.f, 0);

	// ---- GPU ------------------------------------------------------------
	DDGIVolume gpu;
	gpu.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 4, 4, 4, 8, 16, 16, 4);
	gpu.SetSkyColor(Vec3(0.f,0.f,0.f));

	DDGICompute compute;
	check(compute.Initialize(scene, gpu, "resources/shaders"), "DDGICompute initialises (kernels compile)");
	if (failures) { printf("\nFAIL  ddgi_gpu: %d failure(s)\n", failures); return 1; }
	check(compute.Update(gpu, lights, kRays, kFrame, 0.f, 0), "GPU update runs");

	// ---- compare the atlases -------------------------------------------
	//
	// Every interior texel, not a sample: a bug that only affects one
	// corner of one tile is exactly the kind the borders exist for.
	{
		const ProbeAtlas &a = cpu.GetIrradianceAtlas();
		const ProbeAtlas &b = gpu.GetIrradianceAtlas();
		check(a.GetWidth()==b.GetWidth() && a.GetHeight()==b.GetHeight(), "irradiance atlases are the same shape");
		f32 worst=0.f, magnitude=0.f; uint32 worstProbe=0, worstTexel=0;
		const uint32 R=a.GetResolution();
		for (uint32 p=0; p<cpu.ProbeCount(); p++)
			for (uint32 y=0; y<R; y++)
				for (uint32 x=0; x<R; x++)
				{
					const f32 *ca=a.At(p,x,y), *cb=b.At(p,x,y);
					for (uint32 c=0;c<3;c++)
					{
						magnitude += fabsf(ca[c]);
						const f32 d=fabsf(ca[c]-cb[c]);
						if (d>worst){worst=d;worstProbe=p;worstTexel=y*R+x;}
					}
				}
		check(magnitude > 1.f, "the CPU reference is not all zeros",
			"sum |E| = " + std::to_string(magnitude));
		check(worst < 2e-3f, "GPU irradiance matches the CPU volume texel for texel",
			"worst |d| = " + std::to_string(worst) + " at probe " + std::to_string(worstProbe)
			+ " texel " + std::to_string(worstTexel));
	}
	{
		const ProbeAtlas &a = cpu.GetVisibilityAtlas();
		const ProbeAtlas &b = gpu.GetVisibilityAtlas();
		f32 worst=0.f; const uint32 R=a.GetResolution();
		for (uint32 p=0; p<cpu.ProbeCount(); p++)
			for (uint32 y=0; y<R; y++)
				for (uint32 x=0; x<R; x++)
				{
					const f32 *ca=a.At(p,x,y), *cb=b.At(p,x,y);
					// Relative: these are squared distances and run to
					// hundreds, so an absolute epsilon would be either
					// meaningless or impossible.
					for (uint32 c=0;c<2;c++)
					{
						const f32 scale = fmaxf(1.f, fabsf(ca[c]));
						worst = fmaxf(worst, fabsf(ca[c]-cb[c]) / scale);
					}
				}
		check(worst < 5e-3f, "GPU visibility moments match the CPU volume",
			"worst relative |d| = " + std::to_string(worst));
	}

	// ---- prefiltered radiance ------------------------------------------
	//
	// The specular atlas is levels times larger and gathered through a
	// different lobe, so it is the one most likely to be indexed
	// wrongly - a level or a probe out of step still produces a
	// plausible-looking reflection of the wrong part of the room.
	// Comparing every texel of every level catches exactly that.
	{
		const ProbeAtlas &a = cpu.GetRadianceAtlas();
		const ProbeAtlas &b = gpu.GetRadianceAtlas();
		check(a.GetWidth()==b.GetWidth() && a.GetHeight()==b.GetHeight() && a.GetResolution()>0,
			"radiance atlases are the same shape");
		check(cpu.GetRadianceLevels()==4 && gpu.GetRadianceLevels()==4,
			"both volumes prefiltered four roughness levels");

		f32 worst=0.f, magnitude=0.f; uint32 worstTile=0;
		const uint32 R=a.GetResolution();
		const uint32 tiles = cpu.ProbeCount() * cpu.GetRadianceLevels();
		for (uint32 t=0; t<tiles; t++)
			for (uint32 y=0;y<R;y++)
				for (uint32 x=0;x<R;x++)
				{
					const f32 *ca=a.At(t,x,y), *cb=b.At(t,x,y);
					for (uint32 c=0;c<3;c++)
					{
						magnitude = fmaxf(magnitude, fabsf(ca[c]));
						const f32 d=fabsf(ca[c]-cb[c]);
						if (d>worst){worst=d;worstTile=t;}
					}
				}
		check(magnitude > 1.f, "the CPU radiance reference is not all zeros",
			"max = " + std::to_string(magnitude));
		check(worst < 2e-3f, "GPU prefiltered radiance matches the CPU volume texel for texel",
			"worst |d| = " + std::to_string(worst) + " at tile " + std::to_string(worstTile));

		// A level-indexing error keeps every texel present but swaps
		// which lobe wrote it, so the check above could in principle
		// pass on a symmetric scene. This one cannot: the levels must
		// differ from each other, and in the right direction.
		f32 lvl0=0.f, lvl3=0.f;
		for (uint32 p=0; p<cpu.ProbeCount(); p++)
			for (uint32 y=0;y<R;y++)
				for (uint32 x=0;x<R;x++)
				{
					const f32 *c0=b.At(gpu.RadianceTile(p,0),x,y);
					const f32 *c3=b.At(gpu.RadianceTile(p,3),x,y);
					lvl0 = fmaxf(lvl0, fabsf(c0[0]-c0[1]));
					lvl3 = fmaxf(lvl3, fabsf(c3[0]-c3[1]));
				}
		check(lvl0 > lvl3 * 1.1f,
			"the sharp level holds more colour contrast than the rough one",
			"level 0 " + std::to_string(lvl0) + " vs level 3 " + std::to_string(lvl3));
	}

	// ---- and that it still bleeds colour --------------------------------
	//
	// The atlases matching is the strong claim; this is the sanity check
	// that both are producing GI rather than agreeing on garbage.
	{
		const Vec3 up(0,1,0);
		const Vec3 nearRed   = gpu.SampleIrradiance(Vec3(-3.5f,-3.6f,0.f), up);
		const Vec3 nearGreen = gpu.SampleIrradiance(Vec3( 3.5f,-3.6f,0.f), up);
		printf("      GPU floor near red   R=%.4f G=%.4f\n", nearRed.x, nearRed.y);
		printf("      GPU floor near green R=%.4f G=%.4f\n", nearGreen.x, nearGreen.y);
		check(nearRed.x > nearRed.y * 1.15f, "GPU volume still bleeds red near the red wall",
			"R/G = " + std::to_string(nearRed.x / fmaxf(1e-6f, nearRed.y)));
		check(nearGreen.y > nearGreen.x * 1.15f, "and green near the green wall",
			"G/R = " + std::to_string(nearGreen.y / fmaxf(1e-6f, nearGreen.x)));
	}

	compute.Shutdown();
	// ---- more probes than one dispatch batch ---------------------------
	//
	// DDGICompute traces at most kMaxBatchProbes (128) probes per
	// dispatch and rewrites its Params buffer between batches. A
	// storage barrier only ORDERS work inside the command buffer; it
	// does not submit it, so without a flush every batch ends up
	// reading the last batch's probe offset and all but the final
	// batch is silently never written.
	//
	// Everything above this point uses 64 probes - one batch - and
	// passes either way. That gap shipped: the Cornell demo has 294
	// probes and only 37 of them had any irradiance at all, which
	// looks exactly like a volume that has not converged yet.
	{
		const uint32 N = 7;                       // 343 probes = 3 batches
		DDGIVolume bigCPU, bigGPU;
		bigCPU.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(1.33f,1.33f,1.33f), N,N,N, 8, 16, 16, 4);
		bigGPU.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(1.33f,1.33f,1.33f), N,N,N, 8, 16, 16, 4);
		check(bigCPU.ProbeCount() > 128, "the volume needs more than one dispatch batch",
			std::to_string(bigCPU.ProbeCount()) + " probes");

		bigCPU.Update(scene, lights, 64, kFrame, 0.f, 0);

		DDGICompute big;
		if (!big.Initialize(scene, bigGPU, "resources/shaders"))
			check(false, "DDGICompute initialises for the large volume");
		else
		{
			check(big.Update(bigGPU, lights, 64, kFrame, 0.f, 0), "GPU update runs over three batches");

			// Every probe, not a sample: the failure mode is a
			// CONTIGUOUS RANGE of probes left untouched, which an
			// average or a spot check sails straight past.
			const ProbeAtlas &a = bigCPU.GetIrradianceAtlas();
			const ProbeAtlas &b = bigGPU.GetIrradianceAtlas();
			const uint32 R = a.GetResolution();
			uint32 emptyGPU = 0, emptyCPU = 0, firstEmpty = 0xFFFFFFFFu;
			f32 worst = 0.f;
			for (uint32 p = 0; p < bigCPU.ProbeCount(); p++)
			{
				f32 mA = 0.f, mB = 0.f;
				for (uint32 y = 0; y < R; y++)
					for (uint32 x = 0; x < R; x++)
					{
						const f32 *ca = a.At(p,x,y), *cb = b.At(p,x,y);
						for (uint32 c = 0; c < 3; c++)
						{
							mA = fmaxf(mA, fabsf(ca[c]));
							mB = fmaxf(mB, fabsf(cb[c]));
							worst = fmaxf(worst, fabsf(ca[c]-cb[c]));
						}
					}
				if (mA <= 1e-6f) emptyCPU++;
				if (mB <= 1e-6f) { emptyGPU++; if (firstEmpty == 0xFFFFFFFFu) firstEmpty = p; }
			}
			check(emptyGPU == emptyCPU,
				"every probe the CPU lit, the GPU lit too - no batch was skipped",
				"GPU empty " + std::to_string(emptyGPU) + ", CPU empty " + std::to_string(emptyCPU)
				+ ", first GPU-empty probe " + std::to_string(firstEmpty)
				+ " of " + std::to_string(bigCPU.ProbeCount()));
			check(worst < 2e-3f, "and the three batches agree with the CPU texel for texel",
				"worst |d| = " + std::to_string(worst));
			big.Shutdown();
		}
	}

	printf("\n%s  ddgi_gpu: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
