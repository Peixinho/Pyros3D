//============================================================================
// Name        : GIDemo.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See GIDemo.h.
//============================================================================

#include "GIDemo.h"

using namespace p3d;

GIDemo::GIDemo()
	: BaseExample(1024, 768, "Pyros3D - Global Illumination", WindowType::Close | WindowType::Resize)
{
}

GIDemo::~GIDemo() {}

void GIDemo::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);
	if (Renderer)
		Renderer->Resize(width, height);
	projection.Perspective(60.f, (f32)width / (f32)height, 0.1f, 200.f);
}

std::shared_ptr<GameObject> GIDemo::AddBox(const Vec3 &position, const Vec3 &halfExtents,
	const std::shared_ptr<GenericShaderMaterial> &material)
{
	// Cube's arguments are HALF-extents, not sizes - a "10" here is 20
	// units across. Getting that wrong is invisible from inside a room
	// and wrecks anything derived from the scene's bounds, the probe
	// volume above all.
	std::shared_ptr<Renderable> mesh = std::make_shared<Cube>(halfExtents.x, halfExtents.y, halfExtents.z);
	std::shared_ptr<GameObject> obj = std::make_shared<GameObject>();
	obj->Add(std::make_shared<RenderingComponent>(mesh, material));
	obj->SetPosition(position);
	Scene->Add(obj);
	objects.push_back(obj);
	return obj;
}

std::shared_ptr<GameObject> GIDemo::AddSphere(const Vec3 &position, const f32 radius,
	const std::shared_ptr<GenericShaderMaterial> &material)
{
	std::shared_ptr<Renderable> mesh = std::make_shared<Sphere>(radius, 32, 32, true);
	std::shared_ptr<GameObject> obj = std::make_shared<GameObject>();
	obj->Add(std::make_shared<RenderingComponent>(mesh, material));
	obj->SetPosition(position);
	Scene->Add(obj);
	objects.push_back(obj);
	return obj;
}

void GIDemo::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);
	projection.Perspective(60.f, (f32)Width / (f32)Height, 0.1f, 200.f);

	FPSCamera->SetPosition(Vec3(0.f, 0.f, 15.f));
	FPSCamera->RefreshTransformation();

	// GlobalIllumination is what declares the probe samplers. Without it
	// the material has no way to read the volume, whatever the renderer
	// has baked.
	const uint32 diffuseGI = ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::GlobalIllumination;
	// PBR is what gives a surface an F0 and a roughness, and therefore
	// what lets it reflect the room rather than only receive it.
	const uint32 metalGI = ShaderUsage::Color | ShaderUsage::PBR | ShaderUsage::GlobalIllumination;

	whiteMat = std::make_shared<GenericShaderMaterial>(diffuseGI);
	whiteMat->SetColor(Vec4(0.80f, 0.80f, 0.78f, 1.f));
	redMat = std::make_shared<GenericShaderMaterial>(diffuseGI);
	redMat->SetColor(Vec4(0.75f, 0.07f, 0.07f, 1.f));
	greenMat = std::make_shared<GenericShaderMaterial>(diffuseGI);
	greenMat->SetColor(Vec4(0.08f, 0.70f, 0.10f, 1.f));

	mirrorMat = std::make_shared<GenericShaderMaterial>(metalGI);
	mirrorMat->SetColor(Vec4(0.95f, 0.93f, 0.88f, 1.f));
	mirrorMat->SetMetallic(1.f);
	mirrorMat->SetRoughness(0.10f);
	brushedMat = std::make_shared<GenericShaderMaterial>(metalGI);
	brushedMat->SetColor(Vec4(0.95f, 0.93f, 0.88f, 1.f));
	brushedMat->SetMetallic(1.f);
	brushedMat->SetRoughness(0.35f);

	// The box: five slabs 0.2 thick, walls at +-5.
	const f32 H = 5.f;
	AddBox(Vec3(0.f, -H, 0.f), Vec3(H + 0.2f, 0.2f, H + 0.2f), whiteMat);   // floor
	AddBox(Vec3(0.f,  H, 0.f), Vec3(H + 0.2f, 0.2f, H + 0.2f), whiteMat);   // ceiling
	AddBox(Vec3(0.f, 0.f, -H), Vec3(H + 0.2f, H + 0.2f, 0.2f), whiteMat);   // back
	AddBox(Vec3(-H, 0.f, 0.f), Vec3(0.2f, H + 0.2f, H + 0.2f), redMat);     // left
	AddBox(Vec3( H, 0.f, 0.f), Vec3(0.2f, H + 0.2f, H + 0.2f), greenMat);   // right

	blockObj = AddBox(Vec3(0.f, -3.2f, -1.f), Vec3(1.5f, 1.6f, 1.5f), whiteMat);
	blockHome = blockObj->GetPosition();

	AddSphere(Vec3(-2.6f, -3.9f, 1.6f), 0.9f, mirrorMat);
	AddSphere(Vec3( 2.6f, -3.9f, 1.6f), 0.9f, brushedMat);

	lightObj = std::make_shared<GameObject>();
	lamp = std::make_shared<PointLight>(Vec4(1.f, 1.f, 1.f, 1.f), 30.f);
	lightObj->Add(lamp);
	lightObj->SetPosition(Vec3(0.f, 4.f, 0.f));
	Scene->Add(lightObj);

	InitImGui();
}

void GIDemo::Update()
{
	BaseExample::Update();

	const f64 now = GetTime();
	if (startTime == 0.0) startTime = now;
	const f32 t = (f32)(now - startTime);

	// The light orbits and the block slides, so both halves of "dynamic"
	// are visible: indirect light following a light, and following
	// geometry.
	lightObj->SetPosition(Vec3(sinf(t * 0.6f) * 3.2f, 4.f, cosf(t * 0.6f) * 2.0f));
	blockObj->SetPosition(Vec3(blockHome.x + sinf(t * 0.5f) * 2.2f, blockHome.y, blockHome.z));

	Scene->Update(now);

	if (!giReady)
	{
		// Solved after the first Scene::Update, never before: the
		// rendering components register during that traversal, and a
		// bake that runs first sees an empty scene.
		SceneGISettings gi;
		gi.enabled = true;
		gi.counts[0] = 7; gi.counts[1] = 6; gi.counts[2] = 7;
		// Fewer rays than the desktop demos use. Every target without
		// compute traces these on the CPU - which on the web is every
		// target - and the initial solve is a visible pause if it is
		// too greedy. It refines every frame afterwards.
		gi.raysPerProbe = 64;
		gi.passes = 2;
		gi.skyColor = Vec3(0.f, 0.f, 0.f);
		gi.shaderRoot = "shaders";
		giReady = Renderer->BakeGlobalIllumination(Scene, gi);
	}
	else
	{
		// 0 = as many probes as fit. On a GPU that is all of them; on
		// the CPU the renderer's own millisecond ceiling decides, which
		// is what keeps this smooth in a browser.
		Renderer->UpdateGlobalIllumination(Scene, 0, 0.9f);
	}

	PrepareImGuiFrame();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	EndImGuiFrame();
}

void GIDemo::DrawUI()
{
}

void GIDemo::Shutdown()
{
	if (Renderer) { delete Renderer; Renderer = NULL; }
	BaseExample::Shutdown();
}
