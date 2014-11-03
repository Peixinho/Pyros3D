//============================================================================
// Name        : PickingPainterMethod.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PickingExample With Painter Method
//============================================================================

#include "PickingPainterMethod.h"

using namespace p3d;

PickingPainterMethod::PickingPainterMethod() : ClassName(1024,768,"Pyros3D - Custom Material",WindowType::Close | WindowType::Resize)
{
    
}

void PickingPainterMethod::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,500.f);
    
    // Resize Picking
    picking->Resize(Width,Height);
}

void PickingPainterMethod::Init()
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
        Camera->SetPosition(Vec3(0,10.0,200));
        
        // Add Camera to Scene
        Scene->Add(Camera);
        
        // Add a Directional Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1), Vec3(1,1,0));
        Light->Add(dLight);
        
        // Add Light to Scene
        Scene->Add(Light);
        
        // Create Materials
        UnselectedMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
        UnselectedMaterial->SetColor(Vec4(0.8,0.8,0.8,1.0));
        
        SelectedMaterial = new GenericShaderMaterial(ShaderUsage::Color);
        SelectedMaterial->SetColor(Vec4(1,1,0,1));
        
        // Set Selected Mesh PTR NULL
        SelectedMesh = NULL;
        
        // Create Cubes
        srand( time( NULL ) );
        
        // Create Geometry
        cubeHandle = new Cube(10,10,10);
        
        // Create 100 Cubes
        for (uint32 i=0;i<100;i++)
        {
            // Create GameObject
            GameObject* Cube = new GameObject();
            // Add Cube to GameObjects List
            Cubes.push_back(Cube);
            // Create Rendering Component using Geometry Previously Created with AssetManager
            RenderingComponent* rCube = new RenderingComponent(cubeHandle, UnselectedMaterial);
            // Add Rendering Component to Rendering Components List
            rCubes.push_back(rCube);
            // Add Rendering Component to GameObject
            Cube->Add(rCube);
            // Add GameObject to Scene
            Scene->Add(Cube);
            // Set Random Position to the GameObject
            Cube->SetPosition(Vec3((rand() % 100) -50,(rand() % 100)-50,(rand() % 100) -50));
        }

        // Create On Mouse Press Event
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &PickingPainterMethod::OnMouseRelease);

        // Painter Method Initialization
        picking = new PainterPick(Width,Height);
        picking->SetViewPort(0,0,Width,Height);
}

void PickingPainterMethod::Update()
{
    // Update - Game Loop

    // Update Scene
    Scene->Update(GetTime());

    // Render Scene
    Renderer->RenderScene(projection,Camera,Scene);    
}

void PickingPainterMethod::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Camera);
    
        Scene->Remove(Light);
        
        // GameObjects and Components
        for (uint32 i=0;i<100;i++)
        {
            // Remove From Scene
            Scene->Remove(Cubes[i]);
            // Remove From GameObject Owner
            Cubes[i]->Remove(rCubes[i]);
            // Delete Rendering Component
            delete rCubes[i];
            // Delete GameObject
            delete Cubes[i];
        }
        
        // Remove Directional Light Component
        Light->Remove(dLight);
        
        // Delete All Components and GameObjects
        delete cubeHandle;
        delete dLight;
        delete Light;
        delete picking;
        delete SelectedMaterial;
        delete UnselectedMaterial;
        delete Camera;
        delete Renderer;
        delete Scene;
}

PickingPainterMethod::~PickingPainterMethod() {}

void PickingPainterMethod::OnMouseRelease(Event::Input::Info e)
{
    // Get Picked Mesh
    RenderingMesh* mesh = picking->PickObject(GetMousePosition().x,GetMousePosition().y,projection,Camera,Scene);
    
    // If Mesh Exists
    if (mesh!=NULL) 
    {
        // If there is an Old Mesh Selected
        if (SelectedMesh!=NULL)
            SelectedMesh->Material=(IMaterial*)UnselectedMaterial;
        
        // Set Mesh Selected Color
        mesh->Material=(IMaterial*)SelectedMaterial;
        
        // Save Mesh Selected PTR
        SelectedMesh = mesh;
    }
}