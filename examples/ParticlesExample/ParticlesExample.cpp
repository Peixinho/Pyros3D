//============================================================================
// Name        : ParticlesExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "ParticlesExample.h"

using namespace p3d;

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

	particleTexture = new Texture();
	particleTexture->LoadTexture(STR(EXAMPLES_PATH)"/assets/smoke.png", TextureType::Texture);
	particleTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	// Smoke: soft, gray, alpha-blended, slow rise.
	smokeEmissionRate = 15.f;
	smokeSpread = 0.6f;
	smokeRiseSpeed = 2.0f;

	ParticleSystemDesc smokeDesc;
	smokeDesc.maxParticles = 200;
	smokeDesc.texture = particleTexture;
	smokeDesc.looping = true;
	smokeDesc.emissionRate = smokeEmissionRate;
	smokeDesc.burstCount = 1;
	smokeDesc.minLifetime = 3.0f;
	smokeDesc.maxLifetime = 5.0f;
	smokeDesc.direction = Vec3(0, 1, 0);
	smokeDesc.spreadAngle = (f32)DEGTORAD(20.0);
	smokeDesc.minSpeed = smokeRiseSpeed * 0.8f;
	smokeDesc.maxSpeed = smokeRiseSpeed * 1.2f;
	smokeDesc.gravity = Vec3::ZERO;
	smokeDesc.damping = 0.3f;
	smokeDesc.startSize = 0.8f;
	smokeDesc.endSize = 3.5f;
	smokeDesc.sizeRandomJitter = 0.3f;
	smokeDesc.startColor = Vec4(0.6f, 0.6f, 0.6f, 1.0f);
	smokeDesc.endColor = Vec4(0.3f, 0.3f, 0.3f, 0.0f);
	smokeDesc.fadeInFraction = 0.1f;
	smokeDesc.fadeOutFraction = 0.6f;
	smokeDesc.minRotationSpeed = -0.4f;
	smokeDesc.maxRotationSpeed = 0.4f;
	smokeDesc.blendMode = ParticleBlendMode::AlphaBlend;

	smoke = new ParticleSystem(smokeDesc);
	gSmoke = new GameObject();
	gSmoke->Add(smoke);
	gSmoke->SetPosition(Vec3(-4, 0, 0));
	Scene->Add(gSmoke);

	// Embers: bright, additive, warm color gradient, faster/smaller -
	// demonstrates the second blend mode and a real color-over-life
	// gradient, reusing the same smoke.png sprite (additive blending
	// against a bright gradient is what gives it a glow, not a different
	// texture).
	ParticleSystemDesc emberDesc;
	emberDesc.maxParticles = 150;
	emberDesc.texture = particleTexture;
	emberDesc.looping = true;
	emberDesc.emissionRate = 30.f;
	emberDesc.burstCount = 1;
	emberDesc.minLifetime = 1.0f;
	emberDesc.maxLifetime = 1.8f;
	emberDesc.direction = Vec3(0, 1, 0);
	emberDesc.spreadAngle = (f32)DEGTORAD(25.0);
	emberDesc.minSpeed = 3.0f;
	emberDesc.maxSpeed = 5.0f;
	emberDesc.gravity = Vec3(0, -1.5f, 0);
	emberDesc.damping = 0.1f;
	emberDesc.startSize = 0.6f;
	emberDesc.endSize = 0.15f;
	emberDesc.sizeRandomJitter = 0.4f;
	emberDesc.startColor = Vec4(1.0f, 0.9f, 0.4f, 1.0f);
	emberDesc.endColor = Vec4(1.0f, 0.2f, 0.0f, 1.0f);
	emberDesc.fadeInFraction = 0.05f;
	emberDesc.fadeOutFraction = 0.5f;
	emberDesc.minRotationSpeed = -2.0f;
	emberDesc.maxRotationSpeed = 2.0f;
	emberDesc.blendMode = ParticleBlendMode::Additive;

	embers = new ParticleSystem(emberDesc);
	gEmbers = new GameObject();
	gEmbers->Add(embers);
	gEmbers->SetPosition(Vec3(4, 0, 0));
	Scene->Add(gEmbers);

	// Initialize ImGui
	InitImGui();
}

void ParticlesExample::Update()
{
	// Update - Game Loop

	// Update Scene - this alone drives both particle systems' simulation
	// (spawn/age/integrate/upload) via IComponent::Update(), automatically
	// called for every component on every GameObject. No manual per-frame
	// particle code needed here, unlike the old hand-rolled version of
	// this example.
	Scene->Update(GetTime());

	BaseExample::Update();

	// Live-tune the smoke system from the ImGui sliders below.
	smoke->SetEmissionRate(smokeEmissionRate);
	smoke->SetSpeed(smokeRiseSpeed * 0.8f, smokeRiseSpeed * 1.2f);
	smoke->SetSpread((f32)DEGTORAD(20.0 * smokeSpread));

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
		ImGui::Text("  Smoke particles: %u (AlphaBlend)", smoke->GetLiveParticleCount());
		ImGui::Text("  Ember particles: %u (Additive)", embers->GetLiveParticleCount());
		ImGui::Text("  Component: p3d::ParticleSystem (engine feature)");
		ImGui::Text("  Texture: smoke.png (shared, alpha-blended)");

		ImGui::Separator();
		ImGui::Text("Smoke Emission Controls:");
		ImGui::SliderFloat("Rate (particles/sec)", &smokeEmissionRate, 1.0f, 60.0f);
		ImGui::SliderFloat("Spread", &smokeSpread, 0.0f, 2.0f);
		ImGui::SliderFloat("Rise Speed", &smokeRiseSpeed, 0.0f, 10.0f);

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
	gSmoke->Remove(smoke);
	Scene->Remove(gSmoke);
	gEmbers->Remove(embers);
	Scene->Remove(gEmbers);

	// Delete
	delete smoke;
	delete embers;
	delete gSmoke;
	delete gEmbers;
	delete particleTexture;

	// Renderer is deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}

ParticlesExample::~ParticlesExample() {}
