//============================================================================
// Name        : SSRTest.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See SSRTest.h.
//============================================================================

#include "SSRTest.h"

using namespace p3d;

SSRTest::SSRTest() : BaseExample(1280, 800, "Pyros3D - SSR Test", WindowType::Close | WindowType::Resize)
{
	Renderer = nullptr;
	EffectManager = nullptr;
	deferredFBO = nullptr;
}

SSRTest::~SSRTest() {}

void SSRTest::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);

	Renderer->Resize(width, height);
	EffectManager->Resize(width, height);
	projection.Perspective(60.f, (f32)width / (f32)height, 0.1f, 200.f);

	albedoTexture->Resize(Width, Height);
	specularTexture->Resize(Width, Height);
	depthTexture->Resize(Width, Height);
	normalTexture->Resize(Width, Height);
	metallicRoughnessTexture->Resize(Width, Height);
}

void SSRTest::Init()
{
	BaseExample::Init();

	// Elevated and angled down, so both the spheres and their floor
	// reflections are visible in the same frame - the point of this
	// example, unlike DeferredPBRSpheres' flat, face-on calibration grid.
	FPSCamera->SetPosition(Vec3(0.f, 7.f, 18.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-18.f), 0.f, 0.f));
	FPSCamera->RefreshTransformation();

	albedoTexture = std::make_shared<Texture>();
	albedoTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	specularTexture = std::make_shared<Texture>();
	specularTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	depthTexture = std::make_shared<Texture>();
	depthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	normalTexture = std::make_shared<Texture>();
	normalTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, Width, Height, false);
	metallicRoughnessTexture = std::make_shared<Texture>();
	metallicRoughnessTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);

	albedoTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	specularTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	depthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	normalTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	metallicRoughnessTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	deferredFBO = new FrameBuffer();
	deferredFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthTexture.get());
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, albedoTexture.get());
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, specularTexture.get());
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, normalTexture.get());
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, metallicRoughnessTexture.get());

	Renderer = new DeferredRenderer(Width, Height, deferredFBO);
	Renderer->SetGlobalLight(Vec4(0.12f, 0.12f, 0.14f, 1.f));
	Renderer->EnableSSR();

	EffectManager = new PostEffectsManager(Width, Height);
	EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));

	projection.Perspective(60.f, (f32)Width / (f32)Height, 0.1f, 200.f);

	// Floor - low roughness dielectric; SetSSREnabled + SetReflectivity are
	// both required for material-aware SSR (see lastPass.glsl).
	floorMesh = std::make_shared<Plane>(30.f, 30.f, true);
	floorMaterial = std::make_shared<GenericShaderMaterial>(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::PBR);
	floorMaterial->SetColor(Vec4(0.5f, 0.5f, 0.55f, 1.f));
	floorMaterial->SetMetallic(0.0f);
	floorMaterial->SetRoughness(0.08f);
	floorMaterial->SetSSREnabled(true);
	floorMaterial->SetReflectivity(1.0f);
	floorMaterial->SetCullFace(CullFace::DoubleSided);
	floorObj = std::make_shared<GameObject>();
	floorObj->SetPosition(Vec3(0.f, -3.f, 0.f));
	floorObj->SetRotation(Vec3(DEGTORAD(-90.f), 0.f, 0.f));
	rFloor = std::make_shared<RenderingComponent>(floorMesh, floorMaterial);
	floorObj->Add(rFloor);
	Scene->Add(floorObj);

	Vec4 albedo(0.75f, 0.2f, 0.15f, 1.f);
	sphereMesh = std::make_shared<Sphere>(1.4f, 24, 16, true);
	struct { f32 x, y, z, metallic, roughness; } sphereDesc[NUM_SPHERES] = {
		{ -6.f, 0.f, -2.f, 1.0f, 0.05f },
		{ -3.f, 0.f,  1.f, 0.0f, 0.15f },
		{  0.f, 0.f, -1.f, 1.0f, 0.35f },
		{  3.f, 0.f,  1.f, 0.0f, 0.6f },
		{  6.f, 0.f, -2.f, 0.5f, 0.9f },
	};
	for (uint32 i = 0; i < NUM_SPHERES; i++)
	{
		auto mat = std::make_shared<GenericShaderMaterial>(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::PBR);
		mat->SetColor(albedo);
		mat->SetMetallic(sphereDesc[i].metallic);
		mat->SetRoughness(sphereDesc[i].roughness);
		mat->SetCullFace(CullFace::DoubleSided);
		sphereMaterials[i] = mat;

		auto obj = std::make_shared<GameObject>();
		obj->SetPosition(Vec3(sphereDesc[i].x, sphereDesc[i].y, sphereDesc[i].z));
		auto r = std::make_shared<RenderingComponent>(sphereMesh, mat);
		obj->Add(r);
		Scene->Add(obj);

		sphereObjs[i] = obj;
		rSpheres[i] = r;
	}

	Vec3 lightPositions[NUM_LIGHTS] = { Vec3(-8.f, 8.f, 10.f), Vec3(8.f, 8.f, 10.f) };
	for (uint32 i = 0; i < NUM_LIGHTS; i++)
	{
		lightObjs[i] = std::make_shared<GameObject>();
		lightObjs[i]->SetPosition(lightPositions[i]);
		pointLights[i] = std::make_shared<PointLight>(Vec4(1.6f, 1.6f, 1.6f, 1.f), 30.f);
		lightObjs[i]->Add(pointLights[i]);
		Scene->Add(lightObjs[i]);
	}

	dirLightObj = std::make_shared<GameObject>();
	dirLight = std::make_shared<DirectionalLight>(Vec4(0.5f, 0.5f, 0.5f, 1.f), Vec3(-0.3f, -0.7f, -0.5f));
	dirLightObj->Add(dirLight);
	Scene->Add(dirLightObj);

	InitImGui();
}

void SSRTest::Update()
{
	// Free-fly camera (WASD + mouse) via BaseExample - no orbit.
	BaseExample::Update();
	Scene->Update(GetTime());

	EffectManager->CaptureFrame();
	PrepareImGuiFrame();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	EffectManager->EndCapture();
	EffectManager->ProcessPostEffects(&projection);
	EndImGuiFrame();
}

void SSRTest::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("SSR Test", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Material-aware SSR via DeferredRenderer's lastPass.glsl");
		ImGui::Separator();
		ImGui::Text("Spheres left->right: metallic/roughness varies");
		ImGui::Text("Low-roughness/metal spheres should show sharp");
		ImGui::Text("reflections in the floor; high-roughness ones fade out.");
		ImGui::Text("WASD + mouse (TAB to capture) - free camera.");
		ImGui::Separator();
		ImGui::Text("Backend: %s", GetActiveRenderDevice().IsVulkan() ? "Vulkan" : "OpenGL");

		static int debugMode = 0;
		const char* modes[] = {
			"0 Normal",
			"1 Gate (ssrReflective / mr.b)",
			"2 March hits (red)",
			"3 Full hit color (no Fresnel)",
		};
		if (ImGui::Combo("SSR Debug", &debugMode, modes, 4))
			Renderer->SetSSRDebugMode((uint32)debugMode);
	}
	ImGui::End();
}

void SSRTest::Shutdown()
{
	if (dirLightObj)
	{
		dirLightObj->Remove(dirLight);
		Scene->Remove(dirLightObj);
	}
	dirLight.reset();
	dirLightObj.reset();

	for (uint32 i = 0; i < NUM_LIGHTS; i++)
	{
		if (lightObjs[i])
		{
			lightObjs[i]->Remove(pointLights[i]);
			Scene->Remove(lightObjs[i]);
		}
		pointLights[i].reset();
		lightObjs[i].reset();
	}

	for (uint32 i = 0; i < NUM_SPHERES; i++)
	{
		if (sphereObjs[i])
		{
			sphereObjs[i]->Remove(rSpheres[i]);
			Scene->Remove(sphereObjs[i]);
		}
		rSpheres[i].reset();
		sphereObjs[i].reset();
		sphereMaterials[i].reset();
	}
	sphereMesh.reset();

	if (floorObj)
	{
		floorObj->Remove(rFloor);
		Scene->Remove(floorObj);
	}
	rFloor.reset();
	floorObj.reset();
	floorMaterial.reset();
	floorMesh.reset();

	delete deferredFBO;
	deferredFBO = nullptr;
	albedoTexture.reset();
	specularTexture.reset();
	depthTexture.reset();
	normalTexture.reset();
	metallicRoughnessTexture.reset();

	delete EffectManager;
	EffectManager = nullptr;
	delete Renderer;
	Renderer = nullptr;

	BaseExample::Shutdown();
}
