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
	// Inward-facing - see the note in tools/tests/ddgi.cpp's AddQuad.
	RayTriangle t1, t2;
	t1.v0=a; t1.v1=c; t1.v2=b; t1.materialIndex=m;
	t2.v0=a; t2.v1=d; t2.v2=c; t2.materialIndex=m;
	Vec3 n=(c-a).cross(b-a).normalize();
	t1.n0=t1.n1=t1.n2=n; t2.n0=t2.n1=t2.n2=n;
	s.triangles.push_back(t1); s.triangles.push_back(t2);
}
// A solid object: faces pointing OUT of it, the opposite of AddQuad's
// room shell. See tools/tests/ddgi.cpp.
static void AddSolidBox(RayScene &s, const Vec3 &c, const Vec3 &h, uint32 mat)
{
	const Vec3 p000(c.x-h.x, c.y-h.y, c.z-h.z), p100(c.x+h.x, c.y-h.y, c.z-h.z);
	const Vec3 p110(c.x+h.x, c.y+h.y, c.z-h.z), p010(c.x-h.x, c.y+h.y, c.z-h.z);
	const Vec3 p001(c.x-h.x, c.y-h.y, c.z+h.z), p101(c.x+h.x, c.y-h.y, c.z+h.z);
	const Vec3 p111(c.x+h.x, c.y+h.y, c.z+h.z), p011(c.x-h.x, c.y+h.y, c.z+h.z);
	AddQuad(s, p001, p101, p100, p000, mat);
	AddQuad(s, p010, p110, p111, p011, mat);
	AddQuad(s, p000, p100, p110, p010, mat);
	AddQuad(s, p101, p001, p011, p111, mat);
	AddQuad(s, p001, p000, p010, p011, mat);
	AddQuad(s, p100, p101, p111, p110, mat);
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

	// ---- multi-bounce, over several updates ------------------------------
	//
	// Everything above runs ONE update from a zeroed atlas, where the
	// feedback term reads zeros and contributes nothing - so none of it
	// tests multi-bounce at all. The interesting part only appears once
	// there is light in the volume to feed back, which takes repeated
	// updates, and it is also where the CPU and GPU could most easily
	// drift: the CPU walks probes one at a time and the GPU traces 128
	// at once, so they agree only because both snapshot the feedback
	// source before either writes anything.
	{
		DDGIVolume mbCPU, mbGPU;
		mbCPU.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 4,4,4, 8, 16, 16, 4);
		mbGPU.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 4,4,4, 8, 16, 16, 4);
		check(mbCPU.GetMultiBounce() > 0.f, "multi-bounce is on by default",
			std::to_string(mbCPU.GetMultiBounce()));

		DDGICompute mb;
		if (!mb.Initialize(scene, mbGPU, "resources/shaders"))
			check(false, "DDGICompute initialises for the multi-bounce volume");
		else
		{
			f32 firstPass = 0.f, lastPass = 0.f;
			for (uint32 f = 0; f < 5; f++)
			{
				mbCPU.Update(scene, lights, 64, f, 0.f, 0);
				mb.Update(mbGPU, lights, 64, f, 0.f, 0);
				const f32 e = mbCPU.SampleIrradiance(Vec3(0.f,-3.5f,0.f), Vec3(0,1,0)).x;
				if (f == 0) firstPass = e;
				lastPass = e;
			}

			// The point of multi-bounce: the same scene, the same
			// rays, more light. If these were equal the feedback term
			// would be doing nothing and the parity check below would
			// be comparing two copies of the single-bounce answer.
			check(lastPass > firstPass * 1.2f,
				"light keeps arriving as bounces accumulate",
				"pass 1 = " + std::to_string(firstPass) + ", pass 5 = " + std::to_string(lastPass));

			const ProbeAtlas &a = mbCPU.GetIrradianceAtlas();
			const ProbeAtlas &b = mbGPU.GetIrradianceAtlas();
			const uint32 R = a.GetResolution();
			f32 worst = 0.f, mag = 0.f;
			for (uint32 p = 0; p < mbCPU.ProbeCount(); p++)
				for (uint32 y = 0; y < R; y++)
					for (uint32 x = 0; x < R; x++)
					{
						const f32 *ca = a.At(p,x,y), *cb = b.At(p,x,y);
						for (uint32 c = 0; c < 3; c++)
						{
							mag = fmaxf(mag, fabsf(ca[c]));
							worst = fmaxf(worst, fabsf(ca[c]-cb[c]));
						}
					}
			// Looser than the single-update checks above on purpose:
			// five updates of feedback compound float ordering
			// differences between a sequential CPU gather and a
			// parallel GPU one. Still three orders of magnitude below
			// the values themselves.
			check(worst < 5e-3f * fmaxf(mag, 1.f),
				"five bounces of GPU feedback still match the CPU",
				"worst |d| = " + std::to_string(worst) + " against max " + std::to_string(mag));
			mb.Shutdown();
		}
	}

	// ---- probe relocation ------------------------------------------------
	//
	// Nothing above relocates: the Cornell box has no probe inside
	// geometry, so both paths leave every offset at zero and would
	// agree perfectly while testing nothing. This scene puts a plane
	// of probes inside a solid slab.
	//
	// The DECISION is shared C++ - the kernel only reduces each
	// probe's rays to the statistics it needs - so what this actually
	// checks is that the GPU's reduction produces the same numbers as
	// the CPU's, and that the offsets it feeds back are then used when
	// tracing. Get either wrong and the two diverge immediately,
	// because a relocated probe traces from somewhere else.
	{
		RayScene room;
		const uint32 grey = AddMat(room, Vec3(0.8f, 0.8f, 0.8f));
		const f32 B = 6.f;
		AddQuad(room, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(room, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), grey);
		AddQuad(room, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(room, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), grey);
		AddQuad(room, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), grey);
		AddQuad(room, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), grey);
		// Deliberately taller and deeper than the room, so it pokes
		// through the walls instead of meeting them. A slab that ends
		// exactly at a wall puts two triangles in the same plane -
		// one facing in, one facing out - and a ray landing there can
		// legitimately return either, so whether it counts as a
		// backface becomes a coin toss decided by traversal order.
		// The CPU and GPU walk the BVH differently and disagreed on
		// 18 of 125 probes because of it, which looked like a
		// relocation bug and was a modelling one.
		AddSolidBox(room, Vec3(0.f, 0.f, 0.f), Vec3(0.8f, B + 1.f, B + 1.f), grey);
		room.Build(4);

		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(3.f, 0.f, 0.f);
		lamp[0].color = Vec3(6.f, 6.f, 6.f);
		lamp[0].range = 30.f;

		DDGIVolume rc, rg;
		rc.Allocate(Vec3(-6,-6,-6), Vec3(3,3,3), 5,5,5, 8, 16);
		rg.Allocate(Vec3(-6,-6,-6), Vec3(3,3,3), 5,5,5, 8, 16);

		DDGICompute rel;
		if (!rel.Initialize(room, rg, "resources/shaders"))
			check(false, "DDGICompute initialises for the relocation volume");
		else
		{
			for (uint32 f = 0; f < 8; f++)
			{
				rc.Update(room, lamp, 128, f, 0.f, 0);
				rel.Update(rg, lamp, 128, f, 0.f, 0);
			}

			uint32 movedCPU = 0, movedGPU = 0, offCPU = 0, offGPU = 0;
			f32 worstOffset = 0.f, worstFlag = 0.f;
			for (uint32 p = 0; p < rc.ProbeCount(); p++)
			{
				const Vec4 &a = rc.GetProbeData()[p];
				const Vec4 &b = rg.GetProbeData()[p];
				if (fmaxf(fabsf(a.x), fmaxf(fabsf(a.y), fabsf(a.z))) > 1e-4f) movedCPU++;
				if (fmaxf(fabsf(b.x), fmaxf(fabsf(b.y), fabsf(b.z))) > 1e-4f) movedGPU++;
				if (a.w <= 0.5f) offCPU++;
				if (b.w <= 0.5f) offGPU++;
				worstOffset = fmaxf(worstOffset, fmaxf(fabsf(a.x-b.x),
					fmaxf(fabsf(a.y-b.y), fabsf(a.z-b.z))));
				worstFlag = fmaxf(worstFlag, fabsf(a.w - b.w));
			}
			printf("      relocation: CPU %u moved / %u off, GPU %u moved / %u off\n",
				movedCPU, offCPU, movedGPU, offGPU);
			check(movedGPU > 0, "the GPU relocates probes buried in the slab",
				std::to_string(movedGPU));
			check(worstFlag == 0.f, "and switches off exactly the probes the CPU does",
				"CPU " + std::to_string(offCPU) + " vs GPU " + std::to_string(offGPU));
			// Tight: both sides run the same decision on statistics
			// that should be identical, so any difference is the
			// reduction disagreeing, not float noise in a long chain.
			check(movedCPU == movedGPU, "the same probes, not merely the same number",
				"CPU " + std::to_string(movedCPU) + " vs GPU " + std::to_string(movedGPU));
			// Tight, and it stays tight: relocation is a feedback loop
			// - an offset changes where the probe traces from, which
			// changes the statistics, which changes the offset - so a
			// real difference does not stay small. Measured over eight
			// updates the worst disagreement is 1.2e-5 and does not
			// grow, which is float noise in two different intersectors
			// and nothing else.
			check(worstOffset < 1e-3f, "with offsets matching the CPU",
				"worst |d| = " + std::to_string(worstOffset));

			// And the offsets have to reach the trace: a probe moved
			// on one side and not the other lights a different place.
			const ProbeAtlas &ia = rc.GetIrradianceAtlas();
			const ProbeAtlas &ib = rg.GetIrradianceAtlas();
			const uint32 R = ia.GetResolution();
			f32 worst = 0.f, mag = 0.f;
			for (uint32 p = 0; p < rc.ProbeCount(); p++)
				for (uint32 y = 0; y < R; y++)
					for (uint32 x = 0; x < R; x++)
					{
						const f32 *ca = ia.At(p,x,y), *cb = ib.At(p,x,y);
						for (uint32 c = 0; c < 3; c++)
						{
							mag = fmaxf(mag, fabsf(ca[c]));
							worst = fmaxf(worst, fabsf(ca[c]-cb[c]));
						}
					}
			// Looser than the 1e-6 the non-relocating comparisons hold
			// to, and necessarily so: here the probe POSITIONS differ
			// by ~1e-5, each probe traces a slightly different set of
			// world points, and eight updates of multi-bounce feedback
			// compound that. Under 1% of the brightest texel is the
			// measured result and is not visible; what would matter is
			// a probe placed somewhere else entirely, and the offset
			// check above is what rules that out.
			check(worst < 2e-2f * fmaxf(mag, 1.f),
				"and the relocated probes trace to the same irradiance",
				"worst |d| = " + std::to_string(worst) + " against max " + std::to_string(mag));
			rel.Shutdown();
		}
	}

	// ---- geometry that moves ---------------------------------------------
	//
	// The triangles are baked into world space and the BVH built over
	// them once. A moving LIGHT was always followed; moving GEOMETRY
	// was not, so a door that opened went on blocking light where it
	// used to be. RefitBVH updates the tree and DDGICompute::
	// UpdateGeometry re-uploads it - and that upload is the part
	// nothing else exercises.
	{
		RayScene room;
		const uint32 grey = AddMat(room, Vec3(0.8f, 0.8f, 0.8f));
		const f32 B = 6.f;
		AddQuad(room, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(room, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), grey);
		AddQuad(room, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(room, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), grey);
		AddQuad(room, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), grey);
		AddQuad(room, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), grey);
		// A shutter between the lamp and the far corner. Its triangles
		// are the last ones added, so they are easy to move.
		const uint32 firstShutter = room.TriangleCount();
		AddSolidBox(room, Vec3(0.f, 0.f, 0.f), Vec3(0.4f, B + 1.f, B + 1.f), grey);
		const uint32 shutterCount = room.TriangleCount() - firstShutter;
		room.Build(4);

		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(3.5f, 0.f, 0.f);
		lamp[0].color = Vec3(8.f, 8.f, 8.f);
		lamp[0].range = 40.f;

		// Behind the shutter, where the lamp cannot reach directly.
		const Vec3 shadowed(-4.f, 0.f, 0.f);
		const Vec3 up2(0.f, 1.f, 0.f);

		DDGIVolume mc, mg;
		mc.Allocate(Vec3(-5,-5,-5), Vec3(2.5f,2.5f,2.5f), 5,5,5, 8, 16);
		mg.Allocate(Vec3(-5,-5,-5), Vec3(2.5f,2.5f,2.5f), 5,5,5, 8, 16);

		DDGICompute mv;
		if (!mv.Initialize(room, mg, "resources/shaders"))
			check(false, "DDGICompute initialises for the moving-geometry volume");
		else
		{
			for (uint32 f = 0; f < 6; f++)
			{
				mc.Update(room, lamp, 128, f, 0.f, 0);
				mv.Update(mg, lamp, 128, f, 0.f, 0);
			}
			const f32 blockedCPU = mc.SampleIrradiance(shadowed, up2).x;
			const f32 blockedGPU = mg.SampleIrradiance(shadowed, up2).x;

			// Slide the shutter out of the way, exactly as an object
			// moving in a scene would.
			for (uint32 t = firstShutter; t < firstShutter + shutterCount; t++)
			{
				room.triangles[t].v0.y += 14.f;
				room.triangles[t].v1.y += 14.f;
				room.triangles[t].v2.y += 14.f;
			}
			room.RefitBVH();
			check(mv.UpdateGeometry(room), "the moved geometry uploads to the GPU");

			for (uint32 f = 6; f < 14; f++)
			{
				mc.Update(room, lamp, 128, f, 0.f, 0);
				mv.Update(mg, lamp, 128, f, 0.f, 0);
			}
			const f32 openCPU = mc.SampleIrradiance(shadowed, up2).x;
			const f32 openGPU = mg.SampleIrradiance(shadowed, up2).x;

			printf("      behind the shutter: CPU %.4f -> %.4f, GPU %.4f -> %.4f\n",
				blockedCPU, openCPU, blockedGPU, openGPU);

			// The point of the whole feature: moving the blocker has to
			// change the light behind it. If this passes without the
			// refit and the re-upload, the scene is being traced
			// against geometry that is no longer there.
			check(openCPU > blockedCPU * 1.5f,
				"moving the shutter lets light reach what it was shading",
				std::to_string(blockedCPU) + " -> " + std::to_string(openCPU));
			check(openGPU > blockedGPU * 1.5f,
				"and the GPU sees the move too - which needs the re-upload",
				std::to_string(blockedGPU) + " -> " + std::to_string(openGPU));
			check(fabsf(openGPU - openCPU) < 0.05f * fmaxf(openCPU, 1.f),
				"with both agreeing on how much",
				"CPU " + std::to_string(openCPU) + " vs GPU " + std::to_string(openGPU));
			mv.Shutdown();
		}
	}

	printf("\n%s  ddgi_gpu: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
