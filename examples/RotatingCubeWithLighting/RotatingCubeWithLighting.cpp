//============================================================================
// Name        : RotatingCubeWithLighting.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCubeWithLighting.h"

using namespace p3d;

RotatingCubeWithLighting::RotatingCubeWithLighting() : ClassName(1024,768,"Pyros3D - Rotating Cube With Lighting",WindowType::Close | WindowType::Resize)
{
    
}

void RotatingCubeWithLighting::OnResize(const uint32 width, const uint32 height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,100.f);
}

void RotatingCubeWithLighting::Init()
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
        Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
        Diffuse->SetColor(Vec4(1,0,0,1));
        
        // Add a Directional Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1),Vec3(-1,-1,0));
        Light->Add(dLight);
        
        Scene->Add(Light);
        
        // Create Game Object
        CubeObject = new GameObject();
        cubeMesh = new Cube(15,15,15);
        rCube = new RenderingComponent(cubeMesh,Diffuse);
        CubeObject->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(CubeObject);
        Camera->LookAt(Vec3::ZERO);
}

void RotatingCubeWithLighting::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        CubeObject->SetRotation(Vec3(0,(f32)GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void RotatingCubeWithLighting::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(CubeObject);
        Scene->Remove(Camera);
        Scene->Remove(Light);
        
        CubeObject->Remove(rCube);
        Light->Remove(dLight);
        
        // Delete
        delete rCube;
        delete CubeObject;
        delete cubeMesh;
        delete dLight;
        delete Light;
        delete Diffuse;
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingCubeWithLighting::~RotatingCubeWithLighting() {}
