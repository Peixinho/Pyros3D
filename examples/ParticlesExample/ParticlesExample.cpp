//============================================================================
// Name        : ParticlesExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "ParticlesExample.h"

using namespace p3d;

// Must match particle.glsl's PARTICLE_LIFETIME - both the shader's
// scale/fade curve and this removal threshold are keyed off the same
// particle age.
static const f32 PARTICLE_LIFETIME = 5.0f;

ParticlesExample::ParticlesExample() : BaseExample(1024, 768, "Pyros3D - Particles", WindowType::Close | WindowType::Resize) {}

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

	FPSCamera->SetPosition(Vec3(0, 2, 15));

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
	emissionAccumulator1 = emissionAccumulator2 = 0.f;

	emissionRate = 15.f;
	burstSize = 1;
	spread = 0.6f;
	riseSpeed = 2.0f;

	// Initialize ImGui
	InitImGui();
}

void ParticlesExample::SpawnParticle(ParticleEmitter* emitter, const f32 time)
{
	Particle p;
	p.Position = Vec4(0, 0, 0, ((rand() % 200) - 100) * 0.01f);
	f32 vx = ((rand() % 200) - 100) * 0.01f * spread;
	f32 vz = ((rand() % 200) - 100) * 0.01f * spread;
	p.Details = Vec4(vx, riseSpeed, vz, time);
	emitter->particles.push_back(p);
}

void ParticlesExample::UpdateEmitter(ParticleEmitter* emitter, const f32 time, const f32 dt)
{
	for (std::vector<Particle>::iterator i = emitter->particles.begin(); i != emitter->particles.end();)
	{
		Particle* p = &(*i);
		if (time - p->Details.w > PARTICLE_LIFETIME) {
			i = emitter->particles.erase(i);
		} else {
			p->Position.x += p->Details.x * dt;
			p->Position.y += p->Details.y * dt;
			p->Position.z += p->Details.z * dt;
			i++;
		}
	}

	if (emitter->particles.size() > 0)
	{
		emitter->SetNumberInstances(emitter->particles.size());
		emitter->particles_position_buffer->Buffer->Update(&emitter->particles[0], emitter->particles.size() * sizeof(Particle));
	}
}

void ParticlesExample::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic Here - continuous emission (accumulator-based, so the
	// rate is real particles/second regardless of frame rate) instead of
	// the previous fixed "one particle every 0.1s" - both emissionRate
	// and burstSize are live-tunable via DrawUI().
	f32 time = GetTime();
	f32 dt = time - lastTime;
	lastTime = time;
	if (dt < 0.f) dt = 0.f;
	if (dt > 0.1f) dt = 0.1f; // clamp - avoid a spawn burst after a stall/breakpoint

	f32 interval = 1.0f / (emissionRate > 0.01f ? emissionRate : 0.01f);

	emissionAccumulator1 += dt;
	while (emissionAccumulator1 >= interval)
	{
		emissionAccumulator1 -= interval;
		for (int32 i = 0; i < burstSize; i++)
			SpawnParticle(emitter1, time);
	}

	emissionAccumulator2 += dt;
	while (emissionAccumulator2 >= interval)
	{
		emissionAccumulator2 -= interval;
		for (int32 i = 0; i < burstSize; i++)
			SpawnParticle(emitter2, time);
	}

	UpdateEmitter(emitter1, time, dt);
	UpdateEmitter(emitter2, time, dt);

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	// Render ImGui
	RenderImGui();
}

void ParticlesExample::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();

	// Particle System Information
	if (ImGui::Begin("Particles Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Particle System Information");
		ImGui::Separator();

		ImGui::Text("Scene Objects:");
		ImGui::Text("  Emitter 1 particles: %zu", emitter1->particles.size());
		ImGui::Text("  Emitter 2 particles: %zu", emitter2->particles.size());
		ImGui::Text("  Material: CustomShaderMaterial (particle.glsl)");
		ImGui::Text("  Texture: smoke.png (alpha-blended)");
		ImGui::Text("  Lifetime: %.1fs (fade in/out, spherical billboard)", PARTICLE_LIFETIME);

		ImGui::Separator();
		ImGui::Text("Emission Controls:");
		ImGui::SliderFloat("Rate (particles/sec)", &emissionRate, 1.0f, 60.0f);
		ImGui::SliderInt("Burst Size", &burstSize, 1, 10);
		ImGui::SliderFloat("Spread", &spread, 0.0f, 2.0f);
		ImGui::SliderFloat("Rise Speed", &riseSpeed, 0.0f, 10.0f);

		ImGui::Separator();
		ImGui::Text("Performance:");
		ImGui::Text("  FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("  Resolution: %dx%d", Width, Height);

		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  Tab: Toggle mouse capture");
		ImGui::Text("  WASD: Move camera");
		ImGui::Text("  Mouse: Look around");
	}
	ImGui::End();
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

	// Renderer is deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}

ParticlesExample::~ParticlesExample() {}
