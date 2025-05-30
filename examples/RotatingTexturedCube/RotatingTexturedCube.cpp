//============================================================================
// Name        : RotatingTexturedCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingTexturedCube.h"

using namespace p3d;

RotatingTexturedCube::RotatingTexturedCube() : BaseExample(1024, 768, "Pyros3D - Rotating Textured Cube", WindowType::Close | WindowType::Resize)
{

}

void RotatingTexturedCube::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void RotatingTexturedCube::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);

	FPSCamera->SetPosition(Vec3(0, 10, 80));

	// Material
	material = new GenericShaderMaterial(ShaderUsage::Texture);
	texture = new Texture();
	texture->LoadTexture(STR(EXAMPLES_PATH)"/assets/pyros.png", TextureType::Texture);
	material->SetColorMap(texture);

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh, material);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);

}

void RotatingTexturedCube::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());
	
	BaseExample::Update();

	Renderer->SetBackground(Vec4(fabs(sinf((f32)GetTime() + 0.1f)), fabs(sinf((f32)GetTime() + 0.5f)), fabs(sinf((f32)GetTime() + 0.9f)), 1.f));

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime(), 0));

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
}

void RotatingTexturedCube::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);

	CubeObject->Remove(rCube);

	// Delete
	delete material;
	delete texture;
	delete rCube;
	delete CubeObject;
	delete cubeMesh;
	delete Renderer;

	BaseExample::Shutdown();
}

RotatingTexturedCube::~RotatingTexturedCube() {}
