//============================================================================
// Name        : DeferredPBRSpheres.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See DeferredPBRSpheres.h.
//============================================================================

#include "DeferredPBRSpheres.h"

using namespace p3d;

DeferredPBRSpheres::DeferredPBRSpheres() : BaseExample(1280, 720, "Pyros3D - Deferred PBR Spheres", WindowType::Close | WindowType::Resize)
{
}

DeferredPBRSpheres::~DeferredPBRSpheres() {}

void DeferredPBRSpheres::OnResize(const uint32 width, const uint32 height)
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

void DeferredPBRSpheres::Init()
{
	BaseExample::Init();

	FPSCamera->SetPosition(Vec3(0.f, 0.f, 20.f));

	// G-buffer setup - same 4-color-attachment shape as examples/DeferredRendering,
	// see PyrosShader.glsl's FragData_r/g/b/pbr for what each carries.
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

	Renderer = new DeferredRenderer(Width, Height, deferredFBO);
	Renderer->SetGlobalLight(Vec4(0.12f, 0.12f, 0.14f, 1.f));

	EffectManager = new PostEffectsManager(Width, Height);
	EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));

	projection.Perspective(60.f, (f32)Width / (f32)Height, 0.1f, 200.f);

	// Shared albedo, same reasoning as examples/PBRSpheres: makes the
	// dielectric/metal + rough/smooth distinction easy to read by eye.
	Vec4 albedo(0.7f, 0.15f, 0.15f, 1.f);

	sphereMesh = new Sphere(1.5f, 24, 16, true);

	const f32 spacing = 4.5f;
	const f32 half = (GRID - 1) * 0.5f * spacing;

	for (uint32 col = 0; col < GRID; col++)
	{
		for (uint32 row = 0; row < GRID; row++)
		{
			f32 metallic = (f32)col / (f32)(GRID - 1);
			f32 roughness = 0.05f + ((f32)row / (f32)(GRID - 1)) * 0.95f;

			GenericShaderMaterial* mat = new GenericShaderMaterial(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::PBR);
			mat->SetColor(albedo);
			mat->SetMetallic(metallic);
			mat->SetRoughness(roughness);
			// Sphere.cpp's hand-authored index winding is opposite Cube's -
			// see examples/PBRSpheres for the same pre-existing-bug workaround.
			mat->SetCullFace(CullFace::DoubleSided);
			gridMaterials[col][row] = mat;

			GameObject* obj = new GameObject();
			obj->SetPosition(Vec3(col * spacing - half, row * spacing - half, 0.f));
			RenderingComponent* r = new RenderingComponent(sphereMesh, mat);
			obj->Add(r);
			Scene->Add(obj);

			gridObjs[col][row] = obj;
			rGrid[col][row] = r;
		}
	}

	// A handful of deliberately-placed, close point lights - unlike
	// examples/DeferredRendering's 100 randomly-scattered tiny-radius
	// lights, these are positioned to actually illuminate the grid from
	// visibly different angles.
	Vec3 lightPositions[NUM_LIGHTS] = {
		Vec3(-10.f, 10.f, 12.f), Vec3(10.f, 10.f, 12.f),
		Vec3(-10.f, -10.f, 12.f), Vec3(10.f, -10.f, 12.f)
	};
	for (uint32 i = 0; i < NUM_LIGHTS; i++)
	{
		lightObjs[i] = new GameObject();
		lightObjs[i]->SetPosition(lightPositions[i]);
		pointLights[i] = new PointLight(Vec4(1.5f, 1.5f, 1.5f, 1.f), 35.f);
		lightObjs[i]->Add(pointLights[i]);
		Scene->Add(lightObjs[i]);
	}

	InitImGui();
}

void DeferredPBRSpheres::Update()
{
	Scene->Update(GetTime());

	BaseExample::Update();

	EffectManager->CaptureFrame();
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();
	EffectManager->ProcessPostEffects(&projection);

	RenderImGui();
}

void DeferredPBRSpheres::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("Deferred PBR Spheres", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Metallic/roughness calibration grid via DeferredRenderer");
		ImGui::Separator();
		ImGui::Text("Columns: metallic 0 -> 1 (left to right)");
		ImGui::Text("Rows: roughness 0.05 -> 1 (bottom to top)");
		ImGui::Text("Lighting: Cook-Torrance GGX (PBR), %u point lights", NUM_LIGHTS);
	}
	ImGui::End();
}

void DeferredPBRSpheres::Shutdown()
{
	for (uint32 i = 0; i < NUM_LIGHTS; i++)
	{
		lightObjs[i]->Remove(pointLights[i]);
		Scene->Remove(lightObjs[i]);
		delete pointLights[i];
		delete lightObjs[i];
	}

	for (uint32 col = 0; col < GRID; col++)
	{
		for (uint32 row = 0; row < GRID; row++)
		{
			gridObjs[col][row]->Remove(rGrid[col][row]);
			Scene->Remove(gridObjs[col][row]);
			delete rGrid[col][row];
			delete gridObjs[col][row];
			delete gridMaterials[col][row];
		}
	}
	delete sphereMesh;

	delete deferredFBO;
	delete albedoTexture;
	delete specularTexture;
	delete depthTexture;
	delete normalTexture;
	delete metallicRoughnessTexture;

	// EffectManager's destructor deletes the TonemapEffect it owns.
	delete EffectManager;

	// Renderer/Scene/FPSCamera are deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}
