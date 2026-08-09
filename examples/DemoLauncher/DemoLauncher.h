//============================================================================
// Name        : DemoLauncher.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Thin demo browser - window, SceneGraph, physics, ImGui list.
//                Renderer, camera, and per-demo behavior live in JSON/Lua.
//============================================================================

#ifndef DEMOLAUNCHER_H
#define	DEMOLAUNCHER_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#elif defined(_SDL2METAL)
#include "../WindowManagers/SDL2Metal/SDL2MetalContext.h"
#define ClassName SDL2MetalContext
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#else
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <string>
#include <vector>
#include <memory>

using namespace p3d;
using json = nlohmann::json;

struct DemoEntry
{
	std::string name;
	std::string description;
	std::string scene;
	// Opaque render config forwarded to RenderHost.setup().
	json render;
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
	// See its definition - reads the active point light's shadow
	// cubemap back and draws all six faces.
	void DrawShadowCubemapViewer();

	void InitImGui();
	void ShutdownImGui();
	void BeginImGuiFrame();
	void PrepareImGuiFrame();
	void EndImGuiFrame();

	sol::state lua;

	SceneGraph* Scene;
	Physics* physics;
	Projection projection;

	std::vector<DemoEntry> demos;
	int activeDemo;

	LoadedSceneAssets currentAssets;
	bool activeDemoHasPhysics;

	bool imguiInitialized;
	bool showCubemapViewer;

};

#endif	/* DEMOLAUNCHER_H */
