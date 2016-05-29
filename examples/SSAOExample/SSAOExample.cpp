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
		"gl_FragColor.rgb =  texture2D(uTex0, vTexcoord).rgb * texture2D(uTex1, vTexcoord).rgb;\n"
		"gl_FragColor.w = 1.0;\n"
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
	projection.Perspective(70.f, (f32)width / (f32)height, 0.01f, 500.f);
}

void SSAOExample::Init()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.01f, 500.f);

	// Create Camera
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0, 0, 0.1));

	// Add Camera to Scene
	Scene->Add(Camera);

	// Create Geometry
	cubeHandle = new Cube(1, 0.5, 0.25);
	Renderable* sphereHandle = new Sphere(0.05f, 10, 10,true);
	GenericShaderMaterial* mat2 = new GenericShaderMaterial(ShaderUsage::Color);
	mat2->SetCullFace(CullFace::DoubleSided);
	mat2->SetColor(Vec4(0.5, 0.5, 0.5, 1.0));
	
	EffectManager = new PostEffectsManager(Width, Height);
	ssao = new SSAOEffect(RTT::Depth, Width, Height);
	EffectManager->AddEffect(ssao);
	EffectManager->AddEffect(new ResizeEffect(RTT::LastRTT, Width*.25f, Height*.25f));
	EffectManager->AddEffect(new BlurXEffect(RTT::LastRTT, Width*0.25f, Height*.25f));
	EffectManager->AddEffect(new BlurYEffect(RTT::LastRTT, Width*0.25f, Height*0.25f));
	EffectManager->AddEffect(new SSAOEffectFinal(RTT::Color, RTT::LastRTT, Width, Height));

	GameObject* go1 = new GameObject();
	GameObject* go2 = new GameObject();

	GenericShaderMaterial* SelectedMaterial = new GenericShaderMaterial(ShaderUsage::Color);
	SelectedMaterial->SetColor(Vec4(1, 1, 0, 1));

	RenderingComponent* rcomp1 = new RenderingComponent(cubeHandle, mat2);
	RenderingComponent* rcomp2 = new RenderingComponent(sphereHandle, SelectedMaterial);
	go1->Add(rcomp1);
	go2->Add(rcomp2);

	Scene->Add(go1);
	Scene->Add(go2);

	SetMousePosition((uint32)Width / 2, (uint32)Height / 2);
	mouseCenter = Vec2((uint32)Width / 2, (uint32)Height / 2);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0;

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
	static int last_time; int curr_time = GetTimeMicroSeconds();

	ssao->SetViewMatrix(Camera->GetWorldTransformation());

	// Update Scene
	Scene->Update(GetTime());
	
	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->RenderScene(projection, Camera, Scene);
	EffectManager->EndCapture();

	EffectManager->ProcessPostEffects(&projection);

	Vec3 finalPosition;
	Vec3 direction = Camera->GetDirection();
	float dt = GetTimeInterval();
	float speed = dt * 0.02f;
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

void SSAOExample::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(Camera);

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