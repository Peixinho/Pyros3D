//============================================================================
// Name        : ParticlesExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "ParticlesExample.h"

using namespace p3d;

ParticlesExample::ParticlesExample() : ClassName(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void ParticlesExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);
}

void ParticlesExample::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 10, 80));
	Scene->Add(Camera);

	particleMaterial = new ParticleMaterial();
	
	// Create Game Object
	gSmoke1 = new GameObject();
	gSmoke2 = new GameObject();
	smokeParticle1 = new Plane(1,1);
	smokeParticle2 = new Plane(1,1);
	
	// Create Particle Emitter
	emitter1 = new ParticleEmitter(smokeParticle1, particleMaterial, 0, 100);
	emitter2 = new ParticleEmitter(smokeParticle2, particleMaterial, 0, 100);

	gSmoke1->Add(emitter1);
	gSmoke1->SetPosition(Vec3(-10,0,0));
	Scene->Add(gSmoke1);

	gSmoke2->Add(emitter2);
	gSmoke2->SetPosition(Vec3(10,0,0));
	Scene->Add(gSmoke2);

	SetMousePosition((uint32)Width / 2, (uint32)Height / 2);
	mouseCenter = Vec2((uint32)Width / 2, (uint32)Height / 2);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0;
	counterY = -45;

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &ParticlesExample::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &ParticlesExample::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &ParticlesExample::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &ParticlesExample::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &ParticlesExample::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &ParticlesExample::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &ParticlesExample::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &ParticlesExample::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &ParticlesExample::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
	HideMouse();
	lastTime = GetTime();
}

void ParticlesExample::Update()
{
	// Update - Game Loop

	Vec3 finalPosition;
	Vec3 direction = Camera->GetDirection();
	float dt = GetTimeInterval();
	float speed = dt * 200.0f;
	if (_moveFront)
	{
		finalPosition -= direction * speed;
	}
	if (_moveBack)
	{
		finalPosition += direction * speed;
	}
	if (_strafeLeft)
	{
		finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}

	Camera->SetPosition(Camera->GetPosition() + finalPosition);

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic HereW
	
	f32 time = GetTime();
	if (time-lastTime > 0.1)
	{
		lastTime = time;
		{
			Particle p;
			p.Position = Vec4(0,0,0,(rand()%20-10.f)*0.1f);
			p.Details = Vec4((rand()%40-20)*0.01f,3.f,(rand()%40-20)*0.01f,time);
			emitter1->particles.push_back(p);
		}
		{
			Particle p;
			p.Position = Vec4(0,0,0,(rand()%20-10.f)*0.1f);
			p.Details = Vec4((rand()%40-20)*0.01f,3.f,(rand()%40-20)*0.01f,time);
			emitter2->particles.push_back(p);
		}
	}

	for (std::vector<Particle>::iterator i=emitter1->particles.begin();i!=emitter1->particles.end();)
	{
		Particle* p = &(*i);
		if (time-p->Details.w>10.0) {
			i=emitter1->particles.erase(i);
		} else {
			p->Position.x += p->Details.x * 0.005f;
			p->Position.y += p->Details.y * 0.005f;
			p->Position.z += p->Details.z * 0.005f;
			i++;
		}
	}

	for (std::vector<Particle>::iterator i=emitter2->particles.begin();i!=emitter2->particles.end();)
	{
		Particle* p = &(*i);
		if (time-p->Details.w>10.0) {
			i=emitter2->particles.erase(i);
		} else {
			p->Position.x += p->Details.x * 0.005f;
			p->Position.y += p->Details.y * 0.005f;
			p->Position.z += p->Details.z * 0.005f;
			i++;
		}
	}

	if (emitter1->particles.size() > 0)
	{
		emitter1->SetNumberInstances(emitter1->particles.size());
		emitter1->particles_position_buffer->Buffer->Update(&emitter1->particles[0], emitter1->particles.size() * sizeof(Particle));
	}
	if (emitter2->particles.size() > 0)
	{
		emitter2->SetNumberInstances(emitter2->particles.size());
		emitter2->particles_position_buffer->Buffer->Update(&emitter2->particles[0], emitter2->particles.size() * sizeof(Particle));
	}

	// Render Scene
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
}

void ParticlesExample::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	gSmoke1->Remove(emitter1);
	Scene->Remove(gSmoke1);
	gSmoke2->Remove(emitter2);
	Scene->Remove(gSmoke2);

	Scene->Remove(Camera);

	// Delete
	delete emitter1;
	delete emitter2;
	delete particleMaterial;
	delete gSmoke1;
	delete gSmoke2;
	delete smokeParticle1;
	delete smokeParticle2;
	delete Camera;
	delete Renderer;
	delete Scene;
}

ParticlesExample::~ParticlesExample() {}

void ParticlesExample::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void ParticlesExample::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void ParticlesExample::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void ParticlesExample::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void ParticlesExample::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void ParticlesExample::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void ParticlesExample::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void ParticlesExample::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void ParticlesExample::LookTo(Event::Input::Info e)
{
	if (mouseCenter != GetMousePosition())
	{
		mousePosition = InputManager::GetMousePosition();
		Vec2 mouseDelta = (mousePosition - mouseLastPosition);
		if (mouseDelta.x != 0 || mouseDelta.y != 0)
		{
			counterX -= mouseDelta.x / 10.f;
			counterY -= mouseDelta.y / 10.f;
			if (counterY<-80.f) counterY = -80.f;
			if (counterY>80.f) counterY = 80.f;
			Quaternion qX, qY;
			qX.AxisToQuaternion(Vec3(1.f, 0.f, 0.f), DEGTORAD(counterY));
			qY.AxisToQuaternion(Vec3(0.f, 1.f, 0.f), DEGTORAD(counterX));
			//                Matrix rotX, rotY;
			//                rotX.RotationX(DEGTORAD(counterY));
			//                rotY.RotationY(DEGTORAD(counterX));
			Camera->SetRotation((qY*qX).GetEulerFromQuaternion());
			SetMousePosition((int)(mouseCenter.x), (int)(mouseCenter.y));
			mouseLastPosition = mouseCenter;
		}
	}
}
