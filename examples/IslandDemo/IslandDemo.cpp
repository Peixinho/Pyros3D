//============================================================================
// Name        : IslandDemo.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "IslandDemo.h"

using namespace p3d;

IslandDemo::IslandDemo() : BaseExample(1024, 768, "Pyros3D - Island Demo", WindowType::Close) {}

void IslandDemo::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 10000.f);
}

void IslandDemo::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Scene
	SceneWater = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 10000.f);

	// Create Reflection Camera
	CameraReflection = new GameObject();

	// Create Game Object
	gIsland = new GameObject();
	island = new Model(STR(EXAMPLES_PATH)"/assets/island.p3dm", true);
	rIsland = new RenderingComponent(island, ShaderUsage::Diffuse | ShaderUsage::ClipPlane);
	gIsland->Add(rIsland);
	// Add GameObject to Scene
	Scene->Add(gIsland);

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);
	Scene->Add(Light);

	// Add Camera to Scene
	Scene->Add(CameraReflection);

	// Water
	gWater = new GameObject();
	water = new Plane(500, 500);
	matWater = new WaterMaterial(STR(EXAMPLES_PATH)"/assets/WaterShader.glsl");
	rWater = new RenderingComponent(water, matWater);
	gWater->Add(rWater);
	gWater->SetRotation(Vec3((f32)DEGTORAD(-90.f), 0.f, 0.f));
	gWater->SetPosition(Vec3(0.f, 6.8f, 0.f));
	SceneWater->Add(gWater);

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

	fboReflection = new FrameBuffer();
	reflectionTexture = new Texture();
	reflectionTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	fboReflection->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
	fboReflection->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, reflectionTexture);

	fboRefraction = new FrameBuffer();
	refractionTexture = new Texture();
	refractionTextureDepth = new Texture();
	refractionTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	refractionTextureDepth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	fboRefraction->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, refractionTextureDepth);
	fboRefraction->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, refractionTexture);

	int32 imgID = matWater->textures.size();
	matWater->textures.push_back(reflectionTexture);
	matWater->AddUniform(Uniform("uReflectionMap", Uniforms::DataType::Int, &imgID));
	imgID = matWater->textures.size();
	matWater->textures.push_back(refractionTextureDepth);
	matWater->AddUniform(Uniform("uRefractionMapDepth", Uniforms::DataType::Int, &imgID));
	imgID = matWater->textures.size();
	matWater->textures.push_back(refractionTexture);
	matWater->AddUniform(Uniform("uRefractionMap", Uniforms::DataType::Int, &imgID));

	normalMap = new Texture();
	normalMap->LoadTexture(STR(EXAMPLES_PATH)"/assets/normal.png");
	DUDVmap = new Texture();
	DUDVmap->LoadTexture(STR(EXAMPLES_PATH)"/assets/waterDUDV.png");

	imgID = matWater->textures.size();
	matWater->textures.push_back(normalMap);
	matWater->AddUniform(Uniform("uNormalmap", Uniforms::DataType::Int, &imgID));

	imgID = matWater->textures.size();
	matWater->textures.push_back(DUDVmap);
	matWater->AddUniform(Uniform("uDUDVmap", Uniforms::DataType::Int, &imgID));
}

void IslandDemo::Update()
{
	// Update - Game Loop
	
	float distance = 2 * (FPSCamera->GetPosition().y - gWater->GetPosition().y);
	CameraReflection->SetPosition(Vec3(FPSCamera->GetPosition().x, FPSCamera->GetPosition().y - distance, FPSCamera->GetPosition().z));
	CameraReflection->SetRotation(Vec3(-FPSCamera->GetRotation().x, FPSCamera->GetRotation().y, -FPSCamera->GetRotation().z));

	// Update Scene
	Scene->Update(GetTime());
	SceneWater->Update(GetTime());

	BaseExample::Update();

	fboReflection->Bind();
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, 1, 0, -gWater->GetPosition().y));
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->PreRender(CameraReflection, Scene);
	Renderer->RenderScene(projection, CameraReflection, Scene);
	Renderer->DisableClipPlane();
	fboReflection->UnBind();

	fboRefraction->Bind();
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, -1, 0, gWater->GetPosition().y));
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	Renderer->DisableClipPlane();
	fboRefraction->UnBind();

	// Render Scene
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClearDepthBuffer();
	Renderer->RenderScene(projection, FPSCamera, Scene);
	Renderer->ClearBufferBit(Buffer_Bit::None);
	Renderer->PreRender(FPSCamera, SceneWater);
	Renderer->RenderScene(projection, FPSCamera, SceneWater);
}

void IslandDemo::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(gIsland);
	Scene->Remove(CameraReflection);

	gIsland->Remove(rIsland);

	// Delete
	delete island;
	delete rIsland;
	delete gIsland;
	delete CameraReflection;
	delete Renderer;
	delete Scene;
}

IslandDemo::~IslandDemo() {}
