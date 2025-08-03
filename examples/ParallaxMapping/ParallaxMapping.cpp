//============================================================================
// Name        : ParallaxMapping.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping
//============================================================================

#include "ParallaxMapping.h"

using namespace p3d;

ParallaxMapping::ParallaxMapping() : BaseExample(1024, 768, "Pyros3D - Parallax Mapping", WindowType::Close | WindowType::Resize)
{

}

void ParallaxMapping::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.001f, 1000.f);
}

void ParallaxMapping::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.001f, 1000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 0, 80));
	
	texturemap = new Texture();
	normalmap = new Texture();
	displacementmap = new Texture();

	texturemap->LoadTexture(STR(EXAMPLES_PATH)"/assets/bricks.png");
	normalmap->LoadTexture(STR(EXAMPLES_PATH)"/assets/bricks_normal.png");
	displacementmap->LoadTexture(STR(EXAMPLES_PATH)"/assets/bricks_disp.png");

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(30, 30, 30, false, false, true);
	rCube = new RenderingComponent(cubeMesh, ShaderUsage::Diffuse | ShaderUsage::ParallaxMapping | ShaderUsage::BumpMapping | ShaderUsage::Texture | ShaderUsage::SpecularColor);
	CubeObject->Add(rCube);
	GenericShaderMaterial* mat = (GenericShaderMaterial*)rCube->GetMeshes()[0]->Material;
	
	mat->SetColorMap(texturemap); 
	mat->SetNormalMap(normalmap);
	mat->SetDisplacementMap(displacementmap);
	mat->SetSpecular(Vec4(1, 1, 1, 1));
	mat->SetShininess(32);

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(1, -1, 0));
	Light->Add(dLight);

	Scene->Add(Light);

	// Initialize ImGui
	InitImGui();
}

void ParallaxMapping::Update()
{
	// Update - Game Loop
	BaseExample::Update();

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));

	// Render Scene
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
	
	// Render ImGui
	RenderImGui();
}

void ParallaxMapping::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Parallax Mapping Information
	if (ImGui::Begin("Parallax Mapping Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Parallax Mapping System Information");
		ImGui::Separator();
		
		// Material Information
		ImGui::Text("Material Properties:");
		ImGui::Text("  Diffuse Texture: Bricks");
		ImGui::Text("  Normal Map: Bricks Normal");
		ImGui::Text("  Displacement Map: Bricks Displacement");
		ImGui::Text("  Specular: White");
		ImGui::Text("  Shininess: 32");
		
		ImGui::Separator();
		
		// Shader Information
		ImGui::Text("Shader Features:");
		ImGui::Text("  Diffuse Lighting: Active");
		ImGui::Text("  Parallax Mapping: Active");
		ImGui::Text("  Bump Mapping: Active");
		ImGui::Text("  Texture Mapping: Active");
		ImGui::Text("  Specular Lighting: Active");
		
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Information:");
		ImGui::Text("  Object: Rotating Cube");
		ImGui::Text("  Camera Position: (%.1f, %.1f, %.1f)", 
			Camera->GetPosition().x, 
			Camera->GetPosition().y, 
			Camera->GetPosition().z);
		ImGui::Text("  Resolution: %dx%d", Width, Height);
		
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

void ParallaxMapping::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Camera);

	CubeObject->Remove(rCube);

	// Delete
	delete rCube;
	delete CubeObject;
	delete cubeMesh;
	delete texturemap;
	delete normalmap;
	delete displacementmap;
	delete Camera;
	delete Renderer;
	delete Scene;
}

ParallaxMapping::~ParallaxMapping() {}

void ParallaxMapping::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void ParallaxMapping::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void ParallaxMapping::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void ParallaxMapping::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void ParallaxMapping::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void ParallaxMapping::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void ParallaxMapping::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void ParallaxMapping::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void ParallaxMapping::LookTo(Event::Input::Info e)
{
	if (mouseCenter != GetMousePosition())
	{
		mousePosition = InputManager::GetMousePosition();
		Vec2 mouseDelta = (mousePosition - mouseLastPosition);
		if (mouseDelta.x != 0 || mouseDelta.y != 0)
		{
			counterX -= mouseDelta.x / 10.f;
			counterY -= mouseDelta.y / 10.f;
			if (counterY<-80.f) counterY = -80.f;
			if (counterY>80.f) counterY = 80.f;
			Quaternion qX, qY;
			qX.AxisToQuaternion(Vec3(1.f, 0.f, 0.f), DEGTORAD(counterY));
			qY.AxisToQuaternion(Vec3(0.f, 1.f, 0.f), DEGTORAD(counterX));
			//                Matrix rotX, rotY;
			//                rotX.RotationX(DEGTORAD(counterY));
			//                rotY.RotationY(DEGTORAD(counterX));
			Camera->SetRotation((qY*qX).GetEulerFromQuaternion());
			SetMousePosition((int)(mouseCenter.x), (int)(mouseCenter.y));
			mouseLastPosition = mouseCenter;
		}
	}
}
