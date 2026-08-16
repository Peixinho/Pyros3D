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
	#include "editor/UI/MaterialEditor.h"
	#include "editor/MaterialEditorDocument.h"
	#include "editor/MaterialPreview.h"
	#include "editor/SceneEditor.h"
#include "editor/ProjectManager.h"
#include "editor/CodeEditorDocument.h"
#include "editor/AgentServer.h"
#include <Pyros3D/Utils/Json/json.hpp>

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
	void EnsureWelcomeLogo();
	void DrawProjectDialogs();
	void DrawAssetsWindow();
	void DrawSceneTreeWindow();
	void DrawSceneViewWindow();
	void DrawSceneTabBar();
	void DrawScriptEditorWindows();
	void DrawMaterialEditorWindows();
	void UpdateWindowTitle();
	// Live renderer switch, shared by the Project Settings "Apply" button
	// and the "set_renderer" AgentServer command - applies to every open
	// scene tab (SceneEditor::SwitchRenderer() no-ops if already the
	// requested type). Does not touch project.json itself; callers decide
	// whether/how to persist the choice.
	void SwitchAllScenesRenderer(bool useDeferred);
	// Recompiles every live CustomShaderMaterial for the branch matching
	// useDeferred: open Material Editor tabs through their own docs (whose
	// compiledShader ownership and generated-GLSL text stay in sync),
	// scene-assigned ones through SceneEditor::RecompileOrphanedCustomMaterials.
	// Called at the end of SwitchAllScenesRenderer() - safe there because
	// program compile/link already runs synchronously from UI callbacks
	// (the Apply/Save buttons); it's the G-buffer FBO rebuild that stays
	// deferred to the next ShowViewport, not the shader recompilation.
	void RecompileCustomMaterialsForRenderer(bool useDeferred);
	bool OpenLuaScriptDocument(const std::string& absPath);
	void CloseLuaScriptDocument(uint32_t id);
	void CloseAllLuaScriptDocuments();
	CodeEditorDocument* FindLuaScriptDocument(const std::string& absPath);
	// Opens (or focuses, if already open) the .mat file as a dockable window.
	bool OpenMaterialDocument(const std::string& absPath);
	// Properties-panel "Edit Material" case: no backing file yet. Finds-or-
	// creates a document keyed by the live IMaterial* pointer identity so
	// clicking twice on the same submesh re-focuses the same window.
	MaterialEditorDocument* EditMaterialInline(std::shared_ptr<p3d::IMaterial> mat, const std::string& ownerLabel);
	void CloseMaterialDocument(uint32_t id);
	void CloseAllMaterialDocuments();
	MaterialEditorDocument* FindMaterialDocumentByPath(const std::string& absPath) const;
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
	// Local command server for external agents (MCP bridge). Started in
	// Init(), processed once per frame from Update() (main thread), stopped
	// in Shutdown(). See AgentServer.h.
	AgentServer agentServer;
	// Dispatches one agent command (cmd + args) to the editor API. Runs on
	// the main thread. Throws std::runtime_error on error.
	nlohmann::json HandleAgentCommand(const nlohmann::json& cmd);
	// Agent/MCP bridge helper: resolves a project-relative or absolute .mat
	// path and finds-or-opens it as a live MaterialEditorDocument. Returns
	// NULL and sets errOut on failure (no project, file doesn't exist, ...).
	MaterialEditorDocument* AgentOpenMaterial(const std::string& pathArg, std::string& errOut);
	// Which of a generated material's two compiled branches (see
	// MaterialCodegen.cpp) is active - driven by the project's Renderer
	// setting, not a per-scene choice (see ProjectManager::rendererType's
	// own comment on why).
	bool UseDeferredGBuffer() const { return project.GetSettings().rendererType == ProjectRendererType::Deferred; }
	// Loads/opens materialPath and assigns it onto the given object's
	// submesh in the active scene document. Shared by the agent bridge
	// (assign_material) and the Properties panel's material picker.
	bool AssignMaterialAsset(const std::string& objectName, int submeshIndex, const std::string& materialPath, std::string& errOut);
	static std::string HostAssignMaterialAsset(const std::string& objectName, int submeshIndex, const std::string& materialPath);

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

	// Material editors are top-level dock windows (peer of Scene View and
	// scripts), same convention as scriptDocs above - no permanent panel.
	std::vector<MaterialEditorDocument*> materialDocs;
	MaterialEditorDocument* activeMaterialDoc;
	uint32 nextMaterialDocId;
	// Forces focus on a material window after OpenMaterialDocument()/EditMaterialInline().
	uint32 pendingSelectMaterialDocId;

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
	static void HostEditMaterialInline(std::shared_ptr<p3d::IMaterial> mat, const std::string& ownerLabel);

	ProjectManager project;
	AudioManager* sharedAudio;
	std::vector<std::string> recentProjects;

	bool showingSceneView, showingTabTools, showingTabProperties, showingLog, showingSceneTree;
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
	bool openNewMaterialModal;
	std::string newMaterialName;
	std::string newMaterialError;
	int newMaterialKindCombo; // 0 = Generic, 1 = Custom
	std::map<std::string, Texture*> assetPreviewCache;
	Texture* welcomeLogo = NULL;
	// Freed after EndFrame so ImGui never samples a destroyed MTLTexture /
	// VkDescriptorSet from the same frame's draw list (Metal crash was
	// setFragmentTexture on a released id).
	std::vector<Texture*> deferredDestroyPreviews;
	void FlushDeferredPreviewDestroy();
	// MaterialPreview objects pulled out of a closing material document
	// (doc.preview.release()) and destroyed after the frame's ImGui
	// rasterization - their FBO color texture is still referenced by this
	// frame's draw list, the same mid-frame-free hazard as
	// deferredDestroyPreviews above.
	std::vector<MaterialPreview*> deferredDestroyPreviewRenderers;
	void FlushDeferredPreviewRenderers();
	// Pulls doc's sphere preview (if any) out of the doc so it can be
	// destroyed after the frame's rasterization instead of mid-frame.
	void QueuePreviewForDeferredDestroy(MaterialEditorDocument* doc);
#ifdef LUA_BINDINGS
	void InitLuaHost();
	sol::state lua;
	bool luaReady;
#endif
};

#endif	/* EDITOR_H */
