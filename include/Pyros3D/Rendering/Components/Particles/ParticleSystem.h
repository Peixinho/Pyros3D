//============================================================================
// Name        : ParticleSystem.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Particle System
//============================================================================

#ifndef PARTICLESYSTEM_H
#define	PARTICLESYSTEM_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Core/Math/Easing.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Core/Math/Random.h>
#include <Pyros3D/Other/Export.h>
#include <vector>
#include <memory>

namespace p3d {

	class Texture;
	class ParticleSystemMaterial;

	namespace ParticleBlendMode {
		enum {
			AlphaBlend = 0,
			Additive
		};
	}

	// Configuration for a ParticleSystem - see the class comment below for
	// the overall design. Every field has a sane default (ParticleSystemDesc());
	// `texture` is the only one that must be set explicitly (there's no
	// sane default sprite to fall back to).
	struct PYROS3D_API ParticleSystemDesc
	{
		// Fixed capacity, chosen once - see ParticleSystem's class comment
		// for why this is a hard cap (spawns beyond it are silently
		// dropped) rather than a growable buffer.
		uint32 maxParticles;

		// Shared with the material (and any other holders) - ParticleSystem
		// does not uniquely own this texture.
		std::shared_ptr<Texture> texture;

		// true: continuous emission at `emissionRate`, `burstCount`
		// particles per emission tick. false: `burstCount` particles all
		// spawn once at Play(), no further emission (a one-shot burst,
		// e.g. an explosion).
		bool looping;
		f32 emissionRate;   // particles/second, only used if looping
		uint32 burstCount;

		f32 minLifetime, maxLifetime;   // seconds, randomized per particle

		// Cone emission: origin is the owning GameObject's world position
		// at the instant each particle spawns (see SpawnParticle()'s
		// comment on the one-frame lag this implies); `direction` is a
		// fixed world-space axis (NOT rotated by the emitter's own
		// orientation - a deliberate v1 simplification, see
		// ParticleSystem's class comment).
		Vec3 direction;
		f32 spreadAngle;     // cone half-angle, radians (0 = no spread)
		f32 minSpeed, maxSpeed;

		Vec3 gravity;        // world-space acceleration, applied every frame
		f32 damping;         // velocity *= (1 - damping*dt) every frame, 0 = none

		// In a 2D scene: particles stop at the bodies of the 2D world -
		// sparks that bounce along a floor, rain that ends at a roof, dust
		// that settles. Each one is a point moved along a ray, so it costs
		// a ray a particle a frame: for tens or hundreds, not thousands.
		// `bounce` is how much of its speed into the surface it keeps (0
		// lands and slides, 1 loses nothing), `collisionMask` which
		// collision layers it sees.
		bool collide2D;
		f32 bounce;
		uint32 collisionMask;

		f32 startSize, endSize;     // world-space diameter, ramped over normalized age
		f32 sizeRandomJitter;        // +/- fraction of size, randomized per particle

		Vec4 startColor, endColor;   // ramped over normalized age, multiplies the texture

		// How each ramp travels from start to end - InterpolationMode from
		// Core/Math/Easing.h, the same enum keyframes use. Linear (the v1
		// behaviour) is the default, so an existing system is unchanged.
		//
		// A ramp has two ends rather than a series of keys, so INTERP_BEZIER
		// has no second key to take its incoming tangent from and is treated
		// as linear; the editor does not offer it here. INTERP_STEP holds the
		// start value for the particle's whole life, which is the useful
		// reading of "no interpolation" for a ramp.
		uchar sizeEase, colorEase;

		// Alpha fades in over [0,fadeInFraction] and out over
		// [fadeOutFraction,1] of normalized age.
		f32 fadeInFraction, fadeOutFraction;

		f32 minRotationSpeed, maxRotationSpeed;   // radians/sec, randomized per particle

		uint32 blendMode;    // ParticleBlendMode::*

		// Metres over which a particle fades where it meets what is behind it (the deferred
		// renderer's depth): smoke on the ground without a hard line along it. 0: off.
		f32 softness;

		// Simulated on the GPU (a compute pass the renderer runs): hundreds of thousands, a million, for the cost of a
		// few thousand on the CPU. What it does not do there: stop at a 2D world (collide2D keeps it on the CPU), keep its
		// see-through particles in depth order. The CPU keeps only the emitter: how many spawn each frame, and from where.
		bool gpuSimulation;

		// 0 = auto-estimate from motion parameters (see ParticleSystem.cpp).
		// Note: cull-testing is disabled by default regardless (see the
		// class comment) - this only matters if a caller re-enables it, or
		// for any future system that reads a component's bounding sphere.
		f32 boundingSphereRadius;

		ParticleSystemDesc();
	};

	// A real, reusable per-GameObject particle emitter - simulated on the
	// CPU, self-contained (attach to a GameObject and it runs itself every frame via the
	// IComponent::Update() hook SceneGraph::Update() already calls
	// automatically - no manual per-frame driving needed by the owner,
	// unlike the old hand-rolled ParticlesExample code this replaces).
	//
	// One ParticleSystem = one emitter/config (matches e.g. one
	// DirectionalLight per component) - a "fire + smoke" effect is two
	// ParticleSystems on two GameObjects.
	//
	// Simulation is world-space: once a particle spawns, it is NOT dragged
	// by the owning GameObject moving afterward - only new spawns read the
	// owner's current world position. This also means the shader needs
	// neither a per-object model matrix nor a global clock uniform -
	// position and normalized age are computed here on the CPU every frame
	// (GetParticleData), and the renderer draws the live ones from there.
	//
	// Cull-testing is disabled in the constructor (DisableCullTest()) -
	// see ParticleSystem.cpp for why a GameObject-anchored bounding sphere
	// is the wrong volume once particles are decoupled into world space.
	class PYROS3D_API ParticleSystem : public IRenderingInstancedComponent
	{
	public:
		ParticleSystem(const ParticleSystemDesc &desc);
		virtual ~ParticleSystem();

		// The one real lifecycle override - see the class comment. Init()/
		// Destroy()/Register()/Unregister() are deliberately NOT overridden:
		// Init()/Destroy() are dead hooks nothing in the engine calls, and
		// Register()/Unregister() are already fully handled by the
		// inherited RenderingComponent implementation.
		virtual void Update(const f64 time);

		void Play();
		// Stops new emission; particles already alive finish their
		// lifetime and fade out naturally.
		void Stop();
		// Kills every live particle immediately.
		void Clear();

		uint32 GetLiveParticleCount() const { return liveCount; }
		bool IsPlaying() const { return playing; }

		virtual uint32 GetComponentType() const { return ComponentType::ParticleSystem; }

		// Full current config - e.g. for scene serialization, which
		// otherwise has no way to read back any of this (every setter
		// below is write-only without this).
		const ParticleSystemDesc &GetDesc() const { return desc; }

		// Live setters - each takes effect immediately for new spawns
		// (and, where the field affects the shader directly - color/size/
		// fade/blend mode - for already-alive particles too, since those
		// are read from the material's uniforms every frame, not baked
		// per-particle at spawn time). None of these retroactively update
		// the owning GameObject's cached bounding sphere - see
		// ParticleSystemDesc::boundingSphereRadius's comment.
		void SetEmissionRate(const f32 rate) { desc.emissionRate = rate; }
		void SetBurstCount(const uint32 count) { desc.burstCount = count; }
		void SetLifetime(const f32 minLifetime, const f32 maxLifetime) { desc.minLifetime = minLifetime; desc.maxLifetime = maxLifetime; }
		void SetDirection(const Vec3 &direction) { desc.direction = direction; }
		void SetSpread(const f32 spreadAngle) { desc.spreadAngle = spreadAngle; }
		void SetSpeed(const f32 minSpeed, const f32 maxSpeed) { desc.minSpeed = minSpeed; desc.maxSpeed = maxSpeed; }
		void SetGravity(const Vec3 &gravity) { desc.gravity = gravity; }
		void SetDamping(const f32 damping) { desc.damping = damping; }
		void SetCollide2D(const bool on, const f32 bounce = 0.3f, const uint32 mask = 0xFFFFFFFFu)
		{ desc.collide2D = on; desc.bounce = bounce < 0.f ? 0.f : (bounce > 1.f ? 1.f : bounce); desc.collisionMask = mask; }
		bool GetCollide2D() const { return desc.collide2D; }
		void SetSizes(const f32 startSize, const f32 endSize, const f32 sizeRandomJitter);
		void SetColors(const Vec4 &startColor, const Vec4 &endColor);
		// Curve each ramp follows across a particle's life. See sizeEase.
		void SetEasing(const uchar sizeEase, const uchar colorEase);
		void SetFade(const f32 fadeInFraction, const f32 fadeOutFraction);
		void SetRotationSpeed(const f32 minRotationSpeed, const f32 maxRotationSpeed) { desc.minRotationSpeed = minRotationSpeed; desc.maxRotationSpeed = maxRotationSpeed; }
		void SetBlendMode(const uint32 blendMode);
		void SetSoftness(const f32 metres);
		// Lit (off by default: the colours are drawn as they are, as before): the scene's sun on each particle -
		// scattered forward as mist and spray are, `forwardScatter` the Henyey-Greenstein g (0 even, toward 1
		// a glow round the sun behind it) - and the sky's light over it.
		void SetLit(const bool on, const f32 forwardScatter = 0.6f);
		// Switching to a one-shot burst stops the continuous emission
		// immediately; switching back to looping resumes it. Neither
		// touches particles already alive.
		void SetLooping(const bool looping) { desc.looping = looping; }
		// The sprite lives on the material, not in a uniform, so unlike
		// every setter above this one can't just write into `desc`.
		void SetTexture(const std::shared_ptr<Texture> &texture);

		// Re-allocates the fixed capacity. Unlike the setters above this is
		// NOT free (it reallocates the particles' state) and it drops every live
		// particle, since slot indices don't survive the resize. Meant for
		// an authoring tool retuning an emitter, not for per-frame use.
		void SetMaxParticles(const uint32 maxParticles);

		// The live particles as a renderer draws them: two vec4s each - xyz world position, w normalized age; then
		// rotation, random seed, alive (1/0) - GetLiveParticleCount() of them (the CPU simulation's)
		const Vec4* GetParticleData() const { return gpuState.empty() ? NULL : &gpuState[0].data0; }

		// On the GPU (gpuSimulation, and not stopping at a 2D world): what this frame's step asks of the renderer's
		// compute pass - the ring's slots from `start`, `count` of them, started this frame at `origin`; `capacity`
		// slots in all (what can be alive at once: the rate times the longest life, at most maxParticles); `reset`:
		// every slot emptied first. `id` goes up each Update: a step is run once, by the first view that draws it.
		bool SimulatedOnGPU() const { return desc.gpuSimulation && !desc.collide2D; }
		void SetGPUSimulation(const bool on);
		struct GpuStep { uint64 id = 0; uint32 start = 0, count = 0, capacity = 0; Vec3 origin; f32 time = 0.f, dt = 0.f; bool reset = false; };
		const GpuStep &GetGpuStep() const { return gpuStep; }
		// Lit by the scene (SetLit), and how much of the light it scatters onward
		bool IsLit() const { return lit; }
		f32 GetForwardScatter() const { return forwardScatter; }

	private:
		bool lit = false;
		f32 forwardScatter = 0.6f;

		// Reads the owner's CURRENT world position as the spawn origin -
		// note SceneGraph::Update()'s per-object call order means this can
		// be one frame stale relative to a GameObject::Update() override
		// that moves the emitter *this* frame (Update()/UpdateComponents()
		// runs before InternalUpdate() recomputes the world transform) -
		// invisible at real framerates, documented here so nobody "fixes"
		// it into something worse.
		void SpawnParticle(const f64 time);
		void UpdateGPU(const f64 time, const f32 dt);
		GpuStep gpuStep;
		uint32 gpuCursor = 0;
		bool gpuResetPending = true;

		// Per-particle CPU-only simulation state (never uploaded to the
		// GPU directly - GPU only ever sees the derived ParticleGPU below).
		struct ParticleCPU
		{
			Vec3 velocity;
			f32 lifetime;
			f32 spawnTime;
			f32 rotationSpeed;
			f32 rotation;
			f32 randomSeed;
		};

		// What the renderer draws a particle from - see GetParticleData.
		// data0: xyz = world position, w = normalized age [0,1].
		// data1: x = rotation (radians), y = random seed [0,1], z = alive,
		// w reserved. Exactly 2 consecutive Vec4s (the renderer uploads them as they lie).
		struct ParticleGPU
		{
			Vec4 data0;
			Vec4 data1;
		};

		ParticleSystemDesc desc;
		Math::Random rng;

		std::vector<ParticleCPU> cpuState;   // fixed at desc.maxParticles
		std::vector<ParticleGPU> gpuState;   // fixed at desc.maxParticles, mirrors cpuState 1:1
		uint32 liveCount;

		f32 emissionAccumulator;
		f64 lastUpdateTime;
		bool hasUpdatedOnce;
		bool playing;
		// Play() on a one-shot emitter defers its burst to the next Update()
		// instead of spawning immediately - see Play() in ParticleSystem.cpp.
		bool pendingBurst;

		ParticleSystemMaterial* material;
		Renderable* quad;
	};

}

#endif	/* PARTICLESYSTEM_H */
