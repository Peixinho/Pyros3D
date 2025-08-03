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
	fboReflection->Resize(width, height);
	fboRefraction->Resize(width, height);
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
	
	// Initialize ImGui
	InitImGui();
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

	// Render ImGui
	RenderImGui();
}

void IslandDemo::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Island Demo System Information
	if (ImGui::Begin("Island Demo System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Island Demo System Information");
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Island: 3D island model (island.p3dm)");
		ImGui::Text("  Water: Animated water plane (500x500)");
		ImGui::Text("  Directional Light: White light from (-1, -1, 0)");
		ImGui::Text("  Reflection Camera: Mirror camera for water");
		
		ImGui::Separator();
		
		// Water System Information
		ImGui::Text("Water System:");
		ImGui::Text("  Material: Custom WaterMaterial");
		ImGui::Text("  Shader: WaterShader.glsl");
		ImGui::Text("  Textures: Normal map, DUDV map");
		ImGui::Text("  Reflection: Real-time reflection mapping");
		ImGui::Text("  Refraction: Real-time refraction mapping");
		ImGui::Text("  Clip Planes: Active for water rendering");
		
		ImGui::Separator();
		
		// Rendering Information
		ImGui::Text("Rendering System:");
		ImGui::Text("  Renderer: ForwardRenderer");
		ImGui::Text("  Frame Buffers: Reflection and Refraction FBOs");
		ImGui::Text("  Water Position: (0, 6.8, 0)");
		ImGui::Text("  Water Rotation: -90 degrees on X-axis");
		
		ImGui::Separator();
		
		// Performance Information
		ImGui::Text("Performance:");
		ImGui::Text("  FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("  Resolution: %dx%d", Width, Height);
		ImGui::Text("  Camera Position: (%.1f, %.1f, %.1f)", 
			FPSCamera->GetPosition().x, 
			FPSCamera->GetPosition().y, 
			FPSCamera->GetPosition().z);
		
		ImGui::Separator();
		
		// Controls
		ImGui::Text("Controls:");
		ImGui::Text("  Tab: Toggle mouse capture");
		ImGui::Text("  WASD: Move camera");
		ImGui::Text("  Mouse: Look around");
		ImGui::Text("  Water animates automatically");
		ImGui::Text("  Real-time reflections and refractions");
	}
	ImGui::End();
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
