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
		startSize = 1.0f;
		endSize = 2.0f;
		sizeRandomJitter = 0.2f;
		startColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		endColor = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
		fadeInFraction = 0.1f;
		fadeOutFraction = 0.6f;
		minRotationSpeed = -1.0f;
		maxRotationSpeed = 1.0f;
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

		particleBuffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
		particleBuffer->AddAttribute("aParticleData0", Buffer::Attribute::Type::Vec4, NULL, 0, 1);
		particleBuffer->AddAttribute("aParticleData1", Buffer::Attribute::Type::Vec4, NULL, 0, 1);
		particleBuffer->SendBuffer();
		AddBuffer(particleBuffer);

		// Establish the buffer at full capacity right away - see the class
		// comment on why every later Update() reuploads this exact same
		// byte length (keeps GeometryBuffer::Update() on the cheap sub-
		// data path instead of reallocating every frame). This first call
		// is the one legitimate reallocation, from AddAttribute()'s
		// zero-length initial SendBuffer() up to real capacity.
		particleBuffer->Buffer->Update(&gpuState[0], (uint32)(gpuState.size() * sizeof(ParticleGPU)));
	}

	ParticleSystem::~ParticleSystem()
	{
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
		gpuState[slot].data1 = Vec4(cpuState[slot].rotation, cpuState[slot].randomSeed, 0.0f, 0.0f);

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
				SpawnParticle(time);
		}

		if (playing && desc.looping && desc.emissionRate > 0.0f)
		{
			emissionAccumulator += dt;
			f32 interval = 1.0f / desc.emissionRate;
			while (emissionAccumulator >= interval)
			{
				emissionAccumulator -= interval;
				for (uint32 i = 0; i < desc.burstCount; i++)
					SpawnParticle(time);
			}
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

}
