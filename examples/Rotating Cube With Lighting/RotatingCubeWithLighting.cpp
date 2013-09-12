//============================================================================
// Name        : RotatingCubeWithLighting.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCubeWithLighting.h"

using namespace p3d;

RotatingCubeWithLighting::RotatingCubeWithLighting() : SFMLContext(1024,768,"Pyros3D - Rotating Cube With Lighting",WindowType::Close)
{
    
}

void RotatingCubeWithLighting::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    SFMLContext::OnResize(width, height);
    
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
        dLight = new DirectionalLight(Vec4(1,1,1,1));
        Light->Add(dLight);
        // Set Light Position (Direction is Position Normalized)
        Light->SetPosition(Vec3(100,100,0));
        
        Scene->Add(Light);
        
        // Create Game Object
        Cube = new GameObject();
        cubeID = AssetManager::CreateCube(30,30,30);
        rCube = new RenderingComponent(cubeID, Diffuse);
        Cube->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(Cube);
        Camera->LookAt(Vec3::ZERO);

}

void RotatingCubeWithLighting::Update()
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
        rview.setTitle("Pyros3D - Rotating Cube With Lighting - FPS: " + x.str());
}

void RotatingCubeWithLighting::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Cube);
        Scene->Remove(Camera);
        Scene->Remove(Light);
        
        Cube->Remove(rCube);
        Light->Remove(dLight);
        
        // Delete
        delete rCube;
        delete Cube;
        delete dLight;
        delete Light;
        delete Diffuse;
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingCubeWithLighting::~RotatingCubeWithLighting() {}
