//============================================================================
// Name        : SimplePhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Simple Physics Example
//============================================================================

#include "SimplePhysics.h"

using namespace p3d;

SimplePhysics::SimplePhysics() : ClassName(1024,768,"CODENAME: Pyros3D - Simple Physics",WindowType::Close | WindowType::Resize)
{
    
}

void SimplePhysics::OnResize(const uint32 width, const uint32 height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,500.f);
}

void SimplePhysics::Init()
{
    // Initialization
    
    // Initialize Scene
    Scene = new SceneGraph();

    // Initialize Renderer
    Renderer = new ForwardRenderer(Width,Height);

    // Projection
    projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,500.f);

    // Create Camera
    Camera = new GameObject();
    Camera->SetPosition(Vec3(0,20.0,300));

    // Add Camera to Scene
    Scene->Add(Camera);

    // Physics
    physics = new Physics();
    physics->InitPhysics();

    // Add a Directional Light
    Light = new GameObject();
    dLight = new DirectionalLight(Vec4(1,1,1,1),Vec3(-1,-1,0));
    dLight->EnableCastShadows(1024,1024,projection,1,500,1);
    dLight->SetShadowBias(1.f,3.f);
    Light->Add(dLight);

    // Add Light to Scene
    Scene->Add(Light);

    // Create Materials
    Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
    Diffuse->SetColor(Vec4(0.8,0.8,0.8,1.0));

    // Set Selected Mesh PTR NULL
    SelectedMesh = NULL;

    // Create Cubes
    srand( time( NULL ) );

    // Create Geometry
    cubeHandle = new Cube(5,5,5);

    // Create 100 Cubes
#if !defined(GLES2)
    for (uint32 i=0;i<1000;i++)
#else
    for (uint32 i=0;i<100;i++)
#endif
    {
        // Create GameObject
        GameObject* Cube = new GameObject();
        // Add Cube to GameObjects List
        Cubes.push_back(Cube);
        // Create Rendering Component using Geometry Previously Created with AssetManager
        RenderingComponent* rCube = new RenderingComponent(cubeHandle, Diffuse);
        // Add Rendering Component to Rendering Components List
        rCubes.push_back(rCube);
        // Add Rendering Component to GameObject
        Cube->Add(rCube);
        // Create Physics Component
        IPhysicsComponent* pCube = (IPhysicsComponent*)physics->CreateBox(5,5,5,10);
        // Add Physics Component to Physics List
        pCubes.push_back(pCube);
        // Add Physics Component to GameObject
        Cube->Add(pCube);
        // Add GameObject to Scene
        Scene->Add(Cube);
        // Set Random Position to the GameObject
        Cube->SetPosition(Vec3((rand() % 100) -50,(rand() % 100)-50,(rand() % 100) -50));
    }

    // Create Floor
    Floor = new GameObject();
    // Create Geometry
    floorHandle = new Cube(100,3,100);

    // Create Floor Geometry
    rFloor = new RenderingComponent(floorHandle,Diffuse);

    // Create Floor Physics
    pFloor = (IPhysicsComponent*)physics->CreateBox(100,3,100,0);
    // Add Physics Component to GameObject
    Floor->Add(pFloor);
    
    // Add Rendering Component To GameObject
    Floor->Add(rFloor);

    // Add Floor to Scene
    Scene->Add(Floor);

    // Translate Floor
    Floor->SetPosition(Vec3(0,-100,0));
    
}

void SimplePhysics::Update()
{
    // Update - Game Loop
    
    // Update Physics
    physics->Update(GetTime(),10);
    
    // Update Scene
    Scene->Update(GetTime());

    // Render Scene
    Renderer->RenderScene(projection,Camera,Scene);    
}

void SimplePhysics::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Camera);
    
        Scene->Remove(Light);
        
        Scene->Remove(Floor);
        
        // GameObjects and Components
#if !defined(GLES2)
        for (uint32 i=0;i<100;i++)
#else
        for (uint32 i=0;i<1000;i++)
#endif
        {
            // Remove From Scene
            Scene->Remove(Cubes[i]);
            // Remove From GameObject Owner
            Cubes[i]->Remove(rCubes[i]);
            Cubes[i]->Remove(pCubes[i]);
            // Delete Rendering Component
            delete rCubes[i];
            // Delete Physics Component
            delete pCubes[i];
            // Delete GameObject
            delete Cubes[i];
        }
        
        // Remove Directional Light Component
        Light->Remove(dLight);
        
        // Remove Floor Components
        Floor->Remove(pFloor);
        Floor->Remove(rFloor);
        
        // Delete All Components and GameObjects
        delete dLight;
        delete Light;
        delete rFloor;
        delete pFloor;
        delete Floor;
        delete cubeHandle;
        delete floorHandle;
        delete physics;
        delete Diffuse;
        delete Camera;
        delete Renderer;
        delete Scene;
}

SimplePhysics::~SimplePhysics() {}
