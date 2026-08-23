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

		// 0 = auto-estimate from motion parameters (see ParticleSystem.cpp).
		// Note: cull-testing is disabled by default regardless (see the
		// class comment) - this only matters if a caller re-enables it, or
		// for any future system that reads a component's bounding sphere.
		f32 boundingSphereRadius;

		ParticleSystemDesc();
	};

	// A real, reusable per-GameObject particle emitter - CPU-simulated
	// (see the comment on why: no compute-shader/storage-buffer
	// infrastructure exists on either backend today), self-contained
	// (attach to a GameObject and it runs itself every frame via the
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
	// and written directly into the per-instance GPU buffer.
	//
	// The per-instance buffer is allocated ONCE at full `maxParticles`
	// capacity and every frame's update writes that same fixed byte length
	// back (regardless of how many particles are actually alive) - this is
	// what keeps GeometryBuffer::Update() on the cheap sub-data-update path
	// instead of reallocating the whole GPU buffer every frame (see
	// ParticleSystem.cpp for the full explanation). Visibility of just the
	// live prefix is handled by SetNumberInstances(), independent of the
	// buffer's size.
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
		void SetSizes(const f32 startSize, const f32 endSize, const f32 sizeRandomJitter);
		void SetColors(const Vec4 &startColor, const Vec4 &endColor);
		// Curve each ramp follows across a particle's life. See sizeEase.
		void SetEasing(const uchar sizeEase, const uchar colorEase);
		void SetFade(const f32 fadeInFraction, const f32 fadeOutFraction);
		void SetRotationSpeed(const f32 minRotationSpeed, const f32 maxRotationSpeed) { desc.minRotationSpeed = minRotationSpeed; desc.maxRotationSpeed = maxRotationSpeed; }
		void SetBlendMode(const uint32 blendMode);
		// Switching to a one-shot burst stops the continuous emission
		// immediately; switching back to looping resumes it. Neither
		// touches particles already alive.
		void SetLooping(const bool looping) { desc.looping = looping; }
		// The sprite lives on the material, not in a uniform, so unlike
		// every setter above this one can't just write into `desc`.
		void SetTexture(const std::shared_ptr<Texture> &texture);

		// Re-allocates the fixed capacity. Unlike the setters above this is
		// NOT free (it reallocates the GPU buffer - see the class comment on
		// why capacity is otherwise chosen once) and it drops every live
		// particle, since slot indices don't survive the resize. Meant for
		// an authoring tool retuning an emitter, not for per-frame use.
		void SetMaxParticles(const uint32 maxParticles);

	private:

		// Reads the owner's CURRENT world position as the spawn origin -
		// note SceneGraph::Update()'s per-object call order means this can
		// be one frame stale relative to a GameObject::Update() override
		// that moves the emitter *this* frame (Update()/UpdateComponents()
		// runs before InternalUpdate() recomputes the world transform) -
		// invisible at real framerates, documented here so nobody "fixes"
		// it into something worse.
		void SpawnParticle(const f64 time);

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

		// Packed per-instance GPU attributes - see the class comment.
		// aParticleData0: xyz = world position, w = normalized age [0,1].
		// aParticleData1: x = rotation (radians), y = random seed [0,1],
		// z/w reserved. Must stay exactly 2 consecutive Vec4s - AttributeBuffer::
		// SendBuffer()'s interleaving assumes each attribute's raw byte
		// layout matches this struct's memory layout directly (same
		// pattern the old ParticleEmitter/Particle struct relied on).
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

		AttributeBuffer* particleBuffer;
		ParticleSystemMaterial* material;
		Renderable* quad;
	};

}

#endif	/* PARTICLESYSTEM_H */
