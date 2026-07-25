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
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class ParticleMaterial : public CustomShaderMaterial {
	public:
	Texture* tex;
	ParticleMaterial() : CustomShaderMaterial(STR(EXAMPLES_PATH)"/assets/particle.glsl")
	{
		tex = new Texture();
		tex->LoadTexture(STR(EXAMPLES_PATH)"/assets/smoke.png", TextureType::Texture);
		tex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		AddUniform(Uniform("uTime", Uniforms::DataUsage::Timer));
		int id = 0;
		AddUniform(Uniform("uTex0", Uniforms::DataType::Int, &id));
		textures.push_back(tex);

		SetTransparencyFlag(true);
		// smoke.png is a real alpha-edged sprite and particle.glsl computes
		// a proper fade-in/out curve into FragColor.a - neither did
		// anything visually before: IMaterial defaults blending *off*
		// (see its constructor), so every particle drew fully opaque
		// regardless of the shader's alpha. Standard alpha blending, and
		// double-sided since a spherical billboard's winding isn't
		// guaranteed consistent from every camera angle.
		EnableBlending();
		BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		SetCullFace(CullFace::DoubleSided);

		// See IMaterial.h's comment on extraUniforms[2] - matches
		// particle.glsl's ParticleVertParams block exactly. uTex0 is a
		// sampler, exempt from the UBO-wrap rule (opaque types).
		extraUniforms[0].binding = 42;
		extraUniforms[0].blockName = "ParticleVertParams";
		extraUniforms[0].size = 196;
		extraUniforms[0].scratch.resize(extraUniforms[0].size, 0);
		extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		extraUniforms[0].offsets["uViewMatrix"] = 64;
		extraUniforms[0].offsets["uModelMatrix"] = 128;
		extraUniforms[0].offsets["uTime"] = 192;
	}
};

struct Particle {
	Vec4 Position; // xyz position, w rotation speed
	Vec4 Details; // xyz velocity (world units/sec), w spawn time
	bool operator<(Particle& that);
};

class ParticleEmitter : public IRenderingInstancedComponent {
	public:

		std::vector<Particle> particles;
		AttributeBuffer* particles_position_buffer;

		ParticleEmitter(Renderable* particle, IMaterial* Material, const uint32 nrParticles, const f32 boundingSphere) : IRenderingInstancedComponent(particle, Material, nrParticles, boundingSphere)
		{
			particles.resize(nrParticles);

			particles_position_buffer = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Stream);
			particles_position_buffer->AddAttribute("aParticlePosition", Buffer::Attribute::Type::Vec4, NULL, 0, 1);
			particles_position_buffer->AddAttribute("aParticleDetails", Buffer::Attribute::Type::Vec4, NULL, 0, 1);
			particles_position_buffer->SendBuffer();

			this->AddBuffer(particles_position_buffer);
		}

		~ParticleEmitter()
		{
			// Remove buffers from last inserted to first because of divisor
			RemoveBuffer(particles_position_buffer);

			delete particles_position_buffer;
		}
};

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

	void SpawnParticle(ParticleEmitter* emitter, const f32 time);
	void UpdateEmitter(ParticleEmitter* emitter, const f32 time, const f32 dt);

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// GameObject
	GameObject *gSmoke1, *gSmoke2;
	// Rendering Component
	Renderable *smokeParticle1, *smokeParticle2;
	// Emitters
	ParticleEmitter* emitter1;
	ParticleEmitter* emitter2;
	// Material
	ParticleMaterial* particleMaterial;

	f32 lastTime;
	f32 emissionAccumulator1, emissionAccumulator2;

	// Tunable live via ImGui - see DrawUI().
	f32 emissionRate; // particles/second, per emitter
	int32 burstSize;  // particles spawned per emission tick
	f32 spread;       // horizontal jitter scale
	f32 riseSpeed;    // upward drift, world units/second
};

#endif	/* PARTICLESEXAMPLE_H */
