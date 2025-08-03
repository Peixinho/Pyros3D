//============================================================================
// Name        : CustomMaterial.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "CustomMaterial.h"

using namespace p3d;

CustomMaterial::CustomMaterial() : BaseExample(1024, 768, "Pyros3D - Custom Material", WindowType::Close | WindowType::Resize)
{

}

void CustomMaterial::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void CustomMaterial::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);

	// Custom Material
	Material = new CustomMaterialExample();

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(30, 30, 30);
	rCube = new RenderingComponent(cubeMesh, Material);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);
	
	// Initialize ImGui
	InitImGui();
}

void CustomMaterial::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	RenderImGui();
}

void CustomMaterial::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Custom Material System Information
	if (ImGui::Begin("Custom Material System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Custom Material System Information");
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Cube: Rotating cube with custom shader (30x30x30)");
		ImGui::Text("  Material: CustomShaderMaterial");
		
		ImGui::Separator();
		
		// Custom Material Information
		ImGui::Text("Custom Material System:");
		ImGui::Text("  Material Type: CustomShaderMaterial");
		ImGui::Text("  Shader File: custommaterialshader.glsl");
		ImGui::Text("  Uniforms: Projection, View, Model matrices");
		ImGui::Text("  Color Uniform: Random color per frame");
		ImGui::Text("  PreRender: Updates color uniform");
		
		ImGui::Separator();
		
		// Rendering Information
		ImGui::Text("Rendering System:");
		ImGui::Text("  Renderer: ForwardRenderer");
		ImGui::Text("  Shader: Custom GLSL shader");
		ImGui::Text("  Material: CustomShaderMaterial");
		
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
		ImGui::Text("  Color changes randomly each frame");
	}
	ImGui::End();
}

void CustomMaterial::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);

	CubeObject->Remove(rCube);

	// Delete
	delete rCube;
	delete CubeObject;
	delete cubeMesh;
	delete Material;
	delete Renderer;
	BaseExample::Shutdown();
}

CustomMaterial::~CustomMaterial() {}
