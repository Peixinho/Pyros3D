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
    changed = false;
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
        Camera->SetPosition(Vec3(0,10,100));

        // Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1), Vec3(-1,-1,-1));
        Light->Add(dLight);
        Scene->Add(Light);

        // Create Game Object
        ModelObject = new GameObject();
        modelHandle = new Model("../../../../examples/SkeletonAnimationExample/assets/human.p3dm",false,ShaderUsage::Diffuse | ShaderUsage::Skinning);
        ((Model*)modelHandle)->DebugSkeleton();
        rModel = new RenderingComponent(modelHandle);
        ModelObject->Add(rModel);
        ModelObject->AddTag("Teste");
        ModelObject->SetPosition(Vec3(-10,0,0));
        ModelObject->SetScale(Vec3(2,2,2));
 
        SAnim = new SkeletonAnimation();
        SAnim->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/walk.p3da");
        SAnim->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/alert.p3da");
        SAnim->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/run.p3da");
        SAnim->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/walk.p3da");

        ModelObject2 = new GameObject();
        modelHandle2 = new Model("../../../../examples/SkeletonAnimationExample/assets/Model.p3dm",false,ShaderUsage::Diffuse | ShaderUsage::Skinning);
        rModel2 = new RenderingComponent(modelHandle2);
        ModelObject2->Add(rModel2);
        ModelObject2->AddTag("Teste");
        ModelObject2->SetPosition(Vec3(15,12,0));
        ModelObject2->SetScale(Vec3(10,10,10));
 
        SAnim2 = new SkeletonAnimation();
        SAnim2->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/Animation.p3da");
        
        // test->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/walk.p3da");
        // test->LoadAnimation("../../../../examples/SkeletonAnimationExample/assets/walk.p3da");
        anim = SAnim->CreateInstance(rModel);
        anim2 = SAnim2->CreateInstance(rModel2);

        uint32 animationID = 0;
        uint32 animationID2 = 2;

        anim->CreateLayer("Layer1");
        anim->AddBoneAndChilds("Layer1","Bip01_Spine1");

        animationPos = anim->Play(animationID,0.f,-1.f,1.f,0.9f);
        animationPos2 = anim->Play(animationID2,0.f,-1.f,1.f,0.1f);
        anim->Play(1,0,-1,1,0.5,"Layer1");
        anim->Play(3,0,-1,1,0.5,"Layer1");

        anim2->Play(0,0,-1,1,1.0);
        //animationPos2 = 1;
        
        std::cout << "Animations: " << SAnim->GetNumberAnimations() << std::endl;
        std::cout << "Duration: " << SAnim->GetAnimations()[animationPos].Duration << std::endl;
        std::cout << "Ticks: " << SAnim->GetAnimations()[animationPos].TicksPerSecond << std::endl;
        std::cout << "Animation Name: " << SAnim->GetAnimations()[animationPos].AnimationName << std::endl;
        std::cout << "Channels: " << SAnim->GetAnimations()[animationPos].Channels.size() << std::endl;

        std::cout << "Duration: " << SAnim->GetAnimations()[animationPos2].Duration << std::endl;
        std::cout << "Ticks: " << SAnim->GetAnimations()[animationPos2].TicksPerSecond << std::endl;
        std::cout << "Animation Name: " << SAnim->GetAnimations()[animationPos2].AnimationName << std::endl;
        std::cout << "Channels: " << SAnim->GetAnimations()[animationPos2].Channels.size() << std::endl;

        std::cout << "Animation 1 Position on Animator: " << animationPos << std::endl;
        std::cout << "Animation 2 Position on Animator: " << animationPos2 << std::endl;

        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(ModelObject);
        Scene->Add(ModelObject2);

        InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &SkeletonAnimationExample::OnMouseMove);
}

void SkeletonAnimationExample::Update()
{
    // Update - Game Loop
        if (changed)
        {
            anim->ChangeProperties(animationPos,anim->GetAnimationCurrentProgress(animationPos2),-1,speed1,scale1);
            anim->ChangeProperties(animationPos2,anim->GetAnimationCurrentProgress(animationPos2),-1,speed2,scale2);

            changed = false;
        }

        // Updates Animation
        SAnim->Update((f32)GetTime());
        SAnim2->Update((f32)GetTime());

        // Update Scene
        Scene->Update(GetTime());

        // Render Scene
		Renderer->PreRender(Camera, Scene, "Teste");
        Renderer->RenderScene(projection,Camera,Scene);
}

void SkeletonAnimationExample::OnMouseMove(Event::Input::Info e)
{
    if (!changed)
    {
        f32 coord = ((Vec2)e.Value).x;
        f32 w = (f32)Width*0.5f;

        scale1 = (fabs(coord-w*2) / (w*2));
        scale2 = 1.f-scale1;

        f32 Time1 = SAnim->GetAnimations()[animationPos].Duration;
        f32 Time2 = SAnim->GetAnimations()[animationPos2].Duration;

        f32 diffTime1 = (scale1*Time1);
        f32 diffTime2 = (scale2*Time2);
        f32 diffTime = diffTime1 + diffTime2;
        
        speed1 = diffTime / Time2;
        speed2 = diffTime / Time1;
        
        changed = true;
    }
}
void SkeletonAnimationExample::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(ModelObject);
        Scene->Remove(Camera);
        
        ModelObject->Remove(rModel);
        
        SAnim->DestroyInstance(anim);
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
