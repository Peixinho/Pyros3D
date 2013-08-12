//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"

using namespace p3d;

RotatingCube::RotatingCube() : SFMLContext(1024,768,"Pyros3D - Rotating Cube",WindowType::Close)
{
    
}

void RotatingCube::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(1024,768);

        // Projection
        projection.Perspective(70.f,1024.f/768.f,1.f,100.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,10,80));
        
        // Create Game Object
        Cube = new GameObject();
        cubeID = AssetManager::CreateCube(30,30,30);
        rCube = new RenderingComponent(cubeID);
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
        DisplayInfo("CODENAME: Pyros3D \nFPS: " + x.str(),Vec2(0.4f,0.4f),Colors::White);
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
        delete Camera;
        delete Renderer;
        delete Scene;
}

RotatingCube::~RotatingCube() {}
