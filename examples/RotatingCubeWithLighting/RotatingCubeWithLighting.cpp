//============================================================================
// Name        : RotatingCubeWithLighting.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCubeWithLighting.h"

using namespace p3d;

RotatingCubeWithLighting::RotatingCubeWithLighting() : BaseExample(1024, 768, "Pyros3D - Rotating Cube With Lighting", WindowType::Close | WindowType::Resize)
{

}

void RotatingCubeWithLighting::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void RotatingCubeWithLighting::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);

	FPSCamera->SetPosition(Vec3(0, 10, 80));

	// Material
	Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
	Diffuse->SetColor(Vec4(1, 0, 0, 1));

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);

	Scene->Add(Light);

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh, Diffuse);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);
	
	// Initialize ImGui
	InitImGui();
}

void RotatingCubeWithLighting::Update()
{
	// Update - Game Loop

	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime(), 0));

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	// Render ImGui
	RenderImGui();
}

void RotatingCubeWithLighting::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Lighting System Information
	if (ImGui::Begin("Lighting System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Lighting System Information");
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Cube: Rotating red cube");
		ImGui::Text("  Directional Light: White light from (-1, -1, 0)");
		
		ImGui::Separator();
		
		// Lighting Information
		ImGui::Text("Lighting System:");
		ImGui::Text("  Renderer: ForwardRenderer");
		ImGui::Text("  Light Type: Directional Light");
		ImGui::Text("  Light Color: White (1, 1, 1, 1)");
		ImGui::Text("  Light Direction: (-1, -1, 0)");
		ImGui::Text("  Shader Usage: Color | Diffuse");
		
		ImGui::Separator();
		
		// Material Information
		ImGui::Text("Material Properties:");
		ImGui::Text("  Color: Red (1, 0, 0, 1)");
		ImGui::Text("  Shader: GenericShaderMaterial");
		ImGui::Text("  Diffuse Lighting: Enabled");
		
		ImGui::Separator();
		
		// Performance Information
		ImGui::Text("Performance:");
		ImGui::Text("  FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("  Resolution: %dx%d", Width, Height);
		ImGui::Text("  Camera Position: (%.1f, %.1f, %.1f)", 
			FPSCamera->GetPosition().x, 
			FPSCamera->GetPosition().y, 
			FPSCamera->GetPosition().z);
		
		ImGui::Separator();
		
		// Controls
		ImGui::Text("Controls:");
		ImGui::Text("  Tab: Toggle mouse capture");
		ImGui::Text("  WASD: Move camera");
		ImGui::Text("  Mouse: Look around");
		ImGui::Text("  Cube rotates automatically");
	}
	ImGui::End();
}

void RotatingCubeWithLighting::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Light);

	CubeObject->Remove(rCube);
	Light->Remove(dLight);

	// Delete
	delete rCube;
	delete CubeObject;
	delete cubeMesh;
	delete dLight;
	delete Light;
	delete Diffuse;
	delete Renderer;

	BaseExample::Shutdown();
}

RotatingCubeWithLighting::~RotatingCubeWithLighting() {}
