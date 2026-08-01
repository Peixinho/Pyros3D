//============================================================================
// Name        : SceneSerializationExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Scene save/load verification - real round trip, not a demo
//============================================================================

#include "SceneSerializationExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <iostream>
#include <string>

#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#endif

using namespace p3d;

static const char* kScenePath = "scene_test.json";
static int gFailures = 0;

// Raw std::cout, not echo() - echo() is a silent no-op unless this build
// defines _DEBUG/LOG_TO_CONSOLE/LOG_TO_FILE (see Log.h), and this
// verification harness needs its result reliably visible regardless of
// build type.
static void Check(bool condition, const std::string &what)
{
	if (condition)
		std::cout << "PASS: " << what << std::endl;
	else
	{
		std::cout << "FAIL: " << what << std::endl;
		gFailures++;
	}
}

static bool NearlyEqual(f32 a, f32 b, f32 eps = 0.01f)
{
	return fabs(a - b) < eps;
}

static bool NearlyEqual(const Vec3 &a, const Vec3 &b, f32 eps = 0.01f)
{
	return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps) && NearlyEqual(a.z, b.z, eps);
}

SceneSerializationExample::SceneSerializationExample() : ClassName(1024, 768, "Pyros3D - Scene Serialization", WindowType::Close | WindowType::Resize)
{
}

SceneSerializationExample::~SceneSerializationExample() {}

void SceneSerializationExample::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);
	renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);
}

void SceneSerializationExample::Init()
{
	scene = new SceneGraph();
	renderer = new ForwardRenderer(1024, 768);
	projection.Perspective(70.f, 1024.f / 768.f, 1.f, 1000.f);

	camera = new GameObject();
	camera->SetPosition(Vec3(0, 80, 260));
	scene->Add(camera);

	physics = new Physics();
	physics->InitPhysics();

	frameCount = 0;
	verified = false;

	BuildScene();
	RunRoundTripAndVerify();
}

void SceneSerializationExample::BuildScene()
{
	// Shared material - real texture load, real PBR fields set, so the
	// round trip exercises the material pool + texture path dedup, not
	// just default-constructed state.
	Texture* tex = new Texture();
	tex->LoadTexture(STR(EXAMPLES_PATH) "/assets/luaexample/Texture.png", TextureType::Texture, false, 0);
	GenericShaderMaterial* diffuse = new GenericShaderMaterial(ShaderUsage::Texture + ShaderUsage::Diffuse);
	diffuse->SetColorMap(tex);
	diffuse->SetColor(Vec4(0.8f, 0.75f, 0.7f, 1.0f));
	diffuse->SetMetallic(0.15f);
	diffuse->SetRoughness(0.65f);

	// Ground - static physics box + primitive rendering component.
	GameObject* ground = new GameObject(true);
	ground->SetName("Ground");
	Cube* groundMesh = new Cube(80, 2, 80);
	ground->AddComponent(new RenderingComponent(groundMesh, diffuse));
	ground->AddComponent(physics->CreateBox(80, 2, 80, 0, false));
	scene->Add(ground);

	// A PointLight on a CHILD object, to exercise hierarchy save/load.
	GameObject* lampChild = new GameObject();
	lampChild->SetName("Lamp");
	lampChild->SetPosition(Vec3(0, 20, 0));
	lampChild->AddComponent(new PointLight(Vec4(1, 0.9f, 0.8f, 1), 60.0f));
	ground->Add(lampChild);

	// Directional light with real shadow settings.
	GameObject* sun = new GameObject(true);
	sun->SetName("Sun");
	DirectionalLight* dl = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, 0));
	dl->EnableCastShadows(1024, 1024, projection, 1.f, 500.f, 2);
	sun->AddComponent(dl);
	scene->Add(sun);

	// Falling dynamic cube.
	GameObject* cube = new GameObject();
	cube->SetName("Cube");
	cube->SetPosition(Vec3(0, 120, 0));
	Cube* cubeMesh = new Cube(15, 15, 15);
	cube->AddComponent(new RenderingComponent(cubeMesh, diffuse));
	cube->AddComponent(physics->CreateBox(15, 15, 15, 5, false));
	scene->Add(cube);

	// Particle emitter.
	GameObject* smoke = new GameObject();
	smoke->SetName("Smoke");
	smoke->SetPosition(Vec3(0, 140, 0));
	ParticleSystemDesc desc;
	desc.maxParticles = 64;
	desc.texture = tex;
	desc.emissionRate = 8.f;
	smoke->AddComponent(new ParticleSystem(desc));
	scene->Add(smoke);

#ifdef LUA_BINDINGS
	GameObject* scripted = new GameObject();
	scripted->SetName("Scripted");
	scripted->AddComponent(new LuaComponent());
	scene->Add(scripted);
#endif
}

void SceneSerializationExample::RunRoundTripAndVerify()
{
	scene->Update(0.0);

	uint32 preCount = (uint32)scene->GetAllGameObjectList().size();
	std::cout << "Pre-save root object count: " << preCount << std::endl;

	Check(SceneSerializer::SaveScene(scene, kScenePath), "SaveScene() succeeded");

	scene->RemoveAll();
	Check(scene->GetAllGameObjectList().size() == 0, "RemoveAll() actually emptied the scene");

	Check(SceneSerializer::LoadScene(scene, kScenePath, physics), "LoadScene() succeeded");
	scene->Update(0.0);

	uint32 postCount = (uint32)scene->GetAllGameObjectList().size();
	Check(postCount == preCount, "Root object count round-tripped (" + std::to_string(preCount) + " -> " + std::to_string(postCount) + ")");

	GameObject* ground = NULL; GameObject* cube = NULL; GameObject* sun = NULL; GameObject* smoke = NULL; GameObject* lamp = NULL;
	std::vector<GameObject*> &roots = scene->GetAllGameObjectList();
	for (size_t i = 0; i < roots.size(); i++)
	{
		if (roots[i]->GetName() == "Ground") { ground = roots[i]; if (!ground->GetChildren().empty()) lamp = ground->GetChildren()[0]; }
		else if (roots[i]->GetName() == "Cube") cube = roots[i];
		else if (roots[i]->GetName() == "Sun") sun = roots[i];
		else if (roots[i]->GetName() == "Smoke") smoke = roots[i];
	}

	Check(ground != NULL, "Ground object found after load");
	Check(cube != NULL && NearlyEqual(cube->GetPosition(), Vec3(0, 120, 0)), "Cube position round-tripped");
	Check(lamp != NULL, "Lamp child object survived hierarchy round-trip");
	if (lamp)
	{
		PointLight* pl = NULL;
		const std::vector<IComponent*> &comps = lamp->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((pl = dynamic_cast<PointLight*>(comps[i]))) break;
		Check(pl != NULL && NearlyEqual(pl->GetLightRadius(), 60.0f), "PointLight radius round-tripped");
	}
	Check(sun != NULL, "Sun object found after load");
	if (sun)
	{
		DirectionalLight* dl = NULL;
		const std::vector<IComponent*> &comps = sun->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((dl = dynamic_cast<DirectionalLight*>(comps[i]))) break;
		Check(dl != NULL && dl->IsCastingShadows() && dl->GetNumberCascades() == 2, "DirectionalLight shadow settings round-tripped");
	}
	if (ground)
	{
		RenderingComponent* rc = NULL;
		const std::vector<IComponent*> &comps = ground->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((rc = dynamic_cast<RenderingComponent*>(comps[i]))) break;
		Check(rc != NULL, "Ground RenderingComponent round-tripped");
		if (rc && !rc->GetMeshes(0).empty())
		{
			GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(rc->GetMeshes(0)[0]->Material);
			Check(gm != NULL && NearlyEqual(gm->GetMetallic(), 0.15f) && NearlyEqual(gm->GetRoughness(), 0.65f), "Material metallic/roughness round-tripped");
			Check(gm != NULL && gm->GetColorMap() != NULL, "Material texture round-tripped (reloaded from path)");
		}

		IPhysicsComponent* pc = NULL;
		for (size_t i = 0; i < comps.size(); i++) if ((pc = dynamic_cast<IPhysicsComponent*>(comps[i]))) break;
		Check(pc != NULL && pc->GetShape() == CollisionShapes::Box && NearlyEqual(pc->GetMass(), 0.0f), "Ground physics box round-tripped");
	}
	Check(smoke != NULL, "ParticleSystem object found after load");
	if (smoke)
	{
		ParticleSystem* ps = NULL;
		const std::vector<IComponent*> &comps = smoke->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((ps = dynamic_cast<ParticleSystem*>(comps[i]))) break;
		Check(ps != NULL && ps->GetDesc().maxParticles == 64, "ParticleSystem desc round-tripped");
	}

	std::cout << "=================================================" << std::endl;
	std::cout << "SceneSerializationExample: " << gFailures << " check(s) failed" << std::endl;
	std::cout << "=================================================" << std::endl;
	verified = true;
}

void SceneSerializationExample::Update()
{
	scene->Update(GetTime());
	renderer->PreRender(camera, scene);
	renderer->RenderScene(projection, camera, scene);
	frameCount++;
}

void SceneSerializationExample::Shutdown() {}
