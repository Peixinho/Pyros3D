//============================================================================
// Name        : RacingGame.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#include "RacingGame.h"

using namespace p3d;

//RacingGame::RacingGame() : ClassName(1920,1080, "Pyros3D - Racing Game Example", WindowType::Close | WindowType::Fullscreen)
RacingGame::RacingGame() : ClassName(1280, 1024, "Racing Game", WindowType::Close | WindowType::Resize)
{

}

void RacingGame::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.01f, 2100.f);

	SetMousePosition((int)Width / 2, (int)Height / 2);
}

void RacingGame::Init()
{
	GetRaceOnlinScore();

	// Initialization
	_leftPressed = _rightPressed = _upPressed = _downPressed = _brakePressed = false;
	gVehicleSteering = 0.f;
	steeringIncrement = 0.01f;

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetGlobalLight(Vec4(0.3, 0.3, 0.3, 0.3));

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.01f, 2100.f);

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

	// Semaphore
	hSemaphore = new Model(PATH"/assets/semaphore/semaphore.p3dm", true);
	rSemaphore = new RenderingComponent(hSemaphore, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
	redSemaphoreDefault = rSemaphore->GetMeshes()[0]->Material;
	yellowSemaphoreDefault = rSemaphore->GetMeshes()[1]->Material;
	greenSemaphoreDefault = rSemaphore->GetMeshes()[2]->Material;
	gSemaphore = new GameObject();
	gSemaphore->Add(rSemaphore);
	gSemaphore->SetPosition(Vec3(247.6f, -5.24f, -87.6f));
	gSemaphore->SetRotation(Vec3(0.f, -1.57f, .0f));
	Scene->Add(gSemaphore);

	redSemaphore = new GenericShaderMaterial(ShaderUsage::Color);
	redSemaphore->SetColor(Vec4(1, 0, 0, 1));
	yellowSemaphore = new GenericShaderMaterial(ShaderUsage::Color);
	yellowSemaphore->SetColor(Vec4(1, 1, 0, 1));
	greenSemaphore = new GenericShaderMaterial(ShaderUsage::Color);
	greenSemaphore->SetColor(Vec4(0, 1, 0, 1));

	// Create Track GameObject
	Track = new GameObject();

	// Create Track Model
	trackHandle = new Model(PATH"/assets/track/track.p3dm", true);
	rTrack = new RenderingComponent(trackHandle, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);

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
	dLight->EnableCastShadows(2048, 2048, projection, 0.1f, 300.f, 3);
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
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::F, this, &RacingGame::ToggleFS);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &RacingGame::GearUp);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Z, this, &RacingGame::GearDown);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::Tab, this, &RacingGame::ShowRanking);

	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button7, this, &RacingGame::UpDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button7, this, &RacingGame::UpUp);
	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button5, this, &RacingGame::DownDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button5, this, &RacingGame::DownUp);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button6, this, &RacingGame::GearUp);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button4, this, &RacingGame::GearDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button9, this, &RacingGame::ChangeCamera);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button8, this, &RacingGame::Reset);
	InputManager::AddJoypadEvent(Event::Type::OnMove, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Axis::X, this, &RacingGame::AnalogicMove);

	dRenderer = new CubemapRenderer(1024, 1024);

	startedDrivingLikeAGirlTexture = new Texture();
	startedDrivingLikeAGirlTexture->LoadTexture(PATH"/assets/textures/wrong.png");

	RPMGui = new Texture();
	RPMGui->LoadTexture(PATH"/assets/textures/rpm.png");
	RPMPointer = new Texture();
	RPMPointer->LoadTexture(PATH"/assets/textures/pointer.png");

	Texture* skyboxTexture = new Texture();
	skyboxTexture->LoadTexture(PATH"/assets/textures/skybox/negx.png", TextureType::CubemapNegative_X);
	skyboxTexture->LoadTexture(PATH"/assets/textures/skybox/negy.png", TextureType::CubemapNegative_Y);
	skyboxTexture->LoadTexture(PATH"/assets/textures/skybox/negz.png", TextureType::CubemapNegative_Z);
	skyboxTexture->LoadTexture(PATH"/assets/textures/skybox/posx.png", TextureType::CubemapPositive_X);
	skyboxTexture->LoadTexture(PATH"/assets/textures/skybox/posy.png", TextureType::CubemapPositive_Y);
	skyboxTexture->LoadTexture(PATH"/assets/textures/skybox/posz.png", TextureType::CubemapPositive_Z);
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

	carHandle = new Model(PATH"/assets/delorean/delorean.p3dm", true);
	rCar = new RenderingComponent(carHandle, ShaderUsage::EnvMap | ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
	Car->Add(rCar);
	Scene->Add(Car);

	IPhysicsComponent* body = (IPhysicsComponent*)physics->CreateBox(1.f, 0.5f, 2.3f, 1288.f);
	carPhysics = (PhysicsVehicle*)physics->CreateVehicle(body);

	//carPhysics->SetMaxBreakingForce(1000);
	//carPhysics->SetMaxEngineForce(200);
	carPhysics->SetSuspensionStiffness(80.f);
	carPhysics->SetSuspensionDamping(2.3f);
	carPhysics->SetSuspensionCompression(4.4f);
	carPhysics->SetSuspensionRestLength(0.58f);
	carPhysics->SetSteeringClamp(0.45f);

	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.2f, 0.1f, 1.f, 1.f, Vec3(-0.75f, 1.15f, 1.3f), true);
	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.2f, 0.1f, 1.f, 1.f, Vec3(0.75f, 1.15f, 1.3f), true);
	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.2f, 0.1f, 1.f, 1.f, Vec3(-0.75f, 1.15f, -1.3f), false);
	carPhysics->AddWheel(Vec3(0.f, -1.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.2f, 0.1f, 1.f, 1.f, Vec3(0.75f, 1.15f, -1.3f), false);
	Car->Add(carPhysics);
	for (std::vector<RenderingMesh*>::iterator i = rCar->GetMeshes().begin(); i != rCar->GetMeshes().end(); i++)
	{
		GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> ((*i)->Material);
		m->SetEnvMap(dRenderer->GetTexture());
		m->SetReflectivity(0.3f);
		m->SetSpecular(Vec4(1, 1, 1, 1));
	}

	{
		GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> (rTrack->GetMeshes()[16]->Material);
		m->SetSpecular(Vec4(1, 1, 1, 1));
	}

	// Wheels
	{
		gWheelFL = new GameObject();
		gWheelFR = new GameObject();
		gWheelBL = new GameObject();
		gWheelBR = new GameObject();

		wheelFLHandle = new Model(PATH"/assets/delorean/WheelFL.p3dm", true);
		wheelFRHandle = new Model(PATH"/assets/delorean/WheelFR.p3dm", true);
		wheelBLHandle = new Model(PATH"/assets/delorean/WheelBL.p3dm", true);
		wheelBRHandle = new Model(PATH"/assets/delorean/WheelBR.p3dm", true);

		rWheelFL = new RenderingComponent(wheelFLHandle, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
		rWheelFR = new RenderingComponent(wheelFRHandle, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
		rWheelBL = new RenderingComponent(wheelBLHandle, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);
		rWheelBR = new RenderingComponent(wheelBRHandle, ShaderUsage::DirectionalShadow | ShaderUsage::Diffuse);

		gWheelFL->Add(rWheelFL);
		gWheelFR->Add(rWheelFR);
		gWheelBL->Add(rWheelBL);
		gWheelBR->Add(rWheelBR);

		gWheelFL->SetScale(Vec3(0.7f, 0.7f, 0.7f));
		gWheelFR->SetScale(Vec3(0.7f, 0.7f, 0.7f));
		gWheelBL->SetScale(Vec3(0.7f, 0.7f, 0.7f));
		gWheelBR->SetScale(Vec3(0.7f, 0.7f, 0.7f));

		Scene->Add(gWheelFL);
		Scene->Add(gWheelFR);
		Scene->Add(gWheelBL);
		Scene->Add(gWheelBR);
	}

	Scene->Add(FollowCamera);
	Scene->Add(HoodCamera);

	HideMouse();

	timeInterval = 0;

	sound = new Sound();
	sound->LoadFromFile(PATH"/assets/sounds/delorean_sound.ogg");
	sound->Play(true);

	crash = new Sound();
	crash->LoadFromFile(PATH"/assets/sounds/crash_sound.ogg");

	// Set Portals
	planeHandle = new Cube(40.f, 10.f, .1f);
	{
		addPortal(Vec3(234.6f, 0.f, -64.15f), Vec3(0.f, -0.086f, 0.f));
		addPortal(Vec3(237.8f, 0.f, -272.01f), Vec3(0.f, 0.753f, 0.f));
		addPortal(Vec3(151.16f, 0.f, -296.01f), Vec3(-3.142f, 1.145f, -3.142f));
		addPortal(Vec3(148.2f, 0.f, -225.08f), Vec3(3.142f, 0.171f, 3.142f));
		addPortal(Vec3(114.7f, 0.f, -37.11f), Vec3(3.142f, 0.171f, 3.142f));
		addPortal(Vec3(100.89f, 0.f, 5.107f), Vec3(3.142f, 1.025f, 3.142f));
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
		addPortal(Vec3(188.58f, 0.f, 223.95f), Vec3(0.f, -1.435f, 0.f));
		addPortal(Vec3(226.15f, 0.f, 177.9f), Vec3(0.f, 0.056f, 0.f));
		addPortal(Vec3(220.94f, 0.f, 143.f), Vec3(0.f, 0.586f, 0.f));
		addPortal(Vec3(65.47f, 0.f, 122.25f), Vec3(0.f, 0.841f, 0.f));
		addPortal(Vec3(41.711f, 0.f, 86.73f), Vec3(0.f, -0.156f, 0.f));
		addPortal(Vec3(88.282f, 0.f, 58.96f), Vec3(0.f, -1.329f, 0.f));
		addPortal(Vec3(186.62f, 0.f, 93.596f), Vec3(0.f, -1.329f, 0.f));
		addPortal(Vec3(215.28f, 0.f, 96.89f), Vec3(0.f, -0.783f, 0.f));
		addPortal(Vec3(226.33f, 0.f, 35.92f), Vec3(0.f, 0.f, 0.f));


		for (int i = 0; i < portals.size(); i++)
		{
			int k = i + 1;
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
	io.Fonts->AddFontFromFileTTF(PATH"/assets/fonts/BEBAS___.ttf", 16, &config);

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
	btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();

	float dt = GetTimeInterval();

	float speed = dt * 20.f;

	if (raceStart && !raceFinished)
	{
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
				if (gVehicleSteering < 0.f) gVehicleSteering = 0.f;
				else {
					gVehicleSteering += steeringIncrement * dt * 1000 / (fabs(m_vehicle->getCurrentSpeedKmHour())*.5f);
					if (gVehicleSteering > carPhysics->GetSteeringClamp()) gVehicleSteering = carPhysics->GetSteeringClamp();
				}
			}
		}
		if (_rightPressed)
		{
			if (gVehicleSteering > -carPhysics->GetSteeringClamp())
			{
				if (gVehicleSteering > 0.f) gVehicleSteering = 0.f;
				else {
					gVehicleSteering -= steeringIncrement * dt * 1000 / (fabs(m_vehicle->getCurrentSpeedKmHour())*.5f);
					if (gVehicleSteering < -carPhysics->GetSteeringClamp()) gVehicleSteering = -carPhysics->GetSteeringClamp();
				}
			}
		}

		if (!_rightPressed && !_leftPressed)
		{
			if (gVehicleSteering != 0.0f)
			{
				gVehicleSteering = 0.0f;
			}
		}

		if (carPhysics->RigidBodyRegistered())
		{

		}
	}

	timeInterval += dt;
	while (timeInterval >= 1.f / 60.f)
	{
		physics->Update(1.f / 60.f, 10);
		timeInterval -= 1.f / 60.f;

		if (carPhysics->RigidBodyRegistered())
		{
			// Update Camera Position
			btTransform transf = m_vehicle->getChassisWorldTransform();

			Matrix m;
			m.Translate(transf.getOrigin().x(), transf.getOrigin().y(), transf.getOrigin().z());
			Quaternion q = Quaternion(transf.getRotation().w(), transf.getRotation().x(), transf.getRotation().y(), transf.getRotation().z());

			m.SetRotationFromEuler(q.GetEulerFromQuaternion());

			Vec3 CameraTargetPosition = m * Vec3(0.f, 3.f, -10.f);
			CameraPosition += (CameraTargetPosition - CameraPosition) * 0.1f;
			//CameraPosition = CameraTargetPosition;

			f32 gears[7] = { -4.3f, 0.f, 3.36f, 2.06f, 1.38f, 1.061f, 0.82f };
			f32 main_gear = 3.44f;
			f32 gear_mul = gears[num_gear + 1] * main_gear;
			f32 engine_speed = 7000.f;
			f32 max_speed_wheel = 505.f / gear_mul;

			f32 engine_max_power = 500.f * 2.f;
			f32 wheel_rotation_radians = m_vehicle->getWheelInfo(2).m_deltaRotation;
			f32 wheel_rpm = ((wheel_rotation_radians / (2.f * PI))*60.f) / (1 / 60.f);
			engine_rpm = wheel_rpm * gear_mul;

			if (raceStart && abs(num_gear) == 1 && engine_rpm<1000 && gas_pedal>0.f)
				engine_rpm = 1000.f;

			double a = -0.000000082;
			double b = 0.000571429;
			double c = 0;
			double force = (a * (engine_rpm*engine_rpm) + b * engine_rpm + c);

			f32 power = gas_pedal * engine_max_power * force;
			power *= gear_mul;

			if (num_gear == 0 && gas_pedal > 0.f)
			{
				if (engine_rpm_N < engine_speed)
				{
					engine_rpm_N += engine_speed * dt;
					engine_rpm = engine_rpm_N;
				}
				else
					engine_rpm = engine_speed;

			}
			if (num_gear == 0 && gas_pedal == 0.f && engine_rpm_N>0)
			{
				engine_rpm_N -= engine_speed * dt;
				engine_rpm = engine_rpm_N;
			}

			sound->SetPitch(0.5f + (engine_rpm / engine_speed));
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

			if (raceStart && !raceFinished)
			{
				m_vehicle->setSteeringValue(gVehicleSteering, 0);
				//m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 0);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 0);
				m_vehicle->setSteeringValue(gVehicleSteering, 1);
				//m_vehicle->applyEngineForce(carPhysics->GetEngineForce(), 1);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 1);
				m_vehicle->applyEngineForce(power, 2);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 2);
				m_vehicle->applyEngineForce(power, 3);
				m_vehicle->setBrake(carPhysics->GetBreakingForce(), 3);

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

					break;
				};
			}
			else if (raceFinished) {
				m_vehicle->getRigidBody()->setLinearVelocity(btVector3(0, 0, 0));
				m_vehicle->getRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
			}
			else
			{
				m_vehicle->getRigidBody()->clearForces();
			}
		}

		int numManifolds = physics->GetPhysicsWorld()->getDispatcher()->getNumManifolds();
		for (int i = 0; i < numManifolds; i++)
		{
			btPersistentManifold* contactManifold = physics->GetPhysicsWorld()->getDispatcher()->getManifoldByIndexInternal(i);
			const btCollisionObject* obA = contactManifold->getBody0();
			const btCollisionObject* obB = contactManifold->getBody1();

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
										raceStart = false;
										raceTime = GetTime() - raceInitTime;
										raceFinished = true;

										bestRaceTime = raceTime;
										bestLapTime = lapTime[0];
										for (int i = 1; i < lapLimit; i++)
										{
											if (bestLapTime > lapTime[i])
												bestLapTime = lapTime[i];
										}

										showSendScore = true;

									}
									if (lap<lapLimit) lap++;

									// Clean Passages
									for (std::vector<Portal>::iterator _p = portals.begin(); _p != portals.end(); _p++)
									{
										(*_p).portalPassage = 0;
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
	{
		int min;
		f32 sec;
		seconds_to_minutes(fastestRaces[0].score, &min, &sec);
		ImGui::Text("Best  Race  Time: %d\" %.3f - %s", min, sec, &fastestRaces[0].name[0]);
	}
	{
		int min;
		f32 sec;
		seconds_to_minutes(fastestLaps[0].score, &min, &sec);
		ImGui::Text("Best  Lap  Time: %d\" %.3f - %s", min, sec, &fastestLaps[0].name[0]);
	}
	ImGui::Text("-");

	{
		int min;
		f32 sec;
		seconds_to_minutes((raceStart ? GetTime() - raceInitTime : 0.000), &min, &sec);
		ImGui::Text("Race  Time:  %d\" %.3f", min, sec);
	}

	if (lapTime.size() > 0)
	{
		for (int i = 1; i <= lap; i++) {
			if (lap != i)
			{
				int min;
				f32 sec;
				seconds_to_minutes(lapTime[i - 1], &min, &sec);
				ImGui::Text("Lap  %d  Time:  %d\" %.3f", i, min, sec);
			}
			else
			{
				int min;
				f32 sec;
				seconds_to_minutes((raceStart ? GetTime() - lapInitTime : 0.000), &min, &sec);
				ImGui::Text("Lap  %d  Time:  %d\" %.3f", i, min, sec);
			}
		}
	}
	ImGui::Text("Lap  %d/%d", lap, lapLimit);
	ImGui::Text("\n\nKEYS:\n\ARROWS-Drive\nA-Gear Up\nZ-Gear Down\nC-Change Camera\nF-Toggle FullScreen\nR-Restart\nTab-Toggle Ranking");
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

	{
		bool rpm = true;
		ImGui::SetNextWindowPos(ImVec2(Width*.5f - 75, Height - 140));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0, 0.0, 0.0, 0.0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("RPM", &rpm, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		ImGui::Image((void*)RPMGui->GetBindID(), ImVec2(150, 150));
		ImGui::SameLine();
		btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();
		if (num_gear>0 && m_vehicle)
			ImGui::Text("%d Gear\n\n%.0f KM/H", num_gear, fabs(m_vehicle->getCurrentSpeedKmHour()));
		else if (m_vehicle)
			ImGui::Text("%s Gear\n\n%.0f KM/H", (num_gear == 0 ? "N" : "R"), fabs(m_vehicle->getCurrentSpeedKmHour()));
		ImGui::End();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	{
		bool rpm = true;
		ImVec2 size = ImVec2(135, 135);
		ImVec2 pos = ImVec2(Width*.5f - size.x * .5f, Height - size.y);
		Matrix rpmMat;
		rpmMat.Translate(size.x*.5f, 10, 0);

		f32 maxRPMAngle = PI + .675f;
		f32 minRPMAngle = -.675f;
		if (engine_rpm > 7000.f) engine_rpm = 7000.f;
		if (engine_rpm < 0.f) engine_rpm = 0.f;
		f32 RPMAngle = (engine_rpm / 7000.f) * (PI + 1.35f) - 1.35*.5f;
		rpmMat.RotationZ(RPMAngle);
		rpmMat.Translate(Vec3(pos.x + size.x*.5f + 5, pos.y + size.y*.5f + 10, 0.f));
		ImGui::SetNextWindowPos(pos);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0, 0.0, 0.0, 0.0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("RPMPointer", &rpm, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		ImDrawList * drawList = ImGui::GetWindowDrawList();
		ImVec2 a(-size.x *.5f, -10); // topLeft
		ImVec2 c(size.x *.5f, 10); // bottom right
		ImVec2 b(c.x, a.y); // topRight
		ImVec2 d(a.x, c.y); // bottomLeft // CW order
		ImVec2 uv_a(0, 0), uv_c(1, 1);
		ImVec2 uv_b(0, 1), uv_d(1, 0);

		Vec3 a1 = rpmMat * Vec3(a.x, a.y, 0);
		Vec3 b1 = rpmMat * Vec3(b.x, b.y, 0);
		Vec3 c1 = rpmMat * Vec3(c.x, c.y, 0);
		Vec3 d1 = rpmMat * Vec3(d.x, d.y, 0);

		a = ImVec2(a1.x, a1.y);
		b = ImVec2(b1.x, b1.y);
		c = ImVec2(c1.x, c1.y);
		d = ImVec2(d1.x, d1.y);

		ImGui::Dummy(size); // create space for it
		drawList->PushTextureID((void*)RPMPointer->GetBindID());
		drawList->PrimReserve(6, 4);
		drawList->PrimQuadUV(a, b, c, d, uv_a, uv_b, uv_c, uv_d, 0xFFFFFFFF);
		drawList->PopTextureID();
		ImGui::End();

		//ImGui::ShowMetricsWindow();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	if (showSendScore)
	{
		ImGui::SetNextWindowPos(ImVec2(Width*.5f - 300, 50));
		ImGui::SetNextWindowSize(ImVec2(600, 400));
		ImGui::Begin("Save Score", &showSendScore, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("Ranking:");

		ImGui::BeginChild("Fastest", ImVec2(0, 300), true);
		ImGui::Columns(2);
		ImGui::Text("Fastest Laps");

		bool haveBetterLapTime = false;
		bool haveBetterRaceTime = false;
		uint32 counter = 1;
		for (int i = 0; i < fastestLaps.size(); i++)
		{
			if (fastestLaps[i].score > bestLapTime && !haveBetterLapTime && raceFinished)
			{
				int min;
				f32 sec;
				seconds_to_minutes(bestLapTime, &min, &sec);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
				ImGui::Text("%d - YOUR TIME", counter++);
				ImGui::SameLine();
				ImGui::Text("%d\" %.3f", min, sec);
				ImGui::PopStyleColor();
				haveBetterLapTime = true;
				i++;
			}

			if (fastestLaps[(haveBetterLapTime ? (i - 1) : i)].name.size() > 0)
			{
				int min;
				f32 sec;
				seconds_to_minutes(fastestLaps[(haveBetterLapTime ? (i - 1) : i)].score, &min, &sec);
				ImGui::Text("%d - %s", counter++, &fastestLaps[(haveBetterLapTime ? (i - 1) : i)].name[0]);
				ImGui::SameLine();
				ImGui::Text("%d\" %.3f", min, sec);
			}
		}
		if (!haveBetterLapTime && fastestLaps.size() < 10 && raceFinished)
		{
			int min;
			f32 sec;
			seconds_to_minutes(bestLapTime, &min, &sec);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
			ImGui::Text("%d - YOUR TIME", fastestLaps.size()+1);
			ImGui::SameLine();
			ImGui::Text("%d\" %.3f", min, sec);
			ImGui::PopStyleColor();
			haveBetterLapTime = true;
		}
		ImGui::NextColumn();
		ImGui::Text("Fastest Races");
		counter = 1;
		for (int i = 0; i < fastestRaces.size(); i++)
		{
			if (fastestRaces[i].score > bestRaceTime && !haveBetterRaceTime && raceFinished)
			{
				int min;
				f32 sec;
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
				seconds_to_minutes(bestRaceTime, &min, &sec);
				ImGui::Text("%d - YOUR TIME", counter++);
				ImGui::SameLine();
				ImGui::Text("%d\" %.3f", min, sec);
				ImGui::PopStyleColor();
				haveBetterRaceTime = true;
				i++;
			}

			if (fastestRaces[(haveBetterRaceTime ? (i - 1) : i)].name.size() > 0)
			{
				int min;
				f32 sec;
				seconds_to_minutes(fastestRaces[(haveBetterRaceTime ? (i - 1) : i)].score, &min, &sec);
				ImGui::Text("%d - %s", counter++, &fastestRaces[(haveBetterRaceTime ? (i - 1) : i)].name[0]);
				ImGui::SameLine();
				ImGui::Text("%d\" %.3f", min, sec);
			}
		}

		if (!haveBetterRaceTime && fastestRaces.size() < 10 && raceFinished)
		{
			int min;
			f32 sec;
			seconds_to_minutes(bestRaceTime, &min, &sec);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
			ImGui::Text("%d - YOUR TIME", fastestRaces.size()+1);
			ImGui::SameLine();
			ImGui::Text("%d\" %.3f", min, sec);
			ImGui::PopStyleColor();
			haveBetterRaceTime = true;
		}
		ImGui::EndChild();

		if ((haveBetterLapTime || haveBetterRaceTime) && raceFinished)
		{
			name.resize(64);
			ImGui::InputText("Name", &name[0], 64);
			if (ImGui::Button("Save Score and Restart Game")) {
				SaveRaceOnlineScore();

				showSendScore = false;
				Event::Input::Info e;
				Reset(e);
			}
			ImGui::SameLine();
		}
		if (ImGui::Button("Restart Game")) {
			showSendScore = false;
			Event::Input::Info e;
			Reset(e);
		}
		ImGui::End();
	}

	ImGui::SFML::ImGui_ImplSFML_Render((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y, clear_color);

	// ######################### UI ###############################

	// Check our direction
	if (Car->GetDirection().dotProduct(portals[portalNumber].direction) < 0 && raceStart && portalNumber >= 0)
	{
		if (!startedDrivingLikeAGirl) {
			startedDrivingLikeAGirl = true;
			timeStartedDrivingLikeAGirl = GetTime();
		}
	}
	else startedDrivingLikeAGirl = false;

	// Semaphore
	f32 count = GetTime() - countDownInitTime;
	if (count >= 1 && rSemaphore->GetMeshes()[0]->Material != redSemaphore)
		rSemaphore->GetMeshes()[0]->Material = redSemaphore;
	if (count >= 2 && rSemaphore->GetMeshes()[1]->Material != redSemaphore)
		rSemaphore->GetMeshes()[1]->Material = redSemaphore;
	if (count >= 3 && rSemaphore->GetMeshes()[2]->Material != yellowSemaphore)
		rSemaphore->GetMeshes()[2]->Material = yellowSemaphore;
	if (count >= 4 && rSemaphore->GetMeshes()[3]->Material != greenSemaphore)
	{
		// Start Race
		raceStart = true;
		raceInitTime = GetTime();
		lapInitTime = GetTime();

		rSemaphore->GetMeshes()[3]->Material = greenSemaphore;
	}
}

void RacingGame::Shutdown()
{
	// All your Shutdown Code Here
	Car->Remove(rCar);
	Car->Remove(HoodCamera);
	Car->Remove(carPhysics);
	Track->Remove(rTrack);
	gSemaphore->Remove(rSemaphore);
	Light->Remove(dLight);
	Skybox->Remove(rSkybox);
	gWheelBL->Remove(rWheelBL);
	gWheelFL->Remove(rWheelFL);
	gWheelFR->Remove(rWheelFR);
	gWheelBR->Remove(rWheelBR);
	Scene->Remove(gWheelBL);
	Scene->Remove(gWheelBR);
	Scene->Remove(gWheelFR);
	Scene->Remove(gWheelFL);
	Scene->Remove(Skybox);
	Scene->Remove(FollowCamera);
	Scene->Remove(HoodCamera);
	Scene->Remove(gSemaphore);
	Scene->Remove(Light);
	Scene->Remove(gSemaphore);
	Scene->Remove(Track);
	Scene->Remove(Car);
	delete planeHandle;
	delete rSemaphore;
	delete gSemaphore;
	delete hSemaphore;
	delete redSemaphore;
	delete yellowSemaphore;
	delete greenSemaphore;
	delete startedDrivingLikeAGirlTexture;
	delete rCar;
	delete Car;
	delete carPhysics;
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
	delete brakingMat;
	delete gWheelBL;
	delete gWheelFL;
	delete gWheelBR;
	delete gWheelFR;
	delete rWheelBL;
	delete rWheelFL;
	delete rWheelFR;
	delete rWheelBR;
	delete wheelBLHandle;
	delete wheelBRHandle;
	delete wheelFLHandle;
	delete wheelFRHandle;
	delete skyboxHandle;
	delete SkyboxMaterial;
	delete rSkybox;
	delete Skybox;
	delete dLight;
	delete Light;
	delete dRenderer;
	delete physics;
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
	gas_pedal = 0.f;
}
void RacingGame::UpDown(Event::Input::Info e)
{
	_upPressed = true;
	gas_pedal = 1.f;
}
void RacingGame::DownUp(Event::Input::Info e)
{
	_brakePressed = false;
}
void RacingGame::DownDown(Event::Input::Info e)
{
	_brakePressed = true;
}
void RacingGame::SpaceUp(Event::Input::Info e)
{
	//_brakePressed = false;
}
void RacingGame::SpaceDown(Event::Input::Info e)
{
	//_brakePressed = true;
}
void RacingGame::GearUp(Event::Input::Info e)
{
	if (num_gear<5)
		num_gear++;
	engine_rpm_N = 0.f;
}
void RacingGame::GearDown(Event::Input::Info e)
{
	if (num_gear>-1)
		num_gear--;
	engine_rpm_N = 0.f;
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
void RacingGame::ShowRanking(Event::Input::Info e)
{
	showSendScore = !showSendScore;
}
void RacingGame::ToggleFS(Event::Input::Info e)
{
	dLight->DisableCastShadows();
	delete dRenderer;

	ToggleFullscreen();
	ImGui::SFML::ImGui_ImplSFML_Shutdown();
	ImGui::SFML::ImGui_ImplSFML_Init(&rview);

	ImFontConfig config;
	config.OversampleH = 3;
	config.OversampleV = 1;
	config.GlyphExtraSpacing.x = 1.0f;
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF(PATH"/assets/fonts/BEBAS___.ttf", 16, &config);

	dRenderer = new CubemapRenderer(1024, 1024);
	dLight->EnableCastShadows(2048, 2048, projection, 0.1f, 200.f, 2);
	dLight->SetShadowBias(3.1f, 9.0f);
	dLight->SetShadowPCFTexelSize(0.0001f);
}
void RacingGame::Reset(Event::Input::Info e)
{
	btRaycastVehicle* m_vehicle = (btRaycastVehicle*)carPhysics->GetRigidBodyPTR();
	m_vehicle->getRigidBody()->setLinearVelocity(btVector3(0, 0, 0));
	m_vehicle->getRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
	btTransform transf;
	Matrix m;
	m.Translate(Vec3(234.f, -0.44f, -59.f));
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
		HoodCamera->SetPosition(Vec3(0, 1.5f, 0.8f));
		HoodCamera->SetRotation(Vec3(DEGTORAD(5), DEGTORAD(180), 0));
	}

	startedDrivingLikeAGirl = false;
	raceStart = false;
	raceFinished = false;
	lap = 1;
	lapLimit = 3;
	lapTime.clear();
	lapTime.push_back(0);

	// Set Semaphore Colors
	rSemaphore->GetMeshes()[0]->Material = redSemaphoreDefault;
	rSemaphore->GetMeshes()[1]->Material = redSemaphoreDefault;
	rSemaphore->GetMeshes()[2]->Material = yellowSemaphoreDefault;
	rSemaphore->GetMeshes()[3]->Material = greenSemaphoreDefault;

	m_vehicle->applyEngineForce(0, 0);
	m_vehicle->applyEngineForce(0, 1);
	m_vehicle->applyEngineForce(0, 2);
	m_vehicle->applyEngineForce(0, 3);

	// Set countdown Time
	countDownInitTime = GetTime();
	showSendScore = false;

	num_gear = 0;
	gas_pedal = 0.f;
	engine_rpm_N = 0.f;
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