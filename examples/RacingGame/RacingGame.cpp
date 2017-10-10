//============================================================================
// Name        : RacingGame.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#include "RacingGame.h"

using namespace p3d;

RacingGame::RacingGame() : ClassName(1024, 768, "Pyros3D - Racing Game Example", WindowType::Close | WindowType::Fullscreen)
{

}

void RacingGame::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 2100.f);

	SetMousePosition((int)Width / 2, (int)Height / 2);
}

void RacingGame::Init()
{
	// Initialization
	_leftPressed = _rightPressed = _upPressed = _downPressed = _brakePressed = false;
	gVehicleSteering = 0.f;
	steeringIncrement = 0.01f;

	// Initialize Scene
	Scene = new SceneGraph();
	
	// Initialize Renderer
	Renderer = new ForwardRenderer(1024, 768);
	Renderer->SetGlobalLight(Vec4(0.5, 0.5, 0.5, 0.5));

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 2100.f);

	// Physics
	physics = new Physics();
	physics->InitPhysics();
	physics->EnableDebugDraw();

	// Create Camera
	FollowCamera = new GameObject();
	HoodCamera = new GameObject();

	// Set Default Camera
	Camera = FollowCamera;
	_followCamera = true;

	// Create Track GameObject
	Track = new GameObject();

	// Create Track Model
	trackHandle = new Model("../examples/RacingGame/assets/track/track.p3dm", true, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::EnvMap);
	rTrack = new RenderingComponent(trackHandle);

	rTrack->GetMeshes()[19]->Material->DisableCastingShadows();
	rTrack->GetMeshes()[19]->Material->SetCullFace(CullFace::DoubleSided);

	{
		// sand
		{
			std::vector<uint32> index;
			std::vector<Vec3> vertex;
			unsigned indexCount = 0;
			RenderingMesh* rc = (RenderingMesh*)rTrack->GetMeshes()[9];
			for (unsigned i = 0; i < rc->Geometry->GetIndexData().size(); i++)
			{
				index.push_back(indexCount++);
				vertex.push_back(rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i]]);
			}
			pSand = new PhysicsTriangleMesh(physics, index, vertex);
		}
		
		// grass
		{
			std::vector<uint32> index;
			std::vector<Vec3> vertex;
			unsigned indexCount = 0;
			RenderingMesh* rc = (RenderingMesh*)rTrack->GetMeshes()[11];
			for (unsigned i = 0; i < rc->Geometry->GetIndexData().size(); i++)
			{
				index.push_back(indexCount++);
				vertex.push_back(rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i]]);
			}
			pGrass = new PhysicsTriangleMesh(physics, index, vertex);
		}

		// Rest of the Track
		{
			std::vector<uint32> index;
			std::vector<Vec3> vertex;
			unsigned indexCount = 0;
			for (unsigned k = 0; k < rTrack->GetMeshes().size(); k++)
			{
				if (k != 9 && k != 11 && k != 14 && k != 16) {
					RenderingMesh* rc = (RenderingMesh*)rTrack->GetMeshes()[k];
					for (unsigned i = 0; i < rc->Geometry->GetIndexData().size(); i++)
					{
						index.push_back(indexCount++);
						vertex.push_back(rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i]]);
					}
				}
			}
			pRestTrack = new PhysicsTriangleMesh(physics, index, vertex);
		}

		// Track
		{
			std::vector<uint32> index;
			std::vector<Vec3> vertex;
			unsigned indexCount = 0;
			RenderingMesh* rc = (RenderingMesh*)rTrack->GetMeshes()[14];
			for (unsigned i = 0; i < rc->Geometry->GetIndexData().size(); i++)
			{
				index.push_back(indexCount++);
				vertex.push_back(rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i]]);
			}
			pTrack = new PhysicsTriangleMesh(physics, index, vertex);
		}

		Track->Add(pTrack);
		Track->Add(pSand);
		Track->Add(pRestTrack);
		Track->Add(pGrass);

	}

	//pTrack = new PhysicsTriangleMesh(physics, rTrack, 0);
	Track->Add(rTrack);
	Scene->Add(Track);

	// Light
	Light = new GameObject();
	// Light Component
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1.0, 1.1));
	dLight->EnableCastShadows(2048, 2048, projection, 0.1f, 200.f, 2);
	dLight->SetShadowBias(3.1f, 9.0f);
	a = 3.1f;
	b = 9.f;
	dLight->SetShadowPCFTexelSize(0.0001f);
	c = 0.0001f;
	Light->Add(dLight);
	Scene->Add(Light);

	// Input
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
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::C, this, &RacingGame::ChangeCamera);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::R, this, &RacingGame::Reset);

	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button0, this, &RacingGame::UpDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button0, this, &RacingGame::UpUp);
	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button1, this, &RacingGame::DownDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button1, this, &RacingGame::DownUp);
	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button2, this, &RacingGame::SpaceDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button2, this, &RacingGame::SpaceUp);
	InputManager::AddJoypadEvent(Event::Type::OnMove, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Axis::X, this, &RacingGame::AnalogicMove);

	dRenderer = new CubemapRenderer(1024, 1024);

	startedDrivingLikeAGirlTexture = new Texture();
	startedDrivingLikeAGirlTexture->LoadTexture("../examples/RacingGame/assets/textures/wrong.png");

	Texture* skyboxTexture = new Texture();
	skyboxTexture->LoadTexture("../examples/RacingGame/assets/textures/skybox/negx.png", TextureType::CubemapNegative_X);
	skyboxTexture->LoadTexture("../examples/RacingGame/assets/textures/skybox/negy.png", TextureType::CubemapNegative_Y);
	skyboxTexture->LoadTexture("../examples/RacingGame/assets/textures/skybox/negz.png", TextureType::CubemapNegative_Z);
	skyboxTexture->LoadTexture("../examples/RacingGame/assets/textures/skybox/posx.png", TextureType::CubemapPositive_X);
	skyboxTexture->LoadTexture("../examples/RacingGame/assets/textures/skybox/posy.png", TextureType::CubemapPositive_Y);
	skyboxTexture->LoadTexture("../examples/RacingGame/assets/textures/skybox/posz.png", TextureType::CubemapPositive_Z);
	skyboxTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	SkyboxMaterial = new GenericShaderMaterial(ShaderUsage::Skybox);
	SkyboxMaterial->SetSkyboxMap(skyboxTexture);
	SkyboxMaterial->SetCullFace(CullFace::FrontFace);
	Skybox = new GameObject();
	skyboxHandle = new Cube(1000, 1000, 1000);
	rSkybox = new RenderingComponent(skyboxHandle, SkyboxMaterial);
	rSkybox->DisableCastShadows();
	Skybox->Add(rSkybox);
	Scene->Add(Skybox);

	Car = new GameObject();

	carHandle = new Model("../examples/RacingGame/assets/delorean/delorean.p3dm", true, ShaderUsage::EnvMap | ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
	rCar = new RenderingComponent(carHandle);
	Car->Add(rCar);
	Scene->Add(Car);

	IPhysicsComponent* body = (IPhysicsComponent*)physics->CreateBox(1.f, 0.5f, 2.3f, 1288.f);
	carPhysics = (PhysicsVehicle*)physics->CreateVehicle(body);
	
	//carPhysics->SetMaxBreakingForce(1000);
	//carPhysics->SetMaxEngineForce(200);
	carPhysics->SetSuspensionStiffness(80.f);
	carPhysics->SetSuspensionDamping(2.3f);
	carPhysics->SetSuspensionCompression(4.4f);
	carPhysics->SetSuspensionRestLength(0.6f);

	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.3f, 0.1f, 1.f, 1.f, Vec3(-0.75f, 1.15f, 1.3f), true);
	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.3f, 0.1f, 1.f, 1.f, Vec3(0.75f, 1.15f, 1.3f), true);
	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.3f, 0.1f, 1.f, 1.f, Vec3(-0.75f, 1.15f, -1.3f), false);
	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.3f, 0.1f, 1.f, 1.f, Vec3(0.75f, 1.15f, -1.3f), false);
	Car->Add(carPhysics);
	for (std::vector<RenderingMesh*>::iterator i = rCar->GetMeshes().begin(); i != rCar->GetMeshes().end(); i++)
	{
		GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> ((*i)->Material);
		m->SetEnvMap(dRenderer->GetTexture());
		m->SetReflectivity(0.3f);
		m->SetSpecular(Vec4(1, 1, 1, 1));
	}

	for (std::vector<RenderingMesh*>::iterator i = rTrack->GetMeshes().begin(); i != rTrack->GetMeshes().end(); i++)
	{
		GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> ((*i)->Material);
		m->SetEnvMap(dRenderer->GetTexture());
		m->SetReflectivity(0.1f);
		m->SetSpecular(Vec4(1, 1, 1, 1));
	}

	// Wheels
	{
		gWheelFL = new GameObject();
		gWheelFR = new GameObject();
		gWheelBL = new GameObject();
		gWheelBR = new GameObject();

		wheelFLHandle = new Model("../examples/RacingGame/assets/delorean/WheelFL.p3dm", true, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
		wheelFRHandle = new Model("../examples/RacingGame/assets/delorean/WheelFR.p3dm", true, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
		wheelBLHandle = new Model("../examples/RacingGame/assets/delorean/WheelBL.p3dm", true, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
		wheelBRHandle = new Model("../examples/RacingGame/assets/delorean/WheelBR.p3dm", true, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);

		rWheelFL = new RenderingComponent(wheelFLHandle);
		rWheelFR = new RenderingComponent(wheelFRHandle);
		rWheelBL = new RenderingComponent(wheelBLHandle);
		rWheelBR = new RenderingComponent(wheelBRHandle);

		gWheelFL->Add(rWheelFL);
		gWheelFR->Add(rWheelFR);
		gWheelBL->Add(rWheelBL);
		gWheelBR->Add(rWheelBR);

		//Car->Add(gWheelFL);
		//Car->Add(gWheelFR);
		//Car->Add(gWheelBL);
		//Car->Add(gWheelBR);

		Scene->Add(gWheelFL);
		Scene->Add(gWheelFR);
		Scene->Add(gWheelBL);
		Scene->Add(gWheelBR);

		//gWheelFL->SetPosition(Vec3(-1, 3, -1));
		//gWheelFR->SetPosition(Vec3(1, 3, -1));
		//gWheelBL->SetPosition(Vec3(-1, 3, 1));
		//gWheelBR->SetPosition(Vec3(1, 3, 1));
	}

	Scene->Add(FollowCamera);
	Scene->Add(HoodCamera);

	HideMouse();

	timeInterval = 0;

	sound = new Sound();
	sound->LoadFromFile("../examples/RacingGame/assets/sounds/delorean_sound.ogg");
	sound->Play(true);

	crash = new Sound();
	crash->LoadFromFile("../examples/RacingGame/assets/sounds/crash_sound.ogg");

	// Set Portals
	planeHandle = new Cube(40.f, 10.f, .1f);
	{
		addPortal(Vec3(234.6f,0.f,-64.15f), Vec3(0.f,-0.086f,0.f));
		addPortal(Vec3(237.8f, 0.f, -272.01f), Vec3(0.f, 0.753f, 0.f));
		addPortal(Vec3(151.16f, 0.f, -296.01f), Vec3(-3.142f, 1.145f, -3.142f));
		addPortal(Vec3(148.2f, 0.f, -225.08f), Vec3(3.142f, 0.171f, 3.142f));
		addPortal(Vec3(114.7f, 0.f, -37.11f), Vec3(3.142f, 0.171f, 3.142f));
		addPortal(Vec3(99.27f, 0.f, -9.67f), Vec3(3.142f, 1.025f, 3.142f));
		addPortal(Vec3(35.79f, 0.f, -28.99f), Vec3(0.f, 1.022f, 0.f));
		addPortal(Vec3(25.30f, 0.f, -61.02f), Vec3(0.f, -0.151f, 0.f));
		addPortal(Vec3(45.92f, 0.f, -180.51f), Vec3(0.f, -0.151f, 0.f));
		addPortal(Vec3(75.24f, 0.f, -228.18f), Vec3(0.f, -0.151f, 0.f));
		addPortal(Vec3(38.47f, 0.f, -275.75f), Vec3(0.f, 1.f, 0.f));
		addPortal(Vec3(-90.28f, 0.f, -292.85f), Vec3(3.142f, 1.138f, 3.142f));
		addPortal(Vec3(-121.75f, 0.f, -267.284f), Vec3(3.142f, 0.588f, 3.142f));
		addPortal(Vec3(-159.85f, 0.f, -243.38f), Vec3(3.142f, 1.042f, 3.142f));
		addPortal(Vec3(-238.73f, 0.f, -222.08f), Vec3(3.142f, 0.637f, 3.142f));
		addPortal(Vec3(-243.63f, 0.f, -179.24f), Vec3(3.142f, 0.f, 3.142f));
		addPortal(Vec3(-245.75f, 0.f, 232.55f), Vec3(3.142f, 0.f, 3.142f));
		addPortal(Vec3(-230.515f, 0.f, 295.302f), Vec3(3.142f, -0.833f, 3.142f));
		addPortal(Vec3(-172.71f, 0.f, 306.78f), Vec3(0.f, -1.359f, 0.f));
		addPortal(Vec3(-93.17f, 0.f, 259.5f), Vec3(0.f, -0.618f, 0.f));
		addPortal(Vec3(-87.86f, 0.f, 204.87f), Vec3(0.f, 0.055f, 0.f));
		addPortal(Vec3(-102.27f, 0.f, 182.22f), Vec3(0.f, 0.055f, 0.f));
		addPortal(Vec3(-140.64f, 0.f, -1.59f), Vec3(0.f, 0.055f, 0.f));
		addPortal(Vec3(-103.72f, 0.f, -52.92f), Vec3(0.f, -0.853f, 0.f));
		addPortal(Vec3(-36.76f, 0.f, -43.71f), Vec3(3.142f, -1.122f, 3.142f));
		addPortal(Vec3(-21.87f, 0.f, 1.295f), Vec3(3.142f, -0.127f, 3.142f));
		addPortal(Vec3(-17.099f, 0.f, 177.039f), Vec3(3.142f, -0.127f, 3.142f));
		addPortal(Vec3(-8.36f, 0.f, 197.122f), Vec3(3.142f, -0.577f, 3.142f));
		addPortal(Vec3(27.83f, 0.f, 213.18f), Vec3(3.142f, -1.189f, 3.142f));
		addPortal(Vec3(57.30f, 0.f, 214.9f), Vec3(0.f, -1.435f, 0.f));
		addPortal(Vec3(188.58f, 0.f, 208.27f), Vec3(0.f, -1.435f, 0.f));
		addPortal(Vec3(226.15f, 0.f, 177.9f), Vec3(0.f, 0.056f, 0.f));
		addPortal(Vec3(201.23f, 0.f, 157.99f), Vec3(0.f, 0.586f, 0.f));
		addPortal(Vec3(65.47f, 0.f, 122.25f), Vec3(0.f, 0.841f, 0.f));
		addPortal(Vec3(41.711f, 0.f, 86.73f), Vec3(0.f, -0.156f, 0.f));
		addPortal(Vec3(88.282f, 0.f, 58.96f), Vec3(0.f, -1.329f, 0.f));
		addPortal(Vec3(186.62f, 0.f, 93.596f), Vec3(0.f, -1.329f, 0.f));
		addPortal(Vec3(215.28f, 0.f, 96.89f), Vec3(0.f, -0.783f, 0.f));
		addPortal(Vec3(226.33f, 0.f, 35.92f), Vec3(0.f, 0.f, 0.f));


		for (int i = 0; i < portals.size(); i++)
		{
			int k = i+1;
			if (i + 1 == portals.size())
				k = 0;
			portals[i].direction = (portals[k].gPortal->GetWorldPosition() - portals[i].gPortal->GetWorldPosition()).normalize();
		}
	}

	brakingMat = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::DirectionalShadow | ShaderUsage::EnvMap | ShaderUsage::Diffuse);
	((GenericShaderMaterial*)brakingMat)->SetColor(Vec4(1, 0, 0, 1));
	((GenericShaderMaterial*)brakingMat)->SetEnvMap(dRenderer->GetTexture());
	((GenericShaderMaterial*)brakingMat)->SetReflectivity(0.3f);

	defaultBrakingMat = rCar->GetMeshes()[0]->Material;


	// UI
	ImGui::SFML::ImGui_ImplSFML_Init(&rview);
	clear_color = ImColor(114, 144, 154);


	raceStart = false;
	portalNumber = -1;


	ImFontConfig config;
	config.OversampleH = 3;
	config.OversampleV = 1;
	config.GlyphExtraSpacing.x = 1.0f;
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("../examples/RacingGame/assets/fonts/BEBAS___.ttf", 16, &config);

	Update(); // Run once to add and register objects

	Event::Input::Info e;
	Reset(e);
}

void RacingGame::addPortal(const Vec3 &pos, const Vec3 &rot)
{
	Portal portal;
	portal.gPortal = new GameObject();
	//portal.rPortal = new RenderingComponent(planeHandle);
	//portal.gPortal->Add(portal.rPortal);
	portal.pPortal = physics->CreateBox(40.f, 10.f, .1f, 0, true);
	portal.gPortal->Add(portal.pPortal);

	portal.gPortal->SetPosition(pos);
	portal.gPortal->SetRotation(rot);

	Scene->Add(portal.gPortal);

	portal.portalPassage = 0;

	// Add To Vector
	portals.push_back(portal);
}

void RacingGame::Update()
{
	float dt = GetTimeInterval();

	float speed = dt * 20.f;

	if (_upPressed)
	{
		carPhysics->SetEngineForce(carPhysics->GetMaxEngineForce());
	}
	else if (!_downPressed)
	{
		carPhysics->SetEngineForce(0);
	}
	if (_downPressed)
	{
		carPhysics->SetEngineForce(-carPhysics->GetMaxEngineForce());
	}
	else if (!_upPressed) {
		carPhysics->SetEngineForce(0);
	}
	if (_brakePressed)
	{
		LightBrakesON();
		carPhysics->SetBreakingForce(carPhysics->GetMaxBreakingForce());
	}

	if (!_brakePressed)
	{
		LightBrakesOFF();
		carPhysics->SetBreakingForce(0);
	}

	if (_leftPressed)
	{
		if (gVehicleSteering < carPhysics->GetSteeringClamp())
		{
			gVehicleSteering += steeringIncrement*dt * 100;
			if (gVehicleSteering > carPhysics->GetSteeringClamp()) gVehicleSteering = carPhysics->GetSteeringClamp();
		}
	}
	if (_rightPressed)
	{
		if (gVehicleSteering > -carPhysics->GetSteeringClamp())
		{
			gVehicleSteering -= steeringIncrement*dt * 100;
			if (gVehicleSteering < -carPhysics->GetSteeringClamp()) gVehicleSteering = -carPhysics->GetSteeringClamp();
		}
	}

	if (!_rightPressed && !_leftPressed)
	{
		if (gVehicleSteering < 0.0f)
		{
			gVehicleSteering += steeringIncrement*dt * 100;
			if (!_leftPressed && gVehicleSteering > 0.0f)
				gVehicleSteering = 0.0f;
		}
		if (gVehicleSteering > 0.0f)
		{
			gVehicleSteering -= steeringIncrement*dt * 100;
			if (!_rightPressed && gVehicleSteering < 0.0f)
				gVehicleSteering = 0.0f;
		}
	}

	if (carPhysics->RigidBodyRegistered())
	{

	}

	timeInterval += dt;
	while (timeInterval >= 1.f / 60.f)
	{
		physics->Update(1.f / 60.f, 10);
		timeInterval -= 1.f / 60.f;

		if (carPhysics->RigidBodyRegistered())
		{
			btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();

			// Update Camera Position
			btTransform transf = m_vehicle->getChassisWorldTransform();

			sound->SetPitch(0.5 + fabs(m_vehicle->getCurrentSpeedKmHour()) / 200.f);

			Matrix m;
			m.Translate(transf.getOrigin().x(), transf.getOrigin().y(), transf.getOrigin().z());
			Quaternion q = Quaternion(transf.getRotation().w(), transf.getRotation().x(), transf.getRotation().y(), transf.getRotation().z());

			m.SetRotationFromEuler(q.GetEulerFromQuaternion());

			Vec3 CameraTargetPosition = m * Vec3(0.f, 3.f, -10.f);
			CameraPosition += (CameraTargetPosition - CameraPosition) * 0.1f;
			//CameraPosition = CameraTargetPosition;

			// UPDATE WHEELS MANUALLY
			Matrix _m;
			m_vehicle->getWheelInfo(0).m_worldTransform.getOpenGLMatrix(&_m.m[0]);
			gWheelFL->SetTransformationMatrix(_m);
			m_vehicle->getWheelInfo(1).m_worldTransform.getOpenGLMatrix(&_m.m[0]);
			gWheelFR->SetTransformationMatrix(_m);
			m_vehicle->getWheelInfo(2).m_worldTransform.getOpenGLMatrix(&_m.m[0]);
			gWheelBL->SetTransformationMatrix(_m);
			m_vehicle->getWheelInfo(3).m_worldTransform.getOpenGLMatrix(&_m.m[0]);
			gWheelBR->SetTransformationMatrix(_m);

			// Get Wheel Info
			uint32 whatTerrainAreWe = TERRAIN::ASPHALT;
			for (int i = 0; i < m_vehicle->getNumWheels(); i++)
			{
				btWheelInfo* wheel = &m_vehicle->getWheelInfo(i);
				if (wheel->m_raycastInfo.m_isInContact) {

					btCollisionWorld::ClosestRayResultCallback RayCallback(wheel->m_worldTransform.getOrigin(), wheel->m_raycastInfo.m_contactPointWS);

					// Perform raycast
					physics->GetPhysicsWorld()->rayTest(wheel->m_worldTransform.getOrigin(), wheel->m_raycastInfo.m_contactPointWS, RayCallback);

					if (RayCallback.hasHit()) {
						if (RayCallback.m_collisionObject == pTrack->GetRigidBodyPTR())
						{
							if (whatTerrainAreWe != TERRAIN::GRASS || whatTerrainAreWe != TERRAIN::SAND)
								whatTerrainAreWe = TERRAIN::ASPHALT;
						}
						if (RayCallback.m_collisionObject == pGrass->GetRigidBodyPTR())
						{
							if (whatTerrainAreWe != TERRAIN::SAND)
								whatTerrainAreWe = TERRAIN::GRASS;
						}
						if (RayCallback.m_collisionObject == pSand->GetRigidBodyPTR())
						{
							whatTerrainAreWe = TERRAIN::SAND;
						}
					}
				}
			}

			// Use resultant whatTerrainAreWe to make car slow down or speed up
			switch (whatTerrainAreWe)
			{
			case TERRAIN::SAND:
				if (fabs(m_vehicle->getCurrentSpeedKmHour()) > 10)
					m_vehicle->getRigidBody()->setLinearVelocity(m_vehicle->getRigidBody()->getLinearVelocity()*.9);//->applyImpulse((m_vehicle->getForwardVector()*-1.f) * btVector3(0, 0, 1000), m_vehicle->getForwardVector()*-1.f);
				break;
			case TERRAIN::GRASS:
				if (fabs(m_vehicle->getCurrentSpeedKmHour()) > 30)
					m_vehicle->getRigidBody()->setLinearVelocity(m_vehicle->getRigidBody()->getLinearVelocity()*.9);
				break;
			default:
			case TERRAIN::ASPHALT:
				m_vehicle->setSteeringValue(gVehicleSteering, 0);
				m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 0);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 0);
				m_vehicle->setSteeringValue(gVehicleSteering, 1);
				m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 1);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 1);
				m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 2);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 2);
				m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 3);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 3);
				break;
			};
		}

		int numManifolds = physics->GetPhysicsWorld()->getDispatcher()->getNumManifolds();
		for (int i = 0; i < numManifolds; i++)
		{
			btPersistentManifold* contactManifold = physics->GetPhysicsWorld()->getDispatcher()->getManifoldByIndexInternal(i);
			const btCollisionObject* obA = contactManifold->getBody0();
			const btCollisionObject* obB = contactManifold->getBody1();

			btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();


			if (
				(obA == pRestTrack->GetRigidBodyPTR() || obB == pRestTrack->GetRigidBodyPTR())
				&&
				(obA == m_vehicle->getRigidBody() || obB == m_vehicle->getRigidBody())
				)
			{
				int numContacts = contactManifold->getNumContacts();
				for (int j = 0; j < numContacts; j++)
				{
					btManifoldPoint& pt = contactManifold->getContactPoint(j);
					if (pt.getDistance() < 0.f)
					{
						if (!crash->isPlaying()) {
							f32 crashForce = pt.getAppliedImpulse()*0.01f;
							if (crashForce > 5.f)
							{
								crash->SetVolume(clamp(crashForce, 10.f, 100.f));
								crash->Play();
							}
						}
						const btVector3& ptA = pt.getPositionWorldOnA();
						const btVector3& ptB = pt.getPositionWorldOnB();
						const btVector3& normalOnB = pt.m_normalWorldOnB;
					}
				}
			}
			else {
				// Check for Portals
				uint32 portal = 0;
				for (std::vector<Portal>::iterator p = portals.begin(); p != portals.end(); p++)
				{
					if (obA == (*p).pPortal->GetRigidBodyPTR() || obB == (*p).pPortal->GetRigidBodyPTR())
					{
						if (portal != portalNumber)
						{
							if (portal == 0)
							{
								if (!raceStart)
								{
									// Start Race
									raceStart = true;
									raceInitTime = GetTime();
									lapInitTime = GetTime();
								}
								else {
									bool finished = true;
									for (std::vector<Portal>::iterator _p = portals.begin(); _p != portals.end(); _p++)
									{
										if ((*_p).portalPassage != 1) finished = false;
									}
									if (finished)
									{
										// Lap Finished
										lapTime[(lap - 1)] = GetTime() - lapInitTime;
										
										lapInitTime = GetTime(); // Set Init time for next lap

										if (lap == lapLimit)
										{
											// GameOver

										}
										lap++;

										// Clean Passages
										for (std::vector<Portal>::iterator _p = portals.begin(); _p != portals.end(); _p++)
										{
											(*_p).portalPassage = 0;
										}
									}
								}
							}

							{
								if (portalNumber == (portal + 1) || ((portal == portals.size() - 1) && portalNumber == 0))
								{
									portals[portalNumber].portalPassage--;
								}
								else
								{
									portals[portal].portalPassage++;
								}
							}

							portalNumber = portal;
						}
					}
					portal++;
				}
			}
		}
	}

	FollowCamera->SetPosition(CameraPosition);

	// Update Scene
	Scene->Update(GetTime());

	Skybox->SetPosition(Vec3(CameraPosition.x, 0, CameraPosition.y));

	rCar->Disable();
	rWheelFL->Disable();
	rWheelFR->Disable();
	rWheelBL->Disable();
	rWheelBR->Disable();

	dRenderer->RenderCubeMap(Scene, Car, 0.1f, 2000.f);

	rCar->Enable();
	rWheelFL->Enable();
	rWheelFR->Enable();
	rWheelBL->Enable();
	rWheelBR->Enable();

	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClearDepthBuffer();
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);

	//physics->RenderDebugDraw(projection, Camera);




	// ######################### UI ###############################
	ImGui::SFML::ImGui_ImplSFML_NewFrame();

	{
		ImGui::Text("Shadows");
		ImGui::SliderFloat("Factor", &a, 0.0f, 10.0f);
		ImGui::SliderFloat("Units", &b, 0.0f, 10.0f);
		ImGui::InputFloat("Bias", &c);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		for (int i = 0; i < portals.size(); i++)
			ImGui::Text("Portal: %d - passage: %d", i, portals[i].portalPassage);
		dLight->SetShadowBias(a, b);
		dLight->SetShadowPCFTexelSize(c);
	}

	bool showTimers = true;
	ImGui::SetNextWindowPos(ImVec2(5, 5));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1, 0.1, 0.1, 0.1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("Timers", &showTimers, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Best  Race  Time:  %.3f", raceTime);
	ImGui::Text("Best  Lap  Time:  %.3f", lapTime);
	ImGui::Text("-");
	ImGui::Text("Race  Time:  %.3f", GetTime()-raceInitTime);

	if (lapTime.size() > 0)
	{
		for (int i = 1; i <= lap; i++) {
			if (lap != i)
				ImGui::Text("Lap  %d  Time:  %.3f", i, lapTime[i - 1]);
			else ImGui::Text("Lap  %d  Time:  %.3f", i, GetTime() - lapInitTime);
		}
	}
	ImGui::Text("Lap  %d/%d", lap, lapLimit);
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	if (startedDrivingLikeAGirl)
	{
		bool showStartedDrivingLikeAGirl = ((GetTime() - timeStartedDrivingLikeAGirl) > 3); // more than 3 seconds
		if (showStartedDrivingLikeAGirl)
		{
			ImGui::SetNextWindowPos(ImVec2(Width*.5f - 150, 50));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0, 0.0, 0.0, 0.0));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::Begin("DrivingLikeAGirl", &showStartedDrivingLikeAGirl, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			ImGui::Image((void*)startedDrivingLikeAGirlTexture->GetBindID(), ImVec2(300, 300));
			ImGui::End();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}
	}

	ImGui::SFML::ImGui_ImplSFML_Render((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y, clear_color);

	// ######################### UI ###############################

	// Check our direction
	if (Car->GetDirection().dotProduct(portals[portalNumber].direction) < 0)
	{
		if (!startedDrivingLikeAGirl) {
			startedDrivingLikeAGirl = true;
			timeStartedDrivingLikeAGirl = GetTime();
		}
	}
	else startedDrivingLikeAGirl = false;
}

void RacingGame::Shutdown()
{
	// All your Shutdown Code Here
	delete planeHandle;

	Scene->Remove(Track);
	Scene->Remove(Car);
	Track->Remove(rTrack);
	Car->Remove(rCar);
	delete startedDrivingLikeAGirlTexture;
	delete rCar;
	delete Car;
	delete rTrack;
	delete Track;
	delete FollowCamera;
	delete HoodCamera;
	delete Renderer;
	delete Scene;
	delete carHandle;
	delete trackHandle;
	delete crash;
	delete sound;
}

RacingGame::~RacingGame() {}

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
	if ((f32)e.Value > 0.1) _rightPressed = true;
	else if ((f32)e.Value<-0.1) _leftPressed = true;

	gVehicleSteering = fabs((f32)e.Value)*0.3f*0.01f*((f32)e.Value>0.0 ? -1 : 1);

}
void RacingGame::ChangeCamera(Event::Input::Info e)
{
	if (_followCamera) {
		_followCamera = false;
		Camera = HoodCamera;
	}
	else {
		_followCamera = true;
		Camera = FollowCamera;
	}
}
void RacingGame::Reset(Event::Input::Info e)
{
	btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();
	m_vehicle->getRigidBody()->setLinearVelocity(btVector3(0, 0, 0));
	m_vehicle->getRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
	btTransform transf;
	Matrix m;
	m.Translate(Vec3(234.f, -0.25f, -59.f));
	m.SetRotationFromEuler(Vec3(3.14, 0.08, -3.14));

	transf.setFromOpenGLMatrix(&m.m[0]);
	m_vehicle->getRigidBody()->setWorldTransform(transf);
	m_vehicle->getRigidBody()->clearForces();
	m_vehicle->resetSuspension();
	
	// Reset passages in portals
	for (std::vector<Portal>::iterator _p = portals.begin(); _p != portals.end(); _p++)
	{
		(*_p).portalPassage = 0;
	}
	portalNumber = -1;

	{
		// Set Camera Initial Position
		Matrix m;
		m.Translate(Vec3(234.f, -0.25f, -59.f));
		m.SetRotationFromEuler(Vec3(3.14, 0.08, -3.14));
		Vec3 CameraTargetPosition = m * Vec3(0.f, 3.f, -10.f);
		FollowCamera->SetPosition(CameraTargetPosition);
		FollowCamera->LookAt(Car);
		CameraPosition = CameraTargetPosition;

		Car->Add(HoodCamera);
		HoodCamera->SetPosition(Vec3(0, 2.f, 0.6f));
		HoodCamera->SetRotation(Vec3(DEGTORAD(5), DEGTORAD(180), 0));
	}

	startedDrivingLikeAGirl = false;
	raceStart = false;
	lap = 1;
	lapLimit = 3;
	lapTime.clear();
	lapTime.push_back(0);
}
void RacingGame::LightBrakesON()
{
	if (rCar->GetMeshes()[0]->Material == defaultBrakingMat)
		rCar->GetMeshes()[0]->Material = brakingMat;
}
void RacingGame::LightBrakesOFF()
{
	if (rCar->GetMeshes()[0]->Material == brakingMat)
		rCar->GetMeshes()[0]->Material = defaultBrakingMat;
}