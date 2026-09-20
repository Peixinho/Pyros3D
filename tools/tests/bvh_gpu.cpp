// The traversal kernel against the CPU intersector, on identical rays.
//
// tools/tests/bvh.cpp established that RayScene::Intersect agrees with a
// brute-force scan. This establishes that the GLSL agrees with
// RayScene::Intersect - so the chain from "every triangle, linearly" to
// "a compute shader walking an SSBO" is closed, and a disagreement
// anywhere in it fails a test instead of producing noise somebody blames
// on sampling.
//
// It reads the traversal helpers out of resources/shaders/gi/raytrace.glsl
// rather than restating them, for the same reason sh_shader_parity does:
// a copy here would drift exactly as easily as the original.
//
//   c++ -std=c++17 -DMETAL_BACKEND -DRTPARITY_METAL -I include \
//       $(pkg-config --cflags freetype2) tools/tests/bvh_gpu.cpp \
//       -o /tmp/bvh_gpu -L build_metal -lPyrosEngine \
//       -framework Foundation -framework Metal -Wl,-rpath,$PWD/build_metal
//   /tmp/bvh_gpu          # run from the repo root
#if defined(RTPARITY_METAL)
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#elif defined(RTPARITY_VULKAN)
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#else
#error "Define RTPARITY_METAL or RTPARITY_VULKAN"
#endif

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/GI/RayScene.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace p3d;
static int failures = 0;

static void check(bool c, const std::string &what, const std::string &extra = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", what.c_str(), extra.empty()?"":" - ", extra.c_str());
	if(!c) failures++;
}

static uint32 rngState = 12345u;
static f32 Rand01()
{
	rngState = rngState * 1664525u + 1013904223u;
	return (f32)((rngState >> 8) & 0xFFFFFF) / (f32)0xFFFFFF;
}
static f32 RandRange(f32 a, f32 b) { return a + (b - a) * Rand01(); }

static void AddTriangle(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c)
{
	RayTriangle t;
	t.v0=a; t.v1=b; t.v2=c;
	const Vec3 n = (b-a).cross(c-a).normalize();
	t.n0=t.n1=t.n2=n;
	s.triangles.push_back(t);
}

static const uint32 kRays = 1024; // a whole number of 64-wide groups

int main()
{
	std::string traversal;
	{
		std::ifstream f("resources/shaders/gi/raytrace.glsl");
		if (!f.good())
		{
			printf("FAIL  cannot open resources/shaders/gi/raytrace.glsl - run from the repo root\n");
			return 1;
		}
		std::stringstream ss; ss << f.rdbuf(); traversal = ss.str();
	}
	// The file declares a struct and two helpers; the kernel below adds
	// the buffers and the traversal loop. Strip nothing - if it stops
	// compiling as part of a compute shader, that is worth failing on.
	check(traversal.find("p3d_IntersectTriangle") != std::string::npos,
		"the shared traversal helpers were found");

#if defined(RTPARITY_METAL)
	MetalRenderDevice device;
	printf("      backend     Metal\n");
#else
	VulkanRenderDevice device;
	if (device.GetInstance() == VK_NULL_HANDLE || !device.InitializeHeadless())
	{ printf("SKIP  no usable Vulkan device\n"); return 0; }
	printf("      backend     Vulkan (headless)\n");
#endif
	if (!device.SupportsCompute())
	{ printf("SKIP  SupportsCompute() is false on this backend\n"); return 0; }

	// ---- a scene with real structure ------------------------------------
	RayScene scene;
	for (uint32 i = 0; i < 600; i++)
	{
		const Vec3 c(RandRange(-20,20), RandRange(-20,20), RandRange(-20,20));
		const f32 sz = RandRange(0.2f, 3.5f);
		AddTriangle(scene,
			c + Vec3(RandRange(-sz,sz), RandRange(-sz,sz), RandRange(-sz,sz)),
			c + Vec3(RandRange(-sz,sz), RandRange(-sz,sz), RandRange(-sz,sz)),
			c + Vec3(RandRange(-sz,sz), RandRange(-sz,sz), RandRange(-sz,sz)));
	}
	scene.Build(4);
	check(scene.NodeCount() > 1, "scene built",
		std::to_string(scene.TriangleCount()) + " tris, " + std::to_string(scene.NodeCount()) + " nodes");

	std::vector<f32> triData, nodeData;
	std::vector<uint32> idxData;
	scene.PackForGPU(triData, nodeData, idxData);

	// ---- the kernel ------------------------------------------------------
	std::ostringstream src;
	src << "layout(local_size_x = 64) in;\n"
		<< "layout(std430, binding = 0) buffer Tris  { vec4 tris[]; };\n"
		<< "layout(std430, binding = 1) buffer Nodes { vec4 nodes[]; };\n"
		<< "layout(std430, binding = 2) buffer Idx   { uint indices[]; };\n"
		<< "layout(std430, binding = 3) buffer Rays  { vec4 rays[]; };\n"
		<< "layout(std430, binding = 4) buffer Out   { vec4 results[]; };\n"
		<< traversal << "\n"
		<< "void main() {\n"
		<< "    uint gid = gl_GlobalInvocationID.x;\n"
		<< "    vec3 o = rays[gid * 2u + 0u].xyz;\n"
		<< "    vec3 d = rays[gid * 2u + 1u].xyz;\n"
		<< "    vec3 invD = 1.0 / d;\n"
		<< "    float best = 1e30;\n"
		<< "    float hitT = 0.0; uint hitTri = 0u; float hu = 0.0, hv = 0.0; bool didHit = false;\n"
		<< "    uint stack[64]; uint depth = 0u; uint cur = 0u;\n"
		<< "    while (true) {\n"
		<< "        uint nb = cur * NODE_STRIDE;\n"
		<< "        vec4 n0 = nodes[nb + 0u];\n"
		<< "        vec4 n1 = nodes[nb + 1u];\n"
		<< "        uint first = floatBitsToUint(n0.w);\n"
		<< "        uint count = floatBitsToUint(n1.w);\n"
		<< "        if (count > 0u) {\n"
		<< "            for (uint i = 0u; i < count; i++) {\n"
		<< "                uint ti = indices[first + i];\n"
		<< "                uint tb = ti * TRI_STRIDE;\n"
		<< "                float t, u, v;\n"
		<< "                if (p3d_IntersectTriangle(o, d, tris[tb+0u].xyz, tris[tb+1u].xyz, tris[tb+2u].xyz, t, u, v)\n"
		<< "                    && t > 0.001 && t < best) {\n"
		<< "                    best = t; hitT = t; hitTri = ti; hu = u; hv = v; didHit = true;\n"
		<< "                }\n"
		<< "            }\n"
		<< "        } else {\n"
		<< "            uint na = first, nf = first + 1u;\n"
		<< "            vec4 a0 = nodes[na * NODE_STRIDE + 0u], a1 = nodes[na * NODE_STRIDE + 1u];\n"
		<< "            vec4 b0 = nodes[nf * NODE_STRIDE + 0u], b1 = nodes[nf * NODE_STRIDE + 1u];\n"
		<< "            float dNear = p3d_IntersectAABB(o, invD, a0.xyz, a1.xyz, best);\n"
		<< "            float dFar  = p3d_IntersectAABB(o, invD, b0.xyz, b1.xyz, best);\n"
		<< "            if (dNear > dFar) { float tmp = dNear; dNear = dFar; dFar = tmp; uint ts = na; na = nf; nf = ts; }\n"
		<< "            if (dNear < 3.0e38) {\n"
		<< "                if (dFar < 3.0e38 && depth < 64u) { stack[depth] = nf; depth++; }\n"
		<< "                cur = na; continue;\n"
		<< "            }\n"
		<< "        }\n"
		<< "        if (depth == 0u) break;\n"
		<< "        depth--; cur = stack[depth];\n"
		<< "    }\n"
		<< "    results[gid] = vec4(didHit ? hitT : -1.0, uintBitsToFloat(hitTri), hu, hv);\n"
		<< "}\n";

	const DeviceHandle stage = device.CreateShaderStage(ShaderType::ComputeShader);
	{
		std::string log;
		const bool ok = device.CompileShaderStage(stage, device.BuildShaderSource(std::string(), src.str()), log);
		check(ok, "the traversal kernel compiles", log);
		if (!ok) return 1;
	}
	const DeviceHandle program = device.CreateProgram();
	device.AttachShaderStage(program, stage);
	{ std::string log; check(device.LinkProgram(program, log), "LinkProgram", log); }
	const DeviceHandle pipeline = device.CreateComputePipeline(program);
	check(pipeline != 0, "CreateComputePipeline");
	if (pipeline == 0) return 1;

	// ---- identical rays on both sides ------------------------------------
	std::vector<f32> rayData(kRays * 8, 0.f);
	std::vector<Vec3> origins(kRays), dirs(kRays);
	for (uint32 r = 0; r < kRays; r++)
	{
		Vec3 o(RandRange(-30,30), RandRange(-30,30), RandRange(-30,30));
		Vec3 d(RandRange(-1,1), RandRange(-1,1), RandRange(-1,1));
		if (d.magnitude() < 1e-4f) d = Vec3(0,0,1);
		d.normalizeSelf();
		origins[r] = o; dirs[r] = d;
		rayData[r*8+0]=o.x; rayData[r*8+1]=o.y; rayData[r*8+2]=o.z;
		rayData[r*8+4]=d.x; rayData[r*8+5]=d.y; rayData[r*8+6]=d.z;
	}

	const DeviceHandle bTri  = device.CreateStorageBuffer((uint32)(triData.size()*sizeof(f32)), 0, triData.data());
	const DeviceHandle bNode = device.CreateStorageBuffer((uint32)(nodeData.size()*sizeof(f32)), 1, nodeData.data());
	const DeviceHandle bIdx  = device.CreateStorageBuffer((uint32)(idxData.size()*sizeof(uint32)), 2, idxData.data());
	const DeviceHandle bRay  = device.CreateStorageBuffer((uint32)(rayData.size()*sizeof(f32)), 3, rayData.data());
	const DeviceHandle bOut  = device.CreateStorageBuffer(kRays*4*(uint32)sizeof(f32), 4, NULL);
	check(bTri && bNode && bIdx && bRay && bOut, "allocated the scene buffers",
		std::to_string((triData.size()+nodeData.size())*sizeof(f32)/1024) + " KB of geometry");

	device.BindComputePipeline(0, pipeline);
	device.BindStorageBuffer(0, bTri, 0);
	device.BindStorageBuffer(0, bNode, 1);
	device.BindStorageBuffer(0, bIdx, 2);
	device.BindStorageBuffer(0, bRay, 3);
	device.BindStorageBuffer(0, bOut, 4);
	device.Dispatch(0, kRays / 64, 1, 1);
	device.ComputeBarrier(0, ComputeBarrierBit::HostRead);

	std::vector<f32> out(kRays * 4, 0.f);
	device.ReadStorageBuffer(bOut, 0, (uint32)(out.size()*sizeof(f32)), out.data());

	// ---- compare ----------------------------------------------------------
	uint32 disagreeHit = 0, disagreeTri = 0, gpuHits = 0;
	f32 worstT = 0.f;
	for (uint32 r = 0; r < kRays; r++)
	{
		RayHit cpu;
		const bool hcpu = scene.Intersect(origins[r], dirs[r], 0.001f, 1e30f, cpu);
		const f32 gt = out[r*4+0];
		const bool hgpu = gt >= 0.f;
		if (hgpu) gpuHits++;
		if (hcpu != hgpu) { disagreeHit++; continue; }
		if (!hcpu) continue;
		uint32 gtri; memcpy(&gtri, &out[r*4+1], sizeof(gtri));
		const f32 dt = fabsf(gt - cpu.t);
		if (gtri != cpu.triangle && dt > 1e-3f) disagreeTri++;
		if (dt > worstT) worstT = dt;
	}
	check(gpuHits > kRays / 20, "the rays actually hit things on the GPU",
		std::to_string(gpuHits) + " of " + std::to_string(kRays));
	check(disagreeHit == 0, "GPU and CPU agree on hit-or-miss for every ray",
		std::to_string(disagreeHit) + " disagreements");
	check(disagreeTri == 0, "and on WHICH triangle, for every hit",
		std::to_string(disagreeTri) + " disagreements");
	// Same maths, different compilers and hardware - so a small epsilon,
	// but far below anything that could be a different surface.
	check(worstT < 1e-2f, "and on the distance", "worst |dt| = " + std::to_string(worstT));

	device.DestroyStorageBuffer(bOut); device.DestroyStorageBuffer(bRay);
	device.DestroyStorageBuffer(bIdx); device.DestroyStorageBuffer(bNode);
	device.DestroyStorageBuffer(bTri);
	device.DestroyComputePipeline(pipeline);
	device.DeleteProgram(program);
	device.DeleteShaderStage(stage);

	printf("\n%s  bvh_gpu: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
