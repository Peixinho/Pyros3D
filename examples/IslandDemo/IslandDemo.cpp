//============================================================================
// Name        : IslandDemo.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "IslandDemo.h"

using namespace p3d;

IslandDemo::IslandDemo() : ClassName(1024, 768, "Pyros3D - Island Demo", WindowType::Close) {}

void IslandDemo::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 10000.f);
}

void IslandDemo::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();
	SceneWater = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 10000.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 100, 200));

	CameraReflection = new GameObject();

	// Create Game Object
	gIsland = new GameObject();
	island = new Model("../examples/IslandDemo/assets/island.p3dm", true);
	rIsland = new RenderingComponent(island, ShaderUsage::Diffuse | ShaderUsage::ClipPlane);
	gIsland->Add(rIsland);
	// Add GameObject to Scene
	Scene->Add(gIsland);

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	Light->Add(dLight);
	Scene->Add(Light);

	// Add Camera to Scene
	Scene->Add(Camera);
	Scene->Add(CameraReflection);

	// Water
	gWater = new GameObject();
	water = new Plane(500, 500);
	matWater = new WaterMaterial("../examples/IslandDemo/assets/WaterShader.glsl");
	rWater = new RenderingComponent(water, matWater);
	gWater->Add(rWater);
	gWater->SetRotation(Vec3((f32)DEGTORAD(-90.f), 0.f, 0.f));
	gWater->SetPosition(Vec3(0.f, 6.8f, 0.f));
	SceneWater->Add(gWater);

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &IslandDemo::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &IslandDemo::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &IslandDemo::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &IslandDemo::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &IslandDemo::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &IslandDemo::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &IslandDemo::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &IslandDemo::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &IslandDemo::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = false;
	HideMouse();

	fboReflection = new FrameBuffer();
	reflectionTexture = new Texture();
	reflectionTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	fboReflection->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
	fboReflection->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, reflectionTexture);

	fboRefraction = new FrameBuffer();
	refractionTexture = new Texture();
	refractionTextureDepth = new Texture();
	refractionTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	refractionTextureDepth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	fboRefraction->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, refractionTextureDepth);
	fboRefraction->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, refractionTexture);

	int32 imgID = matWater->textures.size();
	matWater->textures.push_back(reflectionTexture);
	matWater->AddUniform(Uniform("uReflectionMap", Uniforms::DataType::Int, &imgID));
	imgID = matWater->textures.size();
	matWater->textures.push_back(refractionTextureDepth);
	matWater->AddUniform(Uniform("uRefractionMapDepth", Uniforms::DataType::Int, &imgID));
	imgID = matWater->textures.size();
	matWater->textures.push_back(refractionTexture);
	matWater->AddUniform(Uniform("uRefractionMap", Uniforms::DataType::Int, &imgID));

	normalMap = new Texture();
	normalMap->LoadTexture("../examples/IslandDemo/assets/normal.png");
	DUDVmap = new Texture();
	DUDVmap->LoadTexture("../examples/IslandDemo/assets/waterDUDV.png");

	imgID = matWater->textures.size();
	matWater->textures.push_back(normalMap);
	matWater->AddUniform(Uniform("uNormalmap", Uniforms::DataType::Int, &imgID));

	imgID = matWater->textures.size();
	matWater->textures.push_back(DUDVmap);
	matWater->AddUniform(Uniform("uDUDVmap", Uniforms::DataType::Int, &imgID));
}

void IslandDemo::Update()
{
	// Update - Game Loop

	float distance = 2 * (Camera->GetPosition().y - gWater->GetPosition().y);
	CameraReflection->SetPosition(Vec3(Camera->GetPosition().x, Camera->GetPosition().y - distance, Camera->GetPosition().z));
	CameraReflection->SetRotation(Vec3(-Camera->GetRotation().x, Camera->GetRotation().y, -Camera->GetRotation().z));

	// Update Scene
	Scene->Update(GetTime());
	SceneWater->Update(GetTime());

	fboReflection->Bind();
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, 1, 0, -gWater->GetPosition().y));
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->PreRender(CameraReflection, Scene);
	Renderer->RenderScene(projection, CameraReflection, Scene);
	Renderer->DisableClipPlane();
	fboReflection->UnBind();

	fboRefraction->Bind();
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, -1, 0, gWater->GetPosition().y));
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
	Renderer->DisableClipPlane();
	fboRefraction->UnBind();

	// Render Scene
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClearDepthBuffer();
	Renderer->RenderScene(projection, Camera, Scene);
	Renderer->ClearBufferBit(Buffer_Bit::None);
	Renderer->PreRender(Camera, SceneWater);
	Renderer->RenderScene(projection, Camera, SceneWater);

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
		finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}

	Camera->SetPosition(Camera->GetPosition() + finalPosition);
}

void IslandDemo::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(gIsland);
	Scene->Remove(Camera);

	gIsland->Remove(rIsland);

	// Delete
	delete island;
	delete rIsland;
	delete gIsland;
	delete Camera;
	delete Renderer;
	delete Scene;
}

IslandDemo::~IslandDemo() {}

void IslandDemo::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void IslandDemo::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void IslandDemo::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void IslandDemo::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void IslandDemo::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void IslandDemo::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void IslandDemo::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void IslandDemo::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void IslandDemo::LookTo(Event::Input::Info e)
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
