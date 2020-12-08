//============================================================================
// Name        : ParticlesExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "ParticlesExample.h"

using namespace p3d;

ParticlesExample::ParticlesExample() : BaseExample(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void ParticlesExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);
}

void ParticlesExample::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	FPSCamera->SetPosition(Vec3(0, 10, 80));

	particleMaterial = new ParticleMaterial();

	// Create Game Object
	gSmoke1 = new GameObject();
	gSmoke2 = new GameObject();
	smokeParticle1 = new Plane(1,1);
	smokeParticle2 = new Plane(1,1);

	// Create Particle Emitter
	emitter1 = new ParticleEmitter(smokeParticle1, particleMaterial, 0, 100);
	emitter2 = new ParticleEmitter(smokeParticle2, particleMaterial, 0, 100);

	gSmoke1->Add(emitter1);
	gSmoke1->SetPosition(Vec3(-10,0,0));
	Scene->Add(gSmoke1);

	gSmoke2->Add(emitter2);
	gSmoke2->SetPosition(Vec3(10,0,0));
	Scene->Add(gSmoke2);

	lastTime = GetTime();
}

void ParticlesExample::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic HereW

	f32 time = GetTime();
	if (time-lastTime > 0.1)
	{
		lastTime = time;
		{
			Particle p;
			p.Position = Vec4(0,0,0,(rand()%20-10.f)*0.1f);
			p.Details = Vec4((rand()%40-20)*0.01f,3.f,(rand()%40-20)*0.01f,time);
			emitter1->particles.push_back(p);
		}
		{
			Particle p;
			p.Position = Vec4(0,0,0,(rand()%20-10.f)*0.1f);
			p.Details = Vec4((rand()%40-20)*0.01f,3.f,(rand()%40-20)*0.01f,time);
			emitter2->particles.push_back(p);
		}
	}

	for (std::vector<Particle>::iterator i=emitter1->particles.begin();i!=emitter1->particles.end();)
	{
		Particle* p = &(*i);
		if (time-p->Details.w>10.0) {
			i=emitter1->particles.erase(i);
		} else {
			p->Position.x += p->Details.x * 0.005f;
			p->Position.y += p->Details.y * 0.005f;
			p->Position.z += p->Details.z * 0.005f;
			i++;
		}
	}

	for (std::vector<Particle>::iterator i=emitter2->particles.begin();i!=emitter2->particles.end();)
	{
		Particle* p = &(*i);
		if (time-p->Details.w>10.0) {
			i=emitter2->particles.erase(i);
		} else {
			p->Position.x += p->Details.x * 0.005f;
			p->Position.y += p->Details.y * 0.005f;
			p->Position.z += p->Details.z * 0.005f;
			i++;
		}
	}

	if (emitter1->particles.size() > 0)
	{
		emitter1->SetNumberInstances(emitter1->particles.size());
		emitter1->particles_position_buffer->Buffer->Update(&emitter1->particles[0], emitter1->particles.size() * sizeof(Particle));
	}
	if (emitter2->particles.size() > 0)
	{
		emitter2->SetNumberInstances(emitter2->particles.size());
		emitter2->particles_position_buffer->Buffer->Update(&emitter2->particles[0], emitter2->particles.size() * sizeof(Particle));
	}

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
}

void ParticlesExample::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	gSmoke1->Remove(emitter1);
	Scene->Remove(gSmoke1);
	gSmoke2->Remove(emitter2);
	Scene->Remove(gSmoke2);

	// Delete
	delete emitter1;
	delete emitter2;
	delete particleMaterial;
	delete gSmoke1;
	delete gSmoke2;
	delete smokeParticle1;
	delete smokeParticle2;
	delete Renderer;

	BaseExample::Shutdown();
}

ParticlesExample::~ParticlesExample() {}
