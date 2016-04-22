//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"

using namespace p3d;

RotatingCube::RotatingCube() : ClassName(1024,768,"Pyros3D - Custom Material",WindowType::Close | WindowType::Resize) {}

void RotatingCube::OnResize(const uint32 width, const uint32 height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
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
        
        // Create Game Object
        CubeObject = new GameObject();
        cubeMesh = new Cube(15,15,15);
        rCube = new RenderingComponent(cubeMesh);
        CubeObject->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(CubeObject);
        Camera->LookAt(Vec3::ZERO);

}

void RotatingCube::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        CubeObject->SetRotation(Vec3(0,GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void RotatingCube::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(CubeObject);
        Scene->Remove(Camera);
        
        CubeObject->Remove(rCube);
    
        // Delete
        delete cubeMesh;
        delete rCube;
        delete CubeObject;
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingCube::~RotatingCube() {}
