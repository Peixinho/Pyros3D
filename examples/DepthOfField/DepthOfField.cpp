//============================================================================
// Name        : DepthOfField.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
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
		"float DecodeNativeDepth(float native_z, vec4 z_info_local)\n"
		"{\n"
			"return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);\n"
		"}\n"
		"uniform sampler2D uTex0, uTex1, uTex2, uTex3;\n"
		"uniform float uFocalPosition, uFocalRange, uRatioL, uRatioH;\n"
		"uniform vec2 uNearFar;\n"
		"varying vec2 vTexcoord;\n"
		"void main() {\n"
			"float ratioL = uRatioL;\n"
			"float ratioH = uRatioH;\n"
			"float focalPosition = uFocalPosition;\n"
			"float focalRange = uFocalRange;\n"
			"vec4 z_info_local = vec4(uNearFar.x,uNearFar.y,uNearFar.x*uNearFar.y,uNearFar.x-uNearFar.y);\n"
			"float depth = texture2D(uTex3, vTexcoord).x;\n"
			"float linearDepth = DecodeNativeDepth(depth, z_info_local);\n"
			"float ratio = clamp(abs(focalPosition-linearDepth)-focalRange, 0.0, ratioL);\n"
			"if (ratio < 0.4) gl_FragColor = mix(texture2D(uTex2, vTexcoord), texture2D(uTex1, vTexcoord), ratio / (ratioL - ratioH));\n"
			"else gl_FragColor =  mix(texture2D(uTex1, vTexcoord), texture2D(uTex0, vTexcoord), (ratio-ratioH) / (ratioL - ratioH));\n"
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

	Uniform focalPosition;
	Uniform focalRange;
	Uniform ratioL;
	Uniform ratioH;

	focalPosition.Name = "uFocalPosition";
	focalPosition.Type = DataType::Float;
	focalPosition.Usage = PostEffects::Other;
	focalPosition.SetValue(&fPosition);
	focalRange.Name = "uFocalRange";
	focalRange.Type = DataType::Float;
	focalRange.Usage = PostEffects::Other;
	focalRange.SetValue(&fRange);
	ratioL.Name = "uRatioL";
	ratioL.Type = DataType::Float;
	ratioL.Usage = PostEffects::Other;
	ratioL.SetValue(&rL);
	ratioH.Name = "uRatioH";
	ratioH.Type = DataType::Float;
	ratioH.Usage = PostEffects::Other;
	ratioH.SetValue(&rH);

	AddUniform(focalPosition);
	AddUniform(focalRange);
	AddUniform(ratioL);
	AddUniform(ratioH);
}

DepthOfField::DepthOfField() : ClassName(1024,768,"Pyros3D - Depth Of Field",WindowType::Close | WindowType::Resize) {}

void DepthOfField::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);
	
	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f,(f32)width/(f32)height,1.f,1000.f);

	EffectManager->Resize(width, height);
	blurX->Resize(width, height);
	blurY->Resize(width, height);
	resize->Resize(width*0.25f, height*0.25f);
	blurXlow->Resize(width*0.25f, height*0.25f);
	blurYlow->Resize(width*0.25f, height*0.25f);
	depthOfField->Resize(width, height);
}

void DepthOfField::Init()
{
	// Initialization
	
		// Initialize Scene
		Scene = new SceneGraph();
		
		// Initialize Renderer
		Renderer = new ForwardRenderer(Width,Height);
		Renderer->SetBackground(Vec4(1,0,0,1));
		// Projection
		projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,1000.f);
		
		// Create Camera
		Camera = new GameObject();
		Camera->SetPosition(Vec3(0,2,20));
		
		// Add a Directional Light
		Light = new GameObject();
		dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
		Light->Add(dLight);

		Scene->Add(Light);

		// Create Game Object
		modelMesh = new Model("../../../../examples/DepthOfField/assets/suzanne.p3dm", false, ShaderUsage::Diffuse);
		
		for (uint32 i = 0; i < 10; i++)
		{
			GameObject* Monkey = new GameObject();
			Monkey->SetPosition(Vec3(-23.f + i * 3.f, 0, -15.f + i * 3.f));
			std::cout << Monkey->GetPosition().toString() << std::endl;
			RenderingComponent* rMonkey = new RenderingComponent(modelMesh);
			Monkey->Add(rMonkey);
			Scene->Add(Monkey);

			go.push_back(Monkey);
			rc.push_back(rMonkey);
		}

		// Add Camera to Scene
		Scene->Add(Camera);

		Camera->LookAt(Vec3::ZERO);

		fullResBlur = new Texture();
		fullResBlur->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
		fullResBlur->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		lowResBlur = new Texture();
		lowResBlur->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width*.25f, Height*.25f);
		lowResBlur->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		EffectManager = new PostEffectsManager(Width, Height);

		blurX = new BlurXEffect(RTT::Color, Width, Height);
		blurY = new BlurYEffect(RTT::LastRTT, Width, Height);
		fullResBlur = blurY->GetTexture();
		resize = new ResizeEffect(RTT::Color, Width*.25f, Height*.25f);
		blurXlow = new BlurXEffect(RTT::LastRTT, Width*.25f, Height*.25f);
		blurYlow = new BlurYEffect(RTT::LastRTT, Width*.25f, Height*.25f);
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
			go[i]->SetRotation(Vec3(0, GetTime(), 0));
		}

		// Render Scene
		EffectManager->CaptureFrame();
		Renderer->RenderScene(projection,Camera,Scene);
		EffectManager->EndCapture();

		// Render Post Processing
		EffectManager->ProcessPostEffects(&projection);
		std::cout << fps.getFPS() << std::endl;
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
