//============================================================================
// Name        : SkeletonAnimationExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Skeleton Animation Example
//============================================================================

#include "SkeletonAnimationExample.h"

using namespace p3d;

SkeletonAnimationExample::SkeletonAnimationExample() : ClassName(1024,768,"Pyros3D - Skeleton Animation Example",WindowType::Close | WindowType::Resize)
{
    
}

void SkeletonAnimationExample::OnResize(const uint32 width, const uint32 height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,1000.f);
}

void SkeletonAnimationExample::Init()
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
        Camera->SetPosition(Vec3(0,5,50));

        // Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1), Vec3(1,1,1));
        Light->Add(dLight);
        Scene->Add(Light);
        Light->SetPosition(Vec3(1,1,1));

        // Create Game Object
        ModelObject = new GameObject();
        modelHandle = new Model("../../../../examples/SkeletonAnimationExample/assets/Model.p3dm",false,ShaderUsage::Diffuse | ShaderUsage::Skinning);
        rModel = new RenderingComponent(modelHandle);
        ModelObject->Add(rModel);
		ModelObject->AddTag("Teste");
        ModelObject->SetScale(Vec3(10,10,10));
 
        SAnim = new SkeletonAnimation();
        SAnim->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/Animation.p3da");
        // test->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/walk.p3da");
        // test->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/walk.p3da");
        anim = SAnim->CreateInstance(rModel);

        anim->Play(0,0,3,1,1);
        //anim->Play(1,-1,0,1,1);

        std::cout << "Animations: " << SAnim->GetNumberAnimations() << std::endl;
        std::cout << "Duration: " << SAnim->GetAnimations()[0].Duration << std::endl;
        std::cout << "Ticks: " << SAnim->GetAnimations()[0].TicksPerSecond << std::endl;
        std::cout << "Animation Name: " << SAnim->GetAnimations()[0].AnimationName << std::endl;

        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(ModelObject);


        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &SkeletonAnimationExample::OnMousePress);
}

void SkeletonAnimationExample::Update()
{
    // Update - Game Loop

        // Updates Animation
        SAnim->Update(GetTime());

        // Update Scene
        Scene->Update(GetTime());

        // Render Scene
		Renderer->RenderSceneByTag(projection,Camera,Scene,"Teste");
}

void SkeletonAnimationExample::OnMousePress(Event::Input::Info e)
{
    //if (anim->IsPaused(0)) anim->ResumeAnimation(0);
    //else anim->PauseAnimation(0);
    if (anim->GetAnimationSpeed(0)==-1) anim->Play(0,anim->GetAnimationCurrentTime(0),-1,1,1);
    else if (anim->GetAnimationSpeed(0)==1) anim->Play(0,anim->GetAnimationCurrentTime(0),-1,-1,1);
}

void SkeletonAnimationExample::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(ModelObject);
        Scene->Remove(Camera);
        
        ModelObject->Remove(rModel);
    
        delete SAnim;

        // Delete
        delete rModel;
        delete ModelObject;
        delete modelHandle;
        delete Camera;
        delete Renderer;
        delete Scene;
}

SkeletonAnimationExample::~SkeletonAnimationExample() {}
