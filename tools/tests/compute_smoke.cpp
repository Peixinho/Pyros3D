// Compute shaders, end to end, through the engine's own device seam.
//
// Proves four separate things, and it matters that they are separate,
// because each one fails differently and a single "it didn't work" would
// not tell them apart:
//
//   1. SupportsCompute() tells the truth about this driver.
//   2. A compute stage compiles and links through IRenderDevice's normal
//      CreateShaderStage/CompileShaderStage/LinkProgram path - the same
//      path graphics shaders take, which is the whole point of adding
//      ShaderType::ComputeShader rather than a parallel API.
//   3. A dispatch actually runs on the GPU and writes an SSBO.
//   4. The writes are visible to a readback afterwards.
//
// The verification (RunComputeVerification) is backend-agnostic and talks
// only to IRenderDevice. Only the setup differs, and it differs a lot:
//
//   GL    needs a real 4.3+ context, so it makes a bare hidden SDL2 window.
//         Deliberately not the engine's Context classes - those live in
//         examples/WindowManagers and drag in ImGui's SDL2 backend.
//   Metal needs nothing at all. MetalRenderDevice's constructor creates its
//         own MTLDevice and command queue; a CAMetalLayer is only required
//         to draw, and this never draws. So there is no window here.
//   Vulkan needs nothing either, via InitializeHeadless(). It used to
//         need a window it never drew to, because a VkDevice only came
//         into being inside InitializeSwapchain(); this test was the
//         thing that made that limitation concrete, and it is fixed.
//
// Build for GL (Linux CI, or any GL 4.3+ machine):
//
//   cc -c -DGL45 -I include -I include/Pyros3D/Ext/gl45 \
//       src/Pyros3D/Ext/gl45/glad.c -o /tmp/glad45.o
//   c++ -std=c++17 -DGL45 -DCOMPUTE_SMOKE_GL \
//       -I include -I include/Pyros3D/Ext/gl45 \
//       $(pkg-config --cflags freetype2) $(pkg-config --cflags sdl2) \
//       tools/tests/compute_smoke.cpp /tmp/glad45.o -o /tmp/compute_smoke \
//       -L build_gl45 -lPyrosEngine $(pkg-config --libs sdl2) \
//       -Wl,-rpath,$PWD/build_gl45
//
// Build for Metal (macOS):
//
//   c++ -std=c++17 -DMETAL_BACKEND -DCOMPUTE_SMOKE_METAL \
//       -I include $(pkg-config --cflags freetype2) \
//       tools/tests/compute_smoke.cpp -o /tmp/compute_smoke_metal \
//       -L build_metal -lPyrosEngine -framework Foundation -framework Metal \
//       -Wl,-rpath,$PWD/build_metal
//
// Build for Vulkan (Linux; needs a Vulkan runtime - lavapipe suffices):
//
//   c++ -std=c++17 -DVULKAN_BACKEND -DCOMPUTE_SMOKE_VULKAN \
//       -I include -I <vulkan-headers> -I <vma> -I <volk> \
//       $(pkg-config --cflags freetype2) \
//       tools/tests/compute_smoke.cpp -o /tmp/compute_smoke_vk \
//       -L build -lPyrosEngine -Wl,-rpath,$PWD/build
//
// (VMA and volk are FetchContent'd, so their headers live in the build
// tree rather than on any install prefix - `find . -name vk_mem_alloc.h`.)
// On macOS the loader needs VK_ICD_FILENAMES pointing at MoltenVK's ICD
// manifest, since volk dlopen()s it.
//
// pkg-config rather than sdl2-config: the latter only emits the inner
// .../include/SDL2 directory, which makes the <SDL2/SDL.h> spelling used
// here (and by every WindowManager in this repo) fail to resolve.
//
// Exit code is 0 for PASS and for SKIP, 1 for FAIL - so CI can run it
// unconditionally and only a real regression breaks the build.

#if !defined(COMPUTE_SMOKE_GL) && !defined(COMPUTE_SMOKE_METAL) && !defined(COMPUTE_SMOKE_VULKAN)
#error "Define COMPUTE_SMOKE_GL, COMPUTE_SMOKE_METAL or COMPUTE_SMOKE_VULKAN - see the build lines above."
#endif

#if defined(COMPUTE_SMOKE_GL)
#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <SDL2/SDL.h>
#endif

#if defined(COMPUTE_SMOKE_METAL)
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#endif

#if defined(COMPUTE_SMOKE_VULKAN)
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#endif

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;

static void check(bool cond, const char *what, const std::string &extra = std::string())
{
	printf("%s  %s%s%s\n", cond ? "PASS" : "FAIL", what,
		extra.empty() ? "" : " - ", extra.c_str());
	if (!cond) failures++;
}

// The payload. Three things worth noting:
//
//   - `values[i] = values[i] * 2 + i` rather than a constant fill. A
//     constant would also pass if the dispatch never ran and the buffer
//     happened to be zeroed to the expected value; a function of both the
//     old contents AND the invocation id can only be produced by the
//     shader actually running, over the right index, on what we uploaded.
//   - the element count arrives in a SECOND storage buffer rather than a
//     uniform. A loose `uniform int` would work on GL, but on Metal it
//     goes through AutoFixForVulkan's synthesized UBO and the engine's
//     per-backend uniform plumbing - a whole extra mechanism this test
//     would then be measuring instead of compute. Two SSBOs also prove
//     multi-buffer binding, which is what real compute work needs anyway.
//   - the bounds check is real, not decoration. See kElementCount.
static const char *kComputeBody =
	"layout(local_size_x = 64) in;\n"
	"layout(std430, binding = 0) buffer Values { uint values[]; };\n"
	"layout(std430, binding = 1) buffer Params { uint count; };\n"
	"void main() {\n"
	"    uint i = gl_GlobalInvocationID.x;\n"
	"    if (i >= count) return;\n"
	"    values[i] = values[i] * 2u + i;\n"
	"}\n";

// Deliberately NOT a multiple of the local size (1000 = 15.625 groups), so
// the last work group really does run invocations past the end of the
// buffer and the shader's bounds check is exercised. A round 1024 would
// make the dispatch exact and quietly test nothing.
static const uint32 kElementCount = 1000;
static const uint32 kLocalSize = 64;

// Everything below talks only to IRenderDevice. Returns true if the device
// reported compute support and the checks ran; false means "skipped".
static bool RunComputeVerification(IRenderDevice &device)
{
	if (!device.SupportsCompute())
		return false;

	check(true, "SupportsCompute()");

	const uint32 maxInvocations = device.GetMaxComputeWorkGroupInvocations();
	check(maxInvocations >= kLocalSize,
		"local_size_x fits the device's max threads per work group",
		"limit=" + std::to_string(maxInvocations));
	check(device.GetMaxComputeWorkGroupCount(0) > 0,
		"max work group count is queryable");

	// ---- compile + link through the engine's own shader path -----------

	const DeviceHandle stage = device.CreateShaderStage(ShaderType::ComputeShader);
	check(stage != 0, "CreateShaderStage(ComputeShader)");
	if (stage == 0) return true;

	{
		// BuildShaderSource, not a hand-written "#version 450" - the point
		// is that the engine's normal preamble assembly works for a
		// compute stage too.
		const std::string source = device.BuildShaderSource(std::string(), std::string(kComputeBody));
		std::string errorLog;
		const bool compiled = device.CompileShaderStage(stage, source, errorLog);
		check(compiled, "CompileShaderStage(compute)", errorLog);
		if (!compiled) { device.DeleteShaderStage(stage); return true; }
	}

	const DeviceHandle program = device.CreateProgram();
	device.AttachShaderStage(program, stage);
	{
		std::string linkLog;
		const bool linked = device.LinkProgram(program, linkLog);
		check(linked, "LinkProgram(compute-only program)", linkLog);
		if (!linked) { device.DeleteShaderStage(stage); return true; }
	}

	const DeviceHandle pipeline = device.CreateComputePipeline(program);
	check(pipeline != 0, "CreateComputePipeline");
	if (pipeline == 0) { device.DeleteShaderStage(stage); return true; }

	// ---- upload, dispatch ----------------------------------------------

	std::vector<uint32> input(kElementCount);
	for (uint32 i = 0; i < kElementCount; i++)
		input[i] = i * 3 + 7;

	const uint32 sizeBytes = kElementCount * (uint32)sizeof(uint32);
	const DeviceHandle ssbo = device.CreateStorageBuffer(sizeBytes, 0, input.data());
	check(ssbo != 0, "CreateStorageBuffer(values)");

	const uint32 countValue = kElementCount;
	const DeviceHandle paramsBuffer = device.CreateStorageBuffer((uint32)sizeof(uint32), 1, &countValue);
	check(paramsBuffer != 0, "CreateStorageBuffer(params)");

	if (ssbo != 0 && paramsBuffer != 0)
	{
		device.BindComputePipeline(0, pipeline);
		device.BindStorageBuffer(0, ssbo, 0);
		device.BindStorageBuffer(0, paramsBuffer, 1);

		// Group count, not thread count - the division is the whole reason
		// Dispatch()'s parameters are named `groups`.
		const uint32 groups = (kElementCount + kLocalSize - 1) / kLocalSize;
		device.Dispatch(0, groups, 1, 1);

		// ---- barrier, read back, verify --------------------------------

		// Without this the readback may legally observe the pre-dispatch
		// contents. It would then fail with "wrong values", pointing at
		// the shader, when the real bug is the missing barrier - which is
		// exactly the confusion this test exists to stop someone else
		// having.
		device.ComputeBarrier(0, ComputeBarrierBit::HostRead);

		std::vector<uint32> output(kElementCount, 0xFFFFFFFFu);
		device.ReadStorageBuffer(ssbo, 0, sizeBytes, output.data());

		uint32 mismatches = 0;
		uint32 firstBadIndex = 0;
		for (uint32 i = 0; i < kElementCount; i++)
		{
			const uint32 expected = input[i] * 2u + i;
			if (output[i] != expected)
			{
				if (mismatches == 0) firstBadIndex = i;
				mismatches++;
			}
		}
		if (mismatches == 0)
		{
			check(true, "dispatch wrote every element correctly",
				std::to_string(kElementCount) + " elements, "
				+ std::to_string(groups) + " work groups");
		}
		else
		{
			const uint32 i = firstBadIndex;
			check(false, "dispatch wrote every element correctly",
				std::to_string(mismatches) + " wrong; first at ["
				+ std::to_string(i) + "] expected "
				+ std::to_string(input[i] * 2u + i) + " got "
				+ std::to_string(output[i]));
		}

		// Out-of-range access must be refused, not passed to the backend -
		// see IRenderDevice::StorageRangeIsValid(). Checked by asking for
		// one element past the end and requiring the sentinel to be zeroed
		// rather than filled with whatever was adjacent.
		uint32 sentinel = 0xABCDEF01u;
		device.ReadStorageBuffer(ssbo, sizeBytes, (uint32)sizeof(uint32), &sentinel);
		check(sentinel == 0,
			"out-of-range ReadStorageBuffer is rejected and zeroes the output",
			"got " + std::to_string(sentinel));
	}

	if (paramsBuffer != 0) device.DestroyStorageBuffer(paramsBuffer);
	if (ssbo != 0) device.DestroyStorageBuffer(ssbo);
	device.DestroyComputePipeline(pipeline);
	device.DeleteProgram(program);
	device.DeleteShaderStage(stage);
	return true;
}

static int Report()
{
	printf("\n%s  compute_smoke: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
	return failures == 0 ? 0 : 1;
}

// =========================================================================
// Backend setup
// =========================================================================

#if defined(COMPUTE_SMOKE_METAL)

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	printf("      backend     Metal\n");

	// No window, no layer, no SDL. MetalRenderDevice's constructor does
	// MTLCreateSystemDefaultDevice() + newCommandQueue, which is the whole
	// of what a dispatch needs; BindToLayer() is only required to draw.
	MetalRenderDevice device;

	if (!RunComputeVerification(device))
	{
		printf("SKIP  SupportsCompute() is false - no Metal GPU, or the engine was\n");
		printf("      built without METAL_SHADER_TOOLING (shaderc/spirv-cross-msl).\n");
		return 0;
	}
	return Report();
}

#elif defined(COMPUTE_SMOKE_VULKAN)

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	printf("      backend     Vulkan (headless)\n");

	// No window, no surface, no SDL. InitializeHeadless() builds the
	// VkDevice, queue, allocator, command pool and fences and stops short
	// of the swapchain - see its comment. This is also the shape a bake
	// would take.
	//
	// No instance extensions requested for the same reason: the surface
	// and platform-surface extensions exist only to present.
	VulkanRenderDevice device;

	if (device.GetInstance() == VK_NULL_HANDLE)
	{
		printf("SKIP  no Vulkan instance - no loader or no ICD on this machine\n");
		return 0;
	}
	if (!device.InitializeHeadless())
	{
		printf("SKIP  InitializeHeadless failed - no usable Vulkan device\n");
		return 0;
	}
	if (!RunComputeVerification(device))
	{
		printf("SKIP  SupportsCompute() is false - the graphics queue family does not\n");
		printf("      advertise VK_QUEUE_COMPUTE_BIT, or SPIRV_TOOLING is off.\n");
		return 0;
	}
	return Report();
}

#elif defined(COMPUTE_SMOKE_GL)

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		printf("FAIL  SDL_Init - %s\n", SDL_GetError());
		return 1;
	}

	// Version fallback chain, highest first. 4.3 is in the list because it
	// is the real floor for compute - GL45 is what this engine's glad
	// generation targets, but a driver that tops out at 4.3 or 4.4 can
	// still run every call this test makes, and refusing it would report
	// "no compute" on a machine that has it.
	//
	// The last entry requests nothing at all and takes the driver's
	// default. That is the entry that makes SupportsCompute() the thing
	// deciding the outcome rather than SDL's context creation: on macOS it
	// yields a 2.1 or 4.1 context, the null-pointer check in
	// SupportsCompute() sees no glDispatchCompute, and the test reports
	// the capability gate working rather than just "couldn't make a
	// window". Which is the half of the gate worth testing there, since it
	// is the only half a Mac can reach.
	static const struct { int major, minor; } kVersions[] = {
		{ 4, 5 }, { 4, 4 }, { 4, 3 }, { 0, 0 }
	};

	SDL_Window *window = NULL;
	SDL_GLContext glContext = NULL;

	for (size_t v = 0; v < sizeof(kVersions) / sizeof(kVersions[0]) && glContext == NULL; v++)
	{
		if (kVersions[v].major != 0)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kVersions[v].major);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kVersions[v].minor);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		}
		else
		{
			SDL_GL_ResetAttributes();
		}
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		// Recreated per attempt, not reused: a window that has already had
		// a failed SDL_GL_CreateContext against it keeps the pixel format
		// chosen for that attempt on some platforms, and the next request
		// then fails for that reason rather than on its own merits.
		//
		// Hidden, because nothing is ever drawn and a visible window under
		// xvfb is one more thing that can fail unrelated to compute.
		window = SDL_CreateWindow("compute_smoke",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64, 64,
			SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
		if (window == NULL)
			continue;

		glContext = SDL_GL_CreateContext(window);
		if (glContext == NULL)
		{
			SDL_DestroyWindow(window);
			window = NULL;
		}
	}

	if (glContext == NULL)
	{
		printf("FAIL  could not create any GL context - %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	if (gladLoadGL() == 0)
	{
		printf("FAIL  gladLoadGL could not resolve core GL entry points\n");
		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	printf("      backend     OpenGL\n");
	printf("      GL_VERSION  %s\n", (const char*)glGetString(GL_VERSION));
	printf("      GL_RENDERER %s\n", (const char*)glGetString(GL_RENDERER));

	int result;
	{
		// Scoped so the device releases its GL objects while the context
		// is still current - see the render-device teardown ordering notes.
		GLRenderDevice device;
		if (!RunComputeVerification(device))
		{
			printf("SKIP  SupportsCompute() is false on this driver - compute needs GL 4.3+\n");
			printf("      (expected on macOS, which caps OpenGL at 4.1)\n");
			result = 0;
		}
		else
		{
			result = Report();
		}
	}

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return result;
}

#endif
