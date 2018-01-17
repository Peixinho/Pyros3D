//============================================================================
// Name        : ParticlesExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef PARTICLESEXAMPLE_H
#define	PARTICLESEXAMPLE_H

#if defined(_SDL)
#include "../WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#elif defined(_SDL2)
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#else
#include "../WindowManagers/SFML/SFMLContext.h"
#define ClassName SFMLContext
#endif

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
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
	ParticleMaterial() : CustomShaderMaterial("../examples/ParticlesExample/assets/particle.glsl")
	{
		tex = new Texture();
		tex->LoadTexture("../examples/ParticlesExample/assets/smoke.png", TextureType::Texture);
		tex->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		AddUniform(Uniform("uTime", Uniforms::DataUsage::Timer));
		int id = 0;
		AddUniform(Uniform("uTex0", Uniforms::DataType::Int, &id));
		textures.push_back(tex);

		SetTransparencyFlag(true);
	}
};

struct Particle {
	Vec4 Position; // xyz position, w rotation
	Vec4 Details; // xyz direction, w life time
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

		void Sort(GameObject* Camera);

		~ParticleEmitter()
		{
			// Remove buffers from last inserted to first because of divisor
			RemoveBuffer(particles_position_buffer);

			delete particles_position_buffer;
		}
};

class ParticlesExample : public ClassName
{

public:

	ParticlesExample();
	virtual ~ParticlesExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Scene
	SceneGraph* Scene;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* Camera;
	// GameObject
	GameObject *gSmoke1, *gSmoke2;
	// Rendering Component
	Renderable *smokeParticle1, *smokeParticle2;
	// Emitters
	ParticleEmitter* emitter1;
	ParticleEmitter* emitter2;
	// Material
	ParticleMaterial* particleMaterial;


	// Events
	void MoveFrontPress(Event::Input::Info e);
	void MoveBackPress(Event::Input::Info e);
	void StrafeLeftPress(Event::Input::Info e);
	void StrafeRightPress(Event::Input::Info e);
	void MoveFrontRelease(Event::Input::Info e);
	void MoveBackRelease(Event::Input::Info e);
	void StrafeLeftRelease(Event::Input::Info e);
	void StrafeRightRelease(Event::Input::Info e);
	void LookTo(Event::Input::Info e);

	float counterX, counterY;
	Vec2 mouseCenter, mouseLastPosition, mousePosition;
	bool _moveFront, _moveBack, _strafeLeft, _strafeRight;

	f32 lastTime;
};

#endif	/* PARTICLESEXAMPLE_H */

