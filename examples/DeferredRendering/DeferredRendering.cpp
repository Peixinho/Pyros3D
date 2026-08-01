//============================================================================
// Name        : DeferredRendering.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "DeferredRendering.h"

using namespace p3d;

DeferredRendering::DeferredRendering() : BaseExample(1024, 768, "Pyros3D - Deferred Rendering Example", WindowType::Close | WindowType::Resize)
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
	normalTexture->Resize(Width, Height);
	metallicRoughnessTexture->Resize(Width, Height);
}

void DeferredRendering::Init()
{
	// Initialization

	BaseExample::Init();

	// Real, pre-existing bug independent of PBR: FPSCamera was never
	// positioned, defaulting to (0,0,0) - dead center of this scene's own
	// tiny cube/light cluster (positions range roughly [-0.5,0.5]), so the
	// camera sat inside the geometry. Moving it outside is still correct
	// regardless of the note below. Confirmed via git-stash that every
	// prior visual check of this example only ever confirmed "zero GL/
	// validation errors", not "actually visible" - the unmodified example
	// is equally solid black, and (also confirmed via git-stash, testing
	// the pre-PBR Blinn-Phong deferred path with this same camera fix
	// applied) the black screen persists even with a correctly-positioned
	// camera. So there is a second, deeper, pre-existing bug somewhere in
	// DeferredRenderer/this example's setup, unrelated to both the camera
	// and to this session's PBR work - out of scope here, not fixed.
	FPSCamera->SetPosition(Vec3(0.f, 0.f, 3.f));

	// Setting Deferred Rendering Framebuffer and Textures
	albedoTexture = new Texture(); albedoTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	specularTexture = new Texture(); specularTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	depthTexture = new Texture(); depthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	normalTexture = new Texture(); normalTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, Width, Height, false);
	metallicRoughnessTexture = new Texture(); metallicRoughnessTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);

	albedoTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	specularTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	depthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	normalTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	metallicRoughnessTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	deferredFBO = new FrameBuffer();
	deferredFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, albedoTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, specularTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, normalTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, metallicRoughnessTexture);

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
	Diffuse = new GenericShaderMaterial(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::PBR);
	Diffuse->SetColor(color);
	// Dielectric-leaning, moderately glossy - avoids a fully-black look
	// under this renderer's no-IBL ambient placeholder (see
	// PyrosShader.glsl's ambientPBR comment).
	Diffuse->SetMetallic(0.2f);
	Diffuse->SetRoughness(0.4f);
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
	
	// Initialize ImGui
	InitImGui();
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

	// Render ImGui
	RenderImGui();
}

void DeferredRendering::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Deferred Rendering System Information
	if (ImGui::Begin("Deferred Rendering Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Deferred Rendering System Information");
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Cubes: %zu (100 rotating cubes)", Cubes.size());
		ImGui::Text("  Point Lights: %zu (100 colored lights)", Lights.size());
		ImGui::Text("  Directional Light: 1 (ambient lighting)");
		
		ImGui::Separator();
		
		// Deferred Rendering Information
		ImGui::Text("Deferred Rendering:");
		ImGui::Text("  Renderer: DeferredRenderer");
		ImGui::Text("  G-Buffer Textures:");
		ImGui::Text("    Albedo: RGBA (%dx%d)", Width, Height);
		ImGui::Text("    Specular: RGBA (%dx%d)", Width, Height);
		ImGui::Text("    Normal: RGBA32F (%dx%d)", Width, Height);
		ImGui::Text("    Metallic/Roughness: RGBA (%dx%d)", Width, Height);
		ImGui::Text("    Depth: DepthComponent (%dx%d)", Width, Height);
		ImGui::Text("  Frame Buffer: Active");
		ImGui::Text("  Lighting: Cook-Torrance GGX (PBR)");

		ImGui::Separator();

		// Material Information
		ImGui::Text("Material Properties:");
		ImGui::Text("  Shader Usage: DeferredRenderer_Gbuffer | PBR");
		ImGui::Text("  Color: (1.0, 0.8, 0.8, 1.0)");
		ImGui::Text("  Metallic: 0.2");
		ImGui::Text("  Roughness: 0.4");
		ImGui::Text("  Cull Face: Double Sided");
		
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
		ImGui::Text("  Cubes rotate automatically");
	}
	ImGui::End();
}

void DeferredRendering::Shutdown()
{
	// All your Shutdown Code Here

}

DeferredRendering::~DeferredRendering() {}
