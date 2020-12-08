//============================================================================
// Name        : DeferredRendering.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "DeferredRendering.h"

using namespace p3d;

DeferredRendering::DeferredRendering() : BaseExample(1024, 768, "Pyros3D - Deferred Rendering Example", WindowType::Close)
{

}

void DeferredRendering::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.001f, 2000.f);

	albedoTexture->Resize(Width, Height);
	specularTexture->Resize(Width, Height);
	depthTexture->Resize(Width, Height);
}

void DeferredRendering::Init()
{
	// Initialization

	BaseExample::Init();

	// Setting Deferred Rendering Framebuffer and Textures
	albedoTexture = new Texture(); albedoTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	specularTexture = new Texture(); specularTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	depthTexture = new Texture(); depthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	normalTexture = new Texture(); normalTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, Width, Height, false);

	albedoTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	specularTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	depthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	normalTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	deferredFBO = new FrameBuffer();
	deferredFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, albedoTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, specularTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, normalTexture);

	// Initialize Renderer
	Renderer = new DeferredRenderer(Width, Height, deferredFBO);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.001f, 2000.f);

	// Create 100 Point Lights
	for (uint32 i = 0; i < 100; i++)
	{
		// Create Light GameObject
		GameObject* Light = new GameObject();
		// Add Light to GameObjects List
		Lights.push_back(Light);
		// Create Point Light
		PointLight* pLight = new PointLight(Vec4((f32)(rand() % 100), (f32)(rand() % 100), (f32)(rand() % 100), (f32)(rand() % 100))*0.1f, 0.5f);
		// Add Rendering Component to Rendering Components List
		pLights.push_back(pLight);
		// Add Point Light Component to GameObject
		Light->Add(pLight);
		// Add GameObject to Scene
		Scene->Add(Light);
		// Set Random Position to the GameObject
		Light->SetPosition(Vec3((f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f)*0.01f);
	}

	// Create Light GameObject
	GameObject* Light = new GameObject();
	// Add Light to GameObjects List
	// Create Point Light
	DirectionalLight* dLight = new DirectionalLight(Vec4(0.5f, 0.5f, 0.5f, 1.f), Vec3(-1.f, -1.f, 0.f));
	// Add Rendering Component to Rendering Components List
	// Add Point Light Component to GameObject
	Light->Add(dLight);
	// Add GameObject to Scene
	Scene->Add(Light);


	// Material
	Vec4 color = Vec4(1.0f, 0.8f, 0.8f, 1.f);
	Diffuse = new GenericShaderMaterial(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::SpecularColor);
	Diffuse->SetColor(color);
	Diffuse->SetSpecular(color);
	Diffuse->SetCullFace(CullFace::DoubleSided);

	Renderable* cubeHandle2 = new Cube(1.f, 1.f, 1.f);
	GameObject* go = new GameObject();
	RenderingComponent* rc = new RenderingComponent(cubeHandle2, Diffuse);
	go->Add(rc);
	//Scene->Add(go);

	// Create Geometry
	cubeHandle = new Cube(0.1f, 0.1f, 0.1f);

	// Create 100 Cubes
	for (uint32 i = 0; i < 100; i++)
	{
		// Create GameObject
		GameObject* CubeObject = new GameObject();
		// Add Cube to GameObjects List
		Cubes.push_back(CubeObject);
		// Create Rendering Component using Geometry Previously Created with AssetManager
		RenderingComponent* rCube = new RenderingComponent(cubeHandle, Diffuse);
		// Add Rendering Component to Rendering Components List
		rCubes.push_back(rCube);
		// Add Rendering Component to GameObject
		CubeObject->Add(rCube);
		// Add GameObject to Scene
		Scene->Add(CubeObject);
		// Set Random Position to the GameObject
		CubeObject->SetPosition(Vec3((f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f, (f32)(rand() % 100) - 50.f)*0.01f);
	}
}

void DeferredRendering::Update()
{
	// Update - Game Loop

	// Create 100 Cubes
	for (uint32 i = 0; i < 100; i++)
	{
		Cubes[i]->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));
	}

	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	Vec3 finalPosition;
	Vec3 direction = FPSCamera->GetDirection();
	float dt = (float)GetTimeInterval();
	float speed = dt * 2.f;
	if (_moveFront)
	{
		finalPosition -= direction*speed;
	}
	if (_moveBack)
	{
		finalPosition += direction*speed;
	}
	if (_strafeLeft)
	{
		finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}

	FPSCamera->SetPosition(FPSCamera->GetPosition() + finalPosition);
}

void DeferredRendering::Shutdown()
{
	// All your Shutdown Code Here

}

DeferredRendering::~DeferredRendering() {}
