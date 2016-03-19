//============================================================================
// Name        : RacingGame.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#include "RacingGame.h"

using namespace p3d;

RacingGame::RacingGame() : ClassName(1920,1080,"CODENAME: Pyros3D - FIRST WINDOW",WindowType::Close | WindowType::Resize)
{
    
}

void RacingGame::OnResize(const uint32 width, const uint32 height)
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
		_moveBack = _moveFront = _strafeLeft = _strafeRight = false;
		_leftPressed = _rightPressed = _upPressed = _downPressed = false;
		gVehicleSteering = 0.f;
	// Initialize Scene
        Scene = new SceneGraph();
        Scene2 = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(1920,1080);
		Renderer->SetGlobalLight(Vec4(0.5, 0.5, 0.5, 0.5));
        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,2100.f);
        projection2.Ortho(0,Width,0,Height,-1,100);
        
        // Physics
        physics = new Physics();
        physics->InitPhysics();
        physics->EnableDebugDraw();
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,30.0,100));
        
        Camera2 = new GameObject();
        
        Scene2->Add(Camera2);
        
        // Add Camera to Scene
        
        
        // Create Track GameObject
        Track = new GameObject();

        // Create Track Model
        trackHandle = new Model("../../../../examples/RacingGame/assets/track.p3dm",false,ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
        rTrack = new RenderingComponent(trackHandle);
        Track->Add(rTrack);
        
        Track->Add(new PhysicsTriangleMesh(physics,rTrack,0));
        
        Scene->Add(Track);

        // Light
        Light = new GameObject();
        // Light Component
        dLight = new DirectionalLight(Vec4(1,1,1,1),Vec3(-1,-1,-1));
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

		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Up, this, &RacingGame::UpDown);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Up, this, &RacingGame::UpUp);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Down, this, &RacingGame::DownDown);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Down, this, &RacingGame::DownUp);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Left, this, &RacingGame::LeftDown);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Left, this, &RacingGame::LeftUp);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Right, this, &RacingGame::RightDown);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Right, this, &RacingGame::RightUp);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Space, this, &RacingGame::SpaceDown);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Space, this, &RacingGame::SpaceUp);

		InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button0, this, &RacingGame::UpDown);
		InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button0, this, &RacingGame::UpUp);
		InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button1, this, &RacingGame::DownDown);
		InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button1, this, &RacingGame::DownUp);
		InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button2, this, &RacingGame::SpaceDown);
		InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button2, this, &RacingGame::SpaceUp);
		InputManager::AddJoypadEvent(Event::Type::OnMove, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Axis::X, this, &RacingGame::AnalogicMove);

        _strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
        HideMouse();
        
        // Create Font
        Font* font = new Font("../../../../examples/RacingGame/assets/verdana.ttf",32);
        font->CreateText("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ,.0123456789[]()!?+-_\\|/ºª");
    
        // Create Text Material
        textMaterial = new GenericShaderMaterial(ShaderUsage::TextRendering);
        // Set Material Font to use Font Map
        textMaterial->SetTextFont(font);
        textMaterial->SetTransparencyFlag(true);
        
        // Create RacingGame Object
        TextRendering = new GameObject();
        textID = new Text(font,"Hello World",12,12,Vec4(1,1,1,1),true);
        rText = new RenderingComponent(textID,textMaterial);
        TextRendering->Add(rText);
        
        // Add GameObject to Scene
        Scene2->Add(TextRendering);

        dRenderer = new CubemapRenderer(256,256);
        
        Texture* skyboxTexture = new Texture();
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/negx.png",TextureType::CubemapNegative_X);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/negy.png",TextureType::CubemapNegative_Y);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/negz.png",TextureType::CubemapNegative_Z);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/posx.png",TextureType::CubemapPositive_X);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/posy.png",TextureType::CubemapPositive_Y);
        skyboxTexture->LoadTexture("../../../../examples/RacingGame/assets/Textures/skybox/posz.png",TextureType::CubemapPositive_Z);
        skyboxTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        SkyboxMaterial = new GenericShaderMaterial(ShaderUsage::Skybox);
        SkyboxMaterial->SetSkyboxMap(skyboxTexture);
        SkyboxMaterial->SetCullFace(CullFace::FrontFace);
        Skybox = new GameObject();
        skyboxHandle = new Cube(1000,1000,1000);
        rSkybox = new RenderingComponent(skyboxHandle,SkyboxMaterial);
        rSkybox->DisableCastShadows();
        Skybox->Add(rSkybox);
        Scene->Add(Skybox);

        carHandle2 = new Model("../../../../examples/RacingGame/assets/lambo.p3dm",true, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
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
            carHandle = new Model("../../../../examples/RacingGame/assets/del.p3dm",true, ShaderUsage::EnvMap | ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
            rCar = new RenderingComponent(carHandle);
            Car->Add(rCar);
            Scene->Add(Car);
            Car->SetPosition(Vec3((rand() % 1000) -500,(rand() % 100),(rand() % 1000) -500));
        }
        Car->SetPosition(Vec3(-23,1,0));
		//Car->SetRotation(Vec3(-23, 100, 0));
		IPhysicsComponent* body = (IPhysicsComponent*) physics->CreateBox(1, 0.5, 2.3, 800);
		carPhysics = (PhysicsVehicle*) physics->CreateVehicle(body);
		carPhysics->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.3f, 0.1f, 1000, 1.f, Vec3(-0.78, 1.15f, 1.35), true);
		carPhysics->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.3f, 0.1f, 1000, 1.f, Vec3(0.75, 1.15f, 1.35), true);
		carPhysics->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.32f, 0.1f, 1000, 1.f, Vec3(-0.78, 1.15f, -1.3), false);
		carPhysics->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.32f, 0.1f, 1000, 1.f, Vec3(0.75, 1.15f, -1.3), false);
		Car->Add(carPhysics);
		Car->Add(Camera);
		Camera->LookAt(Car);
        for (std::vector<RenderingMesh*>::iterator i=rCar->GetMeshes().begin();i!=rCar->GetMeshes().end();i++)
        {
            GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> ((*i)->Material);
            m->SetEnvMap(dRenderer->GetTexture());
            m->SetReflectivity(0.3);
        }
		Scene->Add(Camera);
}

void RacingGame::Update()
{
    // Update - RacingGame Loop
    
    Vec3 finalPosition;
    Vec3 direction = Camera->GetDirection();
    float dt = GetTimeInterval();
    float speed = dt * 20.f;
    /*if (_moveFront)
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
    Camera->SetPosition(Camera->GetPosition()+finalPosition);*/
	Camera->SetPosition(Car->GetWorldPosition());
	Camera->SetPosition(Vec3(0, 3, -10));
	
	Camera->LookAt(Vec3(0,0, -1));
    
	TextRendering->SetPosition(Vec3(5,Height-15.f,0.f));
    std::ostringstream x; x << fps.getFPS();
    
    textID->UpdateText("Pyros3D - Racing RacingGame - FPS: " + x.str());
//    Car->SetPosition(Vec3(20,10,0));
    Car2->SetRotation(Vec3(0,GetTime(),0));

	if (_upPressed == true)
	{
		carPhysics->SetEngineForce(carPhysics->GetMaxEngineForce());
	}
	else if (!_downPressed) {
		carPhysics->SetEngineForce(0);
	}
	if (_downPressed == true)
	{
		carPhysics->SetEngineForce(-carPhysics->GetMaxEngineForce());
	}
	else if (!_upPressed) {
		carPhysics->SetEngineForce(0);
	}
	if (_brakePressed == true)
	{
		carPhysics->SetBreakingForce(carPhysics->GetMaxBreakingForce());
	}
	else {
		carPhysics->SetBreakingForce(0);
	}
	if (_leftPressed == true)
	{
		if (gVehicleSteering<carPhysics->GetSteeringClamp())
		{
			gVehicleSteering += steeringIncrement*dt * 100;
			if (gVehicleSteering>carPhysics->GetSteeringClamp()) gVehicleSteering = carPhysics->GetSteeringClamp();
		}
	}
	else {
		if (gVehicleSteering>0.0f)
		{
			gVehicleSteering -= steeringIncrement*dt * 100;
			if (_rightPressed == false && gVehicleSteering < 0.0f)
				gVehicleSteering = 0.0f;
		}
	}
	if (_rightPressed == true)
	{
		if (gVehicleSteering>-carPhysics->GetSteeringClamp())
		{
			gVehicleSteering -= steeringIncrement*dt * 100;
			if (gVehicleSteering<-carPhysics->GetSteeringClamp()) gVehicleSteering = -carPhysics->GetSteeringClamp();
		}
	}
	else {
		if (gVehicleSteering<0.0f)
		{
			gVehicleSteering += steeringIncrement*dt * 100;
			if (_leftPressed == false && gVehicleSteering > 0.0f)
				gVehicleSteering = 0.0f;
		}
	}

	// Update Physics
	physics->Update(GetTime(), 10);

	if (carPhysics->RigidBodyRegistered())
	{
		btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();
		m_vehicle->setSteeringValue(gVehicleSteering, 0);
		m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 0);
		m_vehicle->setBrake(carPhysics->GetBreakingForce(), 0);
		m_vehicle->setSteeringValue(gVehicleSteering, 1);
		m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 1);
		m_vehicle->setBrake(carPhysics->GetBreakingForce(), 1);
		m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 3);
		m_vehicle->setBrake(carPhysics->GetBreakingForce(), 3);
		m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 4);
		m_vehicle->setBrake(carPhysics->GetBreakingForce(), 4);
	}

	// Update Scene
	Scene->Update(GetTime());
	Scene2->Update(GetTime());

	Skybox->SetPosition(Vec3(Camera->GetWorldPosition().x, 0, Camera->GetWorldPosition().z));

	rCar->Disable();
	dRenderer->RenderCubeMap(Scene, Car, 0.1, 2000);
	rCar->Enable();

	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableDepthTest();
	Renderer->EnableDepthWritting();
	Renderer->EnableClearDepthBuffer();
	Renderer->RenderScene(projection, Camera, Scene);
	Renderer->ClearBufferBit(Buffer_Bit::None);
	Renderer->RenderScene(projection2, Camera2, Scene2);
	//physics->RenderDebugDraw(projection, Camera);
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
    delete rTrack;
    delete Track;
    delete Camera;
    delete Renderer;
    delete Scene;
    delete carHandle;
    delete carHandle2;
    delete trackHandle;
    delete textID;
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
    /*if (mouseCenter!=GetMousePosition())
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
    }*/
}

void RacingGame::CloseApp(Event::Input::Info e)
{
	Close();
}
void RacingGame::LeftUp(Event::Input::Info e)
{
	_leftPressed = false;
}
void RacingGame::LeftDown(Event::Input::Info e)
{
	_leftPressed = true;
}
void RacingGame::RightUp(Event::Input::Info e)
{
	_rightPressed = false;
}
void RacingGame::RightDown(Event::Input::Info e)
{
	_rightPressed = true;
}
void RacingGame::UpUp(Event::Input::Info e)
{
	_upPressed = false;
}
void RacingGame::UpDown(Event::Input::Info e)
{
	_upPressed = true;
}
void RacingGame::DownUp(Event::Input::Info e)
{
	_downPressed = false;
}
void RacingGame::DownDown(Event::Input::Info e)
{
	_downPressed = true;
}
void RacingGame::SpaceUp(Event::Input::Info e)
{
	_brakePressed = false;
}
void RacingGame::SpaceDown(Event::Input::Info e)
{
	_brakePressed = true;
}
void RacingGame::AnalogicMove(Event::Input::Info e)
{
	_leftPressed = _rightPressed = false;
	if ((f32)e.Value>0.1) _rightPressed = true;
	else if ((f32)e.Value<-0.1) _leftPressed = true;
	
	gVehicleSteering = fabs((f32)e.Value)*0.3*0.01*((f32)e.Value>0.0?-1:1);

}
