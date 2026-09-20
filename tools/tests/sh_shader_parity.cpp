// The SH irradiance formula exists twice - once in C++
// (SphericalHarmonicsL2::Irradiance) and once in GLSL
// (PyrosShader.glsl's SHIrradiance). Both comments say "if one changes
// the other must". This is the thing that makes that true.
//
// It does not reimplement the shader. It READS SHIrradiance straight out
// of resources/shaders/PyrosShader.glsl, wraps it in a compute shader,
// dispatches it over a set of normals, and compares the GPU's answers
// against the CPU's for an environment projected by the real projector.
// A copy of the formula pasted in here would drift exactly as easily as
// the two originals, which would defeat the point.
//
// Why this matters: a divergence produces ambient light that is subtly
// the wrong colour, or shifts when a material moves between the forward
// and deferred paths. Nothing errors, nothing looks obviously broken, and
// there is no way to bisect it from a screenshot.
//
//   c++ -std=c++17 -DMETAL_BACKEND -DPARITY_METAL -I include \
//       $(pkg-config --cflags freetype2) \
//       tools/tests/sh_shader_parity.cpp -o /tmp/sh_shader_parity \
//       -L build_metal -lPyrosEngine -framework Foundation -framework Metal \
//       -Wl,-rpath,$PWD/build_metal
//   /tmp/sh_shader_parity            # run from the repo root
//
// Exits 0 for PASS and for SKIP (no compute on this machine), 1 for FAIL.
#if defined(PARITY_METAL)
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#elif defined(PARITY_VULKAN)
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#else
#error "Define PARITY_METAL or PARITY_VULKAN"
#endif

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/GI/SphericalHarmonics.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;

static void check(bool cond, const std::string &what, const std::string &extra = std::string())
{
	printf("%s  %s%s%s\n", cond ? "PASS" : "FAIL", what.c_str(),
		extra.empty() ? "" : " - ", extra.c_str());
	if (!cond) failures++;
}

// Pulls one function out of a GLSL file by name, from its signature to
// the brace that closes it. Brace-counting rather than a regex because
// the body contains braces of its own.
static bool ExtractGLSLFunction(const std::string &source, const std::string &signature, std::string &out)
{
	const size_t start = source.find(signature);
	if (start == std::string::npos)
		return false;
	const size_t open = source.find('{', start);
	if (open == std::string::npos)
		return false;
	int depth = 0;
	for (size_t i = open; i < source.size(); i++)
	{
		if (source[i] == '{') depth++;
		else if (source[i] == '}')
		{
			depth--;
			if (depth == 0)
			{
				out = source.substr(start, i - start + 1);
				return true;
			}
		}
	}
	return false;
}

static const uint32 kNormalCount = 64; // one work group exactly

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	// ---- the shipped GLSL, verbatim ------------------------------------
	std::string shaderFile;
	{
		std::ifstream f("resources/shaders/PyrosShader.glsl");
		if (!f.good())
		{
			printf("FAIL  cannot open resources/shaders/PyrosShader.glsl - run from the repo root\n");
			return 1;
		}
		std::stringstream ss; ss << f.rdbuf(); shaderFile = ss.str();
	}

	std::string shIrradiance;
	if (!ExtractGLSLFunction(shaderFile, "vec3 SHIrradiance(vec3 n)", shIrradiance))
	{
		printf("FAIL  could not find SHIrradiance() in PyrosShader.glsl - was it renamed?\n");
		return 1;
	}
	check(shIrradiance.find("uAmbientSH[0]") != std::string::npos,
		"extracted SHIrradiance references the coefficient array");

	// ---- device --------------------------------------------------------
#if defined(PARITY_METAL)
	MetalRenderDevice device;
	printf("      backend     Metal\n");
#else
	VulkanRenderDevice device;
	if (device.GetInstance() == VK_NULL_HANDLE || !device.InitializeHeadless())
	{
		printf("SKIP  no usable Vulkan device\n");
		return 0;
	}
	printf("      backend     Vulkan (headless)\n");
#endif

	if (!device.SupportsCompute())
	{
		printf("SKIP  SupportsCompute() is false on this backend\n");
		return 0;
	}

	// ---- a real environment, projected by the real projector -----------
	//
	// Asymmetric on purpose: a symmetric one leaves most of bands 1 and 2
	// at zero, and a sign error in any of those terms would go unnoticed.
	SphericalHarmonicsL2 sh;
	{
		const uint32 size = 16;
		std::vector<std::vector<f32> > storage(6, std::vector<f32>(size * size * 3, 0.f));
		for (uint32 face = 0; face < 6; face++)
		{
			for (uint32 i = 0; i < size * size; i++)
			{
				storage[face][i * 3 + 0] = 0.1f + 0.30f * (f32)face;
				storage[face][i * 3 + 1] = 0.7f - 0.11f * (f32)face;
				storage[face][i * 3 + 2] = 0.2f + 0.05f * (f32)(i % 7);
			}
		}
		std::vector<CubemapFacePixels> faces;
		for (uint32 f = 0; f < 6; f++)
			faces.push_back(CubemapFacePixels(storage[f].data(), size));
		check(ProjectCubemapToSH(faces, sh), "projected the test environment");
	}

	// ---- normals to compare over ---------------------------------------
	std::vector<f32> normals(kNormalCount * 4, 0.f);
	std::vector<Vec3> normalsCPU(kNormalCount);
	for (uint32 i = 0; i < kNormalCount; i++)
	{
		// Fibonacci sphere: evenly spread, and none of them axis-aligned,
		// so no term of the formula is accidentally multiplied by zero.
		const f32 k = (f32)i + 0.5f;
		const f32 phi = acosf(1.f - 2.f * k / (f32)kNormalCount);
		const f32 theta = 3.14159265f * (1.f + sqrtf(5.f)) * k;
		Vec3 n(cosf(theta) * sinf(phi), sinf(theta) * sinf(phi), cosf(phi));
		n.normalizeSelf();
		normalsCPU[i] = n;
		normals[i * 4 + 0] = n.x; normals[i * 4 + 1] = n.y; normals[i * 4 + 2] = n.z;
	}

	// ---- the compute shader, built around the extracted function -------
	std::ostringstream body;
	body << "layout(local_size_x = 64) in;\n"
	     << "layout(std430, binding = 0) buffer SHBuf { vec4 uAmbientSH[9]; };\n"
	     << "layout(std430, binding = 1) buffer NBuf { vec4 normals[]; };\n"
	     << "layout(std430, binding = 2) buffer OBuf { vec4 results[]; };\n"
	     << shIrradiance << "\n"
	     << "void main() {\n"
	     << "    uint i = gl_GlobalInvocationID.x;\n"
	     << "    results[i] = vec4(SHIrradiance(normalize(normals[i].xyz)), 0.0);\n"
	     << "}\n";

	const DeviceHandle stage = device.CreateShaderStage(ShaderType::ComputeShader);
	{
		const std::string src = device.BuildShaderSource(std::string(), body.str());
		std::string log;
		const bool ok = device.CompileShaderStage(stage, src, log);
		check(ok, "the shipped SHIrradiance compiles as a compute shader", log);
		if (!ok) return 1;
	}
	const DeviceHandle program = device.CreateProgram();
	device.AttachShaderStage(program, stage);
	{
		std::string log;
		check(device.LinkProgram(program, log), "LinkProgram", log);
	}
	const DeviceHandle pipeline = device.CreateComputePipeline(program);
	check(pipeline != 0, "CreateComputePipeline");
	if (pipeline == 0) return 1;

	// ---- upload, dispatch ----------------------------------------------
	std::vector<f32> shData(9 * 4, 0.f);
	for (uint32 i = 0; i < 9; i++)
	{
		shData[i * 4 + 0] = sh.coefficients[i].x;
		shData[i * 4 + 1] = sh.coefficients[i].y;
		shData[i * 4 + 2] = sh.coefficients[i].z;
	}

	const DeviceHandle shBuf = device.CreateStorageBuffer((uint32)(shData.size() * sizeof(f32)), 0, shData.data());
	const DeviceHandle nBuf  = device.CreateStorageBuffer((uint32)(normals.size() * sizeof(f32)), 1, normals.data());
	const DeviceHandle oBuf  = device.CreateStorageBuffer(kNormalCount * 4 * (uint32)sizeof(f32), 2, NULL);

	device.BindComputePipeline(0, pipeline);
	device.BindStorageBuffer(0, shBuf, 0);
	device.BindStorageBuffer(0, nBuf, 1);
	device.BindStorageBuffer(0, oBuf, 2);
	device.Dispatch(0, 1, 1, 1);
	device.ComputeBarrier(0, ComputeBarrierBit::HostRead);

	std::vector<f32> gpu(kNormalCount * 4, 0.f);
	device.ReadStorageBuffer(oBuf, 0, (uint32)(gpu.size() * sizeof(f32)), gpu.data());

	// ---- compare --------------------------------------------------------
	f32 worst = 0.f;
	uint32 worstIndex = 0;
	for (uint32 i = 0; i < kNormalCount; i++)
	{
		const Vec3 cpu = sh.Irradiance(normalsCPU[i]);
		const f32 d[3] = {
			fabsf(gpu[i * 4 + 0] - cpu.x),
			fabsf(gpu[i * 4 + 1] - cpu.y),
			fabsf(gpu[i * 4 + 2] - cpu.z)
		};
		for (uint32 c = 0; c < 3; c++)
		{
			if (d[c] > worst) { worst = d[c]; worstIndex = i; }
		}
	}
	{
		const Vec3 cpu = sh.Irradiance(normalsCPU[worstIndex]);
		char buf[256];
		snprintf(buf, sizeof(buf), "worst |GPU-CPU| = %.7f at normal %u (CPU %.5f,%.5f,%.5f  GPU %.5f,%.5f,%.5f)",
			worst, worstIndex, cpu.x, cpu.y, cpu.z,
			gpu[worstIndex*4+0], gpu[worstIndex*4+1], gpu[worstIndex*4+2]);
		// Tolerance is float rounding only, not approximation slack: both
		// sides evaluate the identical closed form on the identical
		// coefficients. Anything above this is a real divergence, not
		// precision.
		check(worst < 1e-4f, "the shipped GLSL agrees with SphericalHarmonicsL2::Irradiance", buf);
	}

	// A guard against the comparison being vacuous: if every value were
	// zero the check above would pass perfectly and mean nothing.
	f32 magnitude = 0.f;
	for (uint32 i = 0; i < kNormalCount; i++)
		magnitude += fabsf(gpu[i * 4 + 0]) + fabsf(gpu[i * 4 + 1]) + fabsf(gpu[i * 4 + 2]);
	check(magnitude > 1.f, "the GPU actually produced non-zero irradiance",
		"sum |E| = " + std::to_string(magnitude));

	device.DestroyStorageBuffer(oBuf);
	device.DestroyStorageBuffer(nBuf);
	device.DestroyStorageBuffer(shBuf);
	device.DestroyComputePipeline(pipeline);
	device.DeleteProgram(program);
	device.DeleteShaderStage(stage);

	printf("\n%s  sh_shader_parity: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
	return failures == 0 ? 0 : 1;
}
