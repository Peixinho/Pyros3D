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
		#if defined(GLES2)
			"vec4 FragColor;"
		#else
			"out vec4 FragColor;"
		#endif
		"float DecodeNativeDepth(float native_z, vec4 z_info_local)\n"
		"{\n"
		"return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);\n"
		"}\n"
		"uniform sampler2D uTex0; // lower res blur\n"
		"uniform sampler2D uTex1; // medium res blur\n"
		"uniform sampler2D uTex2; // high res\n"
		"uniform sampler2D uTex3; // depth\n"
		"uniform float uFocalPosition, uFocalRange, uRatioL, uRatioH;\n"
		"uniform vec2 uNearFar;\n"
		"varying_in vec2 vTexcoord;\n"
		"void main() {\n"
			"float ratioL = uRatioL;\n"
			"float ratioH = uRatioH;\n"
			"float focalPosition = uFocalPosition;\n"
			"float focalRange = uFocalRange;\n"
			"vec4 z_info_local = vec4(uNearFar.x,uNearFar.y,uNearFar.x*uNearFar.y,uNearFar.x-uNearFar.y);\n"
			"float depth = texture2D(uTex3, vTexcoord).x;\n"
			"float linearDepth = DecodeNativeDepth(depth, z_info_local);\n"
			"float ratio = clamp(abs(focalPosition-linearDepth)-focalRange, 0.0, ratioL);\n"
			"if (ratio < 0.4) FragColor = mix(texture2D(uTex2, vTexcoord), texture2D(uTex1, vTexcoord), ratio / (ratioL - ratioH));\n"
			"else FragColor =  mix(texture2D(uTex1, vTexcoord), texture2D(uTex0, vTexcoord), (ratio-ratioH) / (ratioL - ratioH));\n"
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
}

DepthOfField::DepthOfField() : ClassName(1024, 768, "Pyros3D - Depth Of Field", WindowType::Close | WindowType::Resize) {}

void DepthOfField::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

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

		// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetBackground(Vec4(1, 0, 0, 1));
	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 2, 20));

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);

	Scene->Add(Light);

	// Create Game Object
	modelMesh = new Model(STR(EXAMPLES_PATH)"/DepthOfField/assets/suzanne.p3dm", false);

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

	// Add Camera to Scene
	Scene->Add(Camera);

	Camera->LookAt(Vec3::ZERO);

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
}

void DepthOfField::Update()
{
	// Update - Game Loop

		// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	for (uint32 i = 0; i < 10; i++)
	{
		go[i]->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));
	}

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
	EffectManager->EndCapture();

	// Render Post Processing
	EffectManager->ProcessPostEffects(&projection);
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

	Scene->Remove(Camera);

	// Delete
	delete modelMesh;
	delete Camera;
	delete Renderer;
	delete Scene;
	delete EffectManager; // this deletes all effects
}

DepthOfField::~DepthOfField() {}
