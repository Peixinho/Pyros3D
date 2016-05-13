//============================================================================
// Name        : DepthOfField.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "DepthOfField.h"

using namespace p3d;

DepthOfFieldEffect::DepthOfFieldEffect(Texture* texture1, Texture* texture2)
{
	// Set RTT
	UseCustomTexture(texture1);
	UseCustomTexture(texture2);

	// Create Fragment Shader
	FragmentShaderString =
		"uniform sampler2D uTex0;"
		"uniform sampler2D uTex1;"
		"varying vec2 vTexcoord;"
		"void main() {"
			"if (vTexcoord.x<0.5) gl_FragColor = texture2D(uTex0,vTexcoord);\n"
			"else gl_FragColor = texture2D(uTex1,vTexcoord);"
		"}";

	CompileShaders();
}

DepthOfField::DepthOfField() : ClassName(1024,768,"Pyros3D - Depth Of Field",WindowType::Close | WindowType::Resize) {}

void DepthOfField::OnResize(const uint32 width, const uint32 height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,1000.f);
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
		modelMesh = new Model("assets/suzanne.p3dm", false, ShaderUsage::Diffuse);
        
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
		EffectManager->AddEffect(new BlurXEffect(RTT::Color, Width));
		EffectManager->AddEffect(new BlurYEffect(RTT::LastRTT, Height), fullResBlur);
		EffectManager->AddEffect(new ResizeEffect(RTT::Color, Width*.25f, Height*.25f));
		EffectManager->AddEffect(new BlurXEffect(RTT::LastRTT, Width*.25f));
		EffectManager->AddEffect(new BlurYEffect(RTT::LastRTT, Height*.25f), lowResBlur);
		EffectManager->AddEffect(new DepthOfFieldEffect(lowResBlur, fullResBlur));
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
		delete EffectManager;
		delete lowResBlur;
		delete fullResBlur;
}

DepthOfField::~DepthOfField() {}
