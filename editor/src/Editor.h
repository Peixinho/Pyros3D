//============================================================================
// Name        : Editor.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ImGui Example
//============================================================================

#ifndef EDITOR_H
#define	EDITOR_H

#include <imgui.h>
#include <imgui_internal.h>	// DockBuilder* - building the default layout
#include <misc/cpp/imgui_stdlib.h>
#include <string>
#include <imgui_impl_sdl2.h>
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
	#include <imgui_impl_opengl3.h>
#endif

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>

#include "editor/libgizmo/IGizmo.h"

#include "editor/UI/UISettings.h"
#include "editor/UI/TabLog.h"
#include "editor/UI/PropertiesTab.h"
#include "editor/UI/ToolsTab.h"
#include "editor/SceneEditor.h"

// Window context per backend, same selection the examples make (see
// BaseExample.h). The editor is no longer OpenGL-only: on Vulkan and Metal
// the ImGui draw is issued inside the device's EndFrame() via its UIRenderHook,
// and the scene viewport goes through IRenderDevice::GetImGuiTextureID().
#if defined(_SDL2VULKAN)
	#include "SDL2Vulkan/SDL2VulkanContext.h"
	#define ClassName SDL2VulkanContext
#elif defined(_SDL2METAL)
	#include "SDL2Metal/SDL2MetalContext.h"
	#define ClassName SDL2MetalContext
#else
	#include "SDL2/SDL2Context.h"
	#define ClassName SDL2Context
#endif

#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#if defined(_SDL2VULKAN)
	#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#elif defined(_SDL2METAL)
	#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#endif

using namespace p3d;

class Editor : public ClassName
{

public:
	
	static Editor* getInstance();
	static void cleanupInstance();
	
	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void Draw();
	virtual void OnResize(const uint32 width, const uint32 height);

	void MouseMove(Event::Input::Info e);
	
	virtual ~Editor();

protected:

	void LoadDefaultLayout();
	void BuildDefaultLayout(const ImGuiID dockspaceID, const ImVec2 &size);
	Editor();

private:

	static Editor* instance;

	void DrawUI();

	TabLog* tabLog;
	
	PropertiesTab* tabProperties;
	ToolsTab* tabTools;
	// Set by LoadDefaultLayout(); consumed by the next DrawUI().
	bool resetLayout;

	SceneEditor* sceneView;

	bool showingSceneView, showingTabTools, showingTabProperties, showingLog, showingSceneTree, showingMaterialEditor;
};

#endif	/* EDITOR_H */
