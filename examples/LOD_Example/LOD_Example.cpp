//============================================================================
// Name        : LOD_Example.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : LOD Example
//============================================================================

#include "LOD_Example.h"

using namespace p3d;

LOD_Example::LOD_Example() : BaseExample(1024, 768, "Pyros3D - LOD Example", WindowType::Close | WindowType::Resize)
{

}

void LOD_Example::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 3000.f);
}

void LOD_Example::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Enable Frustum Culling
	Renderer->ActivateCulling(CullingMode::FrustumCulling);
	Renderer->SetGlobalLight(Vec4(0, 0, 0, 1));

	// Enable Lodding on Renderer
	Renderer->EnableLOD();

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 3000.f);

	FPSCamera->SetPosition(Vec3(0, 10, 80));

	// Light
	Renderable* sHandle = new Sphere(3, 16, 10);
	for (int i = 0; i < 100; i++)
	{
		GameObject* Light = new GameObject();
		PointLight* pLight = new PointLight(Vec4(1, 1, 1, 1), 100);
		Light->Add(pLight);
		Light->SetPosition(Vec3((f32)(rand() % 1000) - 500.f, (f32)(rand() % 1000) - 500.f, (f32)(rand() % 1000) - 500.f));
		Light->Add(new RenderingComponent(sHandle));
		Lights.push_back(Light);
		pLights.push_back(pLight);
		Scene->Add(Light);
	}

	// Load Teapot LODS
	teapotLOD1Handle = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD1.p3dm", false);
	teapotLOD2Handle = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD2.p3dm", false);
	teapotLOD3Handle = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD3.p3dm", false);

	// Create Teapots and Add LODS
	for (int i = 0; i < TEAPOTS; i++)
	{

		GenericShaderMaterial* mTeapot = new GenericShaderMaterial(ShaderUsage::Diffuse | ShaderUsage::SpecularColor | ShaderUsage::Color);
		mTeapot->SetColor(Vec4(0.5, 0.5, 0.5, 1));
		mTeapot->SetSpecular(Vec4(1, 1, 1, 1));

		RenderingComponent* rTeapot = new RenderingComponent(teapotLOD1Handle, mTeapot);
		rTeapot->AddLOD(teapotLOD2Handle, 50, ShaderUsage::Diffuse | ShaderUsage::SpecularColor | ShaderUsage::Color);
		rTeapot->AddLOD(teapotLOD3Handle, 100, ShaderUsage::Diffuse | ShaderUsage::SpecularColor | ShaderUsage::Color);

		GameObject* Teapot = new GameObject(true); //Static Object
		Teapot->SetPosition(Vec3((f32)(rand() % 1000) - 500.f, (f32)(rand() % 1000) - 500.f, (f32)(rand() % 1000) - 500.f));
		Teapot->SetScale(Vec3(.1f, .1f, .1f));
		Teapot->Add(rTeapot);

		Scene->Add(Teapot);

		Teapots.push_back(Teapot);
		rTeapots.push_back(rTeapot);
		mTeapots.push_back(mTeapot);
	}

	octree = new Octree();
	octree->BuildOctree(Scene->GetMinBounds(), Scene->GetMaxBounds(), Scene->GetAllGameObjectList(), 10);
}

void LOD_Example::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	octree->Draw(projection, FPSCamera->GetWorldTransformation().Inverse());

}

void LOD_Example::Shutdown()
{
	// All your Shutdown Code Here

	// Remove all Objects
	for (size_t i = 0; i < rTeapots.size(); i++)
	{
		// Remove From Scene
		Scene->Remove(Teapots[i]);
		// Remove Rendering Component From GameObject
		Teapots[i]->Remove(rTeapots[i]);

		// Delete Both
		delete Teapots[i];
		delete rTeapots[i];
	}

	// Delete Models
	delete teapotLOD1Handle;
	delete teapotLOD2Handle;
	delete teapotLOD3Handle;

	// Delete
	delete Renderer;
}

LOD_Example::~LOD_Example() {}
