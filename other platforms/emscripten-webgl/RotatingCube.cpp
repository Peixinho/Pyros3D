//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"

using namespace p3d;

RotatingCube::RotatingCube() : ClassName(1024,768,"Pyros3D - Custom Material",WindowType::Close | WindowType::Resize)
{
    
}

void RotatingCube::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,100.f);
}

void RotatingCube::Init()
{
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
        dLight = new DirectionalLight(Vec4(1,1,1,1),Vec3(1,1,0));
        Light->Add(dLight);
        
        Scene->Add(Light);
        
        // Material
        material = new GenericShaderMaterial(ShaderUsage::Texture);
        material->SetColorMap(AssetManager::LoadTexture("assets/Texture.png", TextureType::Texture));

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

void RotatingCube::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        Cube->SetRotation(Vec3(0,GetTime(),0));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void RotatingCube::Shutdown()
{

}
RotatingCube::~RotatingCube() {}
