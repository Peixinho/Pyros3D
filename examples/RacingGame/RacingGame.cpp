//============================================================================
// Name        : RacingGame.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#include "RacingGame.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/AssetManager/Assets/Renderable/Primitives/Primitive.h"
#include "Pyros3D/Physics/Components/IPhysicsComponent.h"
#include "Pyros3D/AssetManager/Assets/Font/Font.h"
#include "Pyros3D/AssetManager/Assets/Renderable/Text/Text.h"
#include "Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h"
#include "Pyros3D/Utils/Mouse3D/PainterPick.h"
#include "Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h"

using namespace p3d;

RacingGame::RacingGame() : ClassName(1024,768,"CODENAME: Pyros3D - FIRST WINDOW",WindowType::Close | WindowType::Resize)
{
    
}

void RacingGame::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,2100.f);
    projection2.Ortho(0,Width,0,Height,-1,100);
    
    SetMousePosition((int)Width/2,(int)Height/2);
    mouseCenter = GetMousePosition();
    mouseLastPosition = mouseCenter;
}

void RacingGame::Init()
{
    // Initialization
	
	// Initialize Scene
        Scene = new SceneGraph();
        Scene2 = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(1024,768);

        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,2100.f);
        projection2.Ortho(0,Width,0,Height,-1,100);
        
        // Physics
        physics = new Physics();
        physics->InitPhysics();
        physics->EnableDebugDraw();
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,10.0,20));
        
        Camera2 = new GameObject();
        
        Scene2->Add(Camera2);
        
        // Add Camera to Scene
        Scene->Add(Camera);
        
        // Create Track GameObject
        Track = new GameObject();
        
        test = new GenericShaderMaterial(ShaderUsage::Color);
        test->SetColor(Vec4(1,1,0,1));

        // Create Track Model
        rTrack = new RenderingComponent(AssetManager::LoadModel("../../../../examples/RacingGame/assets/track.p3dm",false,ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow));
        Track->Add(rTrack);
        
        Track->Add(new PhysicsTriangleMesh(physics,rTrack,0));
        
        Scene->Add(Track);

        // Light
        Light = new GameObject();
        // Light Component
        dLight = new DirectionalLight(Vec4(1,1,1,1),Vec3(1,1,1));
        dLight->EnableCastShadows(2048,2048,projection,0.1,200.0,2);
		dLight->SetShadowBias(3.1f,9.0f);
        Light->Add(dLight);
        Scene->Add(Light);

        SetMousePosition((uint32)Width/2,(uint32)Height/2);
        mouseCenter = Vec2((uint32)Width/2,(uint32)Height/2);
        mouseLastPosition = mouseCenter;
        
        // Input
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &RacingGame::MoveFrontPress);
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &RacingGame::MoveBackPress);
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &RacingGame::StrafeLeftPress);
        InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &RacingGame::StrafeRightPress);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &RacingGame::MoveFrontRelease);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &RacingGame::MoveBackRelease);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &RacingGame::StrafeLeftRelease);
        InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &RacingGame::StrafeRightRelease);
        InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &RacingGame::LookTo);

        _strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
        HideMouse();
        
        // Create Font
        Font* font = AssetManager::CreateFont("../../../../examples/RacingGame/assets/verdana.ttf",32);
        font->CreateText("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ,.0123456789[]()!?+-_\\|/ºª");
    
        // Create Text Material
        textMaterial = new GenericShaderMaterial(ShaderUsage::TextRendering);
        // Set Material Font to use Font Map
        textMaterial->SetTextFont(font);
        textMaterial->SetTransparencyFlag(true);
        
        // Create RacingGame Object
        TextRendering = new GameObject();
        textID = (Text*)AssetManager::CreateText(font,"Hello World",12,12,Vec4(1,1,1,1),true);
        rText = new RenderingComponent(textID,textMaterial);
        TextRendering->Add(rText);
        
        // Add GameObject to Scene
        Scene2->Add(TextRendering);

        dRenderer = new CubemapRenderer(256,256);
        
        Texture* skyboxTexture = AssetManager::LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/negx.jpg",TextureType::CubemapNegative_X);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/negy.jpg",TextureType::CubemapNegative_Y);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/negz.jpg",TextureType::CubemapNegative_Z);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/posx.jpg",TextureType::CubemapPositive_X);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/posy.jpg",TextureType::CubemapPositive_Y);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/posz.jpg",TextureType::CubemapPositive_Z);
        skyboxTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        SkyboxMaterial = new GenericShaderMaterial(ShaderUsage::Skybox);
        SkyboxMaterial->SetSkyboxMap(skyboxTexture);
        SkyboxMaterial->SetCullFace(CullFace::FrontFace);
        Skybox = new GameObject();
        rSkybox = new RenderingComponent(AssetManager::CreateCube(1000,1000,1000),SkyboxMaterial);
        rSkybox->DisableCastShadows();
        Skybox->Add(rSkybox);
        Scene->Add(Skybox);

        Renderable* carHandle2 = AssetManager::LoadModel("../../../../examples/RacingGame/assets/lambo.p3dm",true, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
        for (uint32 i=0;i<1;i++)
        {
            Car2 = new GameObject();
            Car2->Add(new RenderingComponent(carHandle2));
            Scene->Add(Car2);
    //        Car2->SetPosition(Vec3((rand() % 1000) -500,(rand() % 100),(rand() % 1000) -500));
            Car2->SetScale(Vec3(0.5,0.5,0.5));
            Car2->SetPosition(Vec3(-13,1,0));
        }
        
        for (uint32 i=0;i<1;i++)
        {
            Car = new GameObject();
            rCar = new RenderingComponent(AssetManager::LoadModel("../../../../examples/RacingGame/assets/del.p3dm",true, ShaderUsage::EnvMap | ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse));
            Car->Add(rCar);
            Scene->Add(Car);
            Car->SetPosition(Vec3((rand() % 1000) -500,(rand() % 100),(rand() % 1000) -500));
        }
        Car->SetPosition(Vec3(-23,1,0));

        for (std::vector<RenderingMesh*>::iterator i=rCar->GetMeshes().begin();i!=rCar->GetMeshes().end();i++)
        {
            GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> ((*i)->Material);
            m->SetEnvMap(dRenderer->GetTexture());
            m->SetReflectivity(0.3);
        }
}

void RacingGame::Update()
{
    // Update - RacingGame Loop
    
    // Update Physics
    physics->Update(GetTime(),10);
    
    // Update Scene
    Scene->Update(GetTime());
    Scene2->Update(GetTime());
    
    Skybox->SetPosition(Vec3(Camera->GetWorldPosition().x,0,Camera->GetWorldPosition().z));

    rCar->Disable();
    dRenderer->RenderCubeMap(Scene,Car,0.1,500);
    rCar->Enable();
    
    // Render Scene
    Renderer->RenderScene(projection,Camera,Scene);
    Renderer->RenderScene(projection2,Camera2,Scene2,false);
//    physics->RenderDebugDraw(projection,Camera);
    
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
    
    
	TextRendering->SetPosition(Vec3(5,Height-15.f,0.f));
    std::ostringstream x; x << fps.getFPS();
    
    textID->UpdateText("Pyros3D - Racing RacingGame - FPS: " + x.str());
//    Car->SetPosition(Vec3(20,10,0));
    Car2->SetRotation(Vec3(0,GetTime(),0));
    
}

void RacingGame::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
//        Scene->Remove(Cube);
//        Scene->Remove(Camera);
//        
//        Cube->Remove(rCube);
//    
//        // Delete
//        delete rCube;
//        delete Cube;
    Scene->Remove(Track);
    Scene->Remove(Car);
    Track->Remove(rTrack);
    Car->Remove(rCar);
    delete rCar;
    delete Car;
    Scene2->Remove(TextRendering);
    TextRendering->Remove(rText);
    delete rText;
    delete TextRendering;
    rTrack->Destroy();
    delete rTrack;
    delete Track;
    AssetManager::DestroyAssets();
    delete Camera;
    delete Renderer;
    delete Scene;
}

RacingGame::~RacingGame() {}


void RacingGame::MoveFrontPress(Event::Input::Info e)
{
    _moveFront = true;
}
void RacingGame::MoveBackPress(Event::Input::Info e)
{
    _moveBack = true;
}
void RacingGame::StrafeLeftPress(Event::Input::Info e)
{
    _strafeLeft = true;
}
void RacingGame::StrafeRightPress(Event::Input::Info e)
{
    _strafeRight = true;
}
void RacingGame::MoveFrontRelease(Event::Input::Info e)
{
    _moveFront = false;
}
void RacingGame::MoveBackRelease(Event::Input::Info e)
{
    _moveBack = false;
}
void RacingGame::StrafeLeftRelease(Event::Input::Info e)
{
    _strafeLeft = false;
}
void RacingGame::StrafeRightRelease(Event::Input::Info e)
{
    _strafeRight = false;
}
void RacingGame::LookTo(Event::Input::Info e)
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
