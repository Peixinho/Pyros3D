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
	// Reads LOG::_LOG's ring buffer - see its comment. The engine records
	// unconditionally and knows nothing about ImGui; this is just one view
	// onto it, which is what lets it show messages logged long before any
	// of the UI existed.
	void DrawLogWindow();

	// Lists every live FrameBuffer and draws the colour attachments it can
	// get an ImTextureID for. Built on FrameBuffer::GetLiveFrameBuffers(),
	// so it follows whatever the active demo actually creates rather than
	// any fixed list.
	void DrawRenderTargetViewer();

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
	bool showRenderTargets;
	float renderTargetThumbSize;
	bool showLog;
	bool logErrorsOnly;
	// Highest message index already seen, so the window can flag that
	// something new arrived while it was closed rather than requiring the
	// user to have been watching.
	unsigned int logSeen;
	// One-shot: the first error of the session pops the window open. The
	// whole reason this exists is that errors were invisible; requiring the
	// user to already suspect something and go looking would keep them so.
	bool logAutoOpened;
	// Set at init when no imgui.ini exists; consumed by the first DrawUI().
	bool buildDefaultDockLayout;

};

#endif	/* DEMOLAUNCHER_H */
