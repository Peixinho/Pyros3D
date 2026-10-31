//============================================================================
// Name        : ParticleSystem.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Particle System
//============================================================================

#include <algorithm>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/GameObjects/GameObject.h>

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
		collide2D = false;
		bounce = 0.3f;
		collisionMask = 0xFFFFFFFFu;
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
		softness = getenv("PYROS_HARD_PARTICLES") ? 0.f : 0.4f;   // (PYROS_HARD_PARTICLES=1: the hard edges, to compare)
		boundingSphereRadius = 0.0f;
		gpuSimulation = false;
	}

	// Its material: how it is blended and its texture - the renderer draws a system from its particles and its desc
	class ParticleSystemMaterial : public CustomShaderMaterial
	{
	public:
		ParticleSystemMaterial(const std::shared_ptr<Texture> &texture) : CustomShaderMaterial()
		{
			if (texture)
				textures.push_back(texture);
			SetTransparencyFlag(true);
			// A spherical billboard's winding isn't guaranteed consistent
			// from every camera angle - double-sided is the safe default.
			SetCullFace(CullFace::DoubleSided);
			EnableBlending();
			SetBlendMode(ParticleBlendMode::AlphaBlend);
		}

		void SetBlendMode(uint32 mode)
		{
			if (mode == ParticleBlendMode::Additive)
				BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One);
			else
				BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		}
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
		// (on the GPU its particles are the GPU's: no copy of a million of them here)
		if (!SimulatedOnGPU())
		{
			cpuState.resize(desc.maxParticles);
			gpuState.resize(desc.maxParticles);
		}
		SetNumberInstances(0);
	}

	ParticleSystem::~ParticleSystem()
	{
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
		gpuResetPending = true;
		SetNumberInstances(0);
	}

	void ParticleSystem::SetGPUSimulation(const bool on)
	{
		if (on == desc.gpuSimulation) return;
		desc.gpuSimulation = on;
		// (what it had is the other side's: started again)
		liveCount = 0; emissionAccumulator = 0.0f; gpuResetPending = true; gpuCursor = 0;
		if (SimulatedOnGPU()) { std::vector<ParticleCPU>().swap(cpuState); std::vector<ParticleGPU>().swap(gpuState); }
		else { cpuState.assign(desc.maxParticles, ParticleCPU()); gpuState.assign(desc.maxParticles, ParticleGPU()); }
		if (!desc.looping && playing) pendingBurst = true;
		SetNumberInstances(0);
	}

	void ParticleSystem::UpdateGPU(const f64 time, const f32 dt)
	{
		// How many spawn: the burst asked for, and the stream's this frame (counted, not spawned one by one)
		uint32 spawn = 0;
		if (pendingBurst) { pendingBurst = false; spawn += desc.burstCount; }
		if (playing && desc.looping && desc.emissionRate > 0.0f)
		{
			emissionAccumulator += dt;
			const f32 interval = 1.0f / desc.emissionRate;
			const uint32 ticks = (uint32)(emissionAccumulator / interval);
			emissionAccumulator -= (f32)ticks * interval;
			spawn += ticks * desc.burstCount;
		}
		// The ring: as many slots as can be alive at once (each spawn takes the next, the oldest's)
		const f64 alive = desc.looping ? (f64)desc.emissionRate * desc.burstCount * Max(desc.maxLifetime, 0.01f) * 1.1 + desc.burstCount + 1.0 : (f64)desc.burstCount;
		const uint32 capacity = Max(1u, (uint32)Min((f64)desc.maxParticles, Max(alive, 1.0)));
		if (capacity != gpuStep.capacity) { gpuResetPending = true; gpuCursor = 0; }
		spawn = Min(spawn, capacity);
		Vec3 origin = Vec3::ZERO;
		if (GetOwner() != NULL) { GetOwner()->RefreshTransformation(); origin = GetOwner()->GetWorldPosition(); }
		gpuStep.id++;
		gpuStep.start = gpuCursor; gpuStep.count = spawn; gpuStep.capacity = capacity;
		gpuStep.origin = origin; gpuStep.time = (f32)time; gpuStep.dt = dt;
		gpuStep.reset = gpuResetPending; gpuResetPending = false;
		gpuCursor = (gpuCursor + spawn) % capacity;
		// (every slot drawn - the dead ones have no size)
		liveCount = capacity;
		SetNumberInstances(capacity);
	}

	void ParticleSystem::SetSizes(const f32 startSize, const f32 endSize, const f32 sizeRandomJitter)
	{
		desc.startSize = startSize;
		desc.endSize = endSize;
		desc.sizeRandomJitter = sizeRandomJitter;
	}

	void ParticleSystem::SetColors(const Vec4 &startColor, const Vec4 &endColor)
	{
		desc.startColor = startColor;
		desc.endColor = endColor;
	}

	void ParticleSystem::SetFade(const f32 fadeInFraction, const f32 fadeOutFraction)
	{
		desc.fadeInFraction = fadeInFraction;
		desc.fadeOutFraction = fadeOutFraction;
	}

	void ParticleSystem::SetLit(const bool on, const f32 forwardScatter)
	{
		lit = on;
		this->forwardScatter = std::max(-0.95f, std::min(0.95f, forwardScatter));
	}

	void ParticleSystem::SetSoftness(const f32 metres)
	{
		desc.softness = metres > 0.f ? metres : 0.f;
	}

	void ParticleSystem::SetEasing(const uchar sizeEase, const uchar colorEase)
	{
		desc.sizeEase = sizeEase;
		desc.colorEase = colorEase;
		if (!material) return;
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
		gpuResetPending = true; gpuCursor = 0;
		if (!SimulatedOnGPU())
		{
			cpuState.assign(capacity, ParticleCPU());
			gpuState.assign(capacity, ParticleGPU());
		}
		SetNumberInstances(0);
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
		// .z is the alive flag the vertex shader multiplies size by: every slot drawn is alive (the live ones are
		// kept at the front).
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
		if (SimulatedOnGPU()) { UpdateGPU(time, dt); return; }
		// (back from the GPU, or a 2D world asked for after it: its particles kept here again)
		if (cpuState.size() != desc.maxParticles) { cpuState.assign(desc.maxParticles, ParticleCPU()); gpuState.assign(desc.maxParticles, ParticleGPU()); liveCount = 0; }

		if (pendingBurst)
		{
			pendingBurst = false;
			for (uint32 i = 0; i < desc.burstCount; i++)
				SpawnParticle(time);
		}

		if (playing && desc.looping && desc.emissionRate > 0.0f)
		{
			PYROS_PROFILE_SCOPE("Particles.Emit");
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
		// (the 2D world to stop at, if this system was asked to)
		const Physics2DWorld* world2D = (desc.collide2D && GetOwner()) ? Physics2DWorld::ForScene(GetOwner()->GetScene()) : NULL;
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
			if (world2D)
			{
				// Along this frame's move as a ray: where it meets a body
				// the particle stops just short, and what speed it had
				// into the surface is turned back, as much as it bounces.
				const Vec3 was = gpuState[i].data0.xyz();
				const Vec2 from(was.x, was.y), to(worldPos.x, worldPos.y);
				if (fabsf(to.x - from.x) + fabsf(to.y - from.y) > 1e-6f)
				{
					const Physics2DWorld::RayHit2D hit = world2D->RayCast(from, to, NULL, NULL, desc.collisionMask);
					if (hit.hit)
					{
						Vec3 &v = cpuState[i].velocity;
						const f32 into = v.x * hit.normal.x + v.y * hit.normal.y;
						if (into < 0.f)
						{
							v.x -= (1.f + desc.bounce) * into * hit.normal.x;
							v.y -= (1.f + desc.bounce) * into * hit.normal.y;
						}
						worldPos.x = hit.point.x + hit.normal.x * 0.01f;
						worldPos.y = hit.point.y + hit.normal.y * 0.01f;
					}
				}
			}
			f32 normalizedAge = cpuState[i].lifetime > 0.0f ? (age / cpuState[i].lifetime) : 1.0f;

			gpuState[i].data0 = Vec4(worldPos, normalizedAge);
			gpuState[i].data1.x = cpuState[i].rotation;

			i++;
		}

		SetNumberInstances(liveCount);
	}



}
