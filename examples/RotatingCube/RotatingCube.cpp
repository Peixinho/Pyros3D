//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"

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

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);

}

void RotatingCube::Update()
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

void RotatingCube::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);

	CubeObject->Remove(rCube);

	// Delete
	delete cubeMesh;
	delete rCube;
	delete CubeObject;
	delete Renderer;

	BaseExample::Shutdown();
}

RotatingCube::~RotatingCube() {}
