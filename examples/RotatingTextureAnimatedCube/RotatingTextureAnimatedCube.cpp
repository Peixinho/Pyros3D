//============================================================================
// Name        : RotatingTextureAnimatedCube.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "RotatingTextureAnimatedCube.h"

using namespace p3d;

RotatingTextureAnimatedCube::RotatingTextureAnimatedCube() : BaseExample(1024, 768, "Pyros3D - Rotating Textured Cube", WindowType::Close | WindowType::Resize)
{

}

void RotatingTextureAnimatedCube::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 100.f);
}

void RotatingTextureAnimatedCube::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 100.f);

	FPSCamera->SetPosition(Vec3(0, 10, 80));

	// Material
	material = new GenericShaderMaterial(ShaderUsage::Texture);

	tex0 = new Texture(); tex0->LoadTexture(STR(EXAMPLES_PATH)"/assets/textureanimation/1.png", TextureType::Texture);
	tex1 = new Texture(); tex1->LoadTexture(STR(EXAMPLES_PATH)"/assets/textureanimation/2.png", TextureType::Texture);
	tex2 = new Texture(); tex2->LoadTexture(STR(EXAMPLES_PATH)"/assets/textureanimation/3.png", TextureType::Texture);
	tex3 = new Texture(); tex3->LoadTexture(STR(EXAMPLES_PATH)"/assets/textureanimation/4.png", TextureType::Texture);
	tex4 = new Texture(); tex4->LoadTexture(STR(EXAMPLES_PATH)"/assets/textureanimation/5.png", TextureType::Texture);
	tex5 = new Texture(); tex5->LoadTexture(STR(EXAMPLES_PATH)"/assets/textureanimation/6.png", TextureType::Texture);
	anim = new TextureAnimation();
	anim->AddFrame(tex0);
	anim->AddFrame(tex1);
	anim->AddFrame(tex2);
	anim->AddFrame(tex3);
	anim->AddFrame(tex4);
	anim->AddFrame(tex5);
	// Create Animation Instance with 30fps
	animInst = anim->CreateInstance(30);
	// Set Animation Loop backwards
	animInst->YoYo(true);
	// Play It
	animInst->Play(0); // 0 = Loop

	// Create Game Object
	CubeObject = new GameObject();
	cubeMesh = new Cube(15, 15, 15);
	rCube = new RenderingComponent(cubeMesh, material);
	CubeObject->Add(rCube);

	// Add GameObject to Scene
	Scene->Add(CubeObject);

	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &RotatingTextureAnimatedCube::OnMousePress);
}

void RotatingTextureAnimatedCube::OnMousePress(Event::Input::Info e)
{
	animInst->Pause();
}

void RotatingTextureAnimatedCube::Update()
{
	// Update - Game Loop
	BaseExample::Update();

	anim->Update((f32)GetTime());

	// Set Texture from Animation Instance
	material->SetColorMap(animInst->GetTexture());

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0, (f32)GetTime(), 0));

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
}

void RotatingTextureAnimatedCube::Shutdown()
{
	// All your Shutdown Code Here

		// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(FPSCamera);

	CubeObject->Remove(rCube);

	// Delete
	delete material;
	delete rCube;
	delete CubeObject;
	delete cubeMesh;
	delete Renderer;
	delete tex0;
	delete tex1;
	delete tex2;
	delete tex3;
	delete tex4;
	delete tex5;

	BaseExample::Shutdown();
}

RotatingTextureAnimatedCube::~RotatingTextureAnimatedCube() {}
