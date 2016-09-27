//============================================================================
// Name        : RotatingCubeWithLightingAndShadow.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingCubeWithLightingAndShadow.h"

using namespace p3d;

RotatingCubeWithLightingAndShadow::RotatingCubeWithLightingAndShadow() : ClassName(1024, 768, "Pyros3D - Rotating Cube With Lighting And Shadow", WindowType::Close | WindowType::Resize)
{

}

void RotatingCubeWithLightingAndShadow::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 200.f);

}

void RotatingCubeWithLightingAndShadow::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 2000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 10, 100));

	// Material
	Diffuse = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::SpecularColor | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow);
	Diffuse->SetColor(Vec4(1, 0, 0, 1));
	Diffuse->SetPCFTexelSize(0.0001f);
	Diffuse->SetSpecular(Vec4(1, 1, 1, 1));

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	// Enable Shadow Casting
	dLight->EnableCastShadows(2048, 2048, projection, 0.1f, (f32)300.0, 1);
	dLight->SetShadowBias(5.f, 3.f);
	Light->Add(dLight);

	Light2 = new GameObject();
	//pLight = new SpotLight(Vec4(1, 1, 1, 1), 300, Vec3(0, -1, 0), 45, 30);
	pLight = new PointLight(Vec4(1, 1, 1, 1), 100);
	pLight->EnableCastShadows(256, 256);
	pLight->SetShadowBias(5.f, 3.f);

	Light2->Add(pLight);
	Scene->Add(Light2);
	Light2->SetPosition(Vec3(0, 20, 0));

	// Add Light to Scene
	Scene->Add(Light);

	// Create Floor Material
	FloorMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow);
	FloorMaterial->SetColor(Vec4(1, 1, 1, 1));
	FloorMaterial->SetPCFTexelCascadesSize(0.0001f);

	// Create Floor
	Floor = new GameObject();
	floorMesh = new Cube(100, 10, 100);
	Ceiling = new GameObject();


	// Create Floor Rendering Component
	rFloor = new RenderingComponent(floorMesh, FloorMaterial);
	rCeiling = new RenderingComponent(floorMesh, FloorMaterial);

	// Add Rendering Component To GameObject
	Floor->Add(rFloor);
	Ceiling->Add(rCeiling);
	Ceiling->SetScale(Vec3(0.5, 0.5, 0.5));

	// Add Floor to Scene
	Scene->Add(Floor);
	Scene->Add(Ceiling);

	// Rotate Floor

	// Translate Floor
	Floor->SetPosition(Vec3(0, -30, -20));
	Ceiling->SetPosition(Vec3(0, 30, -20));

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(5, 5, 5);
	rCube = new RenderingComponent(cubeMesh, Diffuse);
	CubeObject->Add(rCube);

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (uint32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &RotatingCubeWithLightingAndShadow::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &RotatingCubeWithLightingAndShadow::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &RotatingCubeWithLightingAndShadow::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &RotatingCubeWithLightingAndShadow::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &RotatingCubeWithLightingAndShadow::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &RotatingCubeWithLightingAndShadow::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &RotatingCubeWithLightingAndShadow::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &RotatingCubeWithLightingAndShadow::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &RotatingCubeWithLightingAndShadow::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
	HideMouse();

}

void RotatingCubeWithLightingAndShadow::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime(), 0));

	// Render Scene
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);

	Vec3 finalPosition;
	Vec3 direction = Camera->GetDirection();
	float dt = (float)GetTimeInterval();
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
		finalPosition += direction.cross(Vec3(0, 1, 0))*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0))*speed;
	}

	Camera->SetPosition(Camera->GetPosition() + finalPosition);
}

void RotatingCubeWithLightingAndShadow::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Camera);
	Scene->Remove(Light);
	Scene->Remove(Floor);

	CubeObject->Remove(rCube);
	Light->Remove(dLight);
	Floor->Remove(rFloor);

	// Delete
	delete rCube;
	delete CubeObject;
	delete rFloor;
	delete Floor;
	delete cubeMesh;
	delete floorMesh;
	delete dLight;
	delete Light;
	delete Diffuse;
	delete FloorMaterial;
	delete Camera;
	delete Renderer;
	delete Scene;
}

RotatingCubeWithLightingAndShadow::~RotatingCubeWithLightingAndShadow() {}

void RotatingCubeWithLightingAndShadow::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void RotatingCubeWithLightingAndShadow::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void RotatingCubeWithLightingAndShadow::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void RotatingCubeWithLightingAndShadow::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void RotatingCubeWithLightingAndShadow::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void RotatingCubeWithLightingAndShadow::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void RotatingCubeWithLightingAndShadow::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void RotatingCubeWithLightingAndShadow::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void RotatingCubeWithLightingAndShadow::LookTo(Event::Input::Info e)
{
	if (mouseCenter != GetMousePosition())
	{
		mousePosition = InputManager::GetMousePosition();
		Vec2 mouseDelta = (mousePosition - mouseLastPosition);
		if (mouseDelta.x != 0 || mouseDelta.y != 0)
		{
			counterX -= mouseDelta.x / 10.f;
			counterY -= mouseDelta.y / 10.f;
			if (counterY < -90.f) counterY = -90.f;
			if (counterY > 90.f) counterY = 90.f;
			Quaternion qX, qY;
			qX.AxisToQuaternion(Vec3(1.f, 0.f, 0.f), (f32)DEGTORAD(counterY));
			qY.AxisToQuaternion(Vec3(0.f, 1.f, 0.f), (f32)DEGTORAD(counterX));
			//                Matrix rotX, rotY;
			//                rotX.RotationX(DEGTORAD(counterY));
			//                rotY.RotationY(DEGTORAD(counterX));
			Camera->SetRotation((qY*qX).GetEulerFromQuaternion());
			SetMousePosition((int)(mouseCenter.x), (int)(mouseCenter.y));
			mouseLastPosition = mouseCenter;
		}
	}
}