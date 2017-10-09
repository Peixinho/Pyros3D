//============================================================================
// Name        : RacingGame.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#include "RacingGame.h"

using namespace p3d;

RacingGame::RacingGame() : ClassName(1024, 768, "Pyros3D - Racing Game Example", WindowType::Close | WindowType::Resize)
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
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
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

	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button0, this, &RacingGame::UpDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button0, this, &RacingGame::UpUp);
	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button1, this, &RacingGame::DownDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button1, this, &RacingGame::DownUp);
	InputManager::AddJoypadEvent(Event::Type::OnPress, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button2, this, &RacingGame::SpaceDown);
	InputManager::AddJoypadEvent(Event::Type::OnRelease, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Button::Button2, this, &RacingGame::SpaceUp);
	InputManager::AddJoypadEvent(Event::Type::OnMove, Event::Input::Joypad::ID::Joypad0, Event::Input::Joypad::Axis::X, this, &RacingGame::AnalogicMove);

	dRenderer = new CubemapRenderer(1024, 1024);

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
	Car->SetPosition(Vec3(234.f, -0.25f, -59.f));
	Car->SetRotation(Vec3(3.14, 0.08, -3.14));

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
	}

	for (std::vector<RenderingMesh*>::iterator i = rTrack->GetMeshes().begin(); i != rTrack->GetMeshes().end(); i++)
	{
		GenericShaderMaterial* m = static_cast<GenericShaderMaterial*> ((*i)->Material);
		m->SetEnvMap(dRenderer->GetTexture());
		m->SetReflectivity(0.1f);
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
	{
		// Set Camera Initial Position
		Matrix m;
		m.Translate(Vec3(234.f, -0.25f, -59.f));
		m.SetRotationFromEuler(Vec3(3.14, 0.08, -3.14));
		Vec3 CameraTargetPosition = m * Vec3(0.f, 3.f, -10.f);
		FollowCamera->SetPosition(CameraTargetPosition);
		FollowCamera->LookAt(Car);

	}

	Scene->Add(HoodCamera);
	{
		// Set Camera Initial Position
		Car->Add(HoodCamera);
		HoodCamera->SetPosition(Vec3(0, 2.f, 0.6f));
		HoodCamera->SetRotation(Vec3(DEGTORAD(5), DEGTORAD(180), 0));
	}

	HideMouse();

	timeInterval = 0;

	sound = new Sound();
	sound->LoadFromFile("../examples/RacingGame/assets/delorean_sound.ogg");
	sound->Play(true);

	crash = new Sound();
	crash->LoadFromFile("../examples/RacingGame/assets/crash_sound.ogg");

	// Set Portals
	planeHandle = new Cube(20.f, 20.f, .1f);
	{
		addPortal(Vec3(234.6f,0.f,-64.15f), Vec3(0.f,-0.086,0.f));
		addPortal(Vec3(244.5f, 0.f, -222.68f), Vec3(0.f, -0.086, 0.f));
		addPortal(Vec3(147.59f, 0.f, -303.4f), Vec3(-3.142f, 0.873f, 3.142f));
		addPortal(Vec3(117.36f, 0.f, -42.01f), Vec3(0.f, 0.222f, 0.f));
		addPortal(Vec3(28.53f, 0.f, -46.57f), Vec3(0.f, 0.266f, 0.f));
		addPortal(Vec3(44.98f, 0.f, -177.8f), Vec3(0.f, -0.580f, 0.f));
		addPortal(Vec3(76.32f, 0.f, -219.8f), Vec3(0.f, -0.194f, 0.f));
		addPortal(Vec3(-243.6f, 0.f, -175.15f), Vec3(0.f, 0.0f, 0.f));
		addPortal(Vec3(-251.48f, 0.f, 213.75f), Vec3(0.f, 0.0f, 0.f));
		addPortal(Vec3(-89.5f, 0.f, 199.6f), Vec3(0.f, 0.6f, 0.f));
		addPortal(Vec3(-136.5f, 0.f, -6.77f), Vec3(0.f, -0.6f, 0.f));
		addPortal(Vec3(-30.f, 0.f, -26.f), Vec3(0.f, -0.f, 0.f));
		addPortal(Vec3(-24.755f, 0.f, 147.481f), Vec3(0.f, 0.f, 0.f));
		addPortal(Vec3(229.297f, 0.f, 176.55f), Vec3(0.f, 0.f, 0.f));
		addPortal(Vec3(56.336f, 0.f, 100.288f), Vec3(0.f, 0.626f, 0.f));
		addPortal(Vec3(67.107f, 0.f, 67.7f), Vec3(0.f, -1.127f, 0.f));
		addPortal(Vec3(222.34f, 0.f, 90.50f), Vec3(0.f, -0.294f, 0.f));
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
}

void RacingGame::addPortal(const Vec3 &pos, const Vec3 &rot)
{
	Portal portal;
	portal.gPortal = new GameObject();
	//portal.rPortal = new RenderingComponent(planeHandle);
	//portal.gPortal->Add(portal.rPortal);
	portal.pPortal = physics->CreateBox(20.f, 20.f, .1f, 0, true);
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
							if (whatTerrainAreWe!=TERRAIN::GRASS || whatTerrainAreWe!=TERRAIN::SAND)
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
										lapTime = GetTime() - lapInitTime;
										std::cout << lapTime << std::endl;

										// Clean Passages
										for (std::vector<Portal>::iterator _p = portals.begin(); _p != portals.end(); _p++)
										{
											(*_p).portalPassage = 0;
										}
									}
								}
							}
							
							{
								if (portalNumber == (portal+1) || ((portal == portals.size()-1) && portalNumber==0))
								{
									portals[portalNumber].portalPassage--;
								}
								else
									portals[portal].portalPassage++;
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

	// UPDATE WHEELS MANUALLY
	gWheelFL->SetTransformationMatrix(carPhysics->GetWheels()[0].Transformation);
	gWheelFR->SetTransformationMatrix(carPhysics->GetWheels()[1].Transformation);
	gWheelBL->SetTransformationMatrix(carPhysics->GetWheels()[2].Transformation);
	gWheelBR->SetTransformationMatrix(carPhysics->GetWheels()[3].Transformation);

	// Update Scene
	Scene->Update(GetTime());

	Skybox->SetPosition(Vec3(CameraPosition.x, 0, CameraPosition.y));

	rCar->Disable();
	dRenderer->RenderCubeMap(Scene, Car, 0.1f, 2000.f);
	rCar->Enable();

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
		for (int i = 0;i<portals.size(); i++)
			ImGui::Text("Portal: %d - passage: %d", i, portals[i].portalPassage);
		dLight->SetShadowBias(a, b);
		dLight->SetShadowPCFTexelSize(c);
	}

	ImGui::SFML::ImGui_ImplSFML_Render((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y, clear_color);

	// ######################### UI ###############################
}

void RacingGame::Shutdown()
{
	// All your Shutdown Code Here
	delete planeHandle;

	Scene->Remove(Track);
	Scene->Remove(Car);
	Track->Remove(rTrack);
	Car->Remove(rCar);
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