//============================================================================
// Name        : RotatingTexturedCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingTexturedCube.h"

using namespace p3d;

RotatingTexturedCube::RotatingTexturedCube() : ClassName(1024,768,"Pyros3D - Rotating Textured Cube",WindowType::Close | WindowType::Resize)
{
    
}

void RotatingTexturedCube::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,100.f);
}

void RotatingTexturedCube::Init()
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
        material->SetColorMap(AssetManager::LoadTexture("../../../../examples/RotatingTexturedCube/Texture.png", TextureType::Texture));

        // Create Game Object
        Cube = new GameObject();
        rCube = new RenderingComponent(AssetManager::CreateCube(30,30,30), material);
        Cube->Add(rCube);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        // Add GameObject to Scene
        Scene->Add(Cube);
        Camera->LookAt(Vec3::ZERO);

}

void RotatingTexturedCube::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        Cube->SetRotation(Vec3(0,GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void RotatingTexturedCube::Shutdown()
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

RotatingTexturedCube::~RotatingTexturedCube() {}
