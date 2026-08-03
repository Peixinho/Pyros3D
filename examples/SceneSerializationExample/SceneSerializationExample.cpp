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
#include <Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.h>
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
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
	audio = new AudioManager();
	scene = new SceneGraph();
	renderer = new ForwardRenderer(1024, 768);
	projection.Perspective(70.f, 1024.f / 768.f, 1.f, 1000.f);

	camera = new GameObject();
	camera->SetPosition(Vec3(0, 80, 260));
	scene->Add(camera);

	physics = new Physics();
	physics->InitPhysics();

#ifdef LUA_BINDINGS
	GenerateBindings(&lua);
	lua.require_file("class", STR(EXAMPLES_PATH) "/assets/middleclass.lua");
#endif

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
	RenderingComponent* groundRC = new RenderingComponent(groundMesh, diffuse);
	ground->AddComponent(groundRC);
	ground->AddComponent(physics->CreateBox(80, 2, 80, 0, false));
	scene->Add(ground);

	// Decal - baked from a real projection onto the ground's own mesh
	// (ground's world transform is already valid here, scene->Add()
	// above synchronously ran Update()/InternalUpdate() on it). Phase B:
	// only the baked vertex data round-trips, not the DecalGeometry
	// projector itself - see SerializeRenderable's comment.
	{
		DecalGeometry* decalGeom = new DecalGeometry(groundRC->GetMeshes(0)[0], ground->GetWorldTransformation(), Vec3(0, 1, 0), Vec3(0, 0, 0), Vec3(10, 10, 10));
		GameObject* decalObj = new GameObject();
		decalObj->SetName("Decal");
		decalObj->AddComponent(new RenderingComponent(decalGeom->GetDecal(), diffuse));
		scene->Add(decalObj);
	}

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

	// AudioSource - deliberately given non-default settings on every axis the
	// serializer writes (cone included), so a field silently falling back to
	// its default on load shows up as a failed check rather than passing by
	// coincidence.
	GameObject* horn = new GameObject();
	horn->SetName("Horn");
	horn->SetPosition(Vec3(20, 5, -10));
	AudioSource* hornSrc = new AudioSource(STR(EXAMPLES_PATH)"/assets/neonpulse/sfx/ambience.wav", true);
	hornSrc->SetSpatialization(true);
	hornSrc->SetLooping(true);
	hornSrc->SetVolume(0.42f);
	hornSrc->SetPitch(1.25f);
	hornSrc->SetAttenuation(AttenuationModel::Exponential, 3.f, 77.f);
	hornSrc->SetCone(0.4f, 1.1f, 0.25f);
	hornSrc->SetDirectionalAttenuation(0.8f);
	hornSrc->SetDopplerFactor(0.5f);
	hornSrc->SetPan(-0.35f);
	hornSrc->SetFilter(AudioFilterType::LowPass, 1200.f, 4);
	horn->AddComponent(hornSrc);
	scene->Add(horn);

#ifdef LUA_BINDINGS
	GameObject* scripted = new GameObject();
	scripted->SetName("Scripted");
	scripted->AddComponent(new LuaComponent());
	scene->Add(scripted);

	// Real named-class LuaComponent (Phase F) - built from a .lua file
	// (test_behavior.lua) via GameObject:attachScript()'s underlying
	// primitive, its Lua-side `data` table mutated before save so the
	// round trip actually proves something (not just default-constructed
	// state survives).
	GameObject* namedScript = new GameObject();
	namedScript->SetName("NamedScript");
	LuaComponent* behaviorComp = LuaComponent_FromFile(lua, STR(EXAMPLES_PATH) "/assets/test_behavior.lua");
	if (behaviorComp)
	{
		behaviorComp->data["hp"] = 42;
		behaviorComp->data["name"] = "Goblin";
		namedScript->AddComponent(behaviorComp);
	}
	scene->Add(namedScript);
#endif

	// Vehicle - real chassis + wheels.
	{
		GameObject* car = new GameObject();
		car->SetName("Car");
		car->SetPosition(Vec3(60, 5, 0));
		IPhysicsComponent* chassis = physics->CreateBox(2, 1, 4, 500.0f, false);
		PhysicsVehicle* vehicle = dynamic_cast<PhysicsVehicle*>(physics->CreateVehicle(chassis, false));
		vehicle->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.4f, 0.3f, 1.0f, 0.1f, Vec3(-1.0f, -0.5f, 1.5f), true);
		vehicle->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.4f, 0.3f, 1.0f, 0.1f, Vec3(1.0f, -0.5f, 1.5f), true);
		vehicle->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.4f, 0.3f, 1.0f, 0.1f, Vec3(-1.0f, -0.5f, -1.5f), false);
		vehicle->AddWheel(Vec3(0, -1, 0), Vec3(-1, 0, 0), 0.4f, 0.3f, 1.0f, 0.1f, Vec3(1.0f, -0.5f, -1.5f), false);
		vehicle->SetMaxEngineForce(1500.0f);
		vehicle->SetSuspensionStiffness(30.0f);
		car->AddComponent(vehicle);
		scene->Add(car);
	}

	// Skeleton animation - real model + real animation file, actually
	// playing (not just loaded).
	{
		GameObject* human = new GameObject();
		human->SetName("Human");
		human->SetPosition(Vec3(-60, 1, 0));
		GenericShaderMaterial* skinnedMat = new GenericShaderMaterial(ShaderUsage::Texture + ShaderUsage::Diffuse + ShaderUsage::Skinning);
		skinnedMat->SetColorMap(tex);
		Model* humanModel = new Model(STR(EXAMPLES_PATH) "/assets/human.p3dm", false);
		RenderingComponent* humanRC = new RenderingComponent(humanModel, skinnedMat);
		human->AddComponent(humanRC);
		SkeletonAnimation* skelAnim = new SkeletonAnimation();
		skelAnim->LoadAnimation(STR(EXAMPLES_PATH) "/assets/walk.p3da");
		SkeletonAnimationInstance* skelInst = skelAnim->CreateInstance(humanRC);
		skelInst->Play(0, 0.0f, 1.0f, 1.0f, 1.0f, "");
		scene->Add(human);
	}

	// Texture animation - opt-in playback state capture (see
	// RenderingComponent::SetActiveTextureAnimation's comment).
	{
		GameObject* animatedQuad = new GameObject();
		animatedQuad->SetName("AnimatedQuad");
		animatedQuad->SetPosition(Vec3(-60, 40, 0));
		Cube* quadMesh = new Cube(10, 10, 10);
		RenderingComponent* quadRC = new RenderingComponent(quadMesh, diffuse);
		animatedQuad->AddComponent(quadRC);
		TextureAnimation* texAnim = new TextureAnimation();
		texAnim->AddFrame(tex);
		texAnim->AddFrame(tex);
		TextureAnimationInstance* texInst = texAnim->CreateInstance(12.0f);
		texInst->Play(1);
		quadRC->SetActiveTextureAnimation(texInst);
		scene->Add(animatedQuad);
	}

	// Text - real font + real string.
	{
		Font* font = new Font(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 32.0f);
		Text* text = new Text(font, "Hello", 5.0f, 5.0f, Vec4(1, 1, 0, 1));
		GameObject* textObj = new GameObject();
		textObj->SetName("Text");
		textObj->SetPosition(Vec3(0, 60, 100));
		textObj->AddComponent(new RenderingComponent(text, diffuse));
		scene->Add(textObj);
	}
}

void SceneSerializationExample::RunRoundTripAndVerify()
{
	scene->Update(0.0);

	uint32 preCount = (uint32)scene->GetAllGameObjectList().size();
	std::cout << "Pre-save root object count: " << preCount << std::endl;

#ifdef LUA_BINDINGS
	Check(SceneSerializer::SaveScene(scene, kScenePath, &lua), "SaveScene() succeeded");
#else
	Check(SceneSerializer::SaveScene(scene, kScenePath), "SaveScene() succeeded");
#endif

	scene->RemoveAll();
	Check(scene->GetAllGameObjectList().size() == 0, "RemoveAll() actually emptied the scene");

#ifdef LUA_BINDINGS
	Check(SceneSerializer::LoadScene(scene, kScenePath, physics, &lua), "LoadScene() succeeded");
#else
	Check(SceneSerializer::LoadScene(scene, kScenePath, physics), "LoadScene() succeeded");
#endif
	scene->Update(0.0);

	uint32 postCount = (uint32)scene->GetAllGameObjectList().size();
	Check(postCount == preCount, "Root object count round-tripped (" + std::to_string(preCount) + " -> " + std::to_string(postCount) + ")");

	GameObject* ground = NULL; GameObject* cube = NULL; GameObject* sun = NULL; GameObject* smoke = NULL; GameObject* lamp = NULL;
	GameObject* horn = NULL;
	GameObject* decalObj = NULL; GameObject* car = NULL; GameObject* human = NULL; GameObject* animatedQuad = NULL; GameObject* textObj = NULL;
	GameObject* namedScript = NULL;
	std::vector<GameObject*> &roots = scene->GetAllGameObjectList();
	for (size_t i = 0; i < roots.size(); i++)
	{
		if (roots[i]->GetName() == "Ground") { ground = roots[i]; if (!ground->GetChildren().empty()) lamp = ground->GetChildren()[0]; }
		else if (roots[i]->GetName() == "Cube") cube = roots[i];
		else if (roots[i]->GetName() == "Sun") sun = roots[i];
		else if (roots[i]->GetName() == "Smoke") smoke = roots[i];
		else if (roots[i]->GetName() == "Horn") horn = roots[i];
		else if (roots[i]->GetName() == "Decal") decalObj = roots[i];
		else if (roots[i]->GetName() == "Car") car = roots[i];
		else if (roots[i]->GetName() == "Human") human = roots[i];
		else if (roots[i]->GetName() == "AnimatedQuad") animatedQuad = roots[i];
		else if (roots[i]->GetName() == "Text") textObj = roots[i];
		else if (roots[i]->GetName() == "NamedScript") namedScript = roots[i];
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

	Check(horn != NULL, "AudioSource object found after load");
	if (horn)
	{
		AudioSource* a = NULL;
		const std::vector<IComponent*> &comps = horn->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((a = dynamic_cast<AudioSource*>(comps[i]))) break;
		Check(a != NULL, "AudioSource component round-tripped");
		if (a)
		{
			Check(a->IsStreamed() && a->IsLooping() && a->IsSpatialized(), "AudioSource flags round-tripped");
			Check(NearlyEqual(a->GetVolume(), 0.42f) && NearlyEqual(a->GetPitch(), 1.25f), "AudioSource volume/pitch round-tripped");
			Check(a->GetAttenuationModel() == (uint32)AttenuationModel::Exponential
				&& NearlyEqual(a->GetMinDistance(), 3.f) && NearlyEqual(a->GetMaxDistance(), 77.f), "AudioSource attenuation round-tripped");
			Check(a->HasCone() && NearlyEqual(a->GetConeInnerAngle(), 0.4f)
				&& NearlyEqual(a->GetConeOuterAngle(), 1.1f) && NearlyEqual(a->GetConeOuterGain(), 0.25f), "AudioSource cone round-tripped");
			Check(NearlyEqual(a->GetDirectionalAttenuation(), 0.8f) && NearlyEqual(a->GetDopplerFactor(), 0.5f), "AudioSource directional/doppler round-tripped");
			Check(NearlyEqual(a->GetPan(), -0.35f), "AudioSource pan round-tripped");
			Check(a->GetFilterType() == (uint32)AudioFilterType::LowPass
				&& NearlyEqual(a->GetFilterCutoff(), 1200.f) && a->GetFilterOrder() == 4, "AudioSource filter round-tripped");
		}
	}

	Check(decalObj != NULL, "Decal object found after load");
	if (decalObj)
	{
		RenderingComponent* rc = NULL;
		const std::vector<IComponent*> &comps = decalObj->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((rc = dynamic_cast<RenderingComponent*>(comps[i]))) break;
		Decal* decal = rc ? dynamic_cast<Decal*>(rc->GetRenderable()) : NULL;
		Check(decal != NULL && !decal->Geometries.empty(), "Decal baked geometry round-tripped");
	}

	Check(car != NULL, "Vehicle object found after load");
	if (car)
	{
		PhysicsVehicle* v = NULL;
		const std::vector<IComponent*> &comps = car->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((v = dynamic_cast<PhysicsVehicle*>(comps[i]))) break;
		Check(v != NULL && v->GetWheels().size() == 4, "Vehicle wheel count round-tripped");
		Check(v != NULL && NearlyEqual(v->GetMaxEngineForce(), 1500.0f), "Vehicle tunables round-tripped");
		Check(v != NULL && v->GetChassis() != NULL && v->GetChassis()->GetShape() == CollisionShapes::Box, "Vehicle chassis round-tripped");
	}

	Check(human != NULL, "Skeleton-animated object found after load");
	if (human)
	{
		RenderingComponent* rc = NULL;
		const std::vector<IComponent*> &comps = human->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((rc = dynamic_cast<RenderingComponent*>(comps[i]))) break;
		SkeletonAnimationInstance* inst = rc ? static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation()) : NULL;
		Check(inst != NULL && inst->GetNumberPlayingAnimations() == 1, "Skeleton animation instance round-tripped, still playing");
	}

	Check(animatedQuad != NULL, "Texture-animated object found after load");
	if (animatedQuad)
	{
		RenderingComponent* rc = NULL;
		const std::vector<IComponent*> &comps = animatedQuad->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((rc = dynamic_cast<RenderingComponent*>(comps[i]))) break;
		TextureAnimationInstance* inst = rc ? static_cast<TextureAnimationInstance*>(rc->GetActiveTextureAnimation()) : NULL;
		Check(inst != NULL && NearlyEqual(inst->GetFrameSpeed(), 12.0f) && inst->GetOwner()->GetNumberFrames() == 2, "Texture animation playback state round-tripped");
	}

	Check(textObj != NULL, "Text object found after load");
	if (textObj)
	{
		RenderingComponent* rc = NULL;
		const std::vector<IComponent*> &comps = textObj->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((rc = dynamic_cast<RenderingComponent*>(comps[i]))) break;
		Text* text = rc ? dynamic_cast<Text*>(rc->GetRenderable()) : NULL;
		Check(text != NULL && text->GetText() == "Hello", "Text content round-tripped");
	}

#ifdef LUA_BINDINGS
	Check(namedScript != NULL, "Named-class LuaComponent object found after load");
	if (namedScript)
	{
		LuaComponent* lc = NULL;
		const std::vector<IComponent*> &comps = namedScript->GetComponents();
		for (size_t i = 0; i < comps.size(); i++) if ((lc = dynamic_cast<LuaComponent*>(comps[i]))) break;
		bool dataOk = false;
		if (lc && lc->data.valid())
		{
			sol::object hp = lc->data["hp"];
			sol::object name = lc->data["name"];
			// Lua numbers round-trip through JSON as doubles - check
			// numerically, not by exact C++ type (is<int>() is too
			// strict for a value that's really a Lua/JSON double).
			dataOk = hp.valid() && hp.is<double>() && NearlyEqual((f32)hp.as<double>(), 42.0f) && name.valid() && name.is<std::string>() && name.as<std::string>() == "Goblin";
		}
		Check(lc != NULL && !lc->scriptFile.empty() && dataOk, "Named-class LuaComponent real behavior/data round-tripped");
	}
#endif

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

void SceneSerializationExample::Shutdown()
{
	// Last, so every AudioSource the scene still holds is torn down while the
	// engine is up (see AudioManager's voice registry).
	delete audio;
	audio = NULL;
}
