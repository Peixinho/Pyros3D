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
	BaseExample::OnResize(width, height);

	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 10000.f);
	fboReflection->Resize(width, height);
	fboRefraction->Resize(width, height);
}

void IslandDemo::SetIslandCullFace(const uint32 face)
{
	if (!rIsland) return;
	std::vector<RenderingMesh*> &meshes = rIsland->GetMeshes();
	for (size_t i = 0; i < meshes.size(); i++)
	{
		if (meshes[i]->Material)
			meshes[i]->Material->SetCullFace(face);
	}
}

void IslandDemo::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);

	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 10000.f);

	FPSCamera->SetPosition(Vec3(0.f, 30.f, 80.f));

	CameraReflection = std::make_shared<GameObject>();

	gIsland = std::make_shared<GameObject>();
	island = std::make_shared<Model>(STR(EXAMPLES_PATH)"/assets/island.p3dm", true);
	rIsland = std::make_shared<RenderingComponent>(island, ShaderUsage::Diffuse | ShaderUsage::ClipPlane);
	rIsland->DisableCullTest();
	gIsland->Add(rIsland);
	Scene->Add(gIsland);

	Light = std::make_shared<GameObject>();
	dLight = std::make_shared<DirectionalLight>(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);
	Scene->Add(Light);

	Scene->Add(CameraReflection);

	gWater = std::make_shared<GameObject>();
	water = std::make_shared<Plane>(500, 500);
	matWater = std::make_shared<WaterMaterial>(STR(EXAMPLES_PATH)"/assets/WaterShader.glsl");
	matWater->SetTransparencyFlag(true);
	matWater->EnableBlending();
	matWater->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
	matWater->DisableDepthWrite();
	rWater = std::make_shared<RenderingComponent>(water, matWater);
	gWater->Add(rWater);
	gWater->SetRotation(Vec3((f32)DEGTORAD(-90.f), 0.f, 0.f));
	gWater->SetPosition(Vec3(0.f, 6.8f, 0.f));
	Scene->Add(gWater);

	fboReflection = new FrameBuffer();
	reflectionTexture = std::make_shared<Texture>();
	reflectionTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	reflectionTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	fboReflection->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
	fboReflection->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, reflectionTexture.get());

	fboRefraction = new FrameBuffer();
	refractionTexture = std::make_shared<Texture>();
	refractionTextureDepth = std::make_shared<Texture>();
	refractionTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	refractionTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	refractionTextureDepth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	refractionTextureDepth->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	refractionTextureDepth->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
	fboRefraction->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, refractionTextureDepth.get());
	fboRefraction->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, refractionTexture.get());

	// Sampler order must match WaterShader.glsl bindings.
	matWater->AddSampler("uReflectionMap", reflectionTexture);
	matWater->AddSampler("uRefractionMap", refractionTexture);
	matWater->AddSampler("uRefractionMapDepth", refractionTextureDepth);

	normalMap = std::make_shared<Texture>();
	normalMap->LoadTexture(STR(EXAMPLES_PATH)"/assets/normal.png");
	DUDVmap = std::make_shared<Texture>();
	DUDVmap->LoadTexture(STR(EXAMPLES_PATH)"/assets/waterDUDV.png");

	matWater->AddSampler("uNormalmap", normalMap);
	matWater->AddSampler("uDUDVmap", DUDVmap);

	InitImGui();
}

void IslandDemo::Update()
{
	// Move / look first so reflection matches the camera that will draw
	// the main pass. Was inverted before: reflection used last frame's
	// position while BaseExample::Update ran after, which swam the water
	// whenever WASD moved the camera.
	BaseExample::Update();

	// Keep the FPS camera on the same Euler the mouse counters track
	// (matches camera_fly.lua). LookTo writes via quaternion→euler which
	// does not round-trip to (pitch,yaw,0); reflection must use the
	// counters, so force the view camera onto them too.
	FPSCamera->SetRotation(Vec3((f32)DEGTORAD(counterY), (f32)DEGTORAD(counterX), 0.f));

	const f32 waterY = gWater->GetPosition().y;
	const Vec3 camPos = FPSCamera->GetPosition();
	const f32 distance = 2.f * (camPos.y - waterY);
	CameraReflection->SetPosition(Vec3(camPos.x, camPos.y - distance, camPos.z));
	CameraReflection->SetRotation(Vec3((f32)DEGTORAD(-counterY), (f32)DEGTORAD(counterX), 0.f));
	CameraReflection->RefreshTransformation();

	Scene->Update(GetTime());

	rWater->Disable();

	// Camera-mirror reflection (inverted pitch / mirrored position) does NOT
	// reverse triangle winding - FrontFace cull here was discarding the
	// island's exterior and leaving the reflection FBO mostly cleared
	// (black), which made the water flash black↔color as the view moved.
	// Clip plane alone clips below-water geometry; keep BackFace.
	fboReflection->Bind();
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, 1, 0, -waterY));
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->PreRender(CameraReflection.get(), Scene);
	Renderer->RenderScene(projection, CameraReflection.get(), Scene);
	Renderer->DisableClipPlane();
	fboReflection->UnBind();

	fboRefraction->Bind();
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, -1, 0, waterY));
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	Renderer->DisableClipPlane();
	fboRefraction->UnBind();

	rWater->Enable();

	PrepareImGuiFrame();
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClearDepthBuffer();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	EndImGuiFrame();
}

void IslandDemo::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("Island Demo System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Island Demo System Information");
		ImGui::Separator();
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Island: 3D island model (island.p3dm)");
		ImGui::Text("  Water: Animated water plane (500x500)");
		ImGui::Text("  Directional Light: White light from (-1, -1, 0)");
		ImGui::Text("  Reflection Camera: Mirror camera for water");
		ImGui::Separator();
		ImGui::Text("Water System:");
		ImGui::Text("  Material: Custom WaterMaterial");
		ImGui::Text("  Shader: WaterShader.glsl");
		ImGui::Text("  Reflection / refraction FBOs + clip planes");
		ImGui::Separator();
		ImGui::Text("FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("Camera: (%.1f, %.1f, %.1f)",
			FPSCamera->GetPosition().x,
			FPSCamera->GetPosition().y,
			FPSCamera->GetPosition().z);
		ImGui::Separator();
		ImGui::Text("Tab: mouse capture | WASD + mouse: fly");
	}
	ImGui::End();
}

void IslandDemo::Shutdown()
{
	Scene->Remove(gIsland);
	Scene->Remove(CameraReflection);
	Scene->Remove(Light);
	Scene->Remove(gWater);

	gIsland->Remove(rIsland);
	Light->Remove(dLight);
	gWater->Remove(rWater);

	rIsland.reset();
	gIsland.reset();
	CameraReflection.reset();
	Light.reset();
	dLight.reset();
	rWater.reset();
	gWater.reset();
	matWater.reset();
	island.reset();
	water.reset();
	normalMap.reset();
	DUDVmap.reset();
	reflectionTexture.reset();
	refractionTexture.reset();
	refractionTextureDepth.reset();

	delete fboReflection;
	delete fboRefraction;
	delete Renderer;

	BaseExample::Shutdown();
}

IslandDemo::~IslandDemo() {}
