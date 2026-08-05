//============================================================================
// Name        : SimplePhysics.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Simple Physics Example
//============================================================================

#include "SimplePhysics.h"

using namespace p3d;

SimplePhysics::SimplePhysics() : BaseExample(1024, 768, "CODENAME: Pyros3D - Simple Physics", WindowType::Close | WindowType::Resize)
{

}

void SimplePhysics::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);

	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 500.f);
}

void SimplePhysics::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);

	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 500.f);

	FPSCamera->SetPosition(Vec3(0, 20.0, 300));

	physics = new Physics();
	physics->InitPhysics();

	Light = std::make_shared<GameObject>();
	dLight = std::make_shared<DirectionalLight>(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	dLight->EnableCastShadows(1024, 1024, projection, 1, 500, 1);
	dLight->SetShadowBias(1.f, 3.f);
	Light->Add(dLight);

	Scene->Add(Light);

	Diffuse = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
	Diffuse->SetColor(Vec4(0.8f, 0.8f, 0.8f, 1.f));

	SelectedMesh = NULL;

	srand((unsigned int)time(NULL));

	// One shared Cube geometry for every RenderingComponent (Stage 2 fan-out)
	cubeHandle = std::make_shared<Cube>(5, 5, 5);

#if !defined(GLES2)
	for (uint32 i = 0; i < 1000; i++)
#else
	for (uint32 i = 0; i < 100; i++)
#endif
	{
		auto CubeGO = std::make_shared<GameObject>();
		Cubes.push_back(CubeGO);
		auto rCube = std::make_shared<RenderingComponent>(cubeHandle, Diffuse);
		rCubes.push_back(rCube);
		CubeGO->Add(rCube);
		auto pCube = physics->CreateBox(5, 5, 5, 10);
		pCubes.push_back(pCube);
		CubeGO->Add(pCube);
		Scene->Add(CubeGO);
		CubeGO->SetPosition(Vec3((f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f));
	}

	Floor = std::make_shared<GameObject>();
	floorHandle = std::make_shared<Cube>(100, 3, 100);

	rFloor = std::make_shared<RenderingComponent>(floorHandle, Diffuse);

	pFloor = physics->CreateBox(100, 3, 100, 0);
	Floor->Add(pFloor);
	Floor->Add(rFloor);
	Scene->Add(Floor);
	Floor->SetPosition(Vec3(0, -100, 0));
	
	InitImGui();
}

void SimplePhysics::Update()
{
	physics->Update(GetTimeInterval(), 10);

	BaseExample::Update();

	Scene->Update(GetTime());

	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);

	RenderImGui();
}

void SimplePhysics::DrawUI()
{
	DrawBaseUI();
	
	if (ImGui::Begin("Physics System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Physics System Information");
		ImGui::Separator();
		
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Cubes: %zu physics cubes (5x5x5)", Cubes.size());
		ImGui::Text("  Floor: Static physics floor (100x3x100)");
		ImGui::Text("  Directional Light: White light with shadows");
		ImGui::Text("  Physics Engine: Bullet Physics");
		
		ImGui::Separator();
		
		ImGui::Text("Physics System:");
		ImGui::Text("  Engine: Bullet Physics");
		ImGui::Text("  Cubes Mass: 10 units");
		ImGui::Text("  Floor Mass: 0 (static)");
		ImGui::Text("  Physics Steps: 10 iterations");
		ImGui::Text("  Gravity: Enabled");
		
		ImGui::Separator();
		
		ImGui::Text("Rendering System:");
		ImGui::Text("  Renderer: ForwardRenderer");
		ImGui::Text("  Shader Usage: Color | Diffuse | DirectionalShadow");
		ImGui::Text("  Shadow Resolution: 1024x1024");
		ImGui::Text("  Shadow Bias: 1.0, 3.0");
		ImGui::Text("  Shared geometry: 1 Cube -> %zu RenderingComponents", rCubes.size());
		
		ImGui::Separator();
		
		ImGui::Text("Performance:");
		ImGui::Text("  FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("  Resolution: %dx%d", Width, Height);
		ImGui::Text("  Camera Position: (%.1f, %.1f, %.1f)", 
			FPSCamera->GetPosition().x, 
			FPSCamera->GetPosition().y, 
			FPSCamera->GetPosition().z);
		
		ImGui::Separator();
		
		ImGui::Text("Controls:");
		ImGui::Text("  Tab: Toggle mouse capture");
		ImGui::Text("  WASD: Move camera");
		ImGui::Text("  Mouse: Look around");
		ImGui::Text("  Cubes fall with physics simulation");
		ImGui::Text("  Real-time physics and shadows");
	}
	ImGui::End();
}

void SimplePhysics::Shutdown()
{
	Scene->Remove(Light);

	Floor->Remove(pFloor);
	Floor->Remove(rFloor);
	Scene->Remove(Floor);

	for (auto &r : rCubes)
		r->GetOwner()->Remove(r);
	
	for (auto &p : pCubes)
		p->GetOwner()->Remove(p);
	
	for (auto &c : Cubes)
		Scene->Remove(c);
	
	Cubes.clear();
	rCubes.clear();
	pCubes.clear();

	Light->Remove(dLight);

	rFloor.reset();
	pFloor.reset();
	Floor.reset();
	Light.reset();
	dLight.reset();
	cubeHandle.reset();
	floorHandle.reset();
	Diffuse.reset();

	delete physics;

	BaseExample::Shutdown();
}

SimplePhysics::~SimplePhysics() {}
