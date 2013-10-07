//============================================================================
// Name        : RotatingCubeWithLightingAndShadow.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCubeWithLightingAndShadow.h"

using namespace p3d;

RotatingCubeWithLightingAndShadow::RotatingCubeWithLightingAndShadow() : SFMLContext(1024,768,"Pyros3D - Rotating Cube With Lighting And Shadow",WindowType::Close | WindowType::Resize)
{
    
}

void RotatingCubeWithLightingAndShadow::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    SFMLContext::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,200.f);
}

void RotatingCubeWithLightingAndShadow::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(Width,Height);

        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,200.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,10,80));
        
        // Material
        Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
        Diffuse->SetColor(Vec4(1,0,0,1));
        Diffuse->SetPCFTexelSize(0.0001f);
        
        // Add a Directional Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1));
        // Enable Shadow Casting
        dLight->EnableCastShadows(512,512,projection,0.1,200.0,1);
        dLight->SetShadowBias(1.f,3.f);
        Light->Add(dLight);
        // Set Light Position (Direction is Position Normalized)
        Light->SetPosition(Vec3(100,100,100));
        
        // Add Light to Scene
        Scene->Add(Light);
        
        // Create Floor Material
        FloorMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
        FloorMaterial->SetColor(Vec4(1,1,1,1));
        
        // Create Floor
        Floor = new GameObject();
        // Create Floor Rendering Component
        rFloor = new RenderingComponent(AssetManager::CreatePlane(100,150),FloorMaterial);
        
        // Add Rendering Component To GameObject
        Floor->Add(rFloor);
        
        // Add Floor to Scene
        Scene->Add(Floor);
        
        // Rotate Floor
        Floor->SetRotation(Vec3(-1.57,0,0));
        // Translate Floor
        Floor->SetPosition(Vec3(0,-20,-20));
        
        // Create Game Object
        Cube = new GameObject();
        // Create Cube Rendering Component
        rCube = new RenderingComponent(AssetManager::CreateCube(30,30,30), Diffuse);
        Cube->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(Cube);
        Camera->LookAt(Vec3::ZERO);

}

void RotatingCubeWithLightingAndShadow::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        Cube->SetRotation(Vec3(0,GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);

        // Info on Window
        // Should come in the End
        // std::ostringstream x; x << fps.getFPS();
}

void RotatingCubeWithLightingAndShadow::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Cube);
        Scene->Remove(Camera);
        Scene->Remove(Light);
        Scene->Remove(Floor);
        
        Cube->Remove(rCube);
        Light->Remove(dLight);
        Floor->Remove(rFloor);
        
        // Delete
        delete rCube;
        delete Cube;
        delete rFloor;
        delete Floor;
        delete dLight;
        delete Light;
        delete Diffuse;
        delete FloorMaterial;
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingCubeWithLightingAndShadow::~RotatingCubeWithLightingAndShadow() {}
