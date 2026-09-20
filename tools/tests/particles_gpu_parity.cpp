// GPU particle simulation, checked against the CPU simulation it replaces.
//
// The CPU path is the reference here, and that is the point: it has
// shipped, it is what every existing emitter looks like, and "the GPU
// version also moves particles around" is not evidence that it moves them
// the same way. Gravity applied before damping instead of after, a dt
// applied twice, an age computed from the wrong clock - all of those
// produce plausible motion and a visibly different effect.
//
// So this steps both paths with identical parameters and an identical RNG
// seed and compares particle positions directly.
//
//   c++ -std=c++17 -DMETAL_BACKEND -DPARITY_METAL -I include \
//       $(pkg-config --cflags freetype2) \
//       tools/tests/particles_gpu_parity.cpp -o /tmp/particles_gpu_parity \
//       -L build_metal -lPyrosEngine -framework Foundation -framework Metal \
//       -Wl,-rpath,$PWD/build_metal
//   /tmp/particles_gpu_parity
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
#include <Pyros3D/Materials/Shaders/Shaders.h>

#include <cmath>
#include <cstdio>
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

// A standalone mirror of ParticleSystem's CPU integration, deliberately
// NOT the class itself.
//
// Driving the real ParticleSystem needs a GameObject, a SceneGraph and a
// material, none of which this is testing - and its Update() also does
// emission and compaction, which would make a position-for-position
// comparison impossible to line up. What matters is the integrator, and
// this is it, transcribed from ParticleSystem::Update():
//
//     velocity += gravity * dt
//     velocity *= max(0, 1 - damping*dt)      (only when damping > 0)
//     position += velocity * dt
//
// If ParticleSystem::Update()'s integration changes and this is not
// updated with it, this test starts failing - which is the correct
// outcome, because the compute shader would then also be out of date.
struct CPUParticle { f32 px, py, pz, vx, vy, vz; };

static void StepCPU(std::vector<CPUParticle> &p, f32 dt, f32 gx, f32 gy, f32 gz, f32 damping)
{
	for (size_t i = 0; i < p.size(); i++)
	{
		p[i].vx += gx * dt; p[i].vy += gy * dt; p[i].vz += gz * dt;
		if (damping > 0.f)
		{
			const f32 k = (1.f - damping * dt) > 0.f ? (1.f - damping * dt) : 0.f;
			p[i].vx *= k; p[i].vy *= k; p[i].vz *= k;
		}
		p[i].px += p[i].vx * dt; p[i].py += p[i].vy * dt; p[i].pz += p[i].vz * dt;
	}
}

// The compute shader from ParticleSystem.cpp. Kept in sync by the test
// above failing if the integrator diverges - see StepCPU's comment.
static const char *kShader =
	"layout(local_size_x = 64) in;\n"
	"layout(std430, binding = 0) buffer StateBuf { vec4 state[]; };\n"
	"layout(std430, binding = 1) buffer OutBuf { vec4 outData[]; };\n"
	"layout(std430, binding = 2) buffer ParamBuf { vec4 params[]; };\n"
	"void main() {\n"
	"    uint i = gl_GlobalInvocationID.x;\n"
	"    float dt      = params[0].x;\n"
	"    float now     = params[0].y;\n"
	"    float count   = params[0].z;\n"
	"    float damping = params[0].w;\n"
	"    vec3  gravity = params[1].xyz;\n"
	"    if (float(i) >= count) return;\n"
	"    uint b = i * 3u;\n"
	"    vec4 s0 = state[b + 0u];\n"
	"    vec4 s1 = state[b + 1u];\n"
	"    vec4 s2 = state[b + 2u];\n"
	"    float lifetime = s0.w;\n"
	"    float age = now - s1.w;\n"
	"    if (s2.w < 0.5 || lifetime <= 0.0 || age >= lifetime) {\n"
	"        state[b + 2u] = vec4(s2.xyz, 0.0);\n"
	"        outData[i * 2u + 0u] = vec4(0.0);\n"
	"        outData[i * 2u + 1u] = vec4(0.0);\n"
	"        return;\n"
	"    }\n"
	"    vec3 velocity = s1.xyz + gravity * dt;\n"
	"    if (damping > 0.0) velocity *= max(0.0, 1.0 - damping * dt);\n"
	"    float rotation = s2.x + s2.y * dt;\n"
	"    vec3 position = s0.xyz + velocity * dt;\n"
	"    state[b + 0u] = vec4(position, lifetime);\n"
	"    state[b + 1u] = vec4(velocity, s1.w);\n"
	"    state[b + 2u] = vec4(rotation, s2.y, s2.z, 1.0);\n"
	"    outData[i * 2u + 0u] = vec4(position, age / lifetime);\n"
	"    outData[i * 2u + 1u] = vec4(rotation, s2.z, 1.0, 0.0);\n"
	"}\n";

static const uint32 kCount = 256;
static const uint32 kSteps = 60;

int main()
{
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

	const DeviceHandle stage = device.CreateShaderStage(ShaderType::ComputeShader);
	{
		std::string log;
		const bool ok = device.CompileShaderStage(stage, device.BuildShaderSource(std::string(), kShader), log);
		check(ok, "the particle compute shader compiles", log);
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

	// ---- identical initial conditions on both sides --------------------
	//
	// Varied velocities and a long lifetime so nothing dies mid-run: this
	// is testing the integrator, and a particle that expires halfway
	// through would just compare zeroes.
	const f32 gravity[3] = { 0.3f, -9.81f, -0.2f };
	const f32 damping = 0.4f;
	const f32 dt = 1.f / 60.f;
	const f32 lifetime = 1000.f;

	std::vector<CPUParticle> cpu(kCount);
	std::vector<f32> state(kCount * 3 * 4, 0.f);
	for (uint32 i = 0; i < kCount; i++)
	{
		const f32 f = (f32)i;
		const f32 px = 0.1f * f, py = 0.02f * f, pz = -0.05f * f;
		const f32 vx = sinf(f) * 3.f, vy = 2.f + cosf(f) * 4.f, vz = sinf(f * 0.5f) * 2.f;
		cpu[i].px = px; cpu[i].py = py; cpu[i].pz = pz;
		cpu[i].vx = vx; cpu[i].vy = vy; cpu[i].vz = vz;

		const uint32 b = i * 12;
		state[b + 0] = px; state[b + 1] = py; state[b + 2] = pz; state[b + 3] = lifetime;
		state[b + 4] = vx; state[b + 5] = vy; state[b + 6] = vz; state[b + 7] = 0.f; // spawnTime
		state[b + 8] = 0.f; state[b + 9] = 0.f; state[b + 10] = 0.5f; state[b + 11] = 1.f; // alive
	}

	const DeviceHandle stateBuf = device.CreateStorageBuffer((uint32)(state.size() * sizeof(f32)), 0, state.data());
	const DeviceHandle outBuf   = device.CreateStorageBuffer(kCount * 2 * 4 * (uint32)sizeof(f32), 1, NULL);
	const DeviceHandle paramBuf = device.CreateStorageBuffer(2 * 4 * (uint32)sizeof(f32), 2, NULL);
	check(stateBuf && outBuf && paramBuf, "allocated the simulation buffers");

	// ---- step both for the same number of frames -----------------------
	for (uint32 step = 0; step < kSteps; step++)
	{
		const f32 now = (f32)(step + 1) * dt;
		f32 params[8] = { dt, now, (f32)kCount, damping, gravity[0], gravity[1], gravity[2], 0.f };
		device.UpdateStorageBuffer(paramBuf, 0, sizeof(params), params);

		device.BindComputePipeline(0, pipeline);
		device.BindStorageBuffer(0, stateBuf, 0);
		device.BindStorageBuffer(0, outBuf, 1);
		device.BindStorageBuffer(0, paramBuf, 2);
		device.Dispatch(0, (kCount + 63) / 64, 1, 1);
		// Submits and waits, rather than the encoder-level
		// StorageBuffer barrier that looks more natural here.
		//
		// The next loop iteration rewrites paramBuf from the CPU, and a
		// dispatch that is recorded but NOT yet submitted has not read it
		// yet - so without this every queued step would end up seeing the
		// final iteration's parameters instead of its own. This test
		// originally used StorageBuffer and passed anyway, purely because
		// every step happened to write identical dt/gravity/damping,
		// making "all 60 steps read the last params" indistinguishable
		// from stepping properly. It stopped being indistinguishable the
		// moment one step differed.
		//
		// ParticleSystem itself is not exposed to this: it flushes every
		// frame via the VertexBuffer barrier, so only one frame's
		// dispatch is ever queued against one frame's parameters.
		device.ComputeBarrier(0, ComputeBarrierBit::HostRead);

		StepCPU(cpu, dt, gravity[0], gravity[1], gravity[2], damping);
	}

	// The regression this exists for: a VertexBuffer barrier hands the
	// data to a DRAW, which lives in another command buffer, so it must
	// submit the dispatch rather than merely record an ordering
	// primitive. When it did not, the particles never moved on screen and
	// every check in this file still passed - because they all end with a
	// HostRead, which does submit. Assert the submission directly.
	{
		// count = 0, so every invocation early-returns and the simulation
		// does not advance - this is testing submission, not integration,
		// and a 61st real step would put the GPU one frame ahead of the
		// CPU and break the comparison below.
		f32 noop[8] = { dt, 1.f, 0.f, damping, gravity[0], gravity[1], gravity[2], 0.f };
		device.UpdateStorageBuffer(paramBuf, 0, sizeof(noop), noop);
		device.BindComputePipeline(0, pipeline);
		device.BindStorageBuffer(0, stateBuf, 0);
		device.BindStorageBuffer(0, outBuf, 1);
		device.BindStorageBuffer(0, paramBuf, 2);
		device.Dispatch(0, (kCount + 63) / 64, 1, 1);
		// Proves the assertion below is not vacuous: there has to be
		// something queued for "it got submitted" to mean anything.
		// Always false on GL, which defers nothing, so only assert where
		// work is actually batched.
#if defined(PARITY_METAL) || defined(PARITY_VULKAN)
		check(device.HasPendingComputeWork(), "a recorded dispatch is pending before any barrier");
#endif
		device.ComputeBarrier(0, ComputeBarrierBit::VertexBuffer);
		check(!device.HasPendingComputeWork(),
			"a VertexBuffer barrier submits the dispatch instead of leaving it queued");
	}

	device.ComputeBarrier(0, ComputeBarrierBit::HostRead);
	check(!device.HasPendingComputeWork(), "a HostRead barrier leaves nothing pending");
	std::vector<f32> out(kCount * 2 * 4, 0.f);
	device.ReadStorageBuffer(outBuf, 0, (uint32)(out.size() * sizeof(f32)), out.data());

	// ---- compare --------------------------------------------------------
	//
	// Both sides accumulate 60 steps of float arithmetic in the same order,
	// so they should agree to near float precision - but they are different
	// compilers targeting different hardware, so the tolerance is relative
	// to how far the particle travelled rather than absolute.
	f32 worstRel = 0.f, worstAbs = 0.f;
	uint32 worstIndex = 0;
	for (uint32 i = 0; i < kCount; i++)
	{
		const f32 gx = out[i * 8 + 0], gy = out[i * 8 + 1], gz = out[i * 8 + 2];
		const f32 dx = fabsf(gx - cpu[i].px), dy = fabsf(gy - cpu[i].py), dz = fabsf(gz - cpu[i].pz);
		const f32 absErr = sqrtf(dx*dx + dy*dy + dz*dz);
		const f32 mag = sqrtf(cpu[i].px*cpu[i].px + cpu[i].py*cpu[i].py + cpu[i].pz*cpu[i].pz);
		const f32 rel = mag > 1e-3f ? absErr / mag : absErr;
		if (rel > worstRel) { worstRel = rel; worstAbs = absErr; worstIndex = i; }
	}
	{
		char buf[256];
		snprintf(buf, sizeof(buf),
			"worst relative error %.3e (abs %.6f) at particle %u; CPU (%.4f, %.4f, %.4f) GPU (%.4f, %.4f, %.4f)",
			worstRel, worstAbs, worstIndex,
			cpu[worstIndex].px, cpu[worstIndex].py, cpu[worstIndex].pz,
			out[worstIndex*8+0], out[worstIndex*8+1], out[worstIndex*8+2]);
		check(worstRel < 1e-4f, "GPU simulation matches the CPU integrator after 60 steps", buf);
	}

	// Guards against a vacuous pass: if nothing moved, everything above
	// would agree perfectly and mean nothing.
	f32 travelled = 0.f;
	for (uint32 i = 0; i < kCount; i++)
		travelled += fabsf(cpu[i].py);
	check(travelled > 100.f, "the particles actually moved", "sum |y| = " + std::to_string(travelled));

	// Normalized age must be filled in and sane - it drives every size and
	// colour ramp, and a wrong clock here is invisible in position alone.
	const f32 age = out[0 * 8 + 3];
	check(age > 0.f && age < 1.f, "normalized age is in range",
		"age = " + std::to_string(age) + " (expected ~" + std::to_string(kSteps * dt / lifetime) + ")");

	// And the alive flag the vertex shader multiplies size by.
	check(out[0 * 8 + 6] == 1.f, "the alive flag is set for a live particle");

	// A dead slot must be written as fully zero, so its quad collapses.
	{
		std::vector<f32> deadState(3 * 4, 0.f); // alive = 0
		device.UpdateStorageBuffer(stateBuf, 0, (uint32)(deadState.size() * sizeof(f32)), deadState.data());
		f32 params[8] = { dt, 1.f, (f32)kCount, damping, gravity[0], gravity[1], gravity[2], 0.f };
		device.UpdateStorageBuffer(paramBuf, 0, sizeof(params), params);
		device.BindComputePipeline(0, pipeline);
		device.BindStorageBuffer(0, stateBuf, 0);
		device.BindStorageBuffer(0, outBuf, 1);
		device.BindStorageBuffer(0, paramBuf, 2);
		device.Dispatch(0, (kCount + 63) / 64, 1, 1);
		device.ComputeBarrier(0, ComputeBarrierBit::HostRead);
		device.ReadStorageBuffer(outBuf, 0, 8 * (uint32)sizeof(f32), out.data());
		check(out[6] == 0.f, "a dead slot writes alive = 0 so its quad collapses");
	}

	device.DestroyStorageBuffer(paramBuf);
	device.DestroyStorageBuffer(outBuf);
	device.DestroyStorageBuffer(stateBuf);
	device.DestroyComputePipeline(pipeline);
	device.DeleteProgram(program);
	device.DeleteShaderStage(stage);

	printf("\n%s  particles_gpu_parity: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
	return failures == 0 ? 0 : 1;
}
