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
	#include "editor/UI/AnimationEditor.h"
	#include "editor/MaterialEditorDocument.h"
	#include "editor/AnimationEditorDocument.h"
	#include "editor/UI/Character2DEditor.h"
	#include "editor/Character2DDocument.h"
	#include "editor/MaterialPreview.h"
	#include "editor/SceneEditor.h"
#include "editor/ProjectManager.h"
#include "editor/CodeEditorDocument.h"
#include "editor/AgentServer.h"
#include "editor/AIAssistant.h"
#include "editor/UndoStack.h"
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
	// What an Assets tile shows for a .mat. A material's kind and edit mode
	// are fixed at creation but only recorded inside the file, so the panel
	// has to read it; parsing every .mat's JSON every frame would be silly,
	// so results are cached and only re-read when the file's mtime moves.
	enum class AssetMaterialKind { Unknown, Generic, CustomText, CustomNodes };
	struct AssetMaterialInfo { AssetMaterialKind kind; long long mtime; };
	std::map<std::string, AssetMaterialInfo> assetMaterialKinds;
	AssetMaterialKind GetAssetMaterialKind(const std::string& absPath);

	void DrawScriptEditorWindows();
	void DrawMaterialEditorWindows();
	void DrawAnimationEditorWindows();
	// "Save As" for an animation that has no path yet (File > New Animation)
	// or that the user explicitly re-targets. A name field rather than a
	// native file dialog, matching how every other asset is created here
	// (New Script / New Material) - the destination folder is always
	// assets/animations.
	void DrawSaveAnimationAsModal();
	// Closing a script or material tab with unsaved edits parks its id in
	// one of these and opens the shared prompt instead of closing outright -
	// both kinds close through the same "x on the tab" path, and both used
	// to discard silently. Exactly one is non-zero at a time (the prompt is
	// modal, so a second close cannot be requested while one is pending);
	// 0 means nothing pending. Resolved by DrawUnsavedDocumentModal().
	uint32 pendingCloseScriptId;
	uint32 pendingCloseMaterialId;
	uint32 pendingCloseAnimationId;
	// Queues a close, prompting first when the document is dirty.
	void RequestCloseScriptDocument(CodeEditorDocument* doc, std::vector<uint32_t>& closeIds);
	void RequestCloseMaterialDocument(MaterialEditorDocument* doc, std::vector<uint32_t>& closeIds);
	void RequestCloseAnimationDocument(AnimationEditorDocument* doc, std::vector<uint32_t>& closeIds);
	void DrawUnsavedDocumentModal();
	// Fills the Properties panel with the focused material document's
	// property sheet (textures / settings / Generic inspector - see
	// MaterialEditor::DrawProperties), returning true when it did. False
	// means no material has focus and Properties falls back to the scene
	// selection, which is what it has always shown. Installed on
	// PropertiesTab via SetOverrideDrawer() in Init().
	bool DrawActiveMaterialProperties();
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
	// Resolves a scene 'path' argument from the agent/AI API: project-relative
	// (the form this API hands out) or absolute, either way to something the
	// scene loader can open.
	std::string AgentScenePathArg(const std::string& pathArg) const;

	bool OpenLuaScriptDocument(const std::string& absPath);
	// Re-reads an already-open script window from disk. Called after a tool
	// writes the file, so the editor is not sitting on stale text it would
	// happily save back over the change.
	void ReloadScriptDocumentFromDisk(const std::string& absPath);
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

	// Opens (or focuses) a .p3da as an Animation Editor window. Also
	// accepts a .p3dm: that opens an empty, unsaved animation document
	// already bound to that rig, which is how you start authoring for a
	// character that has no clips yet.
	bool OpenAnimationDocument(const std::string& absPath);
	// Empty document, no file. `meshPath` may be empty.
	AnimationEditorDocument* NewAnimationDocument(const std::string& meshPath);
	void CloseAnimationDocument(uint32_t id);
	void CloseAllAnimationDocuments();
	AnimationEditorDocument* FindAnimationDocumentByPath(const std::string& absPath) const;
	// Writes the document to `absPath` (its own path when empty) and
	// registers the rig binding. Returns false and logs on failure.
	bool SaveAnimationDocument(AnimationEditorDocument* doc, const std::string& absPath);
	// ---- 2D characters (.p3d2d) ---------------------------------------
	// Same top-level-dock-window convention as every other document kind.
	// A character is an asset that owns its own bones, artwork and clips, so
	// unlike the 2D rigs this replaces there is nothing to bind and no scene
	// involved - opening one is opening a file.
	bool OpenCharacter2DDocument(const std::string& absPath);
	Character2DDocument* NewCharacter2DDocument();
	void CloseCharacter2DDocument(uint32_t id);
	void CloseAllCharacter2DDocuments();
	Character2DDocument* FindCharacter2DDocumentByPath(const std::string& absPath) const;
	bool SaveCharacter2DDocument(Character2DDocument* doc, const std::string& absPath);
	void DrawCharacter2DEditorWindows();
	void RequestCloseCharacter2DDocument(Character2DDocument* doc, std::vector<uint32_t>& closeIds);
	// Every texture in the project, for the sprite picker.
	void BuildCharacter2DTextureChoices(std::vector<Character2DEditor::TextureChoice>& out) const;
	// Host hook handed to each scene document, so "Edit Character..." on a
	// placed character opens its asset.
	static void HostOpenCharacter2D(const std::string& absPath);

	// Every skinned .p3dm in the project, as (label, absolute path), for the
	// Animation Editor's rig picker.
	void BuildAnimationMeshChoices(std::vector<AnimationMeshChoice>& out) const;
	// project.json's animationBindings map (see ProjectSettings).
	std::string LookupAnimationMeshBinding(const std::string& animAbsPath) const;
	void StoreAnimationMeshBinding(const std::string& animAbsPath, const std::string& meshAbsPath);
	// Blend setup persistence, keyed the same way as the rig binding.
	void LoadAnimationBlend(AnimationEditorDocument& doc) const;
	void StoreAnimationBlend(const AnimationEditorDocument& doc);
	// Picks the rig whose bone names best match a clip's channel names -
	// used when a .p3da is opened with no stored binding, so the common
	// case (one character, one animation folder) needs no manual pick.
	std::string GuessAnimationMesh(const AnimationEditorDocument& doc) const;
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
	// Brings the window forward. Called whenever a close is refused so that
	// the thing being asked can actually be seen - see the implementation.
	void RaiseEditorWindow();
	// Set when quit is blocked by something no dialog owns (a script that
	// will not save, a project file that will not write). Drives the
	// "Cannot Quit" modal, which is the escape hatch from what used to be a
	// silent refusal.
	bool openQuitBlockedModal = false;
	// Quit attempts since the current close gesture began. Two is enough to
	// tell "the save worked, carry on" from "this will never become clean".
	int quitAttempts = 0;
	void DrawQuitBlockedModal();

	// Which document Ctrl+Z/Ctrl+Shift+Z should act on - tracks whichever of
	// {Scene View, Scene Tree, Properties} vs a Material Editor window last
	// had focus (Properties is scene-only chrome, so it doesn't need its
	// own tracking: it only ever gets focus while a scene document is
	// already the last-focused kind). Defaults to Scene so undo works
	// immediately in the common case of a single scene document open.
	enum class FocusedDocKind { Scene, Material, Animation, Character2D };
	FocusedDocKind lastFocusedDocKind = FocusedDocKind::Scene;
	// Project-wide settings (name, renderer type) aren't "a document" the
	// way a scene/material tab is, so they get their own small stack rather
	// than being folded into FocusedDocKind - routed to whenever the
	// Project Settings modal is the thing currently open (see DrawUI()'s
	// Ctrl+Z handling), independent of which tab last had focus.
	UndoStack projectUndo;

	TabLog* tabLog;
	// Local command server for external agents (MCP bridge). Started in
	// Init(), processed once per frame from Update() (main thread), stopped
	// in Shutdown(). See AgentServer.h.
	AgentServer agentServer;
	// Dispatches one agent command (cmd + args) to the editor API. Runs on
	// the main thread. Throws std::runtime_error on error.
	// Pending synthetic key releases: (key code, frames remaining). See the
	// "key" agent command in Editor.cpp.
	std::vector<std::pair<uint32, int> > pendingKeyReleases;

	nlohmann::json HandleAgentCommand(const nlohmann::json& cmd);
	// Agent/MCP bridge helper: resolves a project-relative or absolute .mat
	// path and finds-or-opens it as a live MaterialEditorDocument. Returns
	// NULL and sets errOut on failure (no project, file doesn't exist, ...).
	MaterialEditorDocument* AgentOpenMaterial(const std::string& pathArg, std::string& errOut);
	// Same find-or-load as AgentOpenMaterial, but for callers that only want
	// the compiled IMaterial (to assign it somewhere) and must NOT pop open
	// a Material Editor tab as a side effect - see AssignMaterialAsset and
	// MaterialEditorDocument::hiddenFromTabs.
	MaterialEditorDocument* LoadMaterialQuietly(const std::string& pathArg, std::string& errOut);
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
	// AI Assistant panel - chats with LLM providers and executes the model's
	// tool calls through HandleAgentCommand (the same dispatcher the MCP
	// bridge drives). Tool execution is pumped on the main thread from
	// Update(); the HTTP work runs on the client's worker thread.
	AIAssistantTab* tabAI;
	// Short text summary of the open project/scene, sent to the model as
	// context when the user opts in. "" when no project is open.
	std::string BuildAIContext();
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

	// Character (2D) editors.
	std::vector<Character2DDocument*> character2DDocs;
	Character2DDocument* activeCharacter2DDoc;
	uint32 nextCharacter2DDocId;
	uint32 pendingSelectCharacter2DDocId;
	// Save As modal state, same shape as the animation one.
	bool openSaveCharacter2DAsModal;
	uint32 saveCharacter2DAsDocId;
	std::string saveCharacter2DAsName;
	std::string saveCharacter2DAsError;
	bool saveCharacter2DAsThenClose;
	void DrawSaveCharacter2DAsModal();
	// Deferred for the same mid-frame-free reason the animation ones are.
	std::vector<Character2DDocument*> deferredDestroyCharacter2DDocs;

	// Animation editors, same top-level-dock-window convention as material
	// and script documents.
	std::vector<AnimationEditorDocument*> animationDocs;
	AnimationEditorDocument* activeAnimationDoc;
	uint32 nextAnimationDocId;
	uint32 pendingSelectAnimationDocId;
	// Save As modal state (see DrawSaveAnimationAsModal).
	bool openSaveAnimationAsModal;
	uint32 saveAnimationAsDocId;
	std::string saveAnimationAsName;
	std::string saveAnimationAsError;
	// Set when the Save As was triggered by closing a dirty document, so a
	// successful save also closes it.
	bool saveAnimationAsThenClose;
	// Closed animation documents, destroyed after the frame's ImGui
	// rasterization rather than mid-frame: each owns an AnimationPreview
	// whose FBO color texture is still referenced by this frame's draw list
	// (same hazard as deferredDestroyPreviewRenderers).
	std::vector<AnimationEditorDocument*> deferredDestroyAnimationDocs;
	void FlushDeferredAnimationDocs();
	void FlushDeferredCharacter2DDocs();
	// New Animation modal (File menu) - just a rig choice, since the clips
	// start empty and the file is only written on Save.
	bool openNewAnimationModal;
	std::string newAnimationMeshPath;
	// Import Animation modal (File menu): converts an fbx/dae/... into
	// assets/animations/<name>.p3da through AssimpImporter's --animation
	// mode. Separate from the model import path on purpose - the same
	// source file usually holds both, and which one you meant is not
	// something the extension can tell us.
	bool openImportAnimationModal;
	bool openImportAnimationBrowse;
	std::string importAnimationSource;
	std::string importAnimationName;
	std::string importAnimationError;
	void DrawAnimationAssetModals();
	// Forces focus on a material window after OpenMaterialDocument()/EditMaterialInline().
	uint32 pendingSelectMaterialDocId;

	SceneEditor* CreateSceneDocument();

	// Points Ctrl+Z at whichever document was last EDITED. Called once per
	// document at construction; see UndoStack::onPush for why tracking
	// window focus alone sent undos to the wrong document.
	void BindUndoRouting(SceneEditor* doc);
	void BindUndoRouting(MaterialEditorDocument* doc);
	void BindUndoRouting(AnimationEditorDocument* doc);
	void BindUndoRouting(Character2DDocument* doc);
	void DestroySceneDocument(SceneEditor* doc);
	void CloseAllSceneDocuments();
	void SetActiveSceneDocument(SceneEditor* doc);
	SceneEditor* FindSceneDocumentByPath(const std::string& absPath) const;
	bool OpenSceneDocument(const std::string& absPath);
	bool OpenNewSceneDocument();
	// Same, but the scene starts marked twoD with a Canvas, in 2D mode.
	bool OpenNew2DSceneDocument();
	bool OpenNewUISceneDocument();
	static void HostNewSceneKind();
	// Set by the New Scene menu item; opens the 3D/2D/UI chooser next frame.
	bool openNewSceneKindModal = false;
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

	// Folder the Assets panel is showing. The panel used to list every file
	// under assets/ recursively in one flat grid, which stops being usable the
	// moment a project has more than a screenful - and separates a model from
	// the textures sitting next to it. Empty means the project root, which
	// lists assets/ and scenes/.
	std::string assetBrowseDir = "assets";
	int assetFilter = 0;

	ProjectManager project;
	AudioManager* sharedAudio;
	std::vector<std::string> recentProjects;

	bool showingSceneView, showingTabTools, showingTabProperties, showingLog, showingSceneTree;
	bool showingTabAI;
	bool showingAssets;
	bool assetsWindowHovered;

	bool openNewProjectModal, openOpenProjectModal;
	// Set when "Open Recent" had to defer past the unsaved-work prompt, so
	// HostOpenProject reopens that project instead of the browse dialog.
	std::string pendingRecentProjectPath;
	bool openProjectSettingsModal;
	std::string newProjectDir, newProjectName, openProjectPath;
	std::string projectSettingsName;
	std::string projectDialogError;
	bool newProjectBrowseDir, openProjectBrowse;
	std::string lastDropStatus;

	std::string selectedAssetRel;
	std::string pendingDeleteAssetRel;
	bool openDeleteAssetModal;
	// Warm ember palette shared with DrawWelcomeScreen - see its definition.
	static void ApplyPyrosTheme();
	void ShowAssetCreateMenuItems();
	bool openNewScriptModal;
	std::string newScriptName;
	std::string newScriptError;
	bool openNewMaterialModal;
	std::string newMaterialName;
	std::string newMaterialError;
	int newMaterialKindCombo; // 0 = Generic, 1 = Custom
	int newMaterialCustomModeCombo; // 0 = Node Graph, 1 = Text - only asked/used when newMaterialKindCombo == 1
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
