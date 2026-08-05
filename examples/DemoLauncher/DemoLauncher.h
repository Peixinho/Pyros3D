//============================================================================
// Name        : DemoLauncher.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Single-process demo browser - lists every converted demo
//                in an ImGui panel and hot-swaps between them by clearing
//                and reloading a scene JSON (SceneSerializer::LoadScene/
//                UnloadScene), with all per-frame behavior (rotation,
//                particle tuning, skeleton animation ticking, camera fly)
//                living in small Lua scripts attached to the relevant
//                GameObjects via attachScript(), not a monolithic driver
//                script. Standardized on DeferredRenderer + a shared
//                PostEffectsManager so every demo renders through one
//                renderer instance, never recreated.
//============================================================================

#ifndef DEMOLAUNCHER_H
#define	DEMOLAUNCHER_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#else
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>

// ImGui includes - same bootstrap as BaseExample::InitImGui, duplicated
// rather than inherited from BaseExample since this example doesn't want
// BaseExample's hardcoded ForwardRenderer (see class comment) - matches
// the existing precedent of LuaScripting/SceneSerializationExample, both
// of which already extend ClassName directly for the same reason.
// Resolved via the IMGUI_INCLUDE_DIRS include path (see root
// CMakeLists.txt), not a relative path - ImGui core now lives at
// src/Pyros3D/Ext/imgui (engine-owned), not examples/imgui.
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <string>
#include <vector>
#include <memory>

using namespace p3d;

// One manifest.json entry - see examples/assets/demos/manifest.json.
struct DemoEntry
{
	std::string name;
	std::string description;
	std::string scene;
	std::vector<std::string> effects;
	Vec3 cameraPosition;
};

class DemoLauncher : public ClassName {

public:

	DemoLauncher();
	virtual ~DemoLauncher();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	void LoadManifest();
	void SwitchDemo(int index);
	void DrawUI();

	void InitImGui();
	void ShutdownImGui();
	void BeginImGuiFrame();
	// Begin+DrawUI+Render, called BEFORE Renderer->RenderScene() - on
	// Vulkan, EndFrame() (inside RenderScene()) records ImGui's draw
	// data mid-call, so ImGui::Render() must have already finalized it
	// by then. EndImGuiFrame() (below) is the GL-only tail, called after
	// RenderScene() same as always - a no-op on Vulkan.
	void PrepareImGuiFrame();
	void EndImGuiFrame();

	// One long-lived Lua state, reused across every demo switch - bindings/
	// middleclass stay registered throughout; per-demo behavior lives in
	// each attached LuaComponent's own independent instance table, not in
	// this state's globals, so there's no cross-demo collision risk to
	// isolate against (see VULKAN_ROADMAP.md's demo-launcher notes).
	sol::state lua;

	SceneGraph* Scene;
	Physics* physics;

	// Free-standing, never Scene->Add()'d - shell state, not demo
	// content, so it's never touched by SceneSerializer::UnloadScene()
	// (which only ever sees what its paired LoadScene() call produced)
	// and never cleared on a demo switch. Its camera_fly.lua behavior is
	// driven directly (FPSCamera->GetComponents(), not Scene->Update()).
	std::shared_ptr<GameObject> FPSCamera;
	std::shared_ptr<LuaComponent> cameraComponent;
	Projection projection;

	// G-buffer (4 color attachments + depth) + DeferredRenderer, built
	// once and reused for every demo - see examples/DeferredPBRSpheres
	// for the reference wiring this mirrors.
	Texture *albedoTexture, *specularTexture, *depthTexture, *normalTexture, *metallicRoughnessTexture;
	FrameBuffer* deferredFBO;
	DeferredRenderer* Renderer;

	// One shared instance, chain cleared+rebuilt per demo via
	// RemoveAllEffects() - CaptureFrame()/EndCapture() is renderer-
	// agnostic (see DeferredPBRSpheres' comment), so this wraps every
	// demo's DeferredRenderer::RenderScene() call the same way regardless
	// of whether that demo requests any effects at all.
	PostEffectsManager* EffectManager;

	std::vector<DemoEntry> demos;
	int activeDemo;

	// Precisely what the active demo's LoadScene() call allocated - the
	// only thing UnloadScene() ever touches on switch (see
	// SceneSerializer.h's LoadedSceneAssets comment for why this is
	// deliberately not a generic "clear the whole SceneGraph").
	LoadedSceneAssets currentAssets;

	// Detected from currentAssets after each load (not a manifest flag,
	// so it can't drift from what the scene actually contains) - gates
	// physics->Update() so demos with no physics content don't pay for
	// stepping an empty world every frame.
	bool activeDemoHasPhysics;

	// ParticlesExample is the one demo with ImGui-driven per-frame
	// tunables (see the plan: no ImGui<->Lua binding exists, so this
	// stays a small, explicit special case rather than a generalized
	// mechanism for one demo's 3 sliders). Non-NULL only when the active
	// demo's scene has a GameObject with both a ParticleSystem and an
	// attached LuaComponent (smoke_tuning.lua) - found once in
	// SwitchDemo(), written into every frame before Scene->Update().
	std::shared_ptr<LuaComponent> smokeTuningComponent;
	float smokeEmissionRate, smokeSpread, smokeRiseSpeed;

	bool imguiInitialized;

};

#endif	/* DEMOLAUNCHER_H */
