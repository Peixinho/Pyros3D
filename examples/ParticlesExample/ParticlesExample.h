//============================================================================
// Name        : ParticlesExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Particles Example
//============================================================================

#ifndef PARTICLESEXAMPLE_H
#define	PARTICLESEXAMPLE_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>

using namespace p3d;

// Demonstrates the real, reusable p3d::ParticleSystem engine component (see
// include/Pyros3D/Rendering/Components/Particles/ParticleSystem.h) - two
// emitters showing both blend modes: smoke (AlphaBlend, gray, gentle rise)
// and embers (Additive, warm color gradient, faster/smaller). Both reuse
// smoke.png (the only particle-shaped sprite shipped with the examples) -
// additive blending against a bright color gradient is what gives the
// ember system its glow, not a different texture.
class ParticlesExample : public BaseExample
{

public:

	ParticlesExample();
	virtual ~ParticlesExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;

	// GameObjects
	GameObject *gSmoke, *gEmbers;
	// Particle Systems - self-contained components, no per-frame driving
	// code needed here beyond Scene->Update() (already called for every
	// example) - see ParticleSystem::Update()'s comment.
	ParticleSystem *smoke, *embers;

	// Shared texture, owned here (both systems reference it, neither owns it)
	Texture* particleTexture;

	// Live-tunable via ImGui (smoke system only, to keep the panel simple)
	f32 smokeEmissionRate;
	f32 smokeSpread;
	f32 smokeRiseSpeed;
};

#endif	/* PARTICLESEXAMPLE_H */
