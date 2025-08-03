//============================================================================
// Name        : SimplePhysics.h
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
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 500.f);
}

void SimplePhysics::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 500.f);

	FPSCamera->SetPosition(Vec3(0, 20.0, 300));

	// Physics
	physics = new Physics();
	physics->InitPhysics();

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	dLight->EnableCastShadows(1024, 1024, projection, 1, 500, 1);
	dLight->SetShadowBias(1.f, 3.f);
	Light->Add(dLight);

	// Add Light to Scene
	Scene->Add(Light);

	// Create Materials
	Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
	Diffuse->SetColor(Vec4(0.8f, 0.8f, 0.8f, 1.f));

	// Set Selected Mesh PTR NULL
	SelectedMesh = NULL;

	// Create Cubes
	srand((unsigned int)time(NULL));

	// Create Geometry
	cubeHandle = new Cube(5, 5, 5);

	// Create 100 Cubes
#if !defined(GLES2)
	for (uint32 i = 0; i < 1000; i++)
#else
	for (uint32 i = 0; i < 100; i++)
#endif
	{
		// Create GameObject
		GameObject* Cube = new GameObject();
		// Add Cube to GameObjects List
		Cubes.push_back(Cube);
		// Create Rendering Component using Geometry Previously Created with AssetManager
		RenderingComponent* rCube = new RenderingComponent(cubeHandle, Diffuse);
		// Add Rendering Component to Rendering Components List
		rCubes.push_back(rCube);
		// Add Rendering Component to GameObject
		Cube->Add(rCube);
		// Create Physics Component
		IPhysicsComponent* pCube = (IPhysicsComponent*)physics->CreateBox(5, 5, 5, 10);
		// Add Physics Component to Physics List
		pCubes.push_back(pCube);
		// Add Physics Component to GameObject
		Cube->Add(pCube);
		// Add GameObject to Scene
		Scene->Add(Cube);
		// Set Random Position to the GameObject
		Cube->SetPosition(Vec3((f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f));
	}

	// Create Floor
	Floor = new GameObject();
	// Create Geometry
	floorHandle = new Cube(100, 3, 100);

	// Create Floor Geometry
	rFloor = new RenderingComponent(floorHandle, Diffuse);

	// Create Floor Physics
	pFloor = (IPhysicsComponent*)physics->CreateBox(100, 3, 100, 0);
	// Add Physics Component to GameObject
	Floor->Add(pFloor);

	// Add Rendering Component To GameObject
	Floor->Add(rFloor);

	// Add Floor to Scene
	Scene->Add(Floor);

	// Translate Floor
	Floor->SetPosition(Vec3(0, -100, 0));
	
	// Initialize ImGui
	InitImGui();
}

void SimplePhysics::Update()
{
	// Update - Game Loop

	// Update Physics
	physics->Update(GetTimeInterval(), 10);

	BaseExample::Update();

	// Update Scene
	Scene->Update(GetTime());

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	// Render ImGui
	RenderImGui();
}

void SimplePhysics::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Physics System Information
	if (ImGui::Begin("Physics System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Physics System Information");
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Cubes: %zu physics cubes (5x5x5)", Cubes.size());
		ImGui::Text("  Floor: Static physics floor (100x3x100)");
		ImGui::Text("  Directional Light: White light with shadows");
		ImGui::Text("  Physics Engine: Bullet Physics");
		
		ImGui::Separator();
		
		// Physics System Information
		ImGui::Text("Physics System:");
		ImGui::Text("  Engine: Bullet Physics");
		ImGui::Text("  Cubes Mass: 10 units");
		ImGui::Text("  Floor Mass: 0 (static)");
		ImGui::Text("  Physics Steps: 10 iterations");
		ImGui::Text("  Gravity: Enabled");
		
		ImGui::Separator();
		
		// Rendering Information
		ImGui::Text("Rendering System:");
		ImGui::Text("  Renderer: ForwardRenderer");
		ImGui::Text("  Shader Usage: Color | Diffuse | DirectionalShadow");
		ImGui::Text("  Shadow Resolution: 1024x1024");
		ImGui::Text("  Shadow Bias: 1.0, 3.0");
		
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
		ImGui::Text("  Cubes fall with physics simulation");
		ImGui::Text("  Real-time physics and shadows");
	}
	ImGui::End();
}

void SimplePhysics::Shutdown()
{
	// All your Shutdown Code Here

	Scene->Remove(Light);

	// Remove Floor Components
	Floor->Remove(pFloor);
	Floor->Remove(rFloor);

	Scene->Remove(Floor);

	// GameObjects and Components

	for (std::vector<RenderingComponent*>::iterator i = rCubes.begin(); i != rCubes.end(); i++)
	{
		(*i)->GetOwner()->Remove(*i);
		delete *i;
	}
	
	for (std::vector<IPhysicsComponent*>::iterator i = pCubes.begin(); i != pCubes.end(); i++)
	{
		(*i)->GetOwner()->Remove(*i);
		delete *i;
	}
	
	for (std::vector<GameObject*>::iterator i = Cubes.begin(); i != Cubes.end(); i++)
	{
		Scene->Remove(*i);
		delete *i;
	}
	
	Cubes.clear();
	rCubes.clear();
	pCubes.clear();

	// Remove Directional Light Component
	Light->Remove(dLight);

	// Delete All Components and GameObjects
	delete dLight;
	delete Light;
	delete rFloor;
	delete pFloor;
	delete Floor;
	delete cubeHandle;
	delete floorHandle;
	delete physics;
	delete Diffuse;
	delete Renderer;

	BaseExample::Shutdown();
}

SimplePhysics::~SimplePhysics() {}
