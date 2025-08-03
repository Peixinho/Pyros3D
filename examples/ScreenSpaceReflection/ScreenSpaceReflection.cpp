//============================================================================
// Name        : ScreenSpaceReflection.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen Space Reflection Example
//============================================================================

#include "ScreenSpaceReflection.h"

using namespace p3d;

SSRFinalEffect::SSRFinalEffect(uint32 texture1, uint32 texture2, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
{
	// Set RTT - Use Color buffer as primary, SSR result as secondary
	UseRTT(texture1);  // Color buffer
	UseRTT(texture2);  // SSR result

	// Create Fragment Shader - Combine color with SSR
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
									"#define texture_cube textureCube\n"
									#if defined(GLES3)
										"precision mediump float;\n"
									#endif
								#endif
								#if defined(GLES2)
									"vec4 FragColor;\n"
								#else
									"out vec4 FragColor;\n"
								#endif
								"uniform sampler2D uTex0;\n"
								"uniform sampler2D uTex1;\n"
								"varying_in vec2 vTexcoord;\n"
								"void main() {\n"
								"	vec4 color = texture_2D(uTex0, vTexcoord);\n"
								"	vec4 ssr = texture_2D(uTex1, vTexcoord);\n"
								"	FragColor = color + ssr * 0.5;\n"
								#if defined(GLES2)
								"    gl_FragColor = FragColor;\n"
								#endif
								"}";

	CompileShaders();
}

ScreenSpaceReflection::ScreenSpaceReflection() : BaseExample(1024, 768, "Pyros3D - Screen Space Reflections", WindowType::Close | WindowType::Resize) {}

ScreenSpaceReflection::~ScreenSpaceReflection() {}

void ScreenSpaceReflection::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	EffectManager->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 2000.f);
}

void ScreenSpaceReflection::Init()
{
	// Call parent Init first
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Initialize Post Effects Manager
	EffectManager = new PostEffectsManager(Width, Height);

	// Initialize Screen Space Reflection Effect
	ssr = new ScreenSpaceReflectionEffect(RTT::Depth, Width, Height);
	EffectManager->AddEffect(ssr);
	ssrFinal = new SSRFinalEffect(RTT::Color, RTT::LastRTT, Width, Height);
	EffectManager->AddEffect(ssrFinal);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 2000.f);

	// Update camera position - much higher and further back
	FPSCamera->SetPosition(Vec3(0, 50, 150));
	FPSCamera->SetRotation(Vec3(-20, 0, 0)); // Look down more

	// Add a Directional Light - BRIGHTER
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	Light->Add(dLight);
	Scene->Add(Light);

	// Create Floor - MUCH BIGGER AND BRIGHTER
	gFloor = new GameObject();
	floor = new Cube(400, 1, 400); // Much bigger floor
	GenericShaderMaterial* floorMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
	floorMaterial->SetColor(Vec4(0.6f, 0.6f, 0.6f, 1.0f)); // Brighter floor
	rFloor = new RenderingComponent(floor, floorMaterial);
	gFloor->Add(rFloor);
	gFloor->SetPosition(Vec3(0, 0, 0)); // Lower floor
	Scene->Add(gFloor);

	// Create Teapots - use actual teapot model files
	teapot = new Model(STR(EXAMPLES_PATH) "/assets/teapotLOD1.p3dm");
	
	// Create a grid of teapots with different colors - SPACED OUT
	for (int x = -2; x <= 2; x++)
	{
		for (int z = -2; z <= 2; z++)
		{
			GameObject* g = new GameObject();
			GenericShaderMaterial* teapotMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
			
			// Different colors for each teapot
			Vec4 color;
			if ((x + z) % 3 == 0) {
				color = Vec4(0.8f, 0.2f, 0.2f, 1.0f); // Red
			} else if ((x + z) % 3 == 1) {
				color = Vec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
			} else {
				color = Vec4(0.2f, 0.2f, 0.8f, 1.0f); // Blue
			}
			
			teapotMaterial->SetColor(color);
			RenderingComponent* r = new RenderingComponent(teapot, teapotMaterial);
			g->Add(r);
			g->SetPosition(Vec3(x * 60, 5, z * 60)); // Higher up and spaced out
			g->SetScale(Vec3(0.3f, 0.3f, 0.3f)); // Make teapots bigger
			gTeapots.push_back(g);
			rTeapots.push_back(r);
			Scene->Add(g);
		}
	}

	// Add a big sphere at the top of the scene
	GameObject* topSphere = new GameObject();
	Renderable* sphereMesh = new Sphere(50, 32, 32);
	GenericShaderMaterial* sphereMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
	sphereMaterial->SetColor(Vec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow sphere
	RenderingComponent* rSphere = new RenderingComponent(sphereMesh, sphereMaterial);
	topSphere->Add(rSphere);
	topSphere->SetPosition(Vec3(0, 120, 0)); // Higher up
	Scene->Add(topSphere);
	
	// Initialize ImGui after renderer and materials are set up
	InitImGui();
	printf("ScreenSpaceReflection::Init() - ImGui initialized\n");
	
	// Initialize ImGui SSR controls
	ssrMaxSteps = 256.0f;
	ssrMaxDistance = 10.0f;
	ssrThickness = 0.1f;
	ssrReflectionStrength = 1.0f; // Increased from 0.5f to make reflections more visible
	ssrFloorHeight = 0.0f;
	ssrFloorTolerance = 2.0f;
	ssrDebugMode = false;
}

void ScreenSpaceReflection::Update()
{
	// Call parent Update for camera movement
	BaseExample::Update();

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here - rotate teapots
	for (size_t i = 0; i < gTeapots.size(); i++)
	{
		gTeapots[i]->SetRotation(Vec3(0, (f32)GetTime() * 0.5f + i * 0.5f, 0));
	}

	// Capture frame for post effects
	EffectManager->CaptureFrame();

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	// End capture and process post effects
	EffectManager->EndCapture();
	EffectManager->ProcessPostEffects(&projection);
	
	// Render ImGui AFTER the main scene to avoid interference
	RenderImGui();
}

void ScreenSpaceReflection::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	for (size_t i = 0; i < gTeapots.size(); i++)
	{
		Scene->Remove(gTeapots[i]);
		gTeapots[i]->Remove(rTeapots[i]);
		delete rTeapots[i];
		delete gTeapots[i];
	}

	Scene->Remove(Light);
	Scene->Remove(gFloor);

	Light->Remove(dLight);
	gFloor->Remove(rFloor);

	// Delete resources in proper order
	delete rFloor;
	delete gFloor;
	delete floor;
	delete teapot;
	delete dLight;
	delete Light;
	delete Renderer;
	delete EffectManager; // This will delete all effects including ssr and ssrFinal
	// Don't delete ssr and ssrFinal manually - EffectManager handles it

	// Call parent Shutdown last
	BaseExample::Shutdown();
}

void ScreenSpaceReflection::DrawUI()
{
	// Call base UI first
	BaseExample::DrawUI();
	
	// Create ImGui window for Screen Space Reflection controls
	ImGui::Begin("Screen Space Reflection Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	
	ImGui::Text("Interactive Screen Space Reflection Controls");
	ImGui::Separator();
	
	// SSR Effect Controls
	ImGui::Text("SSR Effect Settings");
	
	// Max Steps slider
	if (ImGui::SliderFloat("Max Steps", &ssrMaxSteps, 64.0f, 512.0f, "%.0f")) {
		printf("SSR Max Steps changed to: %.0f\n", ssrMaxSteps);
		ssr->SetMaxSteps((uint32)ssrMaxSteps);
	}
	
	// Max Distance slider
	if (ImGui::SliderFloat("Max Distance", &ssrMaxDistance, 1.0f, 50.0f, "%.1f")) {
		printf("SSR Max Distance changed to: %.1f\n", ssrMaxDistance);
		ssr->SetMaxDistance(ssrMaxDistance);
	}
	
	// Thickness slider
	if (ImGui::SliderFloat("Surface Thickness", &ssrThickness, 0.01f, 1.0f, "%.3f")) {
		printf("SSR Thickness changed to: %.3f\n", ssrThickness);
		ssr->SetThickness(ssrThickness);
	}
	
	// Reflection Strength slider
	if (ImGui::SliderFloat("Reflection Strength", &ssrReflectionStrength, 0.0f, 2.0f, "%.2f")) {
		printf("SSR Reflection Strength changed to: %.2f\n", ssrReflectionStrength);
		ssr->SetReflectionStrength(ssrReflectionStrength);
	}
	
	ImGui::Separator();
	ImGui::Text("Debug Controls");
	
	// Debug mode toggle
	if (ImGui::Checkbox("Debug Mode", &ssrDebugMode)) {
		printf("SSR Debug Mode: %s\n", ssrDebugMode ? "ON" : "OFF");
	}
	
	ImGui::Separator();
	ImGui::Text("Scene Information");
	ImGui::Text("Teapots: %zu", gTeapots.size());
	ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)", 
		FPSCamera->GetPosition().x, FPSCamera->GetPosition().y, FPSCamera->GetPosition().z);
	
	ImGui::Separator();
	ImGui::Text("Performance");
	ImGui::Text("FPS: %.1u", (uint32)fps.getFPS());
	ImGui::Text("Frame Time: %.3f ms", 1000.0f / fps.getFPS());
	
	ImGui::End();
}
