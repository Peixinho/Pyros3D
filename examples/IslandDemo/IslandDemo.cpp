//============================================================================
// Name        : IslandDemo.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "IslandDemo.h"

using namespace p3d;

IslandDemo::IslandDemo() : ClassName(1024, 768, "Pyros3D - Island Demo", WindowType::Close | WindowType::Resize) {}

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
	island = new Model("../../../../examples/IslandDemo/assets/island.p3dm", true, ShaderUsage::Diffuse | ShaderUsage::ClipPlane);
	rIsland = new RenderingComponent(island);
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
	matWater = new WaterMaterial("../../../../examples/IslandDemo/assets/WaterShader.glsl");
	rWater = new RenderingComponent(water, matWater);
	gWater->Add(rWater);
	gWater->SetRotation(Vec3(DEGTORAD(-90), 0, 0));
	gWater->SetPosition(Vec3(0, 6.8, 0));
	SceneWater->Add(gWater);

	SetMousePosition((uint32)Width / 2, (uint32)Height / 2);
	mouseCenter = Vec2((uint32)Width / 2, (uint32)Height / 2);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0;

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

	_strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
	HideMouse();

	fboReflection = new FrameBuffer();
	reflectionTexture = new Texture();
	reflectionTexture->CreateTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	fboReflection->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
	fboReflection->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, reflectionTexture);

	fboRefraction = new FrameBuffer();
	refractionTexture = new Texture();
	refractionTextureDepth = new Texture();
	refractionTexture->CreateTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	refractionTextureDepth->CreateTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	fboRefraction->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, refractionTextureDepth);
	fboRefraction->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, refractionTexture);

	int32 imgID = matWater->textures.size();
	matWater->textures.push_back(reflectionTexture);
	matWater->AddUniform(Uniforms::Uniform("uReflectionMap", DataType::Int, &imgID));
	imgID = matWater->textures.size();
	matWater->textures.push_back(refractionTextureDepth);
	matWater->AddUniform(Uniforms::Uniform("uRefractionMapDepth", DataType::Int, &imgID));
	imgID = matWater->textures.size();
	matWater->textures.push_back(refractionTexture);
	matWater->AddUniform(Uniforms::Uniform("uRefractionMap", DataType::Int, &imgID));

	normalMap = new Texture();
	normalMap->LoadTexture("../../../../examples/IslandDemo/assets/normal.png");
	DUDVmap = new Texture();
	DUDVmap->LoadTexture("../../../../examples/IslandDemo/assets/waterDUDV.png");

	imgID = matWater->textures.size();
	matWater->textures.push_back(normalMap);
	matWater->AddUniform(Uniforms::Uniform("uNormalmap", DataType::Int, &imgID));

	imgID = matWater->textures.size();
	matWater->textures.push_back(DUDVmap);
	matWater->AddUniform(Uniforms::Uniform("uDUDVmap", DataType::Int, &imgID));
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
	Renderer->RenderScene(projection, CameraReflection, Scene);
	Renderer->DisableClipPlane();
	fboReflection->UnBind();

	fboRefraction->Bind();
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableClipPlane();
	Renderer->SetClipPlane0(Vec4(0, -1, 0, gWater->GetPosition().y));
	Renderer->RenderScene(projection, Camera, Scene);
	Renderer->DisableClipPlane();
	fboRefraction->UnBind();

	// Render Scene
	Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
	Renderer->EnableDepthTest();
	Renderer->EnableDepthWritting();
	Renderer->EnableClearDepthBuffer();
	Renderer->RenderScene(projection, Camera, Scene);
	Renderer->ClearBufferBit(Buffer_Bit::None);
	Renderer->RenderScene(projection, Camera, SceneWater);

	Vec3 finalPosition;
	Vec3 direction = Camera->GetDirection();
	float dt = GetTimeInterval();
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
			if (counterY<-90.f) counterY = -90.f;
			if (counterY>90.f) counterY = 90.f;
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