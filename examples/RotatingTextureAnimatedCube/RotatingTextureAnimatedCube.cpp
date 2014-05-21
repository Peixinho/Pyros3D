//============================================================================
// Name        : RotatingTextureAnimatedCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingTextureAnimatedCube.h"

using namespace p3d;

RotatingTextureAnimatedCube::RotatingTextureAnimatedCube() : ClassName(1024,768,"Pyros3D - Rotating Textured Cube",WindowType::Close | WindowType::Resize)
{
    
}

void RotatingTextureAnimatedCube::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,100.f);
}

void RotatingTextureAnimatedCube::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(Width,Height);

        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,100.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,10,80));
        
        // Material
        material = new GenericShaderMaterial(ShaderUsage::Texture);
        
    	Texture* tex0 = AssetManager::LoadTexture("../../../../examples/RotatingTextureAnimatedCube/assets/1.png", TextureType::Texture);
    	Texture* tex1 = AssetManager::LoadTexture("../../../../examples/RotatingTextureAnimatedCube/assets/2.png", TextureType::Texture);
    	Texture* tex2 = AssetManager::LoadTexture("../../../../examples/RotatingTextureAnimatedCube/assets/3.png", TextureType::Texture);
    	Texture* tex3 = AssetManager::LoadTexture("../../../../examples/RotatingTextureAnimatedCube/assets/4.png", TextureType::Texture);
    	Texture* tex4 = AssetManager::LoadTexture("../../../../examples/RotatingTextureAnimatedCube/assets/5.png", TextureType::Texture);
    	Texture* tex5 = AssetManager::LoadTexture("../../../../examples/RotatingTextureAnimatedCube/assets/6.png", TextureType::Texture);
    	anim = new TextureAnimation(30);
    	anim->AddFrame(tex0);
    	anim->AddFrame(tex1);
    	anim->AddFrame(tex2);
    	anim->AddFrame(tex3);
    	anim->AddFrame(tex4);
    	anim->AddFrame(tex5);
        // Create Animation Instance with 30fps
        animInst = anim->CreateInstance(30);
        // Set Animation Loop backwards
        animInst->YoYo(true);
        // Play It
        animInst->Play(0);
	
        // Create Game Object
        Cube = new GameObject();
        rCube = new RenderingComponent(AssetManager::CreateCube(30,30,30), material);
        Cube->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(Cube);
        Camera->LookAt(Vec3::ZERO);
	   
        anim->Play(0); // Loop

        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &RotatingTextureAnimatedCube::OnMousePress);
}

void RotatingTextureAnimatedCube::OnMousePress(Event::Input::Info e)
{
	animInst->Pause();
}

void RotatingTextureAnimatedCube::Update()
{
    // Update - Game Loop
	
        anim->Update(GetTime());

        // Set Texture from Animation Instance
        material->SetColorMap(animInst->GetTexture());
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        Cube->SetRotation(Vec3(0,GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void RotatingTextureAnimatedCube::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Cube);
        Scene->Remove(Camera);
        
        Cube->Remove(rCube);
    
        // Delete
        AssetManager::DestroyAssets();
        delete material;
        delete rCube;
        delete Cube;
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingTextureAnimatedCube::~RotatingTextureAnimatedCube() {}
