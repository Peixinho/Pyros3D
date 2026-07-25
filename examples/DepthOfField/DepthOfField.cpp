//============================================================================
// Name        : DepthOfField.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)internalFormat3 = GL_FLOAT;
// Description : Game Example
//============================================================================

#include "DepthOfField.h"

using namespace p3d;

DepthOfFieldEffect::DepthOfFieldEffect(Texture* texture1, Texture* texture2, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
{
	// Set RTT
	UseCustomTexture(texture1);
	UseCustomTexture(texture2);
	UseRTT(RTT::Color);
	UseRTT(RTT::Depth);

	// Create Fragment Shader
	FragmentShaderString =
		#if defined(GLES2)
			"#define varying_in varying\n"
			"#define varying_out varying\n"
			"#define attribute_in attribute\n"
			"#define texture_2D texture2D\n"
			"#define texture_cube textureCube\n"
			"precision mediump float;"
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
		// See PostEffects/Effects/SSAOEffect.cpp's identical comment -
		// Vulkan/SPIR-V needs a static layout(binding=) on every UBO/
		// sampler and layout(location=) on every varying/output, and
		// rejects non-opaque uniforms outside a block outright; GL needs
		// none of this. VULKAN is predefined by shaderc itself for any
		// Vulkan-target compile. Binding 30 - see IEffect.h's comment on
		// extraUniformsBinding for why this must be globally distinct
		// from every other effect's own binding.
		"#if defined(VULKAN)\n"
		"#define UBO_BINDING(n) layout(std140, binding = n)\n"
		"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
		"#define IO_LOCATION(n) layout(location = n)\n"
		"#else\n"
		"#define UBO_BINDING(n)\n"
		"#define SAMPLER_BINDING(n)\n"
		"#define IO_LOCATION(n)\n"
		"#endif\n"
		#if defined(GLES2)
			"vec4 FragColor;"
		#else
			"IO_LOCATION(0) out vec4 FragColor;"
		#endif
		"float DecodeNativeDepth(float native_z, vec4 z_info_local)\n"
		"{\n"
		"return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);\n"
		"}\n"
		"SAMPLER_BINDING(0) uniform sampler2D uTex0; // lower res blur\n"
		"SAMPLER_BINDING(1) uniform sampler2D uTex1; // medium res blur\n"
		"SAMPLER_BINDING(2) uniform sampler2D uTex2; // high res\n"
		"SAMPLER_BINDING(3) uniform sampler2D uTex3; // depth\n"
		"UBO_BINDING(30) uniform DepthOfFieldParams {\n"
		"	vec2 uNearFar;\n"
		"	float uFocalPosition;\n"
		"	float uFocalRange;\n"
		"	float uRatioL;\n"
		"	float uRatioH;\n"
		"};\n"
		"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
		"void main() {\n"
			"float ratioL = uRatioL;\n"
			"float ratioH = uRatioH;\n"
			"float focalPosition = uFocalPosition;\n"
			"float focalRange = uFocalRange;\n"
			"vec4 z_info_local = vec4(uNearFar.x,uNearFar.y,uNearFar.x*uNearFar.y,uNearFar.x-uNearFar.y);\n"
			"float depth = texture_2D(uTex3, vTexcoord).x;\n"
			"float linearDepth = DecodeNativeDepth(depth, z_info_local);\n"
			"float ratio = clamp(abs(focalPosition-linearDepth)-focalRange, 0.0, ratioL);\n"
			"if (ratio < 0.4) FragColor = mix(texture_2D(uTex2, vTexcoord), texture_2D(uTex1, vTexcoord), ratio / (ratioL - ratioH));\n"
			"else FragColor =  mix(texture_2D(uTex1, vTexcoord), texture_2D(uTex0, vTexcoord), (ratio-ratioH) / (ratioL - ratioH));\n"
			#if defined(GLES2)
			"gl_FragColor = FragColor;\n"
			#endif
		"}";

	CompileShaders();

	Uniform nearFarPlane;
	nearFarPlane.Name = "uNearFar";
	nearFarPlane.Type = DataType::Vec2;
	nearFarPlane.Usage = PostEffects::NearFarPlane;
	AddUniform(nearFarPlane);

	f32 fPosition = 20.f;
	f32 fRange = 2.f;
	f32 rL = 3.1f;
	f32 rH = 1.0f;

	AddUniform(Uniform("uFocalPosition", Uniforms::DataType::Float, &fPosition));
	AddUniform(Uniform("uFocalRange", Uniforms::DataType::Float, &fRange));
	AddUniform(Uniform("uRatioL", Uniforms::DataType::Float, &rL));
	AddUniform(Uniform("uRatioH", Uniforms::DataType::Float, &rH));

	// See SSAOEffect.cpp's comment on extraUniformsBinding - matches the
	// DepthOfFieldParams block declared in FragmentShaderString above
	// (std140: vec2 uNearFar at 0, then 4 floats packed at 4-byte
	// alignment starting at 8).
	extraUniformsBinding = 30;
	extraUniformsSize = 24;
	extraUniformsScratch.resize(extraUniformsSize, 0);
	extraUniformOffsets["uNearFar"] = 0;
	extraUniformOffsets["uFocalPosition"] = 8;
	extraUniformOffsets["uFocalRange"] = 12;
	extraUniformOffsets["uRatioL"] = 16;
	extraUniformOffsets["uRatioH"] = 20;
}

DepthOfField::DepthOfField() : BaseExample(1024, 768, "Pyros3D - Depth Of Field", WindowType::Close | WindowType::Resize) {}

void DepthOfField::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);

	EffectManager->Resize(width, height);
	blurX->Resize(width, height);
	blurY->Resize(width, height);
	resize->Resize((uint32)(width*0.25f), (uint32)(height*0.25f));
	blurXlow->Resize((uint32)(width*0.25f), (uint32)(height*0.25f));
	blurYlow->Resize((uint32)(width*0.25f), (uint32)(height*0.25f));
	depthOfField->Resize((uint32)(width), (uint32)(height));
}

void DepthOfField::Init()
{
	// Initialization

	BaseExample::Init();

	FPSCamera->SetPosition(Vec3(0, 2, 20));

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetBackground(Vec4(1, 0, 0, 1));
	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);

	Scene->Add(Light);

	// Create Game Object
	modelMesh = new Model(STR(EXAMPLES_PATH)"/assets/suzanne.p3dm", false);

	for (uint32 i = 0; i < 10; i++)
	{
		GameObject* Monkey = new GameObject();
		Monkey->SetPosition(Vec3(-23.f + i * 3.f, 0, -15.f + i * 3.f));
		RenderingComponent* rMonkey = new RenderingComponent(modelMesh, ShaderUsage::Diffuse);
		Monkey->Add(rMonkey);
		Scene->Add(Monkey);

		go.push_back(Monkey);
		rc.push_back(rMonkey);
	}

	fullResBlur = new Texture();
	fullResBlur->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height);
	fullResBlur->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	lowResBlur = new Texture();
	lowResBlur->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, (int32)(Width*.25f), (int32)(Height*.25f));
	lowResBlur->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	EffectManager = new PostEffectsManager(Width, Height);

	blurX = new BlurXEffect(RTT::Color, Width, Height);
	blurY = new BlurYEffect(RTT::LastRTT, Width, Height);
	fullResBlur = blurY->GetTexture();
	resize = new ResizeEffect(RTT::Color, (uint32)(Width*.25f), (uint32)(Height*.25f));
	blurXlow = new BlurXEffect(RTT::LastRTT, (uint32)(Width*.25f), (uint32)(Height*.25f));
	blurYlow = new BlurYEffect(RTT::LastRTT, (uint32)(Width*.25f), (uint32)(Height*.25f));
	lowResBlur = blurYlow->GetTexture();
	depthOfField = new DepthOfFieldEffect(lowResBlur, fullResBlur, Width, Height);

	EffectManager->AddEffect(blurX);
	EffectManager->AddEffect(blurY);
	EffectManager->AddEffect(resize);
	EffectManager->AddEffect(blurXlow);
	EffectManager->AddEffect(blurYlow);
	EffectManager->AddEffect(depthOfField);
	
	// Initialize ImGui
	InitImGui();
}

void DepthOfField::Update()
{
	// Update - Game Loop

		// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	// Game Logic Here
	for (uint32 i = 0; i < 10; i++)
	{
		go[i]->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));
	}

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();

	// Render Post Processing
	EffectManager->ProcessPostEffects(&projection);
	RenderImGui();
}

void DepthOfField::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();
	
	// Depth of Field Information
	if (ImGui::Begin("Depth of Field Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Depth of Field System Information");
		ImGui::Separator();
		
		// Effect Information
		ImGui::Text("Post-Processing Effects:");
		ImGui::Text("  Blur X Effect: Active");
		ImGui::Text("  Blur Y Effect: Active");
		ImGui::Text("  Resize Effect: Active");
		ImGui::Text("  Low-Res Blur X: Active");
		ImGui::Text("  Low-Res Blur Y: Active");
		ImGui::Text("  Depth of Field: Active");
		
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Information:");
		ImGui::Text("  Objects: 10 Monkeys");
		ImGui::Text("  Camera Position: (%.1f, %.1f, %.1f)", 
			FPSCamera->GetPosition().x, 
			FPSCamera->GetPosition().y, 
			FPSCamera->GetPosition().z);
		ImGui::Text("  Resolution: %dx%d", Width, Height);
		ImGui::Text("  Low-Res: %dx%d", (int)(Width*0.25f), (int)(Height*0.25f));
		
		ImGui::Separator();
		
		// Controls
		ImGui::Text("Controls:");
		ImGui::Text("  Tab: Toggle mouse capture");
		ImGui::Text("  WASD: Move camera");
		ImGui::Text("  Mouse: Look around");
		ImGui::Text("  Monkeys rotate automatically");
	}
	ImGui::End();
}

void DepthOfField::Shutdown()
{
	// All your Shutdown Code Here

		// Remove GameObjects From Scene
	for (uint32 i = 0; i < 10; i++)
	{
		Scene->Remove(go[i]);
		go[i]->Remove(rc[i]);
		delete go[i];
		delete rc[i];
	}

	Scene->Remove(FPSCamera);

	// Delete
	delete modelMesh;
	// Renderer is deleted by BaseExample::Shutdown()
	delete EffectManager; // this deletes all effects

	BaseExample::Shutdown();
}

DepthOfField::~DepthOfField() {}
