//============================================================================
// Name        : LOD_example.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : LOD Example
//============================================================================

#include "LOD_example.h"

using namespace p3d;

LOD_example::LOD_example() : ClassName(1024,768,"Pyros3D - LOD Example",WindowType::Close | WindowType::Resize)
{
    
}

void LOD_example::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,1000.f);

    SetMousePosition((int)Width/2,(int)Height/2);
    mouseCenter = GetMousePosition();
    mouseLastPosition = mouseCenter;
}

void LOD_example::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(Width,Height);

        // Enable Frustum Culling
        Renderer->ActivateCulling(CullingMode::FrustumCulling);

        // Enable Lodding on Renderer
        Renderer->EnableLOD();

        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,1000.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,10,80));
        
        // Light
        Light = new GameObject();
        dLight = new DirectionalLight(Vec4(1,1,1,1), Vec3(1,1,1));
        Light->Add(dLight);
        Scene->Add(Light);
        Light->SetPosition(Vec3(1,1,1));

        SetMousePosition((uint32)Width/2,(uint32)Height/2);
        mouseCenter = Vec2((uint32)Width/2,(uint32)Height/2);
        mouseLastPosition = mouseCenter;
        counterX = counterY = 0;

        // Input
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &LOD_example::MoveFrontPress);
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &LOD_example::MoveBackPress);
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &LOD_example::StrafeLeftPress);
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &LOD_example::StrafeRightPress);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &LOD_example::MoveFrontRelease);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &LOD_example::MoveBackRelease);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &LOD_example::StrafeLeftRelease);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &LOD_example::StrafeRightRelease);
        InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &LOD_example::LookTo);

        _strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
        HideMouse();

        // Load Teapot LODS
        teapotLOD1Handle = new Model("../../../../examples/LOD_example/assets/teapots/teapotLOD1.p3dm",false,ShaderUsage::Diffuse);
        teapotLOD2Handle = new Model("../../../../examples/LOD_example/assets/teapots/teapotLOD2.p3dm",false,ShaderUsage::Diffuse);
        teapotLOD3Handle = new Model("../../../../examples/LOD_example/assets/teapots/teapotLOD3.p3dm",false,ShaderUsage::Diffuse);

        // Create Teapots and Add LODS
        for (int i=0;i<TEAPOTS;i++)
        {
            RenderingComponent* rTeapot = new RenderingComponent(teapotLOD1Handle);
            rTeapot->AddLOD(teapotLOD2Handle,50);
            rTeapot->AddLOD(teapotLOD3Handle,100);

            GameObject* Teapot = new GameObject();
            Teapot->SetPosition(Vec3((rand() % 1000) -500,(rand() % 1000)-500,(rand() % 1000) -500));
            Teapot->SetScale(Vec3(.1f,.1f,.1f));
            Teapot->Add(rTeapot);

            Scene->Add(Teapot);

            Teapots.push_back(Teapot);
            rTeapots.push_back(rTeapot);
        }

        // Add Camera to Scene
        Scene->Add(Camera);
}

void LOD_example::Update()
{
    // Update - Game Loop

        // Update Scene
        Scene->Update(GetTime());

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);

        Vec3 finalPosition;
        Vec3 direction = Camera->GetDirection();
        float dt = GetTimeInterval();
        float speed = dt * 20.f;
        if (_moveFront)
        {
            finalPosition -= direction*speed;
        }
        if (_moveBack)
        {
            finalPosition += direction*speed;
        }
        if (_strafeLeft)
        {
            finalPosition += direction.cross(Vec3(0,1,0))*speed;
        }
        if (_strafeRight)
        {
            finalPosition -= direction.cross(Vec3(0,1,0))*speed;
        }

        Camera->SetPosition(Camera->GetPosition()+finalPosition);

}

void LOD_example::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove all Objects
        for (int i=0;i<TEAPOTS;i++)
        {
            // Remove From Scene
            Scene->Remove(Teapots[i]);
            // Remove Rendering Component From GameObject
            Teapots[i]->Remove(rTeapots[i]);

            // Delete Both
            delete Teapots[i];
            delete rTeapots[i];
        }

        // Delete Models
        delete teapotLOD1Handle;
        delete teapotLOD2Handle;
        delete teapotLOD3Handle;


        // Remove GameObjects From Scene
        Scene->Remove(Camera);
    
        // Delete
        delete Camera;
        delete Renderer;
        delete Scene;
}

LOD_example::~LOD_example() {}

void LOD_example::MoveFrontPress(Event::Input::Info e)
{
    _moveFront = true;
}
void LOD_example::MoveBackPress(Event::Input::Info e)
{
    _moveBack = true;
}
void LOD_example::StrafeLeftPress(Event::Input::Info e)
{
    _strafeLeft = true;
}
void LOD_example::StrafeRightPress(Event::Input::Info e)
{
    _strafeRight = true;
}
void LOD_example::MoveFrontRelease(Event::Input::Info e)
{
    _moveFront = false;
}
void LOD_example::MoveBackRelease(Event::Input::Info e)
{
    _moveBack = false;
}
void LOD_example::StrafeLeftRelease(Event::Input::Info e)
{
    _strafeLeft = false;
}
void LOD_example::StrafeRightRelease(Event::Input::Info e)
{
    _strafeRight = false;
}
void LOD_example::LookTo(Event::Input::Info e)
{
    if (mouseCenter!=GetMousePosition())
    {
        mousePosition = InputManager::GetMousePosition();
        Vec2 mouseDelta = (mousePosition-mouseLastPosition);
        if (mouseDelta.x != 0 || mouseDelta.y != 0)
        {
            counterX -= mouseDelta.x/10.f;                
            counterY -= mouseDelta.y/10.f;
            if (counterY<-90.f) counterY = -90.f;
            if (counterY>90.f) counterY = 90.f;
            Quaternion qX, qY;
            qX.AxisToQuaternion(Vec3(1.f,0.f,0.f),DEGTORAD(counterY));
            qY.AxisToQuaternion(Vec3(0.f,1.f,0.f),DEGTORAD(counterX));
//                Matrix rotX, rotY;
//                rotX.RotationX(DEGTORAD(counterY));
//                rotY.RotationY(DEGTORAD(counterX));
            Camera->SetRotation((qY*qX).GetEulerFromQuaternion());
            SetMousePosition((int)(mouseCenter.x),(int)(mouseCenter.y));
            mouseLastPosition = mouseCenter;
        }
    }
}