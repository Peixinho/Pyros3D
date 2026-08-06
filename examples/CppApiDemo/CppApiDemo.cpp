//============================================================================
// Name        : CppApiDemo.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See CppApiDemo.h.
//============================================================================

#include "CppApiDemo.h"

using namespace p3d;

CppApiDemo::CppApiDemo()
	: BaseExample(1280, 800, "Pyros3D - C++ API Demo", WindowType::Close | WindowType::Resize)
{
}

CppApiDemo::~CppApiDemo() {}

void CppApiDemo::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);
	if (Renderer)
		Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.1f, 200.f);
}

void CppApiDemo::Init()
{
	BaseExample::Init();

	// BaseExample already owns Scene + FPSCamera; we create the renderer
	// here (Forward is the simplest path through the public C++ API).
	Renderer = new ForwardRenderer(Width, Height);
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.1f, 200.f);

	FPSCamera->SetPosition(Vec3(0.f, 10.f, 40.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-12.f), 0.f, 0.f));
	FPSCamera->RefreshTransformation();

	material = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse);
	material->SetColor(Vec4(0.85f, 0.25f, 0.2f, 1.f));

	cubeMesh = std::make_shared<Cube>(10.f, 10.f, 10.f);
	cubeObj = std::make_shared<GameObject>();
	rCube = std::make_shared<RenderingComponent>(cubeMesh, material);
	cubeObj->Add(rCube);
	Scene->Add(cubeObj);

	lightObj = std::make_shared<GameObject>();
	dirLight = std::make_shared<DirectionalLight>(Vec4(1.f, 1.f, 1.f, 1.f), Vec3(-0.4f, -1.f, -0.3f));
	lightObj->Add(dirLight);
	Scene->Add(lightObj);

	InitImGui();
}

void CppApiDemo::Update()
{
	BaseExample::Update();
	Scene->Update(GetTime());

	cubeObj->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));

	PrepareImGuiFrame();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	EndImGuiFrame();
}

void CppApiDemo::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("C++ API Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Minimal traditional C++ usage of Pyros3D.");
		ImGui::Separator();
		ImGui::Text("Creates Scene, ForwardRenderer, Cube,");
		ImGui::Text("GenericShaderMaterial and DirectionalLight");
		ImGui::Text("directly in C++ (shared_ptr ownership).");
		ImGui::Separator();
		ImGui::Text("TAB  - capture / free mouse");
		ImGui::Text("WASD - move");
		ImGui::Text("Mouse - look (while captured)");
		ImGui::Separator();
		ImGui::Text("For scene JSON + Lua demos, run DemoLauncher.");
		ImGui::Text("Backend: %s", GetActiveRenderDevice().IsVulkan() ? "Vulkan" : "OpenGL");
	}
	ImGui::End();
}

void CppApiDemo::Shutdown()
{
	if (cubeObj)
	{
		cubeObj->Remove(rCube);
		Scene->Remove(cubeObj);
	}
	rCube.reset();
	cubeObj.reset();
	cubeMesh.reset();
	material.reset();

	if (lightObj)
	{
		lightObj->Remove(dirLight);
		Scene->Remove(lightObj);
	}
	dirLight.reset();
	lightObj.reset();

	// Renderer / Scene / FPSCamera cleaned by BaseExample::Shutdown.
	BaseExample::Shutdown();
}
