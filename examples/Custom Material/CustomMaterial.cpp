//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "CustomMaterial.h"

using namespace p3d;

RotatingCube::RotatingCube() : SFMLContext(1024,768,"Pyros3D - Custom Material",WindowType::Close | WindowType::Resize)
{
    
}

void RotatingCube::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    SFMLContext::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,100.f);
}

void RotatingCube::Init()
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
        
        // Custom Material
        Material = new CustomMaterial();
        
        // Create Game Object
        Cube = new GameObject();
        cubeID = AssetManager::CreateCube(30,30,30);
        rCube = new RenderingComponent(cubeID,Material);
        Cube->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(Cube);
        Camera->LookAt(Vec3::ZERO);

}

void RotatingCube::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update();
        
        // Game Logic Here
        Cube->SetRotation(Vec3(0,GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);

        // Info on Window
        // Should come in the End
        std::ostringstream x; x << fps.getFPS();
        rview.setTitle("Pyros3D - Custom Material - FPS: " + x.str());
}

void RotatingCube::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Cube);
        Scene->Remove(Camera);
        
        Cube->Remove(rCube);
    
        // Delete
        delete rCube;
        delete Cube;
        delete Material;
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingCube::~RotatingCube() {}
