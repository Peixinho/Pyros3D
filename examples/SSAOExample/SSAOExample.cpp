//============================================================================
// Name        : SSAOExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Example
//============================================================================

#include "SSAOExample.h"

using namespace p3d;

SSAOEffectFinal::SSAOEffectFinal(uint32 texture1, uint32 texture2, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
{
	// Set RTT
	UseRTT(texture1);
	UseRTT(texture2);

	// Create Fragment Shader
	FragmentShaderString =
		"uniform sampler2D uTex0, uTex1;\n"
		"varying vec2 vTexcoord;\n"
		"void main() {\n"
		"gl_FragColor.r = texture2D(uTex1, vTexcoord).r;\n"
		"gl_FragColor.g = texture2D(uTex1, vTexcoord).g;\n"
		"gl_FragColor.b = texture2D(uTex1, vTexcoord).b;\n"
		"gl_FragColor.a = 1.0;\n"
		"gl_FragColor = texture2D(uTex0, vTexcoord)*texture2D(uTex1, vTexcoord);\n"
		"}";

	CompileShaders();

}

SSAOExample::SSAOExample() : ClassName(1024, 768, "Pyros3D - Custom Material", WindowType::Close | WindowType::Resize)
{

}

void SSAOExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.01f, 50.f);
}

void SSAOExample::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetGlobalLight(Vec4(0.2, 0.2, 0.2, 0.2));
	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.01f, 50.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0.f,3.f,3.f));
	Camera->SetRotation(Vec3(DEGTORAD(-45.f),0.f,0.f));

	// Add Camera to Scene
	Scene->Add(Camera);

	// Teapots
	teapot = new Model("../examples/SSAOExample/assets/teapot.p3dm");
	for (uint32 j = 0; j < 10; j++)
	for (uint32 i = 0; i < 10; i++)
	{
		GameObject* g = new GameObject();
		RenderingComponent* r = new RenderingComponent(teapot, ShaderUsage::Diffuse);
		g->Add(r);
		g->SetPosition(Vec3(-5.f + (i * 1.f), 0.4f, -5 + (j * 1.f)));
		g->SetScale(Vec3(0.01f, 0.01f, 0.01f));
		g->SetRotation(Vec3(0.f, DEGTORAD(33.f), 0.f));
		gTeapots.push_back(g);
		rTeapots.push_back(r);
		Scene->Add(g);
	}
	
	// Floor
	floor = new Plane(10, 10);
	gFloor = new GameObject();
	gFloor->SetRotation(Vec3(DEGTORAD(-90), 0, 0));
	rFloor = new RenderingComponent(floor);
	gFloor->Add(rFloor);
	Scene->Add(gFloor);
	
	EffectManager = new PostEffectsManager(Width, Height);
	ssao = new SSAOEffect(RTT::Depth, Width, Height);
	EffectManager->AddEffect(ssao);
	EffectManager->AddEffect(new BlurSSAOEffect(RTT::LastRTT, Width, Height));
	EffectManager->AddEffect(new SSAOEffectFinal(RTT::Color, RTT::LastRTT, Width, Height));

	SetMousePosition((uint32)Width / 2, (uint32)Height / 2);
	mouseCenter = Vec2((uint32)Width / 2, (uint32)Height / 2);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0;
	counterY = -45;	
	
	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	Light->Add(dLight);
	Scene->Add(Light);

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &SSAOExample::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &SSAOExample::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &SSAOExample::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &SSAOExample::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &SSAOExample::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &SSAOExample::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &SSAOExample::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &SSAOExample::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &SSAOExample::LookTo);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = 0;
	HideMouse();
}

void SSAOExample::Update()
{

	Vec3 finalPosition;
	Vec3 direction = Camera->GetDirection();
	float dt = GetTimeInterval();
	float speed = dt * 2.0f;
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

	// Update Scene
	Scene->Update(GetTime());

	ssao->SetInverseViewMatrix(Camera->GetWorldTransformation());
	ssao->SetViewMatrix(Camera->GetWorldTransformation().Inverse());

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->PreRender(Camera, Scene);
	Renderer->RenderScene(projection, Camera, Scene);
	EffectManager->EndCapture();

	EffectManager->ProcessPostEffects(&projection);	

}

void SSAOExample::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(Camera);

	for (std::vector<RenderingComponent*>::iterator i = rTeapots.begin(); i!=rTeapots.end(); i++)
	{
		(*i)->GetOwner()->Remove(*i);
		delete (*i);
	}

	for (std::vector<GameObject*>::iterator i = gTeapots.begin(); i!=gTeapots.end(); i++)
	{
		Scene->Remove((*i));
		delete (*i);
	}
	delete teapot;

	Scene->Remove(gFloor);
	gFloor->Remove(rFloor);
	delete gFloor;
	delete rFloor;
	delete floor;


	delete Camera;
	delete Scene;
	delete Renderer;
	delete EffectManager;

}

SSAOExample::~SSAOExample() {}

void SSAOExample::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void SSAOExample::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void SSAOExample::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void SSAOExample::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void SSAOExample::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void SSAOExample::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void SSAOExample::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void SSAOExample::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void SSAOExample::LookTo(Event::Input::Info e)
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