//============================================================================
// Name        : BaseExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping
//============================================================================

#include "BaseExample.h"

using namespace p3d;

BaseExample::BaseExample(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : ClassName(1024, 768, title, WindowType::Close | WindowType::Resize)
{

}

void BaseExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);
}

void BaseExample::Init()
{
	// Initialization
	// Scene
	Scene = new SceneGraph();

	// Create Camera
	FPSCamera = new GameObject();
	FPSCamera->SetPosition(Vec3(0, 0, 80));

	Scene->Add(FPSCamera);
	
	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &BaseExample::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &BaseExample::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &BaseExample::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &BaseExample::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &BaseExample::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &BaseExample::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &BaseExample::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &BaseExample::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &BaseExample::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = false;
	HideMouse();

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

}

void BaseExample::Update()
{
	Vec3 finalPosition;
	Vec3 direction = FPSCamera->GetDirection();
	float dt = (float)GetTime() - lastTime;
	lastTime = GetTime();
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
		finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}

	FPSCamera->SetPosition(FPSCamera->GetPosition() + finalPosition);
}

void BaseExample::Shutdown()
{
	delete FPSCamera;
	delete Scene;
}

BaseExample::~BaseExample() {}

void BaseExample::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void BaseExample::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void BaseExample::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void BaseExample::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void BaseExample::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void BaseExample::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void BaseExample::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void BaseExample::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void BaseExample::LookTo(Event::Input::Info e)
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
			FPSCamera->SetRotation((qY*qX).GetEulerFromQuaternion());
			SetMousePosition((int)(mouseCenter.x), (int)(mouseCenter.y));
			mouseLastPosition = mouseCenter;
		}
	}
}
