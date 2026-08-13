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
#include <map>
#include <vector>
#include <imgui_impl_sdl2.h>
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
	#include <imgui_impl_opengl3.h>
#endif

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Texture/Texture.h>
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
#include "editor/ProjectManager.h"
#include "editor/CodeEditorDocument.h"

#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#ifndef PYROS_EXAMPLES_PATH
#define PYROS_EXAMPLES_PATH ""
#endif
#define _STR_EX(path) #path
#define STR_EX(path) _STR_EX(path)
#endif

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
	void DrawWelcomeScreen();
	void DrawProjectDialogs();
	void DrawAssetsWindow();
	void DrawSceneTreeWindow();
	void DrawSceneViewWindow();
	void DrawSceneTabBar();
	void DrawScriptEditorWindows();
	void UpdateWindowTitle();
	bool OpenLuaScriptDocument(const std::string& absPath);
	void CloseLuaScriptDocument(uint32_t id);
	void CloseAllLuaScriptDocuments();
	CodeEditorDocument* FindLuaScriptDocument(const std::string& absPath);
	bool CreateNewProject(const std::string& parentDir, const std::string& name);
	bool OpenProjectFromPath(const std::string& path);
	void CloseProject();
	void CloseProjectImmediate();
	void ProcessPendingFileDrops();
	void ClearAssetPreviews();
	Texture* GetAssetPreviewTexture(const std::string& absPath);
	void LoadRecentProjects();
	void SaveRecentProjects();
	void AddRecentProject(const std::string& projectPathOrJson);
	static std::string RecentProjectsFilePath();
	bool EditorAllowWindowClose();
	static bool EditorOnWindowClose();
	static void HostCloseProject();
	static void HostQuitApp();
	static void HostQuitDiscardingUnsaved();
	static void HostNewProject();
	static void HostOpenProject();
	Editor();

private:

	static Editor* instance;
	enum { kMaxRecentProjects = 10 };

	void DrawUI();
	void PromptQuitWithUnsaved();

	TabLog* tabLog;
	
	PropertiesTab* tabProperties;
	ToolsTab* tabTools;
	// Set by LoadDefaultLayout(); consumed by the next DrawUI().
	bool resetLayout;

	SceneEditor* sceneView; // active document
	std::vector<SceneEditor*> sceneDocs;
	uint32 nextSceneDocId;
	std::vector<SceneEditor*> pendingCloseSceneDocs;

	// Script editors are top-level dock windows (peer of Scene View), not nested tabs.
	std::vector<CodeEditorDocument*> scriptDocs;
	CodeEditorDocument* activeScriptDoc;
	uint32 nextScriptDocId;
	// Forces focus on a script window after OpenLuaScriptDocument().
	uint32 pendingSelectScriptId;
	// Center dock node from BuildDefaultLayout — new script windows dock here.
	ImGuiID dockCenterId;

	SceneEditor* CreateSceneDocument();
	void DestroySceneDocument(SceneEditor* doc);
	void CloseAllSceneDocuments();
	void SetActiveSceneDocument(SceneEditor* doc);
	SceneEditor* FindSceneDocumentByPath(const std::string& absPath) const;
	bool OpenSceneDocument(const std::string& absPath);
	bool OpenNewSceneDocument();
	void FlushPendingSceneDocumentCloses();
	bool AnySceneHasUnsavedWork() const;
	bool AnySceneDocumentHasUnsavedWork() const;
	bool SaveAllDirtyScripts();
	void FinishQuitIfClean();
	static void HostActivateSceneDocument(SceneEditor* doc);
	static void HostRequestCloseSceneDocument(SceneEditor* doc);
	static void HostNewSceneDocument();
	static void HostOpenSceneDocument(const std::string& absPath);
	static void HostOpenLuaScript(const std::string& absPath);

	ProjectManager project;
	AudioManager* sharedAudio;
	std::vector<std::string> recentProjects;

	bool showingSceneView, showingTabTools, showingTabProperties, showingLog, showingSceneTree, showingMaterialEditor;
	bool showingAssets;
	bool assetsWindowHovered;

	bool openNewProjectModal, openOpenProjectModal;
	bool openProjectSettingsModal;
	std::string newProjectDir, newProjectName, openProjectPath;
	std::string projectSettingsName;
	std::string projectDialogError;
	bool newProjectBrowseDir, openProjectBrowse;
	std::string lastDropStatus;

	std::string selectedAssetRel;
	std::string pendingDeleteAssetRel;
	bool openDeleteAssetModal;
	bool openNewScriptModal;
	std::string newScriptName;
	std::string newScriptError;
	std::map<std::string, Texture*> assetPreviewCache;
	// Freed after EndFrame so ImGui never samples a destroyed MTLTexture /
	// VkDescriptorSet from the same frame's draw list (Metal crash was
	// setFragmentTexture on a released id).
	std::vector<Texture*> deferredDestroyPreviews;
	void FlushDeferredPreviewDestroy();
#ifdef LUA_BINDINGS
	void InitLuaHost();
	sol::state lua;
	bool luaReady;
#endif
};

#endif	/* EDITOR_H */
