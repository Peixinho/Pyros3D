// The specular sampler exists twice: DDGIVolume::SampleRadiance in C++
// and SampleDDGIRadiance in PyrosShader.glsl. This is what keeps them
// the same function.
//
// The risk it exists for is not the lighting maths - that is shared
// with the diffuse path and already covered. It is the ATLAS
// ARITHMETIC. The radiance atlas stacks every probe at level 0, then
// every probe at level 1, and the shader has to reproduce that layout
// from a handful of uniforms. Get the stride wrong and a rough surface
// samples a smooth probe's tile, or probe 7 samples probe 8: still a
// reflection, still plausible, wrong.
//
// Like sh_shader_parity, it READS the shipped GLSL rather than
// restating it. Two substitutions are made, and both are declared here
// rather than hidden:
//
//   - the UBO becomes an SSBO, because a compute dispatch here has no
//     material to bind one through;
//   - texture() becomes a NEAREST fetch from the uploaded atlas. The
//     real shader gets hardware bilinear, which the CPU reference does
//     not do; sampling nearest on both sides compares the addressing,
//     which is the thing at risk, instead of comparing two different
//     filters and calling the difference a tolerance.
//
//   c++ -std=c++17 -DMETAL_BACKEND -DPARITY_METAL -I include \
//       $(pkg-config --cflags freetype2) \
//       tools/tests/ddgi_specular_shader_parity.cpp -o /tmp/ddgi_sp \
//       -L build_metal -lPyrosEngine -framework Foundation -framework Metal \
//       -Wl,-rpath,$PWD/build_metal
//   /tmp/ddgi_sp             # run from the repo root
//
// Exits 0 for PASS and for SKIP (no compute here), 1 for FAIL.
#if defined(PARITY_METAL)
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#elif defined(PARITY_VULKAN)
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#else
#error "Define PARITY_METAL or PARITY_VULKAN"
#endif

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace p3d;
static int failures = 0;
static void check(bool c, const std::string &w, const std::string &e = std::string())
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", w.c_str(), e.empty()?"":" - ", e.c_str());
	if(!c) failures++;
}

static bool ExtractGLSLFunction(const std::string &src, const std::string &sig, std::string &out)
{
	const size_t start = src.find(sig);
	if (start == std::string::npos) return false;
	const size_t open = src.find('{', start);
	if (open == std::string::npos) return false;
	int depth = 0;
	for (size_t i = open; i < src.size(); i++)
	{
		if (src[i]=='{') depth++;
		else if (src[i]=='}') { depth--; if (depth==0) { out = src.substr(start, i-start+1); return true; } }
	}
	return false;
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
static uint32 AddMat(RayScene &s, const Vec3 &a)
{ RayMaterial m; m.albedo=a; s.materials.push_back(m); return (uint32)s.materials.size()-1; }

static const uint32 kSamples = 64;   // one work group exactly

int main()
{
	// ---- the shipped GLSL ----------------------------------------------
	std::string file;
	{
		std::ifstream f("resources/shaders/PyrosShader.glsl");
		if (!f.good()) { printf("FAIL  run from the repo root\n"); return 1; }
		std::stringstream ss; ss << f.rdbuf(); file = ss.str();
	}
	std::string octEncode, probeUV, radianceUV, sampleRad, probePos, probeActive, probeData;
	check(ExtractGLSLFunction(file, "vec2 p3d_OctEncode(vec3 d)", octEncode), "found p3d_OctEncode");
	check(ExtractGLSLFunction(file, "vec2 p3d_ProbeUV(float probeIndex", probeUV), "found p3d_ProbeUV");
	check(ExtractGLSLFunction(file, "vec2 p3d_RadianceUV(float tileIndex", radianceUV), "found p3d_RadianceUV");
	check(ExtractGLSLFunction(file, "vec4 p3d_ProbeData(float index", probeData),
		"found p3d_ProbeData");
	check(ExtractGLSLFunction(file, "vec3 p3d_ProbeWorldPos(vec3 cell", probePos),
		"found p3d_ProbeWorldPos");
	check(ExtractGLSLFunction(file, "bool p3d_ProbeActive(float index", probeActive),
		"found p3d_ProbeActive");
	check(ExtractGLSLFunction(file, "vec3 SampleDDGIRadiance(vec3 worldPos", sampleRad),
		"found SampleDDGIRadiance");
	if (failures) return 1;
	check(sampleRad.find("uDDGIRadiance") != std::string::npos,
		"the extracted function really samples the radiance atlas");

	// ---- device ----------------------------------------------------------
#if defined(PARITY_METAL)
	MetalRenderDevice device;
	printf("      backend     Metal\n");
#else
	VulkanRenderDevice device;
	if (device.GetInstance()==VK_NULL_HANDLE || !device.InitializeHeadless())
	{ printf("SKIP  no usable Vulkan device\n"); return 0; }
	printf("      backend     Vulkan (headless)\n");
#endif
	if (!device.SupportsCompute()) { printf("SKIP  no compute on this backend\n"); return 0; }

	// ---- a volume with something worth reflecting -------------------------
	RayScene scene;
	const uint32 white = AddMat(scene, Vec3(0.75f,0.75f,0.75f));
	const uint32 red   = AddMat(scene, Vec3(0.75f,0.06f,0.06f));
	const uint32 green = AddMat(scene, Vec3(0.06f,0.75f,0.06f));
	const f32 H = 5.f;
	AddQuad(scene, Vec3(-H,-H,-H), Vec3( H,-H,-H), Vec3( H,-H, H), Vec3(-H,-H, H), white);
	AddQuad(scene, Vec3(-H, H, H), Vec3( H, H, H), Vec3( H, H,-H), Vec3(-H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H, H), Vec3( H,-H, H), Vec3( H, H, H), Vec3(-H, H, H), white);
	AddQuad(scene, Vec3( H,-H,-H), Vec3(-H,-H,-H), Vec3(-H, H,-H), Vec3( H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H,-H), Vec3(-H,-H, H), Vec3(-H, H, H), Vec3(-H, H,-H), red);
	AddQuad(scene, Vec3( H,-H, H), Vec3( H,-H,-H), Vec3( H, H,-H), Vec3( H, H, H), green);
	scene.Build(4);

	std::vector<RayLight> lights(1);
	lights[0].isPoint = 1.f;
	lights[0].positionOrDirection = Vec3(0.f, 4.f, 0.f);
	// Bright: the pillar added below blocks a good deal of the room,
	// and the "did the GPU produce anything at all" guard at the end
	// has to stay a real check rather than one tuned down to pass.
	lights[0].color = Vec3(16.f,16.f,16.f);
	lights[0].range = 30.f;

	DDGIVolume vol;
	vol.Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16, 16, 4);
	// A pillar, so some probes are buried and relocation actually
	// produces offsets for the shader to disagree about. Without one
	// every offset is zero and the new code path is never taken.
	{
		RayScene &sc = scene;
		const uint32 grey = AddMat(sc, Vec3(0.7f, 0.7f, 0.7f));
		const f32 P = 1.2f;
		AddQuad(sc, Vec3(-P,-6, P), Vec3( P,-6, P), Vec3( P,-6,-P), Vec3(-P,-6,-P), grey);
		AddQuad(sc, Vec3(-P, 6,-P), Vec3( P, 6,-P), Vec3( P, 6, P), Vec3(-P, 6, P), grey);
		AddQuad(sc, Vec3(-P,-6,-P), Vec3( P,-6,-P), Vec3( P, 6,-P), Vec3(-P, 6,-P), grey);
		AddQuad(sc, Vec3( P,-6, P), Vec3(-P,-6, P), Vec3(-P, 6, P), Vec3( P, 6, P), grey);
		AddQuad(sc, Vec3(-P,-6, P), Vec3(-P,-6,-P), Vec3(-P, 6,-P), Vec3(-P, 6, P), grey);
		AddQuad(sc, Vec3( P,-6,-P), Vec3( P,-6, P), Vec3( P, 6, P), Vec3( P, 6,-P), grey);
		sc.Build(4);
	}
	for (uint32 f = 0; f < 4; f++)
		vol.Update(scene, lights, 256, f, 0.f, 0);
	{
		uint32 moved = 0, offCount = 0;
		for (uint32 p = 0; p < vol.ProbeCount(); p++)
		{
			const Vec4 &d = vol.GetProbeData()[p];
			if (fmaxf(fabsf(d.x), fmaxf(fabsf(d.y), fabsf(d.z))) > 1e-4f) moved++;
			if (d.w <= 0.5f) offCount++;
		}
		printf("      %u probes relocated, %u switched off\n", moved, offCount);
		check(moved + offCount > 0,
			"the scene actually relocates probes, so the offsets are exercised",
			std::to_string(moved) + " moved, " + std::to_string(offCount) + " off");
	}

	const ProbeAtlas &vis = vol.GetVisibilityAtlas();
	const ProbeAtlas &rad = vol.GetRadianceAtlas();

	// ---- sample points, spread through the box ---------------------------
	std::vector<Vec3> posCPU(kSamples), nrmCPU(kSamples), refCPU(kSamples);
	std::vector<f32>  rghCPU(kSamples);
	std::vector<f32>  inputs(kSamples * 12, 0.f);   // pos, normal, refl, (rough,0,0,0)
	for (uint32 i = 0; i < kSamples; i++)
	{
		const f32 k = (f32)i + 0.5f;
		const f32 a = 2.399963f * k;           // golden angle, so no two land alike
		Vec3 p(cosf(a) * 3.f, -4.f + fmodf(k * 0.37f, 6.f), sinf(a) * 3.f);
		Vec3 n(cosf(a*0.7f)*0.3f, 1.f, sinf(a*0.7f)*0.3f); n.normalizeSelf();
		Vec3 r(cosf(a*1.3f), 0.45f, sinf(a*1.3f)); r.normalizeSelf();
		const f32 rough = 0.05f + fmodf(k * 0.13f, 0.9f);
		posCPU[i]=p; nrmCPU[i]=n; refCPU[i]=r; rghCPU[i]=rough;
		inputs[i*12+0]=p.x; inputs[i*12+1]=p.y; inputs[i*12+2]=p.z;
		inputs[i*12+4]=n.x; inputs[i*12+5]=n.y; inputs[i*12+6]=n.z;
		inputs[i*12+8]=r.x; inputs[i*12+9]=r.y; inputs[i*12+10]=r.z; inputs[i*12+11]=rough;
	}

	// ---- the compute shader around the extracted functions ----------------
	std::ostringstream body;
	body << "layout(local_size_x = 64) in;\n"
	     << "layout(std430, binding = 0) buffer UBuf { vec4 U[]; };\n"
	     << "layout(std430, binding = 1) buffer VBuf { vec4 visTex[]; };\n"
	     << "layout(std430, binding = 2) buffer RBuf { vec4 radTex[]; };\n"
	     << "layout(std430, binding = 3) buffer IBuf { vec4 inputs[]; };\n"
	     << "layout(std430, binding = 4) buffer OBuf { vec4 results[]; };\n"
	     // The probe-offset block, which in the real shader is a UBO
	     // sized by DDGI_MAX_PROBE_DATA. Same contents, same indexing.
	     // The probe-data texture, which in the real shader is a
	     // sampler read with texelFetch. Same contents, same indexing.
	     << "layout(std430, binding = 5) buffer PBuf { vec4 probeData[]; };\n"
	     << "const int uDDGIProbeData = 3;\n"
	     << "vec4 p3d_Fetch(int which, ivec2 t) {\n"
	     << "    int perRow = int(U[3].w);\n"
	     << "    return probeData[t.y * perRow + t.x];\n"
	     << "}\n"
	     << "#define texelFetch(s, uv, lod) p3d_Fetch(s, uv)\n"
	     // The UBO the real shader reads, as plain accessors.
	     << "#define uDDGIOrigin U[0]\n"
	     << "#define uDDGISpacing U[1]\n"
	     << "#define uDDGICounts U[2]\n"
	     << "#define uDDGIParams U[3]\n"
	     << "#define uDDGIRadianceParams U[4]\n"
	     << "#define uAmbientParams U[5]\n"
	     // Sampler handles become ids; texture() becomes a nearest fetch.
	     << "const int uDDGIVisibility = 1;\n"
	     << "const int uDDGIRadiance = 2;\n"
	     << "vec4 p3d_Sample(int which, vec2 uv) {\n"
	     << "    vec2 size = (which == 1) ? U[6].xy : U[6].zw;\n"
	     << "    ivec2 t = ivec2(clamp(floor(uv * size), vec2(0.0), size - vec2(1.0)));\n"
	     << "    int o = t.y * int(size.x) + t.x;\n"
	     << "    return (which == 1) ? visTex[o] : radTex[o];\n"
	     << "}\n"
	     << "#define texture(s, uv) p3d_Sample(s, uv)\n"
	     << octEncode << "\n" << probeData << "\n" << probePos << "\n" << probeActive << "\n"
	     << probeUV << "\n" << radianceUV << "\n" << sampleRad << "\n"
	     << "void main() {\n"
	     << "    uint i = gl_GlobalInvocationID.x;\n"
	     << "    vec3 p = inputs[i*3+0].xyz;\n"
	     << "    vec3 n = normalize(inputs[i*3+1].xyz);\n"
	     << "    vec4 r = inputs[i*3+2];\n"
	     << "    results[i] = vec4(SampleDDGIRadiance(p, n, normalize(r.xyz), r.w), 0.0);\n"
	     << "}\n";

	const DeviceHandle stage = device.CreateShaderStage(ShaderType::ComputeShader);
	{
		const std::string src = device.BuildShaderSource(std::string(), body.str());
		std::string log;
		const bool ok = device.CompileShaderStage(stage, src, log);
		check(ok, "the shipped SampleDDGIRadiance compiles", log);
		if (!ok) return 1;
	}
	const DeviceHandle program = device.CreateProgram();
	device.AttachShaderStage(program, stage);
	{ std::string log; check(device.LinkProgram(program, log), "LinkProgram", log); }
	const DeviceHandle pipeline = device.CreateComputePipeline(program);
	check(pipeline != 0, "CreateComputePipeline");
	if (pipeline == 0) return 1;

	// ---- upload ------------------------------------------------------------
	std::vector<f32> U(7 * 4, 0.f);
	U[0]=vol.origin.x; U[1]=vol.origin.y; U[2]=vol.origin.z; U[3]=(f32)vol.GetIrradianceAtlas().GetProbesPerRow();
	U[4]=vol.spacing.x; U[5]=vol.spacing.y; U[6]=vol.spacing.z;
	U[8]=(f32)vol.counts[0]; U[9]=(f32)vol.counts[1]; U[10]=(f32)vol.counts[2];
	U[12]=(f32)vol.GetIrradianceAtlas().GetResolution(); U[13]=(f32)vis.GetResolution();
	U[15]=(f32)vol.GetIrradianceAtlas().GetProbesPerRow();
	U[16]=(f32)rad.GetResolution(); U[17]=(f32)rad.GetProbesPerRow();
	U[18]=(f32)vol.GetRadianceLevels(); U[19]=DDGIVolume::MinRoughness();
	U[11]=1.f;                                    // counts.w: probe offsets are valid
	U[20]=3.f;                                    // ambient mode 3 = DDGI
	U[24]=(f32)vis.GetWidth(); U[25]=(f32)vis.GetHeight();
	U[26]=(f32)rad.GetWidth(); U[27]=(f32)rad.GetHeight();

	// Both atlases as vec4 per texel, which is what the fetch above
	// indexes; the CPU ones are 2- and 4-channel.
	std::vector<f32> visData((size_t)vis.GetWidth()*vis.GetHeight()*4, 0.f);
	for (size_t t = 0; t*2+1 < vis.GetData().size(); t++)
	{ visData[t*4+0]=vis.GetData()[t*2+0]; visData[t*4+1]=vis.GetData()[t*2+1]; }
	std::vector<f32> radData = rad.GetData();

	const DeviceHandle uBuf = device.CreateStorageBuffer((uint32)(U.size()*sizeof(f32)), 0, U.data());
	const DeviceHandle vBuf = device.CreateStorageBuffer((uint32)(visData.size()*sizeof(f32)), 1, visData.data());
	const DeviceHandle rBuf = device.CreateStorageBuffer((uint32)(radData.size()*sizeof(f32)), 2, radData.data());
	const DeviceHandle iBuf = device.CreateStorageBuffer((uint32)(inputs.size()*sizeof(f32)), 3, inputs.data());
	const DeviceHandle oBuf = device.CreateStorageBuffer(kSamples*4*(uint32)sizeof(f32), 4, NULL);
	std::vector<f32> probeBuf(vol.ProbeCount()*4, 0.f);
	for (uint32 i = 0; i < vol.ProbeCount(); i++)
	{
		const Vec4 &d = vol.GetProbeData()[i];
		probeBuf[i*4+0]=d.x; probeBuf[i*4+1]=d.y; probeBuf[i*4+2]=d.z; probeBuf[i*4+3]=d.w;
	}
	const DeviceHandle pBuf = device.CreateStorageBuffer((uint32)(probeBuf.size()*sizeof(f32)), 5, probeBuf.data());

	device.BindComputePipeline(0, pipeline);
	device.BindStorageBuffer(0, uBuf, 0);
	device.BindStorageBuffer(0, vBuf, 1);
	device.BindStorageBuffer(0, rBuf, 2);
	device.BindStorageBuffer(0, iBuf, 3);
	device.BindStorageBuffer(0, oBuf, 4);
	device.BindStorageBuffer(0, pBuf, 5);
	device.Dispatch(0, 1, 1, 1);
	device.ComputeBarrier(0, ComputeBarrierBit::HostRead);

	std::vector<f32> gpu(kSamples*4, 0.f);
	device.ReadStorageBuffer(oBuf, 0, (uint32)(gpu.size()*sizeof(f32)), gpu.data());

	// ---- compare -------------------------------------------------------------
	f32 worst = 0.f, magnitude = 0.f; uint32 worstIndex = 0, nonZero = 0;
	for (uint32 i = 0; i < kSamples; i++)
	{
		const Vec3 cpu = vol.SampleRadiance(posCPU[i], nrmCPU[i], refCPU[i], rghCPU[i]);
		if (cpu.magnitude() > 1e-4f) nonZero++;
		const f32 d[3] = { fabsf(gpu[i*4+0]-cpu.x), fabsf(gpu[i*4+1]-cpu.y), fabsf(gpu[i*4+2]-cpu.z) };
		for (uint32 c = 0; c < 3; c++)
		{
			magnitude += fabsf(gpu[i*4+c]);
			if (d[c] > worst) { worst = d[c]; worstIndex = i; }
		}
	}
	{
		const Vec3 cpu = vol.SampleRadiance(posCPU[worstIndex], nrmCPU[worstIndex],
			refCPU[worstIndex], rghCPU[worstIndex]);
		char buf[256];
		snprintf(buf, sizeof(buf), "worst |GPU-CPU| = %.6f at %u (CPU %.4f,%.4f,%.4f  GPU %.4f,%.4f,%.4f)",
			worst, worstIndex, cpu.x, cpu.y, cpu.z,
			gpu[worstIndex*4+0], gpu[worstIndex*4+1], gpu[worstIndex*4+2]);
		// Float rounding only. Both sides walk the same eight probes,
		// blend the same two levels and address the same texels; a
		// stride or stacking error moves a whole texel and lands orders
		// of magnitude above this.
		check(worst < 2e-3f, "the shipped GLSL agrees with DDGIVolume::SampleRadiance", buf);
	}
	check(nonZero > kSamples/2, "most sample points actually saw a probe",
		std::to_string(nonZero) + "/" + std::to_string(kSamples));
	check(magnitude > 1.f, "the GPU produced non-zero radiance",
		"sum = " + std::to_string(magnitude));

	device.DestroyStorageBuffer(pBuf);
	device.DestroyStorageBuffer(oBuf); device.DestroyStorageBuffer(iBuf);
	device.DestroyStorageBuffer(rBuf); device.DestroyStorageBuffer(vBuf);
	device.DestroyStorageBuffer(uBuf);
	device.DestroyComputePipeline(pipeline);
	device.DeleteProgram(program);
	device.DeleteShaderStage(stage);

	printf("\n%s  ddgi_specular_shader_parity: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
