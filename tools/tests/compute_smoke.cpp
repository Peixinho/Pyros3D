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
//   4. ComputeBarrier(HostRead) makes those writes visible to a readback.
//
// Deliberately does NOT use the engine's Context classes: those live in
// examples/WindowManagers and drag in ImGui's SDL2 backend. A compute test
// needs a GL context and nothing else, so it makes a bare hidden-window one
// itself. That also keeps it runnable headless, which is how CI runs it.
//
// macOS cannot run this: Apple caps OpenGL at 4.1 and compute is 4.3, so
// SupportsCompute() is false there and the test SKIPs rather than fails.
// That is the expected local result, not a problem - see the Linux CI job,
// where Mesa's llvmpipe does implement GL 4.5 compute in software.
//
//   c++ -std=c++17 -DGL45 -I include -I include/Pyros3D/Ext/gl45 \
//       -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       $(pkg-config --cflags sdl2) \
//       tools/tests/compute_smoke.cpp src/Pyros3D/Ext/gl45/glad.c \
//       -o /tmp/compute_smoke \
//       -L build_gl45 -lPyrosEngine $(pkg-config --libs sdl2) \
//       -Wl,-rpath,$PWD/build_gl45
//
// pkg-config rather than sdl2-config: the latter only emits the inner
// .../include/SDL2 directory, which makes the <SDL2/SDL.h> spelling used
// here (and by every WindowManager in this repo) fail to resolve.
//   /tmp/compute_smoke
//
// Exit code is 0 for PASS and for SKIP, 1 for FAIL - so CI can run it
// unconditionally and only a real regression breaks the build.
#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>

#include <SDL2/SDL.h>

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

// The payload. Two things worth noting:
//
//   - `data[i] = data[i] * 2 + i` rather than a constant fill. A constant
//     would also pass if the dispatch never ran and the buffer happened to
//     be zeroed to the expected value; a function of both the old contents
//     AND the invocation id can only be produced by the shader actually
//     running, over the right index, on the data we uploaded.
//   - the bounds check. The dispatch rounds the element count up to a whole
//     number of work groups, so the last group has invocations past the end
//     of the buffer. Writing from those is out-of-bounds; GL's behaviour for
//     that is "undefined", which in practice means it works until it
//     silently corrupts something else.
static const char *kComputeBody =
	"layout(local_size_x = 64) in;\n"
	"layout(std430, binding = 0) buffer Values { uint values[]; };\n"
	// int, not uint, and that is not cosmetic: the engine's only integer
	// uniform path is SendUniformInt -> glUniform1iv, and GL raises
	// GL_INVALID_OPERATION for glUniform1i against a uint uniform. The
	// uniform would then keep its default of 0, every invocation would
	// take the early return, and the buffer would come back unmodified -
	// which reads exactly like "the dispatch never ran".
	"uniform int uCount;\n"
	"void main() {\n"
	"    uint i = gl_GlobalInvocationID.x;\n"
	"    if (int(i) >= uCount) return;\n"
	"    values[i] = values[i] * 2u + i;\n"
	"}\n";

static const uint32 kElementCount = 1024;
static const uint32 kLocalSize = 64;

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
	// window". Which is the half of the gate worth testing locally, since
	// it is the only half a Mac can reach.
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

	printf("      GL_VERSION  %s\n", (const char*)glGetString(GL_VERSION));
	printf("      GL_RENDERER %s\n", (const char*)glGetString(GL_RENDERER));

	GLRenderDevice device;

	if (!device.SupportsCompute())
	{
		printf("SKIP  SupportsCompute() is false on this driver - compute needs GL 4.3+\n");
		printf("      (expected on macOS, which caps OpenGL at 4.1)\n");
		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 0;
	}

	check(true, "SupportsCompute()");

	// The limit that actually constrains a dispatch - see
	// IRenderDevice::GetMaxComputeWorkGroupInvocations().
	const uint32 maxInvocations = device.GetMaxComputeWorkGroupInvocations();
	check(maxInvocations >= kLocalSize,
		"local_size_x fits GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS",
		"limit=" + std::to_string(maxInvocations));
	check(device.GetMaxComputeWorkGroupCount(0) > 0,
		"GL_MAX_COMPUTE_WORK_GROUP_COUNT[0] is queryable");

	// ---- 2. compile + link through the engine's own shader path --------

	const DeviceHandle stage = device.CreateShaderStage(ShaderType::ComputeShader);
	check(stage != 0, "CreateShaderStage(ComputeShader)");
	if (stage == 0) goto done;

	{
		// BuildShaderSource, not a hand-written "#version 450" - the point
		// is that the engine's normal preamble assembly works for a
		// compute stage too.
		const std::string source = device.BuildShaderSource(std::string(), std::string(kComputeBody));
		std::string errorLog;
		const bool compiled = device.CompileShaderStage(stage, source, errorLog);
		check(compiled, "CompileShaderStage(compute)", errorLog);
		if (!compiled) goto done;
	}

	{
		const DeviceHandle program = device.CreateProgram();
		device.AttachShaderStage(program, stage);
		std::string linkLog;
		const bool linked = device.LinkProgram(program, linkLog);
		check(linked, "LinkProgram(compute-only program)", linkLog);
		if (!linked) goto done;

		const DeviceHandle pipeline = device.CreateComputePipeline(program);
		check(pipeline != 0, "CreateComputePipeline");
		if (pipeline == 0) goto done;

		// ---- 3. upload, dispatch --------------------------------------

		std::vector<uint32> input(kElementCount);
		for (uint32 i = 0; i < kElementCount; i++)
			input[i] = i * 3 + 7;

		const uint32 sizeBytes = kElementCount * (uint32)sizeof(uint32);
		const DeviceHandle ssbo = device.CreateStorageBuffer(sizeBytes, 0, input.data());
		check(ssbo != 0, "CreateStorageBuffer");
		if (ssbo == 0) goto done;

		device.BindComputePipeline(0, pipeline);
		device.BindStorageBuffer(0, ssbo, 0);

		// uCount is a plain uniform on the compute program, so it goes
		// through the same GetUniformLocation/SendUniformInt path
		// everything else uses. BindComputePipeline already made the
		// program current, which glUniform* requires.
		const int32 countLocation = device.GetUniformLocation((uint32)program, "uCount");
		check(countLocation >= 0, "GetUniformLocation(uCount) on a compute program");
		if (countLocation >= 0)
		{
			const int32 countValue = (int32)kElementCount;
			device.SendUniformInt(countLocation, &countValue, 1);
		}

		// Group count, not thread count - the division is the whole
		// reason Dispatch()'s parameters are named `groups`.
		const uint32 groups = (kElementCount + kLocalSize - 1) / kLocalSize;
		device.Dispatch(0, groups, 1, 1);

		// ---- 4. barrier, read back, verify ----------------------------

		// Without this the readback below may legally observe the
		// pre-dispatch contents. It would then fail with "wrong values",
		// pointing at the shader, when the real bug is the missing
		// barrier - which is exactly the confusion this test exists to
		// prevent someone else having.
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
				std::to_string(kElementCount) + " elements");
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

		// Out-of-range access must be refused, not passed to GL - see
		// GLRenderDevice::StorageRangeIsValid(). Checked by asking for one
		// element past the end and requiring the sentinel to survive.
		uint32 sentinel = 0xABCDEF01u;
		device.ReadStorageBuffer(ssbo, sizeBytes, (uint32)sizeof(uint32), &sentinel);
		check(sentinel == 0,
			"out-of-range ReadStorageBuffer is rejected and zeroes the output",
			"got " + std::to_string(sentinel));

		device.DestroyStorageBuffer(ssbo);
		device.DestroyComputePipeline(pipeline);
		device.DeleteProgram(program);
	}

done:
	if (stage != 0)
		device.DeleteShaderStage(stage);

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("\n%s  compute_smoke: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
	return failures == 0 ? 0 : 1;
}
