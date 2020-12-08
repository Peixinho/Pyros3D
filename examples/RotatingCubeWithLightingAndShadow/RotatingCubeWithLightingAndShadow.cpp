//============================================================================
// Name        : RotatingCubeWithLightingAndShadow.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCubeWithLightingAndShadow.h"

using namespace p3d;

RotatingCubeWithLightingAndShadow::RotatingCubeWithLightingAndShadow() : BaseExample(1024, 768, "Pyros3D - Rotating Cube With Lighting And Shadow", WindowType::Close | WindowType::Resize)
{

}

void RotatingCubeWithLightingAndShadow::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 200.f);

}

void RotatingCubeWithLightingAndShadow::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 2000.f);

	// Create Camera
	FPSCamera->SetPosition(Vec3(0, 10, 100));

	// Material
	Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::SpecularColor | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow);
	Diffuse->SetColor(Vec4(1, 0, 0, 1));
	Diffuse->SetSpecular(Vec4(1, 1, 1, 1));

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	// Enable Shadow Casting
	dLight->EnableCastShadows(2048, 2048, projection, 0.1f, (f32)300.0, 1);
	dLight->SetShadowBias(5.f, 3.f);
	Light->Add(dLight);

	Light2 = new GameObject();
	//pLight = new SpotLight(Vec4(1, 1, 1, 1), 300, Vec3(0, -1, 0), 45, 30);
	pLight = new PointLight(Vec4(1, 1, 1, 1), 100);
	pLight->EnableCastShadows(256, 256);
	pLight->SetShadowBias(5.f, 3.f);

	Light2->Add(pLight);
	Scene->Add(Light2);
	Light2->SetPosition(Vec3(0, 20, 0));

	// Add Light to Scene
	Scene->Add(Light);

	// Create Floor Material
	FloorMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow);
	FloorMaterial->SetColor(Vec4(1, 1, 1, 1));

	// Create Floor
	Floor = new GameObject();
	floorMesh = new Cube(100, 10, 100);
	Ceiling = new GameObject();

	// Create Floor Rendering Component
	rFloor = new RenderingComponent(floorMesh, FloorMaterial);
	rCeiling = new RenderingComponent(floorMesh, FloorMaterial);

	// Add Rendering Component To GameObject
	Floor->Add(rFloor);
	Ceiling->Add(rCeiling);
	Ceiling->SetScale(Vec3(0.5, 0.5, 0.5));

	// Add Floor to Scene
	Scene->Add(Floor);
	Scene->Add(Ceiling);

	// Rotate Floor

	// Translate Floor
	Floor->SetPosition(Vec3(0, -30, -20));
	Ceiling->SetPosition(Vec3(0, 30, -20));

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(5, 5, 5);
	rCube = new RenderingComponent(cubeMesh, Diffuse);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);
}

void RotatingCubeWithLightingAndShadow::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime(), 0));

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
}

void RotatingCubeWithLightingAndShadow::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Light);
	Scene->Remove(Floor);

	CubeObject->Remove(rCube);
	Light->Remove(dLight);
	Floor->Remove(rFloor);

	// Delete
	delete rCube;
	delete CubeObject;
	delete rFloor;
	delete Floor;
	delete cubeMesh;
	delete floorMesh;
	delete dLight;
	delete Light;
	delete Diffuse;
	delete FloorMaterial;
	delete Renderer;
	delete Scene;
	BaseExample::Shutdown();
}

RotatingCubeWithLightingAndShadow::~RotatingCubeWithLightingAndShadow() {}
