//============================================================================
// Name        : ParticleSystem.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Particle System
//============================================================================

#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

namespace p3d {

	ParticleSystemDesc::ParticleSystemDesc()
	{
		maxParticles = 200;
		texture.reset();
		looping = true;
		emissionRate = 20.0f;
		burstCount = 1;
		minLifetime = 2.0f;
		maxLifetime = 4.0f;
		direction = Vec3(0.0f, 1.0f, 0.0f);
		spreadAngle = (f32)DEGTORAD(15.0);
		minSpeed = 1.0f;
		maxSpeed = 2.0f;
		gravity = Vec3::ZERO;
		damping = 0.0f;
		gpuSimulation = false;
		startSize = 1.0f;
		endSize = 2.0f;
		sizeRandomJitter = 0.2f;
		startColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		endColor = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
		fadeInFraction = 0.1f;
		fadeOutFraction = 0.6f;
		minRotationSpeed = -1.0f;
		maxRotationSpeed = 1.0f;
		sizeEase = INTERP_LINEAR;
		colorEase = INTERP_LINEAR;
		blendMode = ParticleBlendMode::AlphaBlend;
		boundingSphereRadius = 0.0f;
	}

	// Internal material - see the auto-fix comment in particleSystem.glsl
	// itself. Every uniform is set via a stored handle so ParticleSystem's
	// live setters can push new values at any time (Uniform::SetValue()),
	// exactly the same "stored handle" pattern the old ParticleMaterial/
	// CustomMaterialExample examples already used for their own tunables.
	class ParticleSystemMaterial : public CustomShaderMaterial
	{
	public:
		ParticleSystemMaterial(const std::shared_ptr<Texture> &texture) : CustomShaderMaterial("shaders/particleSystem.glsl")
		{
			AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
			AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));

			int32 texUnit = 0;
			AddUniform(Uniform("uTex0", Uniforms::DataType::Int, &texUnit));
			if (texture)
				textures.push_back(texture);

			startColorHandle = AddUniform(Uniform("uStartColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
			endColorHandle = AddUniform(Uniform("uEndColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
			startSizeHandle = AddUniform(Uniform("uStartSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
			endSizeHandle = AddUniform(Uniform("uEndSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
			sizeJitterHandle = AddUniform(Uniform("uSizeRandomJitter", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
			fadeInHandle = AddUniform(Uniform("uFadeInFraction", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
			fadeOutHandle = AddUniform(Uniform("uFadeOutFraction", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
			// Passed as floats, not ints: the mode is compared against
			// thresholds in the shader, and a float uniform needs no
			// integer-uniform support from either backend's auto-fix path.
			sizeEaseHandle = AddUniform(Uniform("uSizeEase", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
			colorEaseHandle = AddUniform(Uniform("uColorEase", Uniforms::DataUsage::Other, Uniforms::DataType::Float));

			SetTransparencyFlag(true);
			// A spherical billboard's winding isn't guaranteed consistent
			// from every camera angle - double-sided is the safe default,
			// matching the old ParticleMaterial's same reasoning.
			SetCullFace(CullFace::DoubleSided);
			EnableBlending();
			SetBlendMode(ParticleBlendMode::AlphaBlend);
		}

		void SetColors(const Vec4 &start, const Vec4 &end)
		{
			startColorHandle->SetValue((void*)&start);
			endColorHandle->SetValue((void*)&end);
		}
		void SetSizes(f32 start, f32 end, f32 jitter)
		{
			startSizeHandle->SetValue(&start);
			endSizeHandle->SetValue(&end);
			sizeJitterHandle->SetValue(&jitter);
		}
		void SetEasing(uchar sizeEase, uchar colorEase)
		{
			f32 s = (f32)sizeEase, c = (f32)colorEase;
			sizeEaseHandle->SetValue(&s);
			colorEaseHandle->SetValue(&c);
		}
		void SetFade(f32 fadeIn, f32 fadeOut)
		{
			fadeInHandle->SetValue(&fadeIn);
			fadeOutHandle->SetValue(&fadeOut);
		}
		void SetBlendMode(uint32 mode)
		{
			if (mode == ParticleBlendMode::Additive)
				BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One);
			else
				BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		}

	private:
		Uniform *sizeEaseHandle, *colorEaseHandle;
		Uniform *startColorHandle, *endColorHandle;
		Uniform *startSizeHandle, *endSizeHandle, *sizeJitterHandle;
		Uniform *fadeInHandle, *fadeOutHandle;
	};

	namespace {

		// See ParticleSystem's constructor comment - a base class's
		// constructor arguments are evaluated (and the base fully
		// constructed) before *any* derived-class member initializer runs,
		// so there's no ordering-safe way for ParticleSystem to both pass
		// a freshly-built ParticleSystemMaterial* to IRenderingInstancedComponent's
		// constructor AND capture that same pointer into its own member via
		// the initializer list alone (RenderingComponent doesn't expose a
		// generic GetMaterial() the way it does GetRenderable()). This
		// single-use handoff slot is the standard, contained way around
		// that: BuildParticleMaterial() (called once, directly in the base
		// class's argument list) stashes the pointer here, and the very
		// next thing evaluated in the same constructor invocation - the
		// `material` member initializer - reads it back. Single-threaded
		// construction only, matching this engine's model throughout; not
		// reentrant, not meant to be.
		ParticleSystemMaterial* g_pendingParticleMaterial = NULL;

		std::shared_ptr<ParticleSystemMaterial> BuildParticleMaterial(const ParticleSystemDesc &desc)
		{
			auto mat = std::make_shared<ParticleSystemMaterial>(desc.texture);
			mat->SetColors(desc.startColor, desc.endColor);
			mat->SetSizes(desc.startSize, desc.endSize, desc.sizeRandomJitter);
			mat->SetFade(desc.fadeInFraction, desc.fadeOutFraction);
			mat->SetEasing(desc.sizeEase, desc.colorEase);
			mat->SetBlendMode(desc.blendMode);
			g_pendingParticleMaterial = mat.get();
			return mat;
		}

		// maxLifetime*maxSpeed (how far the fastest, longest-lived particle
		// can travel from the emission cone alone) plus gravity's own
		// contribution (0.5*|g|*t^2) plus a margin for the particle's own
		// rendered size - see ParticleSystemDesc::boundingSphereRadius's
		// comment for why this is only used as a fallback (cull-testing is
		// disabled by default regardless).
		f32 EstimateBoundingSphereRadius(const ParticleSystemDesc &desc)
		{
			if (desc.boundingSphereRadius > 0.0f)
				return desc.boundingSphereRadius;
			f32 t = desc.maxLifetime;
			f32 speedTerm = desc.maxSpeed * t;
			f32 gravityTerm = 0.5f * desc.gravity.magnitude() * t * t;
			f32 sizeMargin = Max(desc.startSize, desc.endSize);
			return speedTerm + gravityTerm + sizeMargin;
		}
	}

	ParticleSystem::ParticleSystem(const ParticleSystemDesc &d)
		: IRenderingInstancedComponent(std::make_shared<Plane>(0.5f, 0.5f), BuildParticleMaterial(d), 0, EstimateBoundingSphereRadius(d))
		, desc(d)
		, rng()
		, liveCount(0)
		, emissionAccumulator(0.0f)
		, lastUpdateTime(0.0)
		, hasUpdatedOnce(false)
		, playing(true)
		, pendingBurst(false)
		, gpuActive(false)
		, gpuStage(0), gpuProgram(0), gpuPipeline(0)
		, gpuStateBuffer(0), gpuParamsBuffer(0)
		, gpuSpawnCursor(0)
		, particleBuffer(NULL)
		, material(g_pendingParticleMaterial)
		, quad(NULL)
	{
		g_pendingParticleMaterial = NULL;
		quad = GetRenderable();

		// A GameObject-anchored bounding sphere is the wrong volume once
		// particles are simulated in world space and decoupled from the
		// emitter after spawning - see the class comment. Correctness over
		// the modest overdraw savings culling a handful of small emitters
		// would give.
		DisableCullTest();

		if (desc.maxParticles == 0)
			desc.maxParticles = 1;
		cpuState.resize(desc.maxParticles);
		gpuState.resize(desc.maxParticles);

		// Before the attribute buffer, because its draw hint depends on
		// the answer - see CreateGPUPipeline()'s comment.
		BuildSimulationBackend();
	}

	ParticleSystem::~ParticleSystem()
	{
		ShutdownGPUSimulation();
		RemoveBuffer(particleBuffer);
		delete particleBuffer;
		// material + quad (Plane) are owned by RenderingComponent's
		// shared_ptr members - do not delete here.
		material = NULL;
		quad = NULL;
	}

	void ParticleSystem::Play()
	{
		playing = true;
		// A one-shot burst is spawned by the next Update(), not here.
		// Spawning now stamps every particle with `lastUpdateTime`, which is
		// the clock of the emitter's *previous* frame - and 0.0 for an
		// emitter that has never been updated at all (freshly deserialized,
		// or just attached). Against a scene clock that is already seconds
		// in, that made every particle in the burst older than its own
		// lifetime the instant it was first aged, so the burst died on the
		// same frame it was created and simply never appeared.
		if (!desc.looping)
			pendingBurst = true;
	}

	void ParticleSystem::Stop()
	{
		playing = false;
		pendingBurst = false;
	}

	void ParticleSystem::Clear()
	{
		liveCount = 0;
		pendingBurst = false;
		if (gpuActive)
		{
			// The GPU path draws the whole pool every frame and hides dead
			// slots with the alive flag, so "clear" means zero that flag -
			// NOT drop the instance count to 0, which would stop the
			// emitter rendering permanently, since nothing on this path
			// ever raises it again.
			ZeroGPUState();
			return;
		}
		SetNumberInstances(0);
	}

	void ParticleSystem::SetSizes(const f32 startSize, const f32 endSize, const f32 sizeRandomJitter)
	{
		desc.startSize = startSize;
		desc.endSize = endSize;
		desc.sizeRandomJitter = sizeRandomJitter;
		material->SetSizes(startSize, endSize, sizeRandomJitter);
	}

	void ParticleSystem::SetColors(const Vec4 &startColor, const Vec4 &endColor)
	{
		desc.startColor = startColor;
		desc.endColor = endColor;
		material->SetColors(startColor, endColor);
	}

	void ParticleSystem::SetFade(const f32 fadeInFraction, const f32 fadeOutFraction)
	{
		desc.fadeInFraction = fadeInFraction;
		desc.fadeOutFraction = fadeOutFraction;
		material->SetFade(fadeInFraction, fadeOutFraction);
	}

	void ParticleSystem::SetEasing(const uchar sizeEase, const uchar colorEase)
	{
		desc.sizeEase = sizeEase;
		desc.colorEase = colorEase;
		if (!material) return;
		material->SetEasing(sizeEase, colorEase);
	}

	void ParticleSystem::SetBlendMode(const uint32 blendMode)
	{
		desc.blendMode = blendMode;
		material->SetBlendMode(blendMode);
	}

	void ParticleSystem::SetTexture(const std::shared_ptr<Texture> &texture)
	{
		desc.texture = texture;
		// The material's constructor pushes the sprite straight into
		// `textures` (not via AddSampler) and binds uTex0 to unit 0 - so
		// replacing entry 0 is all a swap needs, and dropping the entry
		// entirely leaves uTex0 pointing at an unbound unit, exactly as a
		// texture-less desc did from the start.
		material->textures.clear();
		if (texture)
			material->textures.push_back(texture);
	}

	void ParticleSystem::SetMaxParticles(const uint32 maxParticles)
	{
		uint32 capacity = (maxParticles == 0) ? 1 : maxParticles;
		if (capacity == desc.maxParticles) return;

		desc.maxParticles = capacity;
		pendingBurst = false;
		// Every live particle goes: cpuState[i] and gpuState[i] describe the
		// same particle only for as long as neither vector is reallocated,
		// and nothing here has a particle identity that outlives its slot.
		liveCount = 0;
		emissionAccumulator = 0.0f;
		cpuState.assign(capacity, ParticleCPU());
		gpuState.assign(capacity, ParticleGPU());
		SetNumberInstances(0);
		if (gpuActive)
		{
			// The GPU path's state SSBO and slot mirror are both sized by
			// maxParticles, and neither can be resized in place - so the
			// whole backend is rebuilt. Consistent with this setter's
			// existing contract, which already discards every particle.
			BuildSimulationBackend();
			return;
		}
		// The one legitimate reallocation besides the constructor's - see the
		// class comment on why every per-frame Update() instead reuploads
		// this same (now new) fixed byte length.
		particleBuffer->Buffer->Update(&gpuState[0], (uint32)(gpuState.size() * sizeof(ParticleGPU)));
	}

	void ParticleSystem::SpawnParticle(const f64 time)
	{
		if (liveCount >= desc.maxParticles)
			return; // hard cap - drop the spawn, see the class comment

		// Reads the owner's CURRENT world position - see the header
		// comment on SpawnParticle() for the one-frame-lag this can imply
		// relative to a GameObject::Update() override moving the emitter
		// this same frame. RefreshTransformation() forces local→world now
		// so a freshly positioned emitter (e.g. JS setPosition before the
		// next SceneGraph InternalUpdate) does not spawn at the origin.
		Vec3 origin = Vec3::ZERO;
		if (GetOwner() != NULL)
		{
			GetOwner()->RefreshTransformation();
			origin = GetOwner()->GetWorldPosition();
		}

		// Sample a random direction within the emission cone: pick a polar
		// angle in [0,spreadAngle] and a uniform azimuth around it, in a
		// frame where +Z is the cone axis, then rotate that frame to align
		// with desc.direction.
		f32 cosSpread = cosf(desc.spreadAngle);
		f32 z = rng.Range(cosSpread, 1.0f);
		f32 phi = rng.Range(0.0f, 2.0f * (f32)PI);
		f32 r = sqrtf(Max(0.0f, 1.0f - z * z));
		Vec3 localDir(r * cosf(phi), r * sinf(phi), z);

		Vec3 axis = desc.direction.normalize();
		Vec3 up = (fabsf(axis.y) < 0.99f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
		Vec3 tangent = up.cross(axis).normalize();
		Vec3 bitangent = axis.cross(tangent);
		Vec3 worldDir = (tangent * localDir.x) + (bitangent * localDir.y) + (axis * localDir.z);

		f32 speed = rng.Range(desc.minSpeed, desc.maxSpeed);

		uint32 slot = liveCount;
		cpuState[slot].velocity = worldDir * speed;
		cpuState[slot].lifetime = rng.Range(desc.minLifetime, desc.maxLifetime);
		cpuState[slot].spawnTime = (f32)time;
		cpuState[slot].rotationSpeed = rng.Range(desc.minRotationSpeed, desc.maxRotationSpeed);
		cpuState[slot].rotation = rng.Range(0.0f, 2.0f * (f32)PI);
		cpuState[slot].randomSeed = rng.NextFloat01();

		gpuState[slot].data0 = Vec4(origin, 0.0f);
		// .z is the alive flag the vertex shader multiplies size by (it
		// was a documented "reserved" component until GPU simulation
		// needed a way to hide a dead slot it cannot compact away). The
		// CPU path compacts, so every slot it draws is alive - but it
		// still has to write 1.0 here, because the shader does not know
		// which path produced the buffer.
		gpuState[slot].data1 = Vec4(cpuState[slot].rotation, cpuState[slot].randomSeed, 1.0f, 0.0f);

		liveCount++;
	}

	void ParticleSystem::Update(const f64 time)
	{
		f32 dt = hasUpdatedOnce ? (f32)(time - lastUpdateTime) : 0.0f;
		if (dt < 0.0f) dt = 0.0f;
		if (dt > 0.1f) dt = 0.1f; // clamp - avoid a spawn/integration burst after a stall
		lastUpdateTime = time;
		hasUpdatedOnce = true;

		if (pendingBurst)
		{
			pendingBurst = false;
			for (uint32 i = 0; i < desc.burstCount; i++)
				gpuActive ? SpawnParticleGPU(time) : SpawnParticle(time);
		}

		if (playing && desc.looping && desc.emissionRate > 0.0f)
		{
			emissionAccumulator += dt;
			f32 interval = 1.0f / desc.emissionRate;
			while (emissionAccumulator >= interval)
			{
				emissionAccumulator -= interval;
				for (uint32 i = 0; i < desc.burstCount; i++)
					gpuActive ? SpawnParticleGPU(time) : SpawnParticle(time);
			}
		}

		// Emission stays on the CPU either way - it is a handful of
		// spawns per frame against thousands of integrations, so moving
		// it would buy nothing and cost the ability to place a particle
		// at the emitter's current world transform.
		if (gpuActive)
		{
			UpdateGPU(time, dt);
			return;
		}

		// Integrate + age every live particle; dead ones are removed via
		// swap-with-last-live (O(1)) instead of an ordered erase (O(n)) -
		// order among live particles doesn't matter for rendering.
		for (uint32 i = 0; i < liveCount; )
		{
			f32 age = (f32)time - cpuState[i].spawnTime;
			if (age >= cpuState[i].lifetime)
			{
				uint32 last = liveCount - 1;
				cpuState[i] = cpuState[last];
				gpuState[i] = gpuState[last];
				liveCount--;
				continue; // re-check this same slot, now holding the swapped-in particle
			}

			cpuState[i].velocity += desc.gravity * dt;
			if (desc.damping > 0.0f)
				cpuState[i].velocity *= Max(0.0f, 1.0f - desc.damping * dt);
			cpuState[i].rotation += cpuState[i].rotationSpeed * dt;

			Vec3 worldPos = gpuState[i].data0.xyz() + cpuState[i].velocity * dt;
			f32 normalizedAge = cpuState[i].lifetime > 0.0f ? (age / cpuState[i].lifetime) : 1.0f;

			gpuState[i].data0 = Vec4(worldPos, normalizedAge);
			gpuState[i].data1.x = cpuState[i].rotation;

			i++;
		}

		SetNumberInstances(liveCount);
		// Always the same fixed (maxParticles) byte length - see the class
		// comment on why this keeps GeometryBuffer::Update() off the
		// reallocate-every-frame path.
		particleBuffer->Buffer->Update(&gpuState[0], (uint32)(gpuState.size() * sizeof(ParticleGPU)));
	}


	// =====================================================================
	// GPU simulation
	//
	// The dispatch runs from Update(), which SceneGraph::Update() calls
	// BEFORE any rendering - and that placement is not incidental. Metal
	// refuses a dispatch while a render encoder is open, and Vulkan cannot
	// record one inside a render pass at all, so a per-frame simulation
	// that ran during the frame would have to suspend and resume the pass.
	// Simulating in the update phase sidesteps that entirely: the compute
	// work is submitted and complete before BeginFrame is ever called.
	// =====================================================================

	namespace {

	// Three vec4s of state per particle. Position lives here rather than
	// being read back out of the attribute buffer so that the dispatch
	// only ever WRITES that buffer - the CPU never has to poke at vertex
	// data the draw is about to consume, and spawning touches one buffer
	// the device knows the size of.
	const uint32 kGPUStateVec4s = 3;

	const char *kParticleComputeShader =
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
		"    vec4 s0 = state[b + 0u];\n"   // xyz position, w lifetime
		"    vec4 s1 = state[b + 1u];\n"   // xyz velocity, w spawnTime
		"    vec4 s2 = state[b + 2u];\n"   // x rotation, y rotationSpeed, z seed, w alive
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

	} // namespace

	// Creates the attribute buffer and, if asked for and available, the
	// compute pipeline and its buffers. Shared by the constructor and by
	// anything that has to switch paths afterwards, because the two are
	// not independent: whether the dispatch exists decides the attribute
	// buffer's draw hint (Static vs Stream - see CreateGPUPipeline's
	// comment), so they cannot be set up separately or changed in place.
	void ParticleSystem::BuildSimulationBackend()
	{
		ShutdownGPUSimulation();
		if (particleBuffer != NULL)
		{
			RemoveBuffer(particleBuffer);
			delete particleBuffer;
			particleBuffer = NULL;
		}

		const bool useGPU = desc.gpuSimulation && CreateGPUPipeline();
		if (desc.gpuSimulation && !useGPU)
			echo("ParticleSystem: gpuSimulation requested but unavailable on this backend - using the CPU path.");

		particleBuffer = new AttributeBuffer(Buffer::Type::Attribute,
			useGPU ? Buffer::Draw::Static : Buffer::Draw::Stream);
		particleBuffer->AddAttribute("aParticleData0", Buffer::Attribute::Type::Vec4, NULL, 0, 1);
		particleBuffer->AddAttribute("aParticleData1", Buffer::Attribute::Type::Vec4, NULL, 0, 1);
		particleBuffer->SendBuffer();
		AddBuffer(particleBuffer);
		// Re-materialise the CPU mirrors if a previous GPU build released
		// them (see the end of this function). Switching back to the CPU
		// path would otherwise index an empty vector here, and so would
		// the upload just below - the failure is silent, because an empty
		// vector's data() is not required to be null.
		if (gpuState.size() != desc.maxParticles)
		{
			cpuState.assign(desc.maxParticles, ParticleCPU());
			gpuState.assign(desc.maxParticles, ParticleGPU());
			liveCount = 0;
		}
		// Establish the buffer at full capacity right away - see the class
		// comment on why every later Update() reuploads this exact same
		// byte length (keeps GeometryBuffer::Update() on the cheap sub-
		// data path instead of reallocating every frame). This is the one
		// legitimate reallocation, from AddAttribute()'s zero-length
		// initial SendBuffer() up to real capacity.
		particleBuffer->Buffer->Update(&gpuState[0], (uint32)(gpuState.size() * sizeof(ParticleGPU)));

		// After the attribute buffer exists, because the dispatch writes
		// into it and needs its device handle.
		if (useGPU && !CreateGPUBuffers())
		{
			echo("ParticleSystem: GPU simulation buffers failed to allocate - using the CPU path.");
			ShutdownGPUSimulation();
		}
		if (gpuActive)
		{
			echo("ParticleSystem: GPU simulation active, pool of "
				+ std::to_string(desc.maxParticles) + " particles ("
				+ std::to_string((desc.maxParticles * kGPUStateVec4s * sizeof(Vec4)) >> 20)
				+ " MB state + "
				+ std::to_string((desc.maxParticles * sizeof(ParticleGPU)) >> 20)
				+ " MB instance data).");
			// Nothing reads these again - simulation state lives in the
			// SSBO and the attribute buffer is written by the dispatch.
			// They were only needed to size and zero the attribute buffer
			// just above. At a million particles they are ~60MB of CPU
			// memory that would otherwise sit untouched for the lifetime
			// of the emitter, so they are released rather than merely
			// cleared (clear() keeps the capacity).
			std::vector<ParticleCPU>().swap(cpuState);
			std::vector<ParticleGPU>().swap(gpuState);
		}
		else
		{
			SetNumberInstances(liveCount);
		}
	}

	void ParticleSystem::SetGPUSimulation(const bool enabled)
	{
		if (enabled == desc.gpuSimulation)
			return;
		desc.gpuSimulation = enabled;
		// Every live particle goes, for the same reason SetMaxParticles
		// discards them: the two paths keep their state in different
		// places (cpuState/gpuState here, an SSBO there) and nothing owns
		// a particle identity that could be migrated between them.
		liveCount = 0;
		pendingBurst = false;
		emissionAccumulator = 0.0f;
		BuildSimulationBackend();
	}

	bool ParticleSystem::CreateGPUPipeline()
	{
		IRenderDevice &dev = GetActiveRenderDevice();
		if (!dev.SupportsCompute())
			return false;

		gpuStage = dev.CreateShaderStage(ShaderType::ComputeShader);
		if (gpuStage == 0)
			return false;
		{
			std::string errorLog;
			const std::string source = dev.BuildShaderSource(std::string(), std::string(kParticleComputeShader));
			if (!dev.CompileShaderStage(gpuStage, source, errorLog))
			{
				echo("ParticleSystem: GPU simulation unavailable - compute shader failed to compile: " + errorLog);
				ShutdownGPUSimulation();
				return false;
			}
		}
		gpuProgram = dev.CreateProgram();
		dev.AttachShaderStage(gpuProgram, gpuStage);
		{
			std::string errorLog;
			if (!dev.LinkProgram(gpuProgram, errorLog))
			{
				echo("ParticleSystem: GPU simulation unavailable - link failed: " + errorLog);
				ShutdownGPUSimulation();
				return false;
			}
		}
		gpuPipeline = dev.CreateComputePipeline(gpuProgram);
		if (gpuPipeline == 0)
		{
			ShutdownGPUSimulation();
			return false;
		}
		return true;
	}

	// Clears every slot's alive flag. Chunked rather than done from one
	// pool-sized temporary: at a million particles that staging buffer
	// alone would be 48MB, allocated only to be copied once.
	void ParticleSystem::ZeroGPUState()
	{
		if (gpuStateBuffer == 0)
			return;
		IRenderDevice &dev = GetActiveRenderDevice();
		const uint32 stateBytes = desc.maxParticles * kGPUStateVec4s * (uint32)sizeof(Vec4);
		const uint32 kChunkBytes = 1u << 20;
		std::vector<uint8> zeros(kChunkBytes < stateBytes ? kChunkBytes : stateBytes, 0);
		for (uint32 offset = 0; offset < stateBytes; offset += (uint32)zeros.size())
		{
			const uint32 n = ((stateBytes - offset) < (uint32)zeros.size())
				? (stateBytes - offset) : (uint32)zeros.size();
			dev.UpdateStorageBuffer(gpuStateBuffer, offset, n, zeros.data());
		}
		gpuSpawnCursor = 0;
		gpuSpawnStaging.clear();
	}

	bool ParticleSystem::CreateGPUBuffers()
	{
		IRenderDevice &dev = GetActiveRenderDevice();

		// Must start zeroed: alive (s2.w) being 0 for every slot is what
		// makes an un-spawned pool simulate to nothing instead of to
		// garbage. Filled in bounded chunks rather than from one
		// pool-sized temporary - at a million particles that staging
		// vector alone would be 48MB, allocated only to be memcpy'd once
		// and thrown away.
		const uint32 stateBytes = desc.maxParticles * kGPUStateVec4s * (uint32)sizeof(Vec4);
		gpuStateBuffer = dev.CreateStorageBuffer(stateBytes, 0, NULL);
		ZeroGPUState();
		gpuParamsBuffer = dev.CreateStorageBuffer(2 * (uint32)sizeof(Vec4), 2, NULL);
		if (gpuStateBuffer == 0 || gpuParamsBuffer == 0)
		{
			ShutdownGPUSimulation();
			return false;
		}

		gpuSpawnCursor = 0;
		gpuSpawnStaging.clear();

		// Every slot is drawn every frame; dead ones collapse to nothing
		// in the vertex shader. See ParticleSystemDesc::gpuSimulation.
		SetNumberInstances(desc.maxParticles);
		gpuActive = true;
		return true;
	}

	void ParticleSystem::ShutdownGPUSimulation()
	{
		IRenderDevice &dev = GetActiveRenderDevice();
		if (gpuStateBuffer != 0) { dev.DestroyStorageBuffer(gpuStateBuffer); gpuStateBuffer = 0; }
		if (gpuParamsBuffer != 0) { dev.DestroyStorageBuffer(gpuParamsBuffer); gpuParamsBuffer = 0; }
		if (gpuPipeline != 0) { dev.DestroyComputePipeline(gpuPipeline); gpuPipeline = 0; }
		if (gpuProgram != 0) { dev.DeleteProgram(gpuProgram); gpuProgram = 0; }
		if (gpuStage != 0) { dev.DeleteShaderStage(gpuStage); gpuStage = 0; }
		gpuActive = false;
	}

	void ParticleSystem::SpawnParticleGPU(const f64 time)
	{
		// No search - see gpuSpawnCursor. Staged rather than uploaded here
		// so a frame that spawns ten thousand particles does one upload,
		// not ten thousand.
		if (gpuSpawnStaging.size() >= (size_t)desc.maxParticles * kGPUStateVec4s)
			return; // already staged a whole pool's worth this frame

		Vec3 origin = Vec3::ZERO;
		if (GetOwner() != NULL)
		{
			GetOwner()->RefreshTransformation();
			origin = GetOwner()->GetWorldPosition();
		}

		// Identical sampling to SpawnParticle() - deliberately, so the two
		// paths given the same RNG sequence produce the same particles and
		// can be compared directly (tools/tests/particles_gpu_parity.cpp).
		const f32 cosSpread = cosf(desc.spreadAngle);
		const f32 z = rng.Range(cosSpread, 1.0f);
		const f32 phi = rng.Range(0.0f, 2.0f * (f32)PI);
		const f32 r = sqrtf(Max(0.0f, 1.0f - z * z));
		const Vec3 localDir(r * cosf(phi), r * sinf(phi), z);

		const Vec3 axis = desc.direction.normalize();
		const Vec3 up = (fabsf(axis.y) < 0.99f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
		const Vec3 tangent = up.cross(axis).normalize();
		const Vec3 bitangent = axis.cross(tangent);
		const Vec3 worldDir = (tangent * localDir.x) + (bitangent * localDir.y) + (axis * localDir.z);

		const f32 speed = rng.Range(desc.minSpeed, desc.maxSpeed);
		const f32 lifetime = rng.Range(desc.minLifetime, desc.maxLifetime);
		const f32 rotationSpeed = rng.Range(desc.minRotationSpeed, desc.maxRotationSpeed);
		const f32 rotation = rng.Range(0.0f, 2.0f * (f32)PI);
		const f32 seed = rng.NextFloat01();

		gpuSpawnStaging.push_back(Vec4(origin, lifetime));
		gpuSpawnStaging.push_back(Vec4(worldDir * speed, (f32)time));
		gpuSpawnStaging.push_back(Vec4(rotation, rotationSpeed, seed, 1.0f));
	}

	void ParticleSystem::FlushGPUSpawns()
	{
		if (gpuSpawnStaging.empty())
			return;
		IRenderDevice &dev = GetActiveRenderDevice();

		const uint32 vec4Count = (uint32)gpuSpawnStaging.size();
		const uint32 newParticles = vec4Count / kGPUStateVec4s;
		const uint32 slotBytes = kGPUStateVec4s * (uint32)sizeof(Vec4);

		// One upload, or two when the run wraps past the end of the pool.
		// The cursor makes the slots contiguous; the wrap is the only
		// reason this is not a single memcpy.
		const uint32 firstRun = (gpuSpawnCursor + newParticles <= desc.maxParticles)
			? newParticles : (desc.maxParticles - gpuSpawnCursor);

		dev.UpdateStorageBuffer(gpuStateBuffer, gpuSpawnCursor * slotBytes,
			firstRun * slotBytes, gpuSpawnStaging.data());

		if (firstRun < newParticles)
		{
			dev.UpdateStorageBuffer(gpuStateBuffer, 0,
				(newParticles - firstRun) * slotBytes,
				gpuSpawnStaging.data() + (size_t)firstRun * kGPUStateVec4s);
		}

		gpuSpawnCursor = (gpuSpawnCursor + newParticles) % desc.maxParticles;
		gpuSpawnStaging.clear();
	}

	void ParticleSystem::UpdateGPU(const f64 time, const f32 dt)
	{
		IRenderDevice &dev = GetActiveRenderDevice();
		FlushGPUSpawns();

		Vec4 params[2];
		params[0] = Vec4(dt, (f32)time, (f32)desc.maxParticles, desc.damping);
		params[1] = Vec4(desc.gravity, 0.f);
		dev.UpdateStorageBuffer(gpuParamsBuffer, 0, 2 * (uint32)sizeof(Vec4), params);

		dev.BindComputePipeline(0, gpuPipeline);
		dev.BindStorageBuffer(0, gpuStateBuffer, 0);
		// The attribute buffer the draw reads, bound as a storage buffer
		// so the dispatch can fill it in place. GL and Metal buffers carry
		// no type at creation; Vulkan's do, which is why
		// AllocHostVisibleVertexBuffer asks for STORAGE_BUFFER usage.
		dev.BindStorageBuffer(0, (DeviceHandle)particleBuffer->Buffer->ID, 1);
		dev.BindStorageBuffer(0, gpuParamsBuffer, 2);

		const uint32 groups = (desc.maxParticles + 63u) / 64u;
		dev.Dispatch(0, groups, 1, 1);

		// The very next thing to touch this buffer is a draw call reading
		// it as vertex attributes - not a shader read, which is why this
		// is VertexBuffer and not StorageBuffer. On Vulkan that is the
		// difference between VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT and
		// VK_ACCESS_SHADER_READ_BIT, and getting it wrong produces a
		// barrier that validates, costs the same, and protects nothing.
		dev.ComputeBarrier(0, ComputeBarrierBit::VertexBuffer);
	}

}
