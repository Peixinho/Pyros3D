//============================================================================
// Name        : ParallaxMapping.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping
//============================================================================

#include "ParallaxMapping.h"

using namespace p3d;

ParallaxMapping::ParallaxMapping() : ClassName(1024, 768, "Pyros3D - Parallax Mapping", WindowType::Close | WindowType::Resize)
{

}

void ParallaxMapping::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.001f, 1000.f);
}

void ParallaxMapping::Init()
{
	// Initialization

		// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.001f, 1000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 10, 80));
	
	texturemap = new Texture();
	normalmap = new Texture();
	displacementmap = new Texture();

	texturemap->LoadTexture("../examples/ParallaxMapping/assets/bricks.jpg");
	normalmap->LoadTexture("../examples/ParallaxMapping/assets/bricks_normal.jpg");
	displacementmap->LoadTexture("../examples/ParallaxMapping/assets/bricks_disp.jpg");

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(30, 30, 30, false, false, true);
	rCube = new RenderingComponent(cubeMesh, ShaderUsage::Diffuse | ShaderUsage::ParallaxMapping | ShaderUsage::BumpMapping | ShaderUsage::Texture | ShaderUsage::SpecularColor);
	CubeObject->Add(rCube);
	GenericShaderMaterial* mat = (GenericShaderMaterial*)rCube->GetMeshes()[0]->Material;
	
	mat->SetColorMap(texturemap); 
	mat->SetNormalMap(normalmap);
	mat->SetDisplacementMap(displacementmap);
	mat->SetSpecular(Vec4(1, 1, 1, 1));
	mat->SetShininess(32);

	// Add Camera to Scene
	Scene->Add(Camera);
	// Add GameObject to Scene
	Scene->Add(CubeObject);

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);

	Scene->Add(Light);

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &ParallaxMapping::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &ParallaxMapping::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &ParallaxMapping::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &ParallaxMapping::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &ParallaxMapping::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &ParallaxMapping::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &ParallaxMapping::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &ParallaxMapping::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &ParallaxMapping::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = false;
	HideMouse();

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

}

void ParallaxMapping::Update()
{
	// Update - Game Loop

		// Update Scene
	Scene->Update(GetTime());

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

	// Game Logic Here
	//CubeObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));

	// Render Scene
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
}

void ParallaxMapping::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(Camera);

	CubeObject->Remove(rCube);

	// Delete
	delete rCube;
	delete CubeObject;
	delete cubeMesh;
	delete texturemap;
	delete normalmap;
	delete displacementmap;
	delete Camera;
	delete Renderer;
	delete Scene;
}

ParallaxMapping::~ParallaxMapping() {}

void ParallaxMapping::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void ParallaxMapping::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void ParallaxMapping::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void ParallaxMapping::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void ParallaxMapping::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void ParallaxMapping::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void ParallaxMapping::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void ParallaxMapping::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void ParallaxMapping::LookTo(Event::Input::Info e)
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