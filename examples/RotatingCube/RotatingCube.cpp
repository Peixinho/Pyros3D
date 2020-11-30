//============================================================================
// Name        : RotatingCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCube.h"

using namespace p3d;

RotatingCube::RotatingCube() : ClassName(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void RotatingCube::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	VRenderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);

	EffectManager->Resize(width, height);
	MotionBlur->Resize(width, height);
}

void RotatingCube::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetBackground(Vec4(0.1f, 0.1f, 0.1f, 1));
	VRenderer = new VelocityRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 10, 400));

	// Material
	material = new GenericShaderMaterial(ShaderUsage::Texture);
	texture = new Texture();
	texture->LoadTexture(STR(EXAMPLES_PATH)"/RotatingTexturedCube/assets/Texture.png", TextureType::Texture);
	material->SetColorMap(texture);

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Model(STR(EXAMPLES_PATH)"/RacingGame/assets/delorean/delorean.p3dm");
	//cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh);
	CubeObject->Add(rCube);
	CubeObject->SetScale(Vec3(30,30,30));

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);

	EffectManager = new PostEffectsManager(Width, Height);
	MotionBlur = new MotionBlurEffect(RTT::Color, VRenderer->GetTexture(),Width,Height);
	EffectManager->AddEffect(MotionBlur);

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &RotatingCube::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &RotatingCube::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &RotatingCube::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &RotatingCube::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &RotatingCube::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &RotatingCube::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &RotatingCube::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &RotatingCube::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &RotatingCube::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = false;
	HideMouse();
}

void RotatingCube::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3((f32)GetTime()*2, (f32)GetTime()*5, 0));
	CubeObject->SetPosition(Vec3(sinf(GetTime()*2)*100.f, cos(GetTime()*2)*100.f, CubeObject->GetPosition().z));

	// Render Velocity Map
	Renderer->PreRender(Camera, Scene);
	VRenderer->RenderVelocityMap(projection, Camera, Scene);

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->RenderScene(projection, Camera, Scene);
	EffectManager->EndCapture();

	((MotionBlurEffect*) MotionBlur)->SetCurrentFPS(fps.getFPS());
	((MotionBlurEffect*) MotionBlur)->SetTargetFPS(60);

	// Render Post Processing
	EffectManager->ProcessPostEffects(&projection);
	
	Vec3 finalPosition;
	Vec3 direction = Camera->GetDirection();
	float dt = (float)GetTimeInterval();
	float speed = dt * 200.f;
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
		finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}

	Camera->SetPosition(Camera->GetPosition() + finalPosition);
}

void RotatingCube::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Camera);

	CubeObject->Remove(rCube);

	// Delete
	delete cubeMesh;
	delete rCube;
	delete CubeObject;
	delete Camera;
	delete Renderer;
	delete VRenderer;
	delete EffectManager;
	delete Scene;
}

RotatingCube::~RotatingCube() {}

void RotatingCube::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void RotatingCube::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void RotatingCube::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void RotatingCube::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void RotatingCube::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void RotatingCube::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void RotatingCube::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void RotatingCube::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void RotatingCube::LookTo(Event::Input::Info e)
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
