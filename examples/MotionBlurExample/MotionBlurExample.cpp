//============================================================================
// Name        : MotionBlurExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "MotionBlurExample.h"

using namespace p3d;

MotionBlurExample::MotionBlurExample() : BaseExample(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void MotionBlurExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	VRenderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);

	EffectManager->Resize(width, height);
	MotionBlur->Resize(width, height);
}

void MotionBlurExample::Init()
{
	// Initialization
    BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetBackground(Vec4(0.1f, 0.1f, 0.1f, 1));
	VRenderer = new VelocityRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	FPSCamera->SetPosition(Vec3(0, 10, 400));

	// Material
	material = new GenericShaderMaterial(ShaderUsage::Texture);
	texture = new Texture();
	texture->LoadTexture(STR(EXAMPLES_PATH)"/assets/Texture.png", TextureType::Texture);
	material->SetColorMap(texture);

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD1.p3dm");
	//cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);

	EffectManager = new PostEffectsManager(Width, Height);
	MotionBlur = new MotionBlurEffect(RTT::Color, VRenderer->GetTexture(),Width,Height);
	EffectManager->AddEffect(MotionBlur);
}

void MotionBlurExample::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

    BaseExample::Update();

	// Game Logic Here
	CubeObject->SetRotation(Vec3((f32)GetTime()*2, (f32)GetTime()*5, 0));
	CubeObject->SetPosition(Vec3(sinf(GetTime()*2)*100.f, cos(GetTime()*2)*100.f, CubeObject->GetPosition().z));

	// Render Velocity Map
	Renderer->PreRender(FPSCamera, Scene);
	VRenderer->RenderVelocityMap(projection, FPSCamera, Scene);

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();

	((MotionBlurEffect*) MotionBlur)->SetCurrentFPS(fps.getFPS());
	((MotionBlurEffect*) MotionBlur)->SetTargetFPS(60);

	// Render Post Processing
	EffectManager->ProcessPostEffects(&projection);
}

void MotionBlurExample::Shutdown()
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
	delete VRenderer;
	delete EffectManager;
}

MotionBlurExample::~MotionBlurExample() {}
