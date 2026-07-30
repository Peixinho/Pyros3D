//============================================================================
// Name        : PBRSpheres.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See PBRSpheres.h.
//============================================================================

#include "PBRSpheres.h"

using namespace p3d;

PBRSpheres::PBRSpheres() : BaseExample(1280, 720, "Pyros3D - PBR Spheres", WindowType::Close | WindowType::Resize)
{
}

PBRSpheres::~PBRSpheres() {}

void PBRSpheres::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);

	Renderer->Resize(width, height);
	EffectManager->Resize(width, height);
	projection.Perspective(60.f, (f32)width / (f32)height, 0.1f, 300.f);
}

void PBRSpheres::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetGlobalLight(Vec4(0.12f, 0.12f, 0.14f, 1.f));

	EffectManager = new PostEffectsManager(Width, Height);
	EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));

	projection.Perspective(60.f, (f32)Width / (f32)Height, 0.1f, 300.f);

	FPSCamera->SetPosition(Vec3(0.f, 0.f, 60.f));

	// Shared albedo for every sphere - a mid red, chosen so the difference
	// between "matte dielectric" (visible red diffuse body) and "polished
	// metal" (near-black body, sharp red-tinted highlight only) reads
	// clearly by eye.
	Vec4 albedo(0.7f, 0.15f, 0.15f, 1.f);

	// Smooth-shaded (smooth=true) - Sphere.cpp's default flat shading would
	// make roughness/metallic response hard to read across facets.
	sphereMesh = new Sphere(3.f, 24, 16, true);

	const f32 spacing = 8.f;
	const f32 half = (GRID - 1) * 0.5f * spacing;

	for (uint32 col = 0; col < GRID; col++)
	{
		for (uint32 row = 0; row < GRID; row++)
		{
			f32 metallic = (f32)col / (f32)(GRID - 1);
			// 0.05 floor, not 0.0 - avoids the GGX distribution's fully
			// degenerate mirror case at the edge of the calibration grid.
			f32 roughness = 0.05f + ((f32)row / (f32)(GRID - 1)) * 0.95f;

			GenericShaderMaterial* mat = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::PBR);
			mat->SetColor(albedo);
			mat->SetMetallic(metallic);
			mat->SetRoughness(roughness);
			// Sphere.cpp's hand-authored index winding is opposite Cube's -
			// default BackFace culling discards nearly the whole mesh (a
			// real, pre-existing, unrelated bug - see SkyboxTest.cpp/
			// VULKAN_ROADMAP.md's vertex-layout-generalization session).
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

	// Procedural 8x8 ORM-style gradient texture: G (roughness) increases
	// left->right, B (metalness) increases bottom->top - exercises
	// SetMetallicRoughnessMap()/PBRMAP's binding-16 sampler path, distinct
	// from the grid's scalar-only materials above.
	uchar pixels[8][8][4];
	for (uint32 y = 0; y < 8; y++)
	{
		for (uint32 x = 0; x < 8; x++)
		{
			pixels[y][x][0] = 0;
			pixels[y][x][1] = (uchar)(255.f * (f32)x / 7.f);
			pixels[y][x][2] = (uchar)(255.f * (f32)y / 7.f);
			pixels[y][x][3] = 255;
		}
	}
	metallicRoughnessTexture = new Texture();
	metallicRoughnessTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, 8, 8, false);
	metallicRoughnessTexture->UpdateData(&pixels);
	metallicRoughnessTexture->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
	metallicRoughnessTexture->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);

	mapMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::PBR | ShaderUsage::PBRMap);
	mapMaterial->SetColor(albedo);
	mapMaterial->SetMetallicRoughnessMap(metallicRoughnessTexture);
	mapMaterial->SetCullFace(CullFace::DoubleSided);

	mapObj = new GameObject();
	mapObj->SetPosition(Vec3(GRID * spacing, 0.f, 0.f));
	rMap = new RenderingComponent(sphereMesh, mapMaterial);
	mapObj->Add(rMap);
	Scene->Add(mapObj);

	// Lighting
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1.f, 0.98f, 0.92f, 1.f), Vec3(-0.4f, -0.6f, -0.7f));
	Light->Add(dLight);
	Scene->Add(Light);

	InitImGui();
}

void PBRSpheres::Update()
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

void PBRSpheres::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("PBR Spheres - Phase 1", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Metallic/roughness calibration grid (forward-only PBR)");
		ImGui::Separator();
		ImGui::Text("Columns: metallic 0 -> 1 (left to right)");
		ImGui::Text("Rows: roughness 0.05 -> 1 (bottom to top)");
		ImGui::Text("Rightmost sphere: SetMetallicRoughnessMap() gradient texture");
	}
	ImGui::End();
}

void PBRSpheres::Shutdown()
{
	mapObj->Remove(rMap);
	Scene->Remove(mapObj);
	delete rMap;
	delete mapObj;
	delete mapMaterial;
	delete metallicRoughnessTexture;

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

	Light->Remove(dLight);
	delete dLight;

	// EffectManager's destructor deletes the TonemapEffect it owns.
	delete EffectManager;

	// Renderer/Scene/FPSCamera are deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}
