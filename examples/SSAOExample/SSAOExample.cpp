//============================================================================
// Name        : SSAOExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Example
//============================================================================

#include "SSAOExample.h"

using namespace p3d;

SSAOEffectFinal::SSAOEffectFinal(uint32 texture1, uint32 texture2, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
{
	// Set RTT
	UseRTT(texture1);
	UseRTT(texture2);

	// Create Fragment Shader
	FragmentShaderString =
							#if defined(GLES2)
								"#define varying_in varying\n"
								"#define varying_out varying\n"
								"#define attribute_in attribute\n"
								"#define texture_2D texture2D\n"
								"#define texture_cube textureCube\n"
								"precision mediump float;\n"
							#else
								"#define varying_in in\n"
								"#define varying_out out\n"
								"#define attribute_in in\n"
								"#define texture_2D texture\n"
								"#define texture_cube texture\n"
								#if defined(GLES3)
									"precision mediump float;\n"
								#endif
							#endif
							// See SSAOEffect.cpp's identical comment - this
							// effect has no non-sampler uniforms, so it
							// only needs the sampler/varying-location
							// macros, not UBO_BINDING.
							"#if defined(VULKAN)\n"
							"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
							"#define IO_LOCATION(n) layout(location = n)\n"
							"#else\n"
							"#define SAMPLER_BINDING(n)\n"
							"#define IO_LOCATION(n)\n"
							"#endif\n"
							#if defined(GLES2)
								"vec4 FragColor;\n"
							#else
								"IO_LOCATION(0) out vec4 FragColor;\n"
							#endif
							"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
							"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
							"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
							"void main() {\n"
							"FragColor.r = texture_2D(uTex1, vTexcoord).r;\n"
							"FragColor.g = texture_2D(uTex1, vTexcoord).g;\n"
							"FragColor.b = texture_2D(uTex1, vTexcoord).b;\n"
							"FragColor.a = 1.0;\n"
							"FragColor = texture_2D(uTex0, vTexcoord)*texture_2D(uTex1, vTexcoord);\n"
							#if defined(GLES2)
							"gl_FragColor = FragColor;\n"
							#endif
							"}";

	CompileShaders();

}

SSAOExample::SSAOExample() : BaseExample(1024, 768, "Pyros3D - SSAO Example", WindowType::Close | WindowType::Resize)
{

}

void SSAOExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.01f, 50.f);
	EffectManager->Resize(Width, Height);
}

void SSAOExample::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize ImGui
	InitImGui();

	// Initialize ImGui control variables
	ssaoRadius = 0.2f;
	ssaoStrength = 1.5f;
	ssaoThreshold = 2.0f;
	ssaoScale = 1.0f;
	ssaoBlurIntensity = 1.0f;
	valuesChanged = false;

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetGlobalLight(Vec4(0.2, 0.2, 0.2, 0.2));
	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.01f, 50.f);

	// Create Camera
	FPSCamera->SetPosition(Vec3(0.f,3.f,3.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-45.f),0.f,0.f));

	// Teapots
	teapot = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD1.p3dm");
	for (uint32 j = 0; j < 10; j++)
	for (uint32 i = 0; i < 10; i++)
	{
		GameObject* g = new GameObject();
		RenderingComponent* r = new RenderingComponent(teapot, ShaderUsage::Diffuse);
		g->Add(r);
		g->SetPosition(Vec3(-5.f + (i * 1.f), 0.4f, -5 + (j * 1.f)));
		g->SetScale(Vec3(0.01f, 0.01f, 0.01f));
		g->SetRotation(Vec3(0.f, DEGTORAD(33.f), 0.f));
		gTeapots.push_back(g);
		rTeapots.push_back(r);
		Scene->Add(g);
	}
	
	// Floor
	floor = new Plane(10, 10);
	gFloor = new GameObject();
	gFloor->SetRotation(Vec3(DEGTORAD(-90), 0, 0));
	rFloor = new RenderingComponent(floor);
	gFloor->Add(rFloor);
	Scene->Add(gFloor);
	
	EffectManager = new PostEffectsManager(Width, Height);
	
	// Create SSAO Effect
	ssao = new SSAOEffect(RTT::Depth, Width, Height);
	ssao->SetRadius(ssaoRadius);
	ssao->SetStrength(ssaoStrength);
	ssao->SetTreshOld(ssaoThreshold);
	ssao->SetScale(ssaoScale);
	
	// Create SSAO Blur Effect
	ssaoBlur = new BlurSSAOEffect(RTT::LastRTT, Width, Height);
	
	EffectManager->AddEffect(ssao);
	EffectManager->AddEffect(ssaoBlur);
	EffectManager->AddEffect(new SSAOEffectFinal(RTT::Color, RTT::LastRTT, Width, Height));

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	Light->Add(dLight);
	Scene->Add(Light);
}

void SSAOExample::Update()
{
	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	ssao->SetViewMatrix(FPSCamera->GetWorldTransformation().Inverse());

	// Update SSAO parameters if values changed
	if (valuesChanged && ssao) {
		ssao->SetRadius(ssaoRadius);
		ssao->SetStrength(ssaoStrength);
		ssao->SetTreshOld(ssaoThreshold);
		ssao->SetScale(ssaoScale);
		ssaoBlur->SetIntensity(ssaoBlurIntensity);
		valuesChanged = false;
	}

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();

	EffectManager->ProcessPostEffects(&projection);	

	// Render ImGui
	RenderImGui();
}

void SSAOExample::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene

	for (std::vector<RenderingComponent*>::iterator i = rTeapots.begin(); i!=rTeapots.end(); i++)
	{
		(*i)->GetOwner()->Remove(*i);
		delete (*i);
	}

	for (std::vector<GameObject*>::iterator i = gTeapots.begin(); i!=gTeapots.end(); i++)
	{
		Scene->Remove((*i));
		delete (*i);
	}
	delete teapot;

	Scene->Remove(gFloor);
	gFloor->Remove(rFloor);
	delete gFloor;
	delete rFloor;
	delete floor;

	// Renderer is deleted by BaseExample::Shutdown()
	delete EffectManager;

	BaseExample::Shutdown();

}

void SSAOExample::DrawUI() {
	BaseExample::DrawUI();
	
	ImGui::Begin("SSAO Controls");
	
	// SSAO Controls
	if (ImGui::CollapsingHeader("SSAO Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool changed = false;
		changed |= ImGui::SliderFloat("Radius", &ssaoRadius, 0.05f, 0.5f);
		changed |= ImGui::SliderFloat("Strength", &ssaoStrength, 0.5f, 2.5f);
		changed |= ImGui::SliderFloat("Threshold", &ssaoThreshold, 0.5f, 4.0f);
		changed |= ImGui::SliderFloat("Scale", &ssaoScale, 50.0f, 150.0f);
		changed |= ImGui::SliderFloat("Blur Intensity", &ssaoBlurIntensity, 0.0f, 2.0f);
		
		// Only update if values changed
		if (changed) {
			valuesChanged = true;
		}
	}
	
	ImGui::End();
}

SSAOExample::~SSAOExample() {}
