//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"

using namespace p3d;

RotatingCube::RotatingCube() : ClassName(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void RotatingCube::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	VRenderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);

	EffectManager->Resize(width, height);
	MotionBlur->Resize(width, height);
}

void RotatingCube::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	VRenderer = new VelocityRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 10, 400));

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh);
	CubeObject->Add(rCube);

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);
	Camera->LookAt(Vec3::ZERO);

	EffectManager = new PostEffectsManager(Width, Height);
	MotionBlur = new MotionBlurEffect(RTT::Color, VRenderer->GetTexture(),Width,Height);
	EffectManager->AddEffect(MotionBlur);
}

void RotatingCube::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime(), 0));
	//CubeObject->SetPosition(Vec3(sinf(GetTime()*2)*100.f, sinf(GetTime()*4)*100.f, CubeObject->GetPosition().z));

	// Render Velocity Map
	VRenderer->RenderVelocityMap(projection, Camera, Scene);

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
	EffectManager->EndCapture();

	// Render Post Processing
	EffectManager->ProcessPostEffects(&projection);
	
}

void RotatingCube::Shutdown()
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
	delete VRenderer;
	delete EffectManager;
	delete Scene;
}

RotatingCube::~RotatingCube() {}
