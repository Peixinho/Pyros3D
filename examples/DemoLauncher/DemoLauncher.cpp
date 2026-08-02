//============================================================================
// Name        : DemoLauncher.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See DemoLauncher.h.
//============================================================================

#include "DemoLauncher.h"

#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <SDL2/SDL.h>
#include <fstream>

using namespace p3d;
using json = nlohmann::json;

DemoLauncher::DemoLauncher() : ClassName(1280, 720, "Pyros3D - Demos", WindowType::Close | WindowType::Resize)
{
	Scene = nullptr;
	physics = nullptr;
	FPSCamera = nullptr;
	cameraComponent = nullptr;
	Renderer = nullptr;
	EffectManager = nullptr;
	deferredFBO = nullptr;
	albedoTexture = specularTexture = depthTexture = normalTexture = metallicRoughnessTexture = nullptr;
	activeDemo = -1;
	activeDemoHasPhysics = false;
	smokeTuningComponent = nullptr;
	smokeEmissionRate = 15.0f;
	smokeSpread = 1.0f;
	smokeRiseSpeed = 2.0f;
	imguiInitialized = false;
}

DemoLauncher::~DemoLauncher() {}

void DemoLauncher::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);

	Renderer->Resize(width, height);
	EffectManager->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.1f, 2000.f);

	albedoTexture->Resize(Width, Height);
	specularTexture->Resize(Width, Height);
	depthTexture->Resize(Width, Height);
	normalTexture->Resize(Width, Height);
	metallicRoughnessTexture->Resize(Width, Height);
}

void DemoLauncher::Init()
{
	ClassName::Init();

	Scene = new SceneGraph();

	physics = new Physics();
	physics->InitPhysics();

	lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
	GenerateBindings(&lua);
	lua.require_file("class", STR(EXAMPLES_PATH) "/assets/middleclass.lua");

	// G-buffer - same 5-texture shape as examples/DeferredPBRSpheres.
	albedoTexture = new Texture(); albedoTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	specularTexture = new Texture(); specularTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	depthTexture = new Texture(); depthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	normalTexture = new Texture(); normalTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, Width, Height, false);
	metallicRoughnessTexture = new Texture(); metallicRoughnessTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);

	albedoTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	specularTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	depthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	normalTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	metallicRoughnessTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	deferredFBO = new FrameBuffer();
	deferredFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, albedoTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, specularTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, normalTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, metallicRoughnessTexture);

	Renderer = new DeferredRenderer(Width, Height, deferredFBO);
	Renderer->SetGlobalLight(Vec4(0.12f, 0.12f, 0.14f, 1.f));

	EffectManager = new PostEffectsManager(Width, Height);

	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.1f, 2000.f);

	// Persistent shell camera - see DemoLauncher.h's FPSCamera comment.
	// LUA_GameObject, not plain GameObject - camera_fly.lua receives this
	// object as its `owner` (via IComponent::GetOwner(), see
	// WireLuaComponentLifecycle) and calls real methods on it
	// (getDirection/getPosition/setPosition/setRotation); sol2's RTTI-based
	// type resolution for a polymorphic GameObject* only finds those
	// methods if the actual instance is a registered usertype
	// (LUA_GameObject - plain GameObject itself is never registered, see
	// SceneSerializer.cpp's DeserializeGameObject for the identical fix).
	FPSCamera = new LUA_GameObject();
	FPSCamera->SetPosition(Vec3(0.f, 10.f, 100.f));
	cameraComponent = LuaComponent_FromFile(lua, STR(EXAMPLES_PATH) "/assets/demos/scripts/camera_fly.lua");
	if (cameraComponent) FPSCamera->AddComponent(cameraComponent);
	else echo("ERROR: DemoLauncher - couldn't load camera_fly.lua");

	LoadManifest();
	if (!demos.empty()) SwitchDemo(0);

	InitImGui();
}

void DemoLauncher::LoadManifest()
{
	std::ifstream in((STR(EXAMPLES_PATH) "/assets/demos/manifest.json"));
	if (!in.is_open())
	{
		echo("ERROR: DemoLauncher - couldn't open manifest.json");
		return;
	}
	json root;
	try { in >> root; }
	catch (const std::exception&)
	{
		echo("ERROR: DemoLauncher - invalid JSON in manifest.json");
		return;
	}
	for (auto &e : root)
	{
		DemoEntry entry;
		entry.name = e.value("name", std::string());
		entry.description = e.value("description", std::string());
		entry.scene = e.value("scene", std::string());
		if (e.find("effects") != e.end())
			for (auto &fx : e["effects"]) entry.effects.push_back(fx.get<std::string>());
		if (e.find("cameraPosition") != e.end())
			entry.cameraPosition = Vec3(e["cameraPosition"][0].get<f32>(), e["cameraPosition"][1].get<f32>(), e["cameraPosition"][2].get<f32>());
		else
			entry.cameraPosition = Vec3(0.f, 10.f, 100.f);
		demos.push_back(entry);
	}
}

void DemoLauncher::SwitchDemo(int index)
{
	if (index < 0 || index >= (int)demos.size()) return;

	// Frees exactly what the outgoing demo's LoadScene() call allocated -
	// see SceneSerializer.h's LoadedSceneAssets/UnloadScene comments.
	if (!currentAssets.gameObjects.empty())
		SceneSerializer::UnloadScene(Scene, currentAssets);
	currentAssets = LoadedSceneAssets();

	EffectManager->RemoveAllEffects();

	const DemoEntry &entry = demos[index];
	for (const std::string &fx : entry.effects)
	{
		if (fx == "tonemap")
			EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));
	}

	SceneSerializer::LoadScene(Scene, STR(EXAMPLES_PATH) + std::string("/assets/demos/") + entry.scene, physics, &lua, &currentAssets);

	activeDemoHasPhysics = false;
	smokeTuningComponent = nullptr;
	for (GameObject* go : currentAssets.gameObjects)
	{
		bool hasParticleSystem = false;
		LuaComponent* luaComp = nullptr;
		for (IComponent* c : go->GetComponents())
		{
			if (c->GetComponentType() == ComponentType::Physics || c->GetComponentType() == ComponentType::Vehicle)
				activeDemoHasPhysics = true;
			else if (c->GetComponentType() == ComponentType::ParticleSystem)
				hasParticleSystem = true;
			else if (c->GetComponentType() == ComponentType::LuaComponent)
				luaComp = static_cast<LuaComponent*>(c);
		}
		if (hasParticleSystem && luaComp) smokeTuningComponent = luaComp;
	}

	FPSCamera->SetPosition(entry.cameraPosition);
	FPSCamera->SetRotation(Vec3(0.f, 0.f, 0.f));
	// camera_fly.lua tracks its own accumulated yaw/pitch/last-mouse-
	// position and only reapplies rotation reactively (on mouse move),
	// so the SetRotation() above alone would get silently overwritten
	// by stale state on the next mouse move - reset its data table too.
	if (cameraComponent)
	{
		sol::table data = cameraComponent->data;
		data["yaw"] = 0.0f;
		data["pitch"] = 0.0f;
		data["lastMouseX"] = sol::lua_nil;
		data["lastMouseY"] = sol::lua_nil;
	}

	activeDemo = index;
}

void DemoLauncher::Update()
{
	f64 time = GetTime();
	f64 dt = GetTimeInterval();

	if (activeDemoHasPhysics) physics->Update(dt, 10);

	// Camera-fly script tick - the camera is deliberately never part of
	// `Scene`, so it's driven directly rather than via Scene->Update().
	// RefreshTransformation() is the part SceneGraph::Update() would
	// otherwise provide for free (GO->Update(); GO->InternalUpdate();) -
	// without it, camera_fly.lua's SetPosition()/SetRotation() calls
	// change GetPosition()'s raw value but never touch the actual
	// GetWorldTransformation() the renderer reads, so the camera's
	// internal state moves while the rendered view never does.
	for (IComponent* c : FPSCamera->GetComponents()) c->Update(time);
	FPSCamera->RefreshTransformation();

	if (smokeTuningComponent)
	{
		sol::table data = smokeTuningComponent->data;
		data["emissionRate"] = smokeEmissionRate;
		data["spread"] = smokeSpread;
		data["riseSpeed"] = smokeRiseSpeed;
	}

	Scene->Update(time);

	// Must run before Renderer->RenderScene() below, not after - see
	// PrepareImGuiFrame()'s declaration comment (DemoLauncher.h).
	if (imguiInitialized) PrepareImGuiFrame();

	// Unconditional, for every demo, whether or not it requested an effect:
	// PostEffectsManager runs a trivial copy pass when its chain is empty
	// (see its ProcessPostEffects() comment), so this no longer needs to
	// branch. It must not: branching moved RenderScene()'s render target
	// between ExternalFBO and the swapchain depending on the active demo,
	// and on Vulkan the second target a given mesh+shader is ever drawn
	// into silently stops rasterizing (examples/EffectToggleTest). That is
	// what blacked out the whole scene after switching to a demo whose
	// effect chain differed from the previous one's.
	EffectManager->CaptureFrame();
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);
	EffectManager->EndCapture();
	EffectManager->ProcessPostEffects(&projection);

	if (imguiInitialized) EndImGuiFrame();

}

void DemoLauncher::DrawUI()
{
	ImGui::Begin("Pyros3D Demos");

	ImGui::Text("FPS: %.1u", (uint32)fps.getFPS());
	ImGui::Separator();

	for (size_t i = 0; i < demos.size(); i++)
	{
		bool selected = (activeDemo == (int)i);
		if (ImGui::Selectable(demos[i].name.c_str(), selected))
		{
			if (!selected) SwitchDemo((int)i);
		}
		if (ImGui::IsItemHovered() && !demos[i].description.empty())
			ImGui::SetTooltip("%s", demos[i].description.c_str());
	}

	if (smokeTuningComponent)
	{
		ImGui::Separator();
		ImGui::Text("Smoke Particle Tuning");
		ImGui::SliderFloat("Rate (particles/sec)", &smokeEmissionRate, 1.0f, 60.0f);
		ImGui::SliderFloat("Spread", &smokeSpread, 0.0f, 2.0f);
		ImGui::SliderFloat("Rise Speed", &smokeRiseSpeed, 0.0f, 10.0f);
	}

	ImGui::End();
}

void DemoLauncher::Shutdown()
{
	ShutdownImGui();

	if (!currentAssets.gameObjects.empty())
		SceneSerializer::UnloadScene(Scene, currentAssets);

	if (EffectManager) EffectManager->RemoveAllEffects();

	if (FPSCamera)
	{
		for (IComponent* c : FPSCamera->GetComponents()) delete c;
		delete FPSCamera;
		FPSCamera = nullptr;
	}

	if (Renderer) { delete Renderer; Renderer = nullptr; }
	if (EffectManager) { delete EffectManager; EffectManager = nullptr; }
	if (deferredFBO) { delete deferredFBO; deferredFBO = nullptr; }
	if (albedoTexture) { delete albedoTexture; albedoTexture = nullptr; }
	if (specularTexture) { delete specularTexture; specularTexture = nullptr; }
	if (depthTexture) { delete depthTexture; depthTexture = nullptr; }
	if (normalTexture) { delete normalTexture; normalTexture = nullptr; }
	if (metallicRoughnessTexture) { delete metallicRoughnessTexture; metallicRoughnessTexture = nullptr; }

	if (physics) { delete physics; physics = nullptr; }
	if (Scene) { delete Scene; Scene = nullptr; }

	ClassName::Shutdown();
}

void DemoLauncher::InitImGui()
{
#if !defined(_SDL2VULKAN)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForOpenGL(GetSDLWindow(), GetGLContext());
	ImGui_ImplOpenGL3_Init("#version 330");

	imguiInitialized = true;
#else
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	// imgui_impl_sdl2.cpp has no volk/Vulkan-function dependency, stays
	// example-side (called directly here); the actual ImGui_ImplVulkan_*
	// calls live behind VulkanRenderDevice's wrapper - see its
	// InitImGuiVulkanBackend() comment for why.
	ImGui_ImplSDL2_InitForVulkan(GetSDLWindow());
	imguiInitialized = static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).InitImGuiVulkanBackend();
#endif
}

void DemoLauncher::ShutdownImGui()
{
	if (imguiInitialized)
	{
#if !defined(_SDL2VULKAN)
		ImGui_ImplOpenGL3_Shutdown();
#else
		static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiVulkanBackend();
#endif
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		imguiInitialized = false;
	}
}

void DemoLauncher::BeginImGuiFrame()
{
#if !defined(_SDL2VULKAN)
	ImGui_ImplOpenGL3_NewFrame();
#else
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).NewImGuiVulkanFrame();
#endif
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void DemoLauncher::PrepareImGuiFrame()
{
	BeginImGuiFrame();
	DrawUI();
	ImGui::Render();
}

void DemoLauncher::EndImGuiFrame()
{
#if !defined(_SDL2VULKAN)
	GLint last_program, last_texture, last_array_buffer, last_element_array_buffer, last_vertex_array;
	glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glUseProgram(last_program);
	glBindTexture(GL_TEXTURE_2D, last_texture);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
	glBindVertexArray(last_vertex_array);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
	// Vulkan: no-op - VulkanRenderDevice::EndFrame() (called from inside
	// Renderer->RenderScene(), which already ran by the time this is
	// called) already recorded ImGui's draw data via UIRenderHook, using
	// the draw data PrepareImGuiFrame()'s ImGui::Render() finalized
	// before RenderScene() was called.
}
