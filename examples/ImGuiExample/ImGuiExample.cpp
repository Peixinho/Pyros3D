//============================================================================
// Name        : ImGuiExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ImGui Example
//============================================================================

#include "ImGuiExample.h"

using namespace p3d;

ImGuiExample::ImGuiExample() : ClassName(1024, 768, "Pyros3D - ImGui Example", WindowType::Close | WindowType::Resize) {}

void ImGuiExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void ImGuiExample::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 10, 80));

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(30, 30, 30);
	rCube = new RenderingComponent(cubeMesh);
	CubeObject->Add(rCube);

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);
	Camera->LookAt(Vec3::ZERO);

	ImGui::SFML::ImGui_ImplSFML_Init(&rview);
	clear_color = ImColor(114, 144, 154);
}

void ImGuiExample::Update()
{
	// Update - Game Loop
	ImGui::SFML::ImGui_ImplSFML_NewFrame();

	{
		static float f = 0.0f;
		ImGui::Text("Hello, world!");
		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
		ImGui::ColorEdit3("clear color", (float*)&clear_color);
		//		if (ImGui::Button("Test Window")) show_test_window ^= 1;
		//		if (ImGui::Button("Another Window")) show_another_window ^= 1;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	}
	bool show_test_window = true;
	bool show_another_window = false;
	// 2. Show another simple window, this time using an explicit Begin/End pair
	if (show_another_window)
	{
		ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Another Window", &show_another_window);
		ImGui::Text("Hello");
		ImGui::End();
	}

	// 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
	if (show_test_window)
	{
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
		ImGui::ShowTestWindow(&show_test_window);
	}

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));

	// Render Scene
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);

	ImGui::SFML::ImGui_ImplSFML_Render((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y, clear_color);
}

void ImGuiExample::Shutdown()
{
	// All your Shutdown Code Here

		// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Camera);

	CubeObject->Remove(rCube);

	// Delete
	delete cubeMesh;
	delete rCube;
	delete CubeObject;
	delete Camera;
	delete Renderer;
	delete Scene;

	ImGui::SFML::ImGui_ImplSFML_Shutdown();
}

ImGuiExample::~ImGuiExample() {}
