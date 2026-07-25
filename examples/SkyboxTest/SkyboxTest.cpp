//============================================================================
// Name        : SkyboxTest.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Real cubemap texture upload test (skybox)
//============================================================================

#include "SkyboxTest.h"

using namespace p3d;

SkyboxTest::SkyboxTest() : BaseExample(1024, 768, "Pyros3D - Skybox Cubemap Test", WindowType::Close | WindowType::Resize)
{

}

void SkyboxTest::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 2000.f);
}

void SkyboxTest::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 2000.f);

	FPSCamera->SetPosition(Vec3(0, 0, 0));

	// Real cubemap texture - 6 separate LoadTexture() calls, one per
	// face, onto the same Texture object (matches RacingGame.cpp's own
	// usage, the only other place in this repo that loads a real color
	// cubemap - RacingGame itself is disabled in CMakeLists, so this is
	// the only buildable exercise of this path).
	skyboxTexture = new Texture();
	skyboxTexture->LoadTexture(STR(EXAMPLES_PATH)"/RacingGame/assets/textures/skybox/negx.png", TextureType::CubemapNegative_X);
	skyboxTexture->LoadTexture(STR(EXAMPLES_PATH)"/RacingGame/assets/textures/skybox/negy.png", TextureType::CubemapNegative_Y);
	skyboxTexture->LoadTexture(STR(EXAMPLES_PATH)"/RacingGame/assets/textures/skybox/negz.png", TextureType::CubemapNegative_Z);
	skyboxTexture->LoadTexture(STR(EXAMPLES_PATH)"/RacingGame/assets/textures/skybox/posx.png", TextureType::CubemapPositive_X);
	skyboxTexture->LoadTexture(STR(EXAMPLES_PATH)"/RacingGame/assets/textures/skybox/posy.png", TextureType::CubemapPositive_Y);
	skyboxTexture->LoadTexture(STR(EXAMPLES_PATH)"/RacingGame/assets/textures/skybox/posz.png", TextureType::CubemapPositive_Z);
	skyboxTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	material = new GenericShaderMaterial(ShaderUsage::Skybox);
	material->SetSkyboxMap(skyboxTexture);

	// Create Game Object
	SkyboxObject = new GameObject();
	skyboxMesh = new Cube(1000, 1000, 1000);
	rSkybox = new RenderingComponent(skyboxMesh, material);
	SkyboxObject->Add(rSkybox);

	// Add GameObject to Scene
	Scene->Add(SkyboxObject);

	// Initialize ImGui
	InitImGui();
}

void SkyboxTest::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Keep the skybox centered on the camera
	SkyboxObject->SetPosition(FPSCamera->GetPosition());

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	// Render ImGui
	RenderImGui();
}

void SkyboxTest::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();

	if (ImGui::Begin("Skybox Cubemap Test Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Skybox Cubemap Test");
		ImGui::Separator();
		ImGui::Text("Exercises real cubemap texture upload:");
		ImGui::Text("  6 separate face images loaded via Texture::LoadTexture()");
		ImGui::Text("  ShaderUsage::Skybox + SetSkyboxMap()");
		ImGui::Separator();
		ImGui::Text("Performance:");
		ImGui::Text("  FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("  Resolution: %dx%d", Width, Height);
	}
	ImGui::End();
}

void SkyboxTest::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(SkyboxObject);

	SkyboxObject->Remove(rSkybox);

	// Delete
	delete material;
	delete skyboxTexture;
	delete rSkybox;
	delete SkyboxObject;
	delete skyboxMesh;

	// Renderer is deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}

SkyboxTest::~SkyboxTest() {}
