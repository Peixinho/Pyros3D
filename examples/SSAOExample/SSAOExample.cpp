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
							#if defined(GLES2)
								"#define varying_in varying\n"
								"#define varying_out varying\n"
								"#define attribute_in attribute\n"
								"#define texture_2D texture2D\n"
								"#define texture_cube textureCube\n"
								"precision mediump float;\n"
							#else
								"#define varying_in in\n"
								"#define varying_out out\n"
								"#define attribute_in in\n"
								"#define texture_2D texture\n"
								"#define texture_cube texture\n"
								#if defined(GLES3)
									"precision mediump float;\n"
								#endif
							#endif
							#if defined(GLES2)
								"vec4 FragColor;\n"
							#else
								"out vec4 FragColor;\n"
							#endif
							"uniform sampler2D uTex0, uTex1;\n"
							"varying_in vec2 vTexcoord;\n"
							"void main() {\n"
							"FragColor.r = texture_2D(uTex1, vTexcoord).r;\n"
							"FragColor.g = texture_2D(uTex1, vTexcoord).g;\n"
							"FragColor.b = texture_2D(uTex1, vTexcoord).b;\n"
							"FragColor.a = 1.0;\n"
							"FragColor = texture_2D(uTex0, vTexcoord)*texture_2D(uTex1, vTexcoord);\n"
							#if defined(GLES2)
							"gl_FragColor = FragColor;\n"
							#endif
							"}";

	CompileShaders();

}

SSAOExample::SSAOExample() : BaseExample(1024, 768, "Pyros3D - Custom Material", WindowType::Close | WindowType::Resize)
{

}

void SSAOExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.01f, 50.f);
	EffectManager->Resize(Width, Height);
}

void SSAOExample::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetGlobalLight(Vec4(0.2, 0.2, 0.2, 0.2));
	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.01f, 50.f);

	// Create Camera
	FPSCamera->SetPosition(Vec3(0.f,3.f,3.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-45.f),0.f,0.f));

	// Teapots
	teapot = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD1.p3dm");
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

	// Add a Directional Light
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	Light->Add(dLight);
	Scene->Add(Light);
}

void SSAOExample::Update()
{
	// Update Scene
	Scene->Update(GetTime());

	BaseExample::Update();

	ssao->SetViewMatrix(FPSCamera->GetWorldTransformation().Inverse());

	// Render Scene
	EffectManager->CaptureFrame();
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();

	EffectManager->ProcessPostEffects(&projection);	

}

void SSAOExample::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene

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


	delete Renderer;
	delete EffectManager;

	BaseExample::Shutdown();

}

SSAOExample::~SSAOExample() {}
