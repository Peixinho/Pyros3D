//============================================================================
// Name        : SSRTest.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See SSRTest.h.
//============================================================================

#include "SSRTest.h"

using namespace p3d;

SSRTest::SSRTest() : BaseExample(1280, 800, "Pyros3D - SSR Test", WindowType::Close | WindowType::Resize) {}

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
	// SetRotation takes radians (see SSAOExample's identical DEGTORAD()
	// usage for its own floor+camera) - passing raw degrees here first
	// pointed the camera at an effectively-random angle.
	FPSCamera->SetPosition(Vec3(0.f, 7.f, 18.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-18.f), 0.f, 0.f));

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
	// SSR is opt-in (defaults off - see DeferredRenderer's constructor
	// comment) precisely so examples that never asked for it don't
	// inherit whatever's still unresolved in it. This is the one example
	// actually built to showcase it.
	Renderer->EnableSSR();

	EffectManager = new PostEffectsManager(Width, Height);
	EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));

	projection.Perspective(60.f, (f32)Width / (f32)Height, 0.1f, 200.f);

	// Floor - low roughness, dielectric (metallic=0, so Fresnel still
	// gives real edge-on reflection without tinting it like a metal
	// would) - the reflective surface material-aware SSR is meant to
	// show off. Rotated flat (Plane's default lies in XY, normal +Z).
	floorMesh = new Plane(30.f, 30.f, true);
	floorMaterial = new GenericShaderMaterial(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::PBR);
	floorMaterial->SetColor(Vec4(0.5f, 0.5f, 0.55f, 1.f));
	floorMaterial->SetMetallic(0.0f);
	floorMaterial->SetRoughness(0.08f);
	floorMaterial->SetCullFace(CullFace::DoubleSided);
	floorObj = new GameObject();
	floorObj->SetPosition(Vec3(0.f, -3.f, 0.f));
	floorObj->SetRotation(Vec3(DEGTORAD(-90.f), 0.f, 0.f));
	rFloor = new RenderingComponent(floorMesh, floorMaterial);
	floorObj->Add(rFloor);
	Scene->Add(floorObj);

	// A handful of spheres above the floor, varying roughness/metallic -
	// low-roughness/metal ones should show sharp reflections, high-
	// roughness ones should fade to none (SSR_ROUGHNESS_CUTOFF in
	// lastPass.glsl).
	Vec4 albedo(0.75f, 0.2f, 0.15f, 1.f);
	sphereMesh = new Sphere(1.4f, 24, 16, true);
	struct { f32 x, y, z, metallic, roughness; } sphereDesc[NUM_SPHERES] = {
		{ -6.f, 0.f, -2.f, 1.0f, 0.05f },
		{ -3.f, 0.f,  1.f, 0.0f, 0.15f },
		{  0.f, 0.f, -1.f, 1.0f, 0.35f },
		{  3.f, 0.f,  1.f, 0.0f, 0.6f },
		{  6.f, 0.f, -2.f, 0.5f, 0.9f },
	};
	for (uint32 i = 0; i < NUM_SPHERES; i++)
	{
		GenericShaderMaterial* mat = new GenericShaderMaterial(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::PBR);
		mat->SetColor(albedo);
		mat->SetMetallic(sphereDesc[i].metallic);
		mat->SetRoughness(sphereDesc[i].roughness);
		// Sphere.cpp's hand-authored index winding is opposite Cube's -
		// see examples/PBRSpheres for the same pre-existing-bug workaround.
		mat->SetCullFace(CullFace::DoubleSided);
		sphereMaterials[i] = mat;

		GameObject* obj = new GameObject();
		obj->SetPosition(Vec3(sphereDesc[i].x, sphereDesc[i].y, sphereDesc[i].z));
		RenderingComponent* r = new RenderingComponent(sphereMesh, mat);
		obj->Add(r);
		Scene->Add(obj);

		sphereObjs[i] = obj;
		rSpheres[i] = r;
	}

	Vec3 lightPositions[NUM_LIGHTS] = { Vec3(-8.f, 8.f, 10.f), Vec3(8.f, 8.f, 10.f) };
	for (uint32 i = 0; i < NUM_LIGHTS; i++)
	{
		lightObjs[i] = new GameObject();
		lightObjs[i]->SetPosition(lightPositions[i]);
		pointLights[i] = new PointLight(Vec4(1.6f, 1.6f, 1.6f, 1.f), 30.f);
		lightObjs[i]->Add(pointLights[i]);
		Scene->Add(lightObjs[i]);
	}

	dirLightObj = new GameObject();
	dirLight = new DirectionalLight(Vec4(0.5f, 0.5f, 0.5f, 1.f), Vec3(-0.3f, -0.7f, -0.5f));
	dirLightObj->Add(dirLight);
	Scene->Add(dirLightObj);

	orbitAngle = 0.f;

	InitImGui();
}

void SSRTest::Update()
{
	Scene->Update(GetTime());

	BaseExample::Update();

	// Slow orbit - the reprojection check (does a reflection swim/ghost
	// when the camera moves, rather than only ever being verified static).
	// Pitch also varies (not just yaw): this used to expose a real bug -
	// DeferredRenderer::RenderScene() shifted PrvViewMatrix from
	// IRenderer::ViewMatrix *after* PreRender() had already overwritten
	// ViewMatrix with this same frame's camera, so PrvViewMatrix was
	// never actually one frame stale - reprojection silently collapsed to
	// sampling tPreviousFrameColor at this frame's own hit UV with zero
	// motion compensation. Fixed by giving DeferredRenderer its own
	// dedicated ssrPrvViewMatrix/ssrPrvProjectionMatrix, updated once at
	// the end of RenderScene() from that call's own Camera/projection
	// arguments - see DeferredRenderer.h's comment on those members.
	// Kept varying pitch here (not just yaw) since that's what originally
	// exposed the bug most clearly, as a regression check.
	orbitAngle = (f32)GetTime() * 8.f;
	f32 rad = orbitAngle * (3.14159265f / 180.f);
	f32 pitchDeg = -18.f + 10.f * sinf((f32)GetTime() * 0.5f);
	FPSCamera->SetPosition(Vec3(sinf(rad) * 18.f, 7.f, cosf(rad) * 18.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(pitchDeg), DEGTORAD(orbitAngle), 0.f));

	EffectManager->CaptureFrame();
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();
	EffectManager->ProcessPostEffects(&projection);

	RenderImGui();
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
		ImGui::Text("Camera slowly orbits - reflections should track");
		ImGui::Text("smoothly, not swim/ghost, as the view changes.");
	}
	ImGui::End();
}

void SSRTest::Shutdown()
{
	dirLightObj->Remove(dirLight);
	Scene->Remove(dirLightObj);
	delete dirLight;
	delete dirLightObj;

	for (uint32 i = 0; i < NUM_LIGHTS; i++)
	{
		lightObjs[i]->Remove(pointLights[i]);
		Scene->Remove(lightObjs[i]);
		delete pointLights[i];
		delete lightObjs[i];
	}

	for (uint32 i = 0; i < NUM_SPHERES; i++)
	{
		sphereObjs[i]->Remove(rSpheres[i]);
		Scene->Remove(sphereObjs[i]);
		delete rSpheres[i];
		delete sphereObjs[i];
		delete sphereMaterials[i];
	}
	delete sphereMesh;

	floorObj->Remove(rFloor);
	Scene->Remove(floorObj);
	delete rFloor;
	delete floorObj;
	delete floorMaterial;
	delete floorMesh;

	delete deferredFBO;
	delete albedoTexture;
	delete specularTexture;
	delete depthTexture;
	delete normalTexture;
	delete metallicRoughnessTexture;

	delete EffectManager;

	BaseExample::Shutdown();
}
