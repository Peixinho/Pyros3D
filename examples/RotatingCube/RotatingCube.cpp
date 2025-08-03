//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"
#include <Pyros3D/Other/PyrosGL.h>

using namespace p3d;

RotatingCube::RotatingCube() : BaseExample(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void RotatingCube::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void RotatingCube::Init()
{
	// Initialization
	BaseExample::Init();

	printf("RotatingCube::Init() - Starting initialization\n");

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	printf("RotatingCube::Init() - Renderer created\n");

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);
	printf("RotatingCube::Init() - Projection set\n");

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);
	Scene->Add(Light);
	printf("RotatingCube::Init() - Light added\n");

	// Create material
	Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
	Diffuse->SetColor(Vec4(1, 0, 0, 1));
	printf("RotatingCube::Init() - Material created (red)\n");

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh, Diffuse);
	CubeObject->Add(rCube);
	printf("RotatingCube::Init() - Cube object created and added to scene\n");

	// Add GameObject to Scene
	Scene->Add(CubeObject);
	printf("RotatingCube::Init() - Cube added to scene\n");
	
	// Set camera position to see the cube
	FPSCamera->SetPosition(Vec3(0, 0, 100));
	printf("RotatingCube::Init() - Camera position set to: (%.1f, %.1f, %.1f)\n", 
		FPSCamera->GetPosition().x, FPSCamera->GetPosition().y, FPSCamera->GetPosition().z);
	
	// Initialize ImGui after renderer and materials are set up
	InitImGui();
	printf("RotatingCube::Init() - ImGui initialized\n");
	
	// Initialize ImGui controls
	rotationSpeed = 1.0f;
	cubeScale = 1.0f;
	cubeColor = Vec4(1, 0, 0, 1);
}

void RotatingCube::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime() * rotationSpeed, 0));

	// Render ImGui AFTER the main scene to avoid interference
	//RenderImGui();
	
	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	
	// Render ImGui AFTER the main scene to avoid interference
	RenderImGui();
	
	static int frameCount = 0;
	if (frameCount++ % 60 == 0) { // Print every 60 frames
		printf("Update - Camera: (%.1f, %.1f, %.1f), Cube: (%.1f, %.1f, %.1f)\n", 
			FPSCamera->GetPosition().x, FPSCamera->GetPosition().y, FPSCamera->GetPosition().z,
			CubeObject->GetPosition().x, CubeObject->GetPosition().y, CubeObject->GetPosition().z);
	}
}

void RotatingCube::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Light);

	CubeObject->Remove(rCube);
	Light->Remove(dLight);

	// Delete
	delete cubeMesh;
	delete rCube;
	delete CubeObject;
	delete Diffuse;
	delete dLight;
	delete Light;
	delete Renderer;

	BaseExample::Shutdown();
}

void RotatingCube::DrawUI()
{
	// Call base UI first
	BaseExample::DrawUI();
	
	// Create ImGui window for cube controls
	ImGui::Begin("Cube Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	
	ImGui::Text("Interactive Cube Controls");
	ImGui::Separator();
	
	// Rotation speed slider
	if (ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 5.0f, "%.2f")) {
		printf("Rotation speed changed to: %.2f\n", rotationSpeed);
	}
	
	// Cube scale slider
	if (ImGui::SliderFloat("Cube Scale", &cubeScale, 0.1f, 3.0f, "%.2f")) {
		CubeObject->SetScale(Vec3(cubeScale, cubeScale, cubeScale));
		printf("Cube scale changed to: %.2f\n", cubeScale);
	}
	
	// Color picker
	if (ImGui::ColorEdit4("Cube Color", &cubeColor.x)) {
		Diffuse->SetColor(cubeColor);
		printf("Cube color changed to: (%.2f, %.2f, %.2f, %.2f)\n", 
			cubeColor.x, cubeColor.y, cubeColor.z, cubeColor.w);
	}
	
	ImGui::Separator();
	ImGui::Text("FPS: %.1u", (uint32)fps.getFPS());
	ImGui::Text("Frame Time: %.3f ms", 1000.0f / fps.getFPS());
	
	ImGui::End();
}

RotatingCube::~RotatingCube() {}
