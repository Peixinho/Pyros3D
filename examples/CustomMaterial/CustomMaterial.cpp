//============================================================================
// Name        : CustomMaterial.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "CustomMaterial.h"

using namespace p3d;

CustomMaterial::CustomMaterial() : ClassName(1024, 768, "Pyros3D - Custom Material", WindowType::Close | WindowType::Resize)
{

}

void CustomMaterial::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void CustomMaterial::Init()
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

	// Custom Material
	Material = new CustomMaterialExample();

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(30, 30, 30);
	rCube = new RenderingComponent(cubeMesh, Material);
	CubeObject->Add(rCube);

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);
	Camera->LookAt(Vec3::ZERO);

}

void CustomMaterial::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));

	// Render Scene
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
}

void CustomMaterial::Shutdown()
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
	delete Material;
	delete Camera;
	delete Renderer;
	delete Scene;
}

CustomMaterial::~CustomMaterial() {}
