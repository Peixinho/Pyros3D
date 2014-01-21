//============================================================================
// Name        : SkeletonAnimation.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Skeleton Animation Example
//============================================================================

#include "SkeletonAnimation.h"

using namespace p3d;

SkeletonAnimation::SkeletonAnimation() : SFMLContext(1024,768,"Pyros3D - Skeleton Animation Example",WindowType::Close | WindowType::Resize)
{
    
}

void SkeletonAnimation::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    SFMLContext::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,1000.f);
}

void SkeletonAnimation::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(Width,Height);

        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,1000.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,0,10));

        // Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1), Vec3(1,1,1));
        Light->Add(dLight);
        Scene->Add(Light);
        Light->SetPosition(Vec3(1,1,1));

        // Create Game Object
        Model = new GameObject();
        rModel = new RenderingComponent(AssetManager::LoadModel("../../../../examples/SkeletonAnimation/charRigged.dae",false,ShaderUsage::Skinning | ShaderUsage::Diffuse));
        Model->Add(rModel);

        Animation = new AnimationManager();
        Animation->LoadAnimation("../../../../examples/SkeletonAnimation/charRigged.dae", rModel);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(Model);

}

void SkeletonAnimation::Update()
{
    // Update - Game Loop
        
        currentTime += 0.0005f;

		if (currentTime>5.f)
            currentTime = 0.f;

        // Updates Animation
        Animation->Update(currentTime);

        // Update Scene
        Scene->Update(GetTime());

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void SkeletonAnimation::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Model);
        Scene->Remove(Camera);
        
        Model->Remove(rModel);
    
        // Delete
        AssetManager::DestroyAssets();
        delete rModel;
        delete Model;
        delete Animation;
        delete Camera;
        delete Renderer;
        delete Scene;
}

SkeletonAnimation::~SkeletonAnimation() {}
