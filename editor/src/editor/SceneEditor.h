//============================================================================
// Name        : Scene.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ( ͡° ͜ʖ ͡°)
// Description : Pyros Scene
//============================================================================

#ifndef SCENEEDITOR_H
#define	SCENEEDITOR_H

// nlohmann json is self-contained, so include it BEFORE Mouse3D.h below,
// which defines a global `isnan` macro that would corrupt json.hpp's own
// std::isnan() calls if json.hpp were parsed first (see the `#undef isnan`
// pattern in SceneEditor.cpp).
#include <Pyros3D/Utils/Json/json.hpp>
// Same alias the rest of the editor code uses (MaterialEditor.h defines it
// identically; redeclaring a namespace-scope alias to the same type is legal).
using json = nlohmann::json;

#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>   // LoadedSceneAssets
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>   // Body2DType/Shape2DType defaults
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIInput.h>
#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIDropdown.h>
#include <Pyros3D/Rendering/Components/UI/UIMenu.h>
#include <Pyros3D/Rendering/Components/UI/UIPopup.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioSource.h>
#include <Pyros3D/Audio/Sound.h>
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include "EditorDebugDraw.h"
#include "EditorIcons.h"
#include "UI/IUInterface.h"
#include "libgizmo/IGizmo.h"
#include "SceneObjects.h"
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include "Grid.h"
#include "AxisHelper.h"
#include "Helpers/LightHelper.h"
#include "Helpers/GameObjectHelper.h"
#include "Helpers/SoundHelper.h"
#include "Helpers/ParticleHelper.h"
#include "SelectedMaterial.h"
#include "UI/OpenDir.h"
#include "ProjectManager.h"
// Prefab references in a scene file, resolved outside the engine - shared
// with the player (shared/PrefabResolver.h).
#include "PrefabResolver.h"
// UI style/palette files, likewise resolved outside the engine and shared
// with the player (shared/UIStyleResolver.h).
#include "UIStyleResolver.h"
#include "UndoStack.h"
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <utility>
//#include "../UI/OpenDir.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>
#endif

using namespace p3d;

namespace GizmoFunction {
	enum {
		NONE = 0,
		TRANSLATION,
		ROTATION,
		SCALE
	};
};

class SceneEditor : public IUInterface {
    public:

        SceneEditor(uint32 documentId);
        virtual ~SceneEditor();

	// Must be called before Init() - Init() overrides IUInterface's pure
	// virtual Init(width,height), so the renderer choice can't just be an
	// extra Init() parameter; Init() reads this member instead.
	void SetUseDeferredRenderer(bool use) { pendingUseDeferredRenderer = use; }
	// Live toggle (unlike SetUseDeferredRenderer(), safe to call any time
	// after Init() - queues the switch rather than doing it inline; see
	// ApplyPendingRendererSwitchIfAny()'s comment for why a UI-callback-
	// timed WaitIdle() is a real deadlock risk here, not just a stall.
	void SwitchRenderer(bool useDeferred);
	// Actually swaps Renderer/EffectsManager/gbufferFBO for a queued
	// SwitchRenderer() call, matching Init()'s construction order and
	// Shutdown()'s teardown order. Must run at the very start of
	// ShowViewport(), before this frame's own rendering begins - never
	// call this from a UI callback directly.
	void ApplyPendingRendererSwitchIfAny();
	virtual void Init(const uint32 width, const uint32 height);
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void Update(const f64 time);
	virtual void Show();
	void ShowViewport();
	// Call for any frame in which ShowViewport() will NOT run - the Scene
	// View panel is closed, collapsed, or a background tab (the Material
	// Editor docks into the same tab bar, so this is the common case, not
	// an edge one). ShowViewport() is the only thing that refreshes the
	// cached viewport rect the raw-SDL mouse handlers gate on; left alone,
	// that rect keeps describing where the viewport *used to be* - which is
	// exactly where the window that replaced it now is - so scrolling or
	// dragging inside the Material Editor kept driving the editor camera.
	void NotifyViewportNotDrawn();
	void ShowHierarchy();
	virtual void ShowProperties();
	// GameObject + Scene menus. The scene's own New/Open/Save live in the
	// host's File menu instead - see ShowFileMenuItems.
	virtual void ShowMenubarOptions();
	void ShowFileMenuItems();
	void ShowViewOptions();
	// The canvas being authored: the one owning the selection, else the
	// scene's first. NULL when the scene has none.
	UICanvas* GetEditingCanvas() const;
	// Canvas bounds, canvas-unit grid and the selected element's rect,
	// drawn through the debug renderer in canvas space.
	void DrawCanvasOverlay(UICanvas* canvas, const Vec2& viewSize);
	virtual void ShowTools();
	virtual void Shutdown();

	uint32 GetDocumentId() const { return documentId; }
	// Filename stem, or "Untitled" when unsaved/new.
	std::string GetSceneDisplayName() const;
	// Rebind process-wide gizmo/physics debug hooks to this document.
	void BindSharedEditorHooks();

    private:

	void ShowRightMenu();
	void ShowAddObjectMenuItems();
	ImTextureID IconsTextureID() const;

	void SetObjectProperties(const Vec3 &Translation, const Vec3 &Rotation, const Vec3 &Scale);

	// Scene file handling. SceneSerializer writes *every* GameObject in the
	// SceneGraph, and this editor keeps its grid, cameras and helper icons in
	// the same graph as the user's content - so those are lifted out around a
	// save/load and put back afterwards, otherwise they end up in the file
	// and get duplicated on every load.
	void DetachEditorObjects(std::vector<std::shared_ptr<GameObject>> &out);
	void AttachEditorObjects(std::vector<std::shared_ptr<GameObject>> &saved);
	void RebuildHelpers();

	// Project-relative form of a path for anything the user reads (labels,
	// log lines). Falls back to the path as given when there is no project
	// open or the file lives outside it - see ProjectManager::DisplayPath.
	std::string DisplayPath(const std::string& path) const;

public:

	void NewScene(bool applyProjectDefaults = true);
	bool SaveSceneToFile(const std::string &path);
	bool LoadSceneFromFile(const std::string &path);
	// Recompiles every CustomShaderMaterial assigned to a GameObject in
	// this scene (LOD 0 only, matching AgentSetMaterial/AgentAssignMaterial's
	// own convention) that isn't in skipMaterials - those are handled by
	// the caller via their own open MaterialEditorDocument instead, so
	// their doc state (compiledShader ownership, generated GLSL text) stays
	// in sync. Used after a live Forward/Deferred renderer switch and after
	// a fresh scene load: SceneSerializer::BuildMaterial never chooses a
	// DEFERRED_GBUFFER branch at all, so scene-only custom materials would
	// otherwise keep rendering with a stale/wrong one.
	void RecompileOrphanedCustomMaterials(const std::string& projectRoot, bool deferredGBuffer,
	                                      const std::set<p3d::IMaterial*>& skipMaterials);
	// Recompile just the scene materials generated from `generatedGlslRel`,
	// so editing a material in the Material Editor immediately shows on the
	// objects already using it. Without this an edit only reached the
	// document's own material instance, and anything that got its own copy -
	// notably every material rebuilt by SceneSerializer on scene load - kept
	// the old shader until it was re-assigned to the mesh by hand. Matches
	// on CustomShaderMaterial::GetShaderFile(), which is the only durable
	// link back to the .mat that produced it. Returns how many it refreshed.
	int RefreshMaterialsFromGeneratedGlsl(const std::string& generatedGlslRel, const std::string& projectRoot,
	                                      bool deferredGBuffer, const std::set<p3d::IMaterial*>& skipMaterials);
	const std::string &GetScenePath() const { return scenePath; }
	// For a host that has to hand the physics world to something loading
	// objects into this document (Prefab.instantiate in the Lua host).
	p3d::Physics* GetPhysics() const { return physics; }
	bool IsSceneDirty() const { return sceneDirty; }
	void MarkSceneDirty() { sceneDirty = true; }
	void ClearSceneDirty() { sceneDirty = false; }
	bool IsShowingSaveDialog() const { return showingSceneDialog && sceneDialogIsSave; }
	void OpenSaveSceneDialog();
	bool TrySaveCurrentScene(); // true if saved now; false if Save As dialog opened
	bool HasUnsavedWork() const;
	// Gate: if dirty, open modal and remember `action`. Returns true if caller may proceed now.
	bool ConfirmUnsavedThen(int action, const std::string& path = std::string());
	// True while this document is already asking about (or acting on) unsaved
	// work. A second close request while that is up must not restart the
	// whole flow - macOS sends both SDL_WINDOWEVENT_CLOSE and SDL_QUIT for one
	// click of the close button, so every prompt was raised twice.
	bool HasPendingUnsavedPrompt() const;
	void DrawUnsavedChangesModal();
	void SetHostCallbacks(void (*onCloseProject)(), void (*onQuitApp)(),
		void (*onNewProject)() = NULL, void (*onOpenProject)() = NULL,
		void (*onQuitDiscardingUnsaved)() = NULL);
	void SetHostDocumentCallbacks(void (*onActivate)(SceneEditor*), void (*onRequestClose)(SceneEditor*),
		void (*onNewSceneDocument)() = NULL, void (*onOpenSceneDocument)(const std::string&) = NULL,
		void (*onOpenLuaScript)(const std::string&) = NULL,
		void (*onEditMaterialInline)(std::shared_ptr<p3d::IMaterial>, const std::string&) = NULL,
		std::string (*onAssignMaterialAsset)(const std::string&, int, const std::string&) = NULL);
	void EnterPlayMode();
	void StopPlayMode();
	bool IsPlaying() const { return playMode; }
	bool PlaceAssetInScene(const std::string& absolutePath);
	// Renders a .p3dm once and writes modelDir/.thumbnails/preview.png.
	// Returns absolute thumbnail path on success, empty on failure.
	std::string EnsureModelThumbnail(const std::string& p3dmPath, bool force = false);
	// Queue thumbnail work for ProcessPendingModelThumbnails (safe outside ImGui/Metal frame).
	void QueueModelThumbnail(const std::string& p3dmPath, bool force = false);
	void QueueMissingProjectModelThumbnails();
	// Process up to maxPerFrame queued thumbnails (call from Editor::Update before DrawUI).
	void ProcessPendingModelThumbnails(int maxPerFrame = 1);

	// ---- Agent API ---------------------------------------------------------
	// Executed on the editor's main thread by AgentServer (see AgentServer.h).
	// Each mutation mirrors what the corresponding UI menu action does, and
	// marks the scene dirty like the UI does. All return true on success;
	// on failure `errOut` carries a human-readable message.
	// Screen-space UI - the Op it calls lives in SceneEditOps.cpp.
	bool AgentAddUI(const std::string& objectName, const std::string& kind, const std::string& fontPath, std::string& errOut);
	bool AgentAddObject(const std::string& name, const std::string& parentName,
		const std::vector<f32>& position, const std::vector<f32>& rotation,
		const std::vector<f32>& scale, std::string& errOut);
	bool AgentMakeSprite2DLit(const std::string& name, std::string& errOut);
	// Attaches an Occluder2D by object name.
	bool AgentAddOccluder2D(const std::string& name, std::string& errOut);
	// Records the autoplay clip on the object's RenderingComponent. Starting
	// it is play mode's job, not this one's.
	// Places a .p3d2d in the scene. The only scene-side character command:
	// everything about what a character IS lives in its own editor.
	bool AgentAddCharacter2D(const std::string& characterRel, const std::string& objectName,
		const Vec3& position, std::string& errOut);
	bool AgentSetAutoPlay2D(const std::string& objName, const std::string& clipName,
		const bool loop, std::string& errOut);


	bool AgentSetSpritePivot(const std::string& name, const Vec2 &norm, std::string& errOut);
	bool AgentSliceSpritesheet(const std::string& name, const std::string& sheetPath,
		int cols, int rows, f32 fps, bool loop, std::string& errOut);
	bool AgentAddLayer2D(const std::string& name, std::string& errOut);
	bool AgentAddPhysics2D(const std::string& name, std::string& errOut,
		const uint32 bodyType = Body2DType::Dynamic, const Vec2 &size = Vec2(0.5f, 0.5f));
	// Viewport projection. A 2D scene is authored and judged through an
	// orthographic view, so this is not a debug affordance - it is how you
	// look at one.
	void AgentSetViewportOrthographic(bool ortho) { isPerspective = !ortho; }
	// Points the viewport camera straight down -Z at (x, y) and sets the
	// orthographic half-width. What a 2D scene needs to be looked at at all:
	// the default view is a 3D three-quarter angle at a fixed zoom, and
	// framing a flat scene by selecting something in it does not work - a
	// selection focus zooms to that object, not to the level.
	void AgentSetViewport2D(f32 x, f32 y, f32 orthoHalfWidth);
	// Points the editor camera straight at the XY plane, orthographic, with
	// the orbit state reset so a later pan or orbit starts from here.
	void LookAtPlaneXY(const f32 x, const f32 y);
	// One play-mode frame of the scene's own 2D view (follow, clamp, place),
	// driving the editor camera. Shares SceneMeta::View2D with the player, so
	// the preview cannot drift from what a built game does.
	void UpdateSceneView2D(const f32 dt, const f32 aspect);
	bool AgentIsViewportOrthographic() const { return !isPerspective; }
	bool AgentAddSprite(const std::string& name, const std::string& texturePath,
		const std::string& parentName, std::string& errOut);
	bool AgentAddPrimitive(const std::string& name, const std::string& shape, const json& p,
		const std::string& parentName, const json& color, std::string& errOut);
	bool AgentAddLight(const std::string& name, const std::string& type, const json& p,
		const std::string& parentName, std::string& errOut);
	bool AgentAddAudio(const std::string& name, const std::string& file, const json& p,
		const std::string& parentName, std::string& errOut);
	bool AgentAddParticles(const std::string& name, const json& p,
		const std::string& parentName, std::string& errOut);
	bool AgentAddPhysics(const std::string& name, const json& p, const std::string& parentName, std::string& errOut);
	bool AgentAddModel(const std::string& name, const std::string& modelFile, const std::string& parentName, std::string& errOut);
	bool AgentApplyUIStyle(const std::string& objectName, const std::string& stylePath, std::string& errOut);
	bool AgentRevertUIStyle(const std::string& objectName, std::string& errOut);
	bool AgentClearUIStyle(const std::string& objectName, std::string& errOut);
	// Reachable from the agent dispatch, so public with its siblings.
	std::vector<std::string> ListUIStyles() const;
	bool AgentExtractUIStyle(const std::string& objectName, const std::string& name, std::string& outPath, std::string& errOut);
	bool AgentSetUI(const std::string& objectName, const json& p, std::string& errOut);
	bool AgentCanvasDrag(const std::string& objectName, int handle, const std::vector<f32>& delta, std::string& errOut);
	bool AgentSelect(const std::string& name, std::string& errOut);
	bool AgentSetCanvasMode(bool on, std::string& errOut);
	bool AgentSetCamera(const std::string& name, const json& p, std::string& errOut);
	bool AgentAddCamera(const std::string& name, const std::vector<f32>& position,
		f32 fov, f32 nearPlane, f32 farPlane, bool active, std::string& errOut);
	bool AgentSetTransform(const std::string& name, const json& t, std::string& errOut);
	bool AgentSetTags(const std::string& name, const json& addTags, const json& removeTags, std::string& errOut);
	bool AgentRename(const std::string& name, const std::string& newName, std::string& errOut);
	bool AgentReparent(const std::string& name, const std::string& newParentName, std::string& errOut);
	bool AgentDuplicate(const std::string& name, std::string& errOut);
	// Prefabs - thin name-resolving wrappers over the Op* methods, so the
	// MCP bridge and the AI assistant reach exactly what the menus do.
	bool AgentCreatePrefab(const std::string& name, const std::string& prefabName,
		std::string& outRelPath, std::string& errOut);
	bool AgentInstantiatePrefab(const std::string& relPath, const Vec3& position,
		std::string& outName, std::string& errOut);
	bool AgentApplyPrefab(const std::string& name, std::string& errOut);
	bool AgentRevertPrefab(const std::string& name, std::string& errOut);
	bool AgentUnpackPrefab(const std::string& name, std::string& errOut);
	json AgentPrefabState();
	bool AgentDeleteObject(const std::string& name, std::string& errOut);
	bool AgentAttachScript(const std::string& name, const std::string& scriptFile, const json& data, std::string& errOut);
	// Detaches the first component of `componentType` (matching the "type"
	// strings AgentComponentToJson/find_game_objects_with_component use:
	// RenderingComponent, DirectionalLight, PointLight, SpotLight, Physics,
	// AudioSource, LuaComponent) found on the named object.
	bool AgentDetachComponent(const std::string& objectName, const std::string& componentType, std::string& errOut);
	bool AgentSetMaterial(const std::string& objectName, const json& fields, std::string& errOut);
	// Assigns an already-constructed material (e.g. from a Material Editor
	// document) onto a submesh directly, replacing whatever it had.
	bool AgentAssignMaterial(const std::string& objectName, int submeshIndex, std::shared_ptr<p3d::IMaterial> mat, std::string& errOut);
	// Viewport-image pixels to ImGui screen pixels, for injected mouse input.
	// False when the viewport has not been laid out yet this session.
	bool  AgentViewportToScreen(const f32 vx, const f32 vy, f32 &sx, f32 &sy) const;
	// Keeps alive what the loader creates but nothing else owns. Materials,
	// textures and renderables are held by the objects that reference them,
	// but skeleton and texture animations are reached only through a raw
	// void* back-reference on RenderingComponent - so passing NULL here (what
	// the editor did) let the loader's shared_ptr die at the end of the load
	// and left that pointer dangling. Saving the scene reads it back
	// (tinst->GetOwner()), which is a use-after-free. Traced with a print in
	// ~TextureAnimation: it fired during the load itself.
	LoadedSceneAssets sceneAssets;

	json  AgentSceneState();
	// Rewrites every asset path inside an agent/AI component payload to its
	// project-relative form. AgentComponentToJson and friends are free
	// functions with no project handle, so this runs as a pass over what
	// they produce rather than being threaded through all of them.
	void RelativizeAgentAssetPaths(json& j) const;

	// One object rather than the whole graph. scene_state on a real scene is
	// tens of kilobytes of JSON, which is a lot of an assistant's context to
	// spend when the question is "where is the player standing".
	bool AgentGetObject(const std::string& name, json& outObject, std::string& errOut);
	// Drives the editor's own selection, so the assistant can show the user
	// what it is talking about instead of only describing it.
	bool AgentSelectObject(const std::string& name, std::string& errOut);
	// `componentType` ("RenderingComponent", "ParticleSystem", ...) selects
	// that COMPONENT of the object rather than the object itself. Previews run
	// only for a selected component, so this is how an agent reaches them.
	bool AgentSelectObject(const std::string& name, const std::string& componentType,
		std::string& errOut);
	bool AgentSave(std::string& errOut);
	bool AgentSaveAs(const std::string& path, std::string& errOut);
	bool AgentLoadScene(const std::string& path, std::string& errOut);
	bool AgentPlay(std::string& errOut);
	bool AgentStopPlay(std::string& errOut);
	// Base64 PNG of the current viewport; empty string on failure.
	// liveViewport reads back the texture the Scene View is actually
	// showing this frame - the project's own renderer, Deferred
	// included. The default path re-renders through previewRenderer,
	// which is a ForwardRenderer, so it cannot show a Deferred-only
	// problem (and quietly disagreed with the screen whenever there
	// was one).
	std::string AgentScreenshot(bool liveViewport = false);
	std::string AgentScreenshotLiveViewport();
	// Last `maxLines` entries of the editor log ring ("" if none).
	static std::string AgentLogTail(int maxLines);
	// File modification time, or 0 if it cannot be determined.
	static time_t FileMtime(const std::string& path);
	// Reloads the scene from disk if its mtime changed since the last load
	// and the editor has no unsaved edits. Returns true if it reloaded.
	bool AgentReloadIfChanged();
	// ------------------------------------------------------------------------

	// ---- Undo/Redo ---------------------------------------------------------
	// For the host's Edit menu, which owns Duplicate/Delete now that they are
	// no longer buried in the scene-tree right-click alone.
	bool HasSelection() const { return SelectedSceneObject != NULL; }
	void DuplicateSelection() { DuplicateSelected(); }
	void DeleteSelection() { DeleteSelected(); }

	bool CanUndo() const { return sceneUndo.CanUndo(); }
	bool CanRedo() const { return sceneUndo.CanRedo(); }
	std::string UndoDescription() const { return sceneUndo.UndoDescription(); }
	std::string RedoDescription() const { return sceneUndo.RedoDescription(); }
	void Undo() { sceneUndo.Undo(); }
	void Redo() { sceneUndo.Redo(); }
	// Escape hatch for command types SceneEditor doesn't know about (e.g.
	// AssetCommands' filesystem undo commands) - asset operations are
	// triggered from the Assets panel, which is global chrome rather than
	// belonging to any one document, so they're routed onto the active
	// scene document's stack, same as everything else here.
	void PushUndoCommand(std::unique_ptr<IUndoableCommand> cmd) { sceneUndo.Push(std::move(cmd)); }
	// Lets the host route Ctrl+Z to this document whenever it is edited -
	// see UndoStack::onPush for why window focus alone is not enough.
	void SetUndoPushHook(std::function<void()> fn) { sceneUndo.onPush = std::move(fn); }
	SceneObjects* GetSceneObjects() const { return sceneObjects; }
	// Selects `obj` (NULL clears selection) and focuses it in the hierarchy
	// tree - used by undo/redo commands to restore selection the same way
	// ordinary create/duplicate does.
	void SelectAndFocusSceneObject(SceneObject* obj);

	// ---- Unified edit chokepoint (SceneEditOps.cpp) -------------------------
	// Every Op* method performs the mutation, applies the same selection/
	// camera fixups the UI already relies on, pushes exactly one undo
	// command, and marks the scene dirty - both interactive UI code (below)
	// and the Agent* methods above route through these so the two front
	// ends can no longer drift out of sync with each other.
	bool OpDeleteGameObject(uint32 objId, std::string& errOut);
	uint32 OpDuplicateGameObject(uint32 objId, std::string& errOut);
	bool OpReparentGameObject(uint32 childId, uint32 newParentId, std::string& errOut);
	bool OpRenameGameObject(uint32 objId, const std::string& newName, std::string& errOut);
	bool OpSetTransform(uint32 objId, const Vec3& pos, const Vec3& rot, const Vec3& scale, std::string& errOut);
	bool OpAssignMaterial(uint32 goId, int submeshIndex, std::shared_ptr<p3d::IMaterial> mat, std::string& errOut);

	// ---- Prefabs (SceneEditOps.cpp) ------------------------------------
	// A .prefab is one GameObject subtree saved as a reusable asset; a scene
	// stores instances of it as a reference plus their own name/transform/
	// tags, so editing the prefab updates every instance. See
	// SceneSerializer.h's prefab section for the file format and the
	// override rules.

	// Saves `objId`'s subtree to assets/prefabs/<name>.prefab (uniquified)
	// and turns that object into the first instance of it. `outRelPath`
	// receives the project-relative path written.
	bool OpCreatePrefab(uint32 objId, const std::string& name, std::string& outRelPath, std::string& errOut);
	// Instantiates `relPath` as a new scene root. Returns the new object's
	// id, 0 on failure.
	uint32 OpInstantiatePrefab(const std::string& relPath, const Vec3& position, std::string& errOut);
	// Rebuilds `objId` from its prefab, discarding local changes but keeping
	// its name/transform/tags.
	bool OpRevertPrefab(uint32 objId, std::string& errOut);
	// Breaks the link: the objects stay, the scene stores them in full, and
	// future edits to the prefab no longer reach them.
	bool OpUnpackPrefab(uint32 objId, std::string& errOut);
	// Writes `objId`'s current state back over its prefab, then rebuilds the
	// other instances in this scene that had no local changes of their own.
	// NOT undoable, and says so at the call site: it edits a project asset
	// that scenes other than this one may reference, the same way saving a
	// material does.
	bool OpApplyPrefab(uint32 objId, std::string& errOut);
	// Instances of `relPath` in this scene, excluding `skipId`, whose
	// modified state is `modified`. Used by Apply.
	std::vector<uint32> FindPrefabInstances(const std::string& relPath, uint32 skipId, bool modified);
	// Whether this instance still matches its source, ignoring the
	// overridable fields. Re-serializes the subtree and re-reads the prefab,
	// so it is a per-click question, not a per-frame one.
	bool PrefabInstanceIsModified(uint32 objId);
	// The prefab `objId` is an instance of, or empty.
	std::string PrefabPathOf(uint32 objId) const;
	json LoadPrefabJson(const std::string& relPath) const;
	// SerializeSubtree() with the instance link written into the root, so
	// the link survives every undo command. See the .cpp.
	std::string SnapshotSubtree(uint32 objId);
	// Resolves prefab references in a scene file on the way in, and writes
	// them back on the way out. Both wrap the engine calls, which know
	// nothing about any of this.
	std::string ExpandSceneFileForLoad(const std::string& path);
	void CollapseSceneFileAfterSave(const std::string& path);
	// Re-attaches instance links to the just-loaded objects, by root order.
	void RelinkPrefabInstancesAfterLoad(const std::vector<std::string>& rootPrefabPaths);
	// Shared tail of Revert and Apply's refresh: swaps `objId`'s subtree for
	// a fresh instantiation of `relPath` carrying the live object's
	// overrides. Returns the new object's id (0 on failure). No undo
	// bookkeeping - callers own that.
	uint32 RawRebuildPrefabInstance(uint32 objId, const std::string& relPath);

	// Low-level primitives shared by the Op* methods above and by
	// SceneCommands' Undo()/Redo() implementations - no undo bookkeeping,
	// callers own that.
	void RawDeleteSubtree(uint32 objId);
	SceneObject* RawInsertSubtree(const std::string& subtreeJson, uint32 parentId, bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper, const std::vector<uint32>* preferredIds = NULL);
	std::vector<uint32> RawCollectSubtreeIds(uint32 objId);
	void ApplyTransform(uint32 objId, const Vec3& pos, const Vec3& rot, const Vec3& scale);
	void RawAssignMaterial(uint32 goId, int submeshIndex, std::shared_ptr<p3d::IMaterial> mat);
	// Setter + PropertiesLight*/sceneCameras resync + MarkSceneDirty for one
	// field each - used both by the live widget in ShowProperties() and by
	// the ApplyClosureCommand it pushes on commit (see UndoValueEdit.h).
	void ApplyCameraFov(uint32 goId, f32 fov);
	void ApplyCameraOrthographic(uint32 goId, bool orthographic);
	void ApplyCameraOrthoSize(uint32 goId, f32 orthoSize);
	void ApplyCameraNear(uint32 goId, f32 nearPlane);
	void ApplyCameraFar(uint32 goId, f32 farPlane);
	void ApplyLightColor(uint32 lightId, const Vec4& color);
	void ApplyLightDirection(uint32 lightId, const Vec3& direction);
	void ApplyLightRadius(uint32 lightId, f32 radius);
	void ApplyLightInnerCone(uint32 lightId, f32 innerCone);
	void ApplyLightOuterCone(uint32 lightId, f32 outerCone);
	// Same role for a ParticleSystem, but the whole ParticleSystemDesc at
	// once rather than one field per method: every emitter setting lives in
	// that one struct, so a single before/after pair of descs undoes any of
	// them (including the sprite and the capacity) without a per-field
	// Apply* method each.
	void ApplyParticleDesc(uint32 psId, const ParticleSystemDesc& desc);
	// Applies `after` and pushes the pair as one undo command. Both descs are
	// by value, not by reference - see the .cpp for why that matters.
	void PushParticleDescCommand(uint32 psId, const ParticleSystemDesc before,
		const ParticleSystemDesc after, const std::string& label);
	// Captures `created`'s current subtree and pushes one
	// AddGameObjectCommand - shared tail of every "a new GameObject now
	// fully exists" flow (AgentAdd*, AddFormSubmit, PlaceAssetInScene,
	// CreateSceneCamera).
	void PushAddCommand(SceneObject* created);
	// Pushes one ReplaceGameObjectCommand wrapping `ownerId`'s CURRENT
	// (post-edit) subtree as the "after" state - shared tail of
	// AttachComponent/DetachComponent flows, which capture `beforeSnapshot`
	// themselves (via SerializeSubtree) right before making their edit.
	void PushReplaceCommand(uint32 ownerId, const std::string& beforeSnapshot, const std::string& description);
	// --------------------------------------------------------------------

	static std::string ModelThumbnailPath(const std::string& p3dmPath);
	// Assets-panel sound preview (non-spatialized, one-shot / stoppable).
	void PreviewAssetSound(const std::string& absolutePath);
	void StopAssetSoundPreview();
	bool IsAssetSoundPreviewPlaying() const;
	const std::string& GetAssetSoundPreviewPath() const { return assetSoundPreviewPath; }
	void SetProjectManager(ProjectManager* pm) { project = pm; }
	ProjectManager* GetProjectManager() const { return project; }
	void SetSharedAudioManager(AudioManager* mgr) { sharedAudioManager = mgr; }
#ifdef LUA_BINDINGS
	void SetSharedLua(sol::state* state) { sharedLua = state; }
	sol::state* GetSharedLua() const { return sharedLua; }
	bool AttachLuaScriptToGameObject(uint32 goId, const std::string& absoluteScriptPath);
	bool SetSceneMainScript(const std::string& absoluteOrRelativePath);
	void ClearSceneMainScript();
	const std::string& GetSceneMainScript() const { return sceneMainScriptPath; }
	bool EnsureAndBindSceneCompanionScript();
	bool DebugAutoAttachScript(const std::string& absoluteScriptPath);
#endif
	void SetAsActiveAudioDevice();
	std::string ResolveSoundPath(const std::string& path) const;
	// Same project-relative-then-cwd lookup as ResolveSoundPath(), for a
	// particle sprite (or any other loose asset referenced by path).
	std::string ResolveAssetPath(const std::string& path) const;
	// Drawn by Editor::DrawUI() unconditionally rather than from Show(), so
	// the modal survives the Scene View panel being closed.
	void DrawSceneFileDialog();
	void DrawSceneSettingsInProperties(); // when scene root is selected
	bool IsSceneRootSelected() const { return sceneRootSelected; }
	std::string ResolveScriptPath(const std::string& path) const;

	enum UnsavedAction {
		UnsavedNone = 0,
		UnsavedNewScene,
		UnsavedOpenDialog,
		UnsavedLoadPath,
		UnsavedCloseProject,
		UnsavedQuitApp,
		UnsavedOpenProject,
		UnsavedNewProject
	};

private:

	ProjectManager* project;
	std::string scenePath;
	bool sceneDirty;
	// Per-document undo/redo history (see UndoStack.h / the undo/redo plan).
	UndoStack sceneUndo;
	// Commit-boundary baseline for the Properties panel's Name InputText -
	// captured on IsItemActivated(), consumed on IsItemDeactivatedAfterEdit()
	// to push exactly one RenameGameObjectCommand per rename gesture.
	std::string undoBaselineName;
	// Same pattern (see UndoValueEdit.h) for the Properties panel's
	// Position/Rotation/Scale DragFloat3 widgets - independent of the
	// gizmo's own gizmoBaselinePos/Rot/Scale, since dragging a gizmo and
	// typing in these fields are different gestures that could in
	// principle interleave across frames.
	Vec3 undoBaselinePos, undoBaselineRot, undoBaselineScale;
	// Commit-boundary baselines for camera FOV/Near/Far and light color/
	// direction/radius/cone-angle widgets - see the UndoValueEdit call
	// sites in ShowProperties() for which one backs which widget.
	f32 undoBaselineFov, undoBaselineNear, undoBaselineFar, undoBaselineOrthoSize;
	Vec4 undoBaselineLightColor;
	Vec3 undoBaselineLightDirection;
	f32 undoBaselineLightRadius, undoBaselineLightInnerCone, undoBaselineLightOuterCone;
	// One baseline for the whole particle Properties panel rather than one
	// per widget: UndoValueEdit() only ever has a single active widget to
	// track at a time, and the command is a whole-desc pair anyway.
	ParticleSystemDesc undoBaselineParticleDesc;
	// Scene-level ambient colour - see SceneMeta::ambientLight. Kept in
	// sync with Renderer->SetGlobalLight() any time it changes (edited in
	// Properties), a scene loads, or SwitchRenderer() replaces Renderer
	// wholesale (a freshly constructed one resets to IRenderer's own
	// hardcoded default otherwise).
	Vec4 ambientLightColor = Vec4(0.2f, 0.2f, 0.2f, 0.2f);
	// Environment lighting - see SceneMeta::ambientIntensity/background. The
	// background is deliberately NOT the ambient colour: they used to be the
	// same 0.2 grey, so anything the lights missed landed exactly on the
	// background and disappeared into it.
	f32 ambientIntensity = 1.f;
	Vec4 backgroundColor = Vec4(0.10f, 0.11f, 0.13f, 1.f);
	// Ambient source: 0 flat colour, 1 three-band gradient - see
	// SceneMeta::ambientMode.
	int ambientMode = 0;
	Vec4 ambientSky = Vec4(0.32f, 0.38f, 0.45f, 1.f);
	Vec4 ambientEquator = Vec4(0.20f, 0.20f, 0.20f, 1.f);
	Vec4 ambientGround = Vec4(0.10f, 0.09f, 0.08f, 1.f);
	// Pushes ambientLightColor*ambientIntensity and backgroundColor at the
	// renderer. Called on load, on edit and after a renderer switch (a fresh
	// IRenderer starts on its own hardcoded defaults).
	void ApplyEnvironment();
	// mtime of the scene file as of the last successful load — AgentServer
	// uses this to detect external edits (AgentReloadIfChanged).
	time_t lastLoadMtime = 0;
	int pendingUnsavedAction;
	std::string pendingLoadPath;
	bool showUnsavedModal;
	// Deferred model thumbnail jobs (processed outside ImGui/Metal UI frame).
	std::vector<std::pair<std::string, bool> > pendingModelThumbnails;
	std::set<std::string> pendingModelThumbnailSet;
	bool awaitingSaveDialog;
	void (*hostCloseProject)();
	void (*hostQuitApp)();
	void (*hostQuitDiscardingUnsaved)();
	void (*hostNewProject)();
	void (*hostOpenProject)();
	void (*hostActivateDocument)(SceneEditor*);
	void (*hostRequestCloseDocument)(SceneEditor*);
	void (*hostNewSceneDocument)();
	// Set instead of hostNewSceneDocument when the host can ask which kind of
	// scene to create (3D / 2D / UI). Falls back to the plain one when null.
	void (*hostNewSceneKind)();
	// Opens a .p3d2d in the Character 2D editor. A character IS an asset, so
	// there is also a route to it by double-clicking one in the Assets panel;
	// this is the route from an object that already uses it.
	void (*hostOpenCharacter2D)(const std::string&);
	void (*hostOpenSceneDocument)(const std::string&);
	void (*hostOpenLuaScript)(const std::string&);
	void (*hostEditMaterialInline)(std::shared_ptr<p3d::IMaterial>, const std::string&);
	std::string (*hostAssignMaterialAsset)(const std::string&, int, const std::string&);
	void ExecutePendingUnsavedAction();
	// Open Scene and Save Scene As share one modal.
	bool showingSceneDialog, sceneDialogIsSave, sceneDialogBrowse;
	std::string sceneDialogPath;
	std::string sceneDialogError;
	void CreateGameObject(const std::string &name = "GameObject");
	void CreateSceneCamera();
	void SetActiveSceneCamera(uint32 sceneObjectId);
	void ClearActiveSceneCamera();
	GameObject* GetViewCameraGO() const;
	f32 GetViewFovDeg() const;

	// Script-controlled render camera override (set via Lua setRenderCamera()).
	GameObject* scriptRenderCamera;
	void SetScriptRenderCamera(GameObject* go);

	std::map<uint32, EditorCameraSettings> sceneCameras;
	uint32 activeSceneCameraId;
	ForwardRenderer* previewRenderer;
	PostEffectsManager* previewEffects;
	// Dedicated offscreen path for model Assets thumbnails — never resize
	// the camera-preview FBO (that was crashing Metal readback).
	ForwardRenderer* thumbRenderer;
	PostEffectsManager* thumbEffects;
	enum { previewWidth = 320, previewHeight = 180 };
	enum { thumbWidth = 128, thumbHeight = 128 };

	bool IsSceneCamera(uint32 id) const;
	void UnregisterSceneCamera(uint32 id);
	void RegisterSceneCamera(uint32 id, const EditorCameraSettings& settings = EditorCameraSettings());
	void ApplyCameraTagsFromScene();
	bool SaveEditorSidecar(const std::string& scenePath) const;
	bool LoadEditorSidecar(const std::string& scenePath);
	void BuildSceneCameraDebugList(std::vector<SceneCameraDebugEntry>& out) const;
	Texture* RenderCameraPreview(GameObject* camGO);

private:
	// Owner GO for the current selection (component → GetOwner()).
	GameObject* GetSelectedOwnerGameObject() const;
	// Offscreen render used by EnsureModelThumbnail (RGBA8 PNG on disk).
	bool RenderModelPreviewToRGBA8(const std::string& p3dmPath, std::vector<unsigned char>& outRGBA,
		uint32& outW, uint32& outH);
	std::string TreeLabel(SceneObject* obj) const;
	// One clickable viewport billboard (light bulb, camera, ...) exactly as
	// it was last drawn: screen-space rect in ImGui coordinates, plus the
	// scene object it selects and how far from the eye it sits (used to
	// resolve overlapping icons - nearest wins).
	struct ViewportIcon
	{
		ImVec2 min, max;
		uint32 sceneObjectId;
		f32 viewDepth;
	};
	void DrawSceneViewportIcons(const ImVec2& imgMin, const ImVec2& imgSize, GameObject* viewCam);
	bool TryPickViewportIcon(const Vec2& viewportMouse, uint32& outSceneObjectId) const;
	// Filled by DrawSceneViewportIcons every frame it draws, consumed by
	// TryPickViewportIcon. Deliberately one shared list rather than each
	// side projecting world positions itself: when the two did that
	// independently they drifted, and an icon's clickable area stopped
	// matching the glyph the user was aiming at.
	std::vector<ViewportIcon> viewportIcons;
	void OpenAddFormOnGameObject(uint32 goId, uint32 formType);
	void AddQuickLightOnGameObject(uint32 goId, uint32 formType);
	void ShowAddComponentMenu(uint32 goId);

	// Screen-space UI. See SceneEditOps.cpp.
	// Attaches a Layer2D to a GameObject, making its subtree one 2D layer.
	bool OpAddLayer2D(uint32 goId, std::string& errOut);
	// Attaches a Box2D rigid body.
	// bodyType is Body2DType (Static/Kinematic/Dynamic) and size is the box
	// half-extents. Defaulted so existing callers are unchanged, but exposed
	// because a level needs STATIC ground and there was no way to ask for it
	// outside the Properties panel - every agent-created body fell.
	bool OpAddPhysics2D(uint32 goId, std::string& errOut,
		const uint32 bodyType = Body2DType::Dynamic, const Vec2 &size = Vec2(0.5f, 0.5f));
	// Marks a shape as blocking 2D light, with no physics involved.
	bool OpAddOccluder2D(uint32 goId, std::string& errOut);
	// Opt a sprite into 2D lighting (distance falloff, no N.L).
	bool OpMakeSprite2DLit(uint32 goId, std::string& errOut);

	// Textured quad with alpha blending - an authoring shortcut, not a type.
	bool OpAddSprite(uint32 goId, const std::string& texturePath, std::string& errOut);
	// Cuts `sheetPath` into cols x rows cells, writes them as real PNGs
	// beside the sheet, and drives the object's sprite from them as a
	// TextureAnimation. Writes files rather than keeping frames in memory
	// because SerializeTextureRef() stores a path when a texture has one and
	// embeds the whole image when it does not - slicing in memory would bloat
	// every scene that used it.
	bool OpSliceSpritesheet(uint32 goId, const std::string& sheetPath,
		int cols, int rows, f32 fps, bool loop, std::string& errOut);

	// World axes (and, optionally, an adaptive XY grid) for a 2D scene. See
	// the implementation for why the 3D ground grid is not drawn there.
	void Draw2DReference();
	// World units per viewport pixel on the z = 0 plane, so editor handles can
	// be sized in pixels rather than in world units.
	f32 ViewportWorldPerPixel() const;

public:
	// Off by default: a 2D scene should open looking like the artwork, not
	// like graph paper. View > Show 2D Grid turns it on for layout work.
	bool showGrid2D = false;
private:
	static RenderingComponent* FindRenderingComponent(GameObject* go);

	static bool GetSpritePivot(RenderingComponent* rc, Vec2 &outNorm);
	bool OpSetSpritePivot(uint32 goId, const Vec2 &norm, std::string& errOut);
	bool OpAddUIComponent(uint32 goId, const std::string& kind, const std::string& fontPath, std::string& errOut);
	// Creates a "Canvas" GameObject with a UICanvas on it and selects it.
	void CreateCanvasForEditing();

public:
	// Turns the (empty, just-created) scene into a 2D one: marks it twoD so
	// the player skips the 3D pass, gives it the Canvas its content hangs
	// off, and opens it in Canvas (2D) Mode. See SceneMeta::twoD.
	void SetHostNewSceneKind(void (*fn)()) { hostNewSceneKind = fn; }
	void SetHostOpenCharacter2D(void (*fn)(const std::string&)) { hostOpenCharacter2D = fn; }
	void MakeTwoDScene();
	// A UI screen: 2D, orthographic, with a Canvas and canvas edit mode on.
	void MakeUIScene();

	bool IsTwoDScene() const { return sceneIsTwoD; }
	// What this scene will render with, which is not always what the project
	// asked for - a 2D scene is always forward (see SwitchRenderer).
	//
	// Accounts for a switch that has been requested but not applied yet:
	// SwitchRenderer only QUEUES, and the swap happens at the start of the
	// next ShowViewport(). Reading the live flag straight after asking for a
	// change therefore reports the value you just replaced.
	bool WillUseDeferredRenderer() const
	{
		return queuedRendererSwitch ? queuedUseDeferredRenderer : usingDeferredRenderer;
	}
	bool IsInPlayMode() const { return playMode; }
	// True when a camera GameObject is driving the viewport, which overrides
	// the scene's own 2D view.
	bool HasActiveSceneCamera() const { return activeSceneCameraId != 0; }

	// How this scene is framed when it runs. A 2D scene owns its viewpoint -
	// centre, zoom and what it follows - instead of needing a camera
	// GameObject parked in 3D space. Edited in Scene Settings, drawn as the
	// game-view rectangle in the viewport, and used by play mode, so what you
	// preview is what the player renders.
	//
	// Public because the Scene Settings panel and the agent bridge both edit
	// it directly, the same way ambientLightColor is reached.
	SceneMeta::View2D view2D;
private:
	// Round-trips through SceneMeta::twoD. A 2D scene is authored and played
	// the same either way - on its own as a menu or a 2D game, or shown over
	// a running 3D scene via the player's showOverlay().
	bool sceneIsTwoD;
	bool OpSetUIProperties(uint32 goId, const json& p, std::string& errOut);
	bool RawSetUIProperties(uint32 goId, const json& p, std::string& errOut);
	json CaptureUIProperties(GameObject* go);
	// UI styles - see shared/UIStyleResolver.h.
	bool OpApplyUIStyle(uint32 goId, const std::string& stylePath, std::string& errOut);
	bool OpExtractUIStyle(uint32 goId, const std::string& name, std::string& outPath, std::string& errOut);
	void RawSetUIStyleRef(uint32 goId, const std::string& ref);
	std::string UIStylePalettePath() const;
	bool OpClearUIStyle(uint32 goId, std::string& errOut);
	bool OpRevertUIStyle(uint32 goId, std::string& errOut);
	void RawSetUIStyleOverrides(uint32 goId, const std::vector<std::string>& keys);
	// Scratch for the Properties panel's "extract a style called ..." field.
	std::string uiStyleNameBuf;
	// Re-applies every element's style after a load, so editing a style file
	// or swapping the palette reaches scenes that were saved before it.
	int ReapplyUIStyles();
	// Polls the style files an open scene references and restyles when one
	// changes - see the implementation for why polling.
	void PollUIStyleFiles(const f64 time);
	std::map<std::string, std::filesystem::file_time_type> uiStyleMTimes;
	f64 lastUIStylePoll = 0.0;
	void PushUIPropertyUndo(uint32 goId, const json& before, const json& after, const char* what);
	std::string ResolveUIFontPath(const std::string& requested, std::string& errOut);
	static bool HasUIRect(GameObject* go);
	// Builds a composite widget (its component plus the child elements it
	// drives) onto an existing object. See its definition.
	bool AddUIWidget(GameObject* go, uint32 goId, const std::string& kind,
		const std::string& fontPath, std::string& errOut);
	// Prefab entries of a GameObject's context menu, and the modals they
	// raise (drawn from ShowHierarchy, not from inside the popup - see
	// DrawPrefabModals).
	void ShowPrefabMenu(uint32 goId);
	void DrawPrefabModals();
	// Build Game dialog (File menu). Same deferred-open pattern as the
	// prefab modals - see DrawPrefabModals.
	void DrawBuildModal();
	bool openBuildModal = false;
	std::string buildDialogOutputDir;
	std::string buildDialogTitle;
	std::string buildDialogSceneRel;
	std::string buildDialogError;
	std::string buildDialogResult;
	std::vector<std::string> buildDialogWarnings;
	int32 buildDialogWidth = 1280;
	int32 buildDialogHeight = 720;
	bool buildDialogFullscreen = false;
	bool openCreatePrefabModal = false;
	bool openApplyPrefabModal = false;
	uint32 prefabModalTargetId = 0;
	std::string prefabModalName;
	std::string prefabModalError;
	void DeleteGameObjectById(uint32 objId);
	void DeleteComponentById(uint32 objId);
	void DeleteSelected();
	uint32 DuplicateSelected();
	void PrepareGizmoForDraw(GameObject* viewCam);
	void HandleViewportGizmoInput(GameObject* viewCam);
	// Canvas mode's equivalent: click to select an element, drag its rect or
	// one of its eight handles. The 3D gizmo is not used here - it moves a
	// transform, and a UI element's transform is output, not input.
	void DispatchPlayModeUIInput();
	void DispatchUIClick(GameObject* clicked);
	// Everything else a canvas reported - a value changed, a field
	// submitted - to the handler named on the element.
	void DispatchUIEvents(UICanvas* canvas);
	void HandleCanvasInput(UICanvas* canvas);
	static void ApplyCanvasDrag(UIRect* rect, int handle, const Vec2& delta);
	// Viewport mouse in canvas units, using the same mapping UIRenderer's
	// ortho does. Returns false when the pointer is outside the viewport.
	bool ViewportMouseInCanvas(UICanvas* canvas, Vec2& out) const;
	// -1 none, 0..8 as hy*3+hx over the rect's corners/edges, 4 meaning the
	// body.
	int canvasDragHandle;
	Vec2 canvasDragLast;
	uint32 canvasDragGoId;
	json canvasDragBefore;
	Matrix LocalizeWorldRotation(const Matrix &worldDelta);
	void ApplyGizmoTransformToObject();
	// Picks whatever is under the cursor. `selectComponent` is the
	// double-click behaviour: a single click selects the GAMEOBJECT (which is
	// what you want to move, and what the gizmo acts on), a double click
	// drills into the component under it - the light, the emitter, the mesh.
	void ViewportPickAtMouse(bool selectComponent = false);
	void SyncPhysicsFromScene();
	void SyncPhysicsForGameObject(GameObject* go);
	void UpdateViewportMouse();
	void SyncTransformFromGameObject(SceneObject* obj);
	ImVec2 viewportImgMin;
	ImVec2 viewportImgSize;
	bool viewportOverlayValid;
	Vec2 viewportMouse;
	bool viewportMouseValid;
	bool viewportHovered;
	// Whether Dear ImGui currently considers the viewport image itself the
	// mouse's owner. The camera/gizmo/picking handlers below run off raw
	// InputManager (SDL) events, which know nothing about ImGui's window
	// stack, so without this they act on any event whose *coordinates* land
	// in the cached viewportImgMin/Size rect - including events that ImGui
	// has already routed to a window sitting on top of the viewport (the
	// Material Editor and its floating preview overlay being the usual
	// offenders: right-drag-panning the preview sphere also panned the
	// editor camera, and the wheel zoomed both). Set once per frame from
	// the viewport's own InvisibleButton, and IsItemActive() is folded in
	// deliberately so a drag that *started* on the viewport keeps steering
	// the camera after the cursor leaves it.
	bool viewportInputAllowed;

        virtual void MouseWheel(Event::Input::Info e);
        virtual void MouseLeftRelease(Event::Input::Info e);
        virtual void MouseLeftPress(Event::Input::Info e);
        virtual void MouseMiddlePress(Event::Input::Info e);
        virtual void MouseMiddleRelease(Event::Input::Info e);
        virtual void MouseRightRelease(Event::Input::Info e);
        virtual void MouseRightPress(Event::Input::Info e);
        virtual void MouseMove(Event::Input::Info e);
	virtual void KeyPressed(Event::Input::Info e);
	virtual void KeyReleased(Event::Input::Info e);

	virtual void UseCamera0();
	virtual void UseCamera1(bool invert = false);
	virtual void UseCamera2(bool invert = false);
	virtual void UseCamera3(bool invert = false);

public:
	// Public so the agent bridge can choose a manipulator; the toolbar and the
	// T/R/S shortcuts go through exactly these.
	void UseTranslationManipulator() { if (gizmo!=NULL) delete gizmo; gizmo = CreateMoveGizmo(); gizmo->SetLocation((localTransform?IGizmo::LOCATE_LOCAL:IGizmo::LOCATE_WORLD)); GizmoInUse = GizmoFunction::TRANSLATION; }
	void UseRotationManipulator() { if (gizmo!=NULL) delete gizmo; gizmo = CreateRotateGizmo(); gizmo->SetLocation((localTransform?IGizmo::LOCATE_LOCAL:IGizmo::LOCATE_WORLD)); GizmoInUse = GizmoFunction::ROTATION; }
	void UseScaleManipulator() { if (gizmo!=NULL) delete gizmo; gizmo = CreateScaleGizmo(); gizmo->SetLocation(IGizmo::LOCATE_LOCAL); GizmoInUse = GizmoFunction::SCALE; }
private:
	void UseLocalManipulator() { if (gizmo!=NULL) gizmo->SetLocation(IGizmo::LOCATE_LOCAL); localTransform = true; }
	void UseGlobalManipulator() { if (gizmo!=NULL && GizmoInUse!=GizmoFunction::SCALE) gizmo->SetLocation(IGizmo::LOCATE_WORLD); localTransform = false; }
	void CloseManipulator() { if (gizmo != NULL) delete gizmo; gizmo = NULL; }

        void SelectSceneObject(SceneObject* go);
        void DeselectSceneObject();

        bool _leftMouse, _middleMouse, _rightMouse, _mousePanned;

        Vec2 mouse;
        // Save Scene Rotation
        Quaternion rotation, rotX, rotY, qX, qY;
        Vec3 pos;
        // Mouse Coordinates
        Vec2 mouseCenter, mouseLastPosition, mousePosition;
        f32 counterX, counterY;

	// Scene
	SceneGraph* scene;
	// Chosen once in Init() from the project's Renderer setting (see
	// ProjectManager::ProjectSettings::rendererType) - Forward or Deferred,
	// used uniformly for both the edit viewport and Play Mode so a
	// project's materials render the same in both. Gizmos/axis-helper/
	// physics-debug draw independently of this (see ShowViewport()) so
	// they're unaffected either way; the grid IS a real SceneGraph object
	// and gets ShaderUsage::DeferredRenderer_Gbuffer added to its material
	// when Deferred is active (see Init()).
	IRenderer* Renderer;
	// Draws the scene's UICanvases over the finished 3D frame. Independent
	// of the Forward/Deferred switch - it composites into whatever target
	// the viewport is already assembling, so SwitchRenderer() never touches
	// it.
	UIRenderer* uiRenderer;
	bool usingDeferredRenderer;
	bool pendingUseDeferredRenderer; // set via SetUseDeferredRenderer() before Init() runs
	// Live post-Init() switch, queued by SwitchRenderer() and consumed by
	// ApplyPendingRendererSwitchIfAny() - deliberately separate from
	// pendingUseDeferredRenderer above, which only ever matters once, pre-Init().
	bool queuedRendererSwitch = false;
	bool queuedUseDeferredRenderer = false;
	// G-buffer backing Renderer when usingDeferredRenderer is true - owned
	// here (not by DeferredRenderer), built in Init(), resized in
	// OnResize()/ShowViewport(), torn down in Shutdown().
	Texture* gbufferDepth, *gbufferAlbedo, *gbufferSpecular, *gbufferNormal, *gbufferMatRough;
	FrameBuffer* gbufferFBO;
	void BuildGBuffer(uint32 width, uint32 height);
	void DestroyGBuffer();
	// Projection
	Projection projection, projectionOrtho;
        // Physics
        Physics* physics;
	// Audio device + listener (required before any AudioSource can load)
	AudioManager* audio;
	AudioManager* sharedAudioManager;
#ifdef LUA_BINDINGS
	sol::state* sharedLua;
	void PushLuaHostGlobals();
	void ResetLuaComponentsLifecycle();
	// Scene-level main script (not attached to a GameObject).
	std::string sceneMainScriptPath; // absolute preferred after resolve
	std::shared_ptr<LuaComponent> sceneMainScript;
	// The project's own main script (ProjectSettings::defaultMainScript).
	// Deliberately NOT a LuaComponent hanging off the scene the way
	// sceneMainScript is: it is never rebuilt on load, so its Lua state
	// survives a scene change, which is the entire reason a multi-scene game
	// needs one - boot flow, save data, anything spanning levels. Play mode
	// only, created in EnterPlayMode() and dropped in StopPlayMode(); the
	// editor never runs it, so a script cannot swap scenes underneath you
	// while you are editing.
	std::string projectMainScriptPath;
	std::shared_ptr<LuaComponent> projectMainScript;
	// Scene a script asked for via loadScene(). Applied between frames rather
	// than inside the call: the caller is running from a LuaComponent owned by
	// the very scene graph the load tears down, so switching there would free
	// the script mid-execution. Same deferral ApplyPendingRendererSwitchIfAny()
	// exists for. Empty means nothing pending.
	std::string pendingLoadSceneName;
	// True only while ApplyPendingSceneLoadIfAny() is swapping scenes for a
	// script. NewScene() stops play mode on the way in, which is right when
	// the *editor* loads a scene and wrong for an in-game transition - the
	// game would halt the moment it changed level.
	bool loadingSceneForPlay = false;
	void ApplyPendingSceneLoadIfAny();
	// Loads and Init()s the project script, if the project names one. Called
	// once per play session.
	void StartProjectMainScript();
	// How many LuaComponents the last InitSceneLuaComponents() initialised -
	// only used for EnterPlayMode()'s "no script attached" warning.
	int lastSceneLuaInitCount = 0;
	// Re-runs Init() on every LuaComponent in the current scene. Shared by
	// EnterPlayMode() and a mid-play loadScene(), which lands a brand new
	// scene graph whose components have never been initialised.
	void InitSceneLuaComponents();
	bool RebuildSceneMainScriptInstance();
	void InitSceneMainScript();
	void UpdateSceneMainScript(f64 time);
	void ResetSceneMainScriptLifecycle();
	void DrawGameObjectScriptProperties(uint32 goId);
	// Combo of assets/lua scripts only (not scenes/*.lua) + ASSET_REL drag-drop.
	void DrawScriptAssetPicker(const char* id, std::string& pathBuf);
	// Screen-space UI, inspected on the GameObject that carries it.
	void DrawUIComponentProperties(GameObject* go, uint32 goId);
	// Snapshot-based undo for component property edits, for components whose
	// state CaptureUIProperties() knows nothing about (Layer2D, Physics2D,
	// Occluder2D). Begin on the frame a drag starts, End when it is released,
	// so a drag is one undo entry rather than one per frame.
	void BeginComponentUndo(uint32 goId);
	void EndComponentUndo(uint32 goId, const char* what);
	std::string componentUndoBefore;
	void BeginUIUndo(uint32 goId);
	void EndUIUndo(uint32 goId, const char* what);
	json uiUndoBefore;
	std::string uiTexturePickerPath;
#endif
	bool sceneRootSelected;
	f64 lastListenerTime;
	// Transient Assets-panel preview (not placed in the scene).
	Sound* assetSoundPreview;
	std::string assetSoundPreviewPath;
	// Camera - Its a regular GameObject
	std::shared_ptr<GameObject> Camera, CameraPivot;

        // GameObject
        std::shared_ptr<GameObject> grid;
	// Handle
	std::shared_ptr<Renderable> gridhandle;
        // Rendering Component
        std::shared_ptr<RenderingComponent> rGrid;
        // Grid Material
        std::shared_ptr<GenericShaderMaterial> GridMaterial;

	// Gizmo Manipulator
	IGizmo* gizmo;
	bool localTransform;
	uint32 GizmoInUse;
	bool gizmoDragging;
	// Position/Rotation/Scale (matching _translation/_rotation/_scale) as of
	// the drag's mouse-down, used to push exactly one SetTransformCommand per
	// whole drag gesture on mouse-up rather than per dragged frame.
	Vec3 gizmoBaselinePos, gizmoBaselineRot, gizmoBaselineScale;

	struct PlayModeObjectSnapshot {
		Vec3 position, rotation, scale;
		Matrix localTransform, scaleTransform, globalRotation;
	};
	bool playMode;
	bool showPhysicsDebug;
	// Built so 2D colliders can be *drawn*; never stepped in edit mode, the
	// same way the Box3D world has its simulation disabled there.
	Physics2DWorld* physics2D;
	// Canvas edit mode. A screen-space UI has nothing to do with the 3D
	// scene it is drawn over, so authoring it through a perspective
	// viewport with a floor grid shows the layout at a size and a shape it
	// will never actually have. In this mode the world pass is suppressed,
	// the floor grid and axis widget go away, and the viewport becomes the
	// canvas: its bounds, a grid in canvas units, and the selected
	// element's own rect.
	bool uiEditMode;
	std::map<uint32, PlayModeObjectSnapshot> playModeSnapshots;
	// Edit-mode active camera restored when leaving play mode.
	uint32 playModeSavedCameraId;
	void ResolvePlayModeCamera();
	void SetEditorChromeVisible(bool visible);

	// Selected Scene Object
	SceneObject* SelectedSceneObject;

        // Scene Objects
	SceneObjects* sceneObjects;

	// Painter Pick
	PainterPick* Picking;

	// Object Properties
	Vec3 _translation, _rotation, _scale;

	// Type of Projection
	bool isOrtho;
	f32 zoomOrtho;

	uint32 Width, Height;
	PostEffectsManager* EffectsManager;
	Vec2 dim; // Real available dimensions
	Vec2 mPos; // Mouse Position

	bool isPerspective;
	// Whether the projection the viewport is ACTUALLY drawing with is
	// orthographic, and that projection's half-extents. isPerspective alone
	// does not answer this: it tracks the editor camera's own toggle, and
	// says "perspective" while a scene camera marked orthographic - which is
	// every 2D scene - is being looked through. The gizmo needs the real
	// answer for both the ray it builds and the size it draws at, and got
	// neither, which is why a 2D gizmo came out tiny and never highlighted.
	bool viewIsOrtho;
	f32 viewOrthoL, viewOrthoR, viewOrthoB, viewOrthoT;
	AxisHelper* axisHelper;
	f32 l, r, t, b;

	std::vector<uint64> selection; // Multiple selection
	f32 sub_selection; // No multiple selection

	int32 draggin_id;
	int32 droppin_id;
	int32 node_clicked;
	// Set by DrawTreeNodeWidgets' "Delete GameObject"/"Delete Component"
	// menu items instead of deleting immediately, and processed once
	// DrawNodes() has fully returned (see ShowHierarchy()) - DrawNodes()
	// walks sceneObjects->GetList() directly and keeps dereferencing the
	// SceneObject* for the node it just drew even after the popup menu
	// runs, so erasing/freeing it mid-walk is a use-after-free. 0 = none
	// (SceneObjects hands out ids starting at 1, same "0 = none/root"
	// convention ParentID already uses).
	uint32 pendingDeleteId;
	void DrawNodes(uint32 parentID = 0, uint32 depth = 0);
	void DrawTreeNodeWidgets(SceneObject* obj, bool node_open);
	bool IsInternalGameObject(p3d::GameObject* go) const;

	// Selected Mesh
	std::shared_ptr<SelectedMaterial> SelectedMeshMaterial;
	std::shared_ptr<RenderingComponent> SelectedRenderingComponent;
	std::shared_ptr<IMaterial> tempMaterial; RenderingMesh* SelectedMesh;
	void DeselectMesh();
	void SelectMesh(RenderingMesh* rmesh);

	// Debug Renderer
	DebugRenderer* debugRenderer;
	EditorDebugDraw* editorDebugDraw;
	void DrawBoundings(SceneObject* obj);
	void DrawBoundingBox(const Vec3 &min, const Vec3 &max, const Matrix &transform);
	void DrawBoundingSphere(const f32 radius, const Matrix &transform);
	void DrawBoundingCone(const f32 radius, const f32 height, const Matrix &transform);
	void DrawBoundingCylinder(const f32 radius, const f32 height, const Matrix &transform);

	// Properties
	f32 PropertiesLightRadius;
	Vec4 PropertiesLightColor;
	Vec3 PropertiesLightDirection;
	f32 PropertiesLightOutterCone, PropertiesLightInnerCone;
	// glPolygonOffset factor/units used while rendering the shadow map.
	// Seeded from the selected light, and used as the initial value when
	// shadows are first switched on.
	f32 PropertiesShadowBiasFactor, PropertiesShadowBiasUnits;
	// Shadow map setup. Unlike bias (applied in place), changing any of
	// these needs the map rebuilt, so the widget reports back whether the
	// caller should re-run EnableCastShadows() - the signature of which
	// differs per light type.
	int32 PropertiesShadowMapSize;
	f32 PropertiesShadowNear, PropertiesShadowFar;
	int32 PropertiesShadowCascades;
	bool ShowShadowProperties(ILightComponent* light, bool directional);
	void SeedShadowProperties(ILightComponent* light);

	// Edit-mode particle preview. An emitter left running in the viewport is
	// a permanent distraction (and a permanent cost) on a scene that may hold
	// dozens, so outside Play the only one that simulates is the one the user
	// is actually looking at: the selected emitter, or every emitter on the
	// selected GameObject. Edge-triggered on selection change rather than
	// forced every frame, so the Properties panel's own Play/Stop buttons
	// still mean something while the selection sits still.
	void UpdateParticlePreview();
	// Same rule for sprite-sheet animations: only the selected component
	// animates outside Play. See the implementation.
	void UpdateTextureAnimationPreview();
	void StartTextureAnimationsForPlayMode();
	uint32 texturePreviewSelectionId = 0;
	bool texturePreviewSynced = false;
	bool ParticleSystemPreviewsForSelection(ParticleSystem* ps) const;
	// Stops and clears every emitter in the scene, and makes the next
	// UpdateParticlePreview() re-evaluate from scratch. Used wherever the
	// world changes underneath the preview (scene load, leaving Play).
	void ResetParticlePreview();
	uint32 particlePreviewSelectionId;
	bool particlePreviewSynced;
	// The emitter currently previewing, if any - kept so a one-shot burst can
	// be re-fired once it empties out without walking every scene object
	// again each frame. Never dereferenced without re-checking it against the
	// live selection first (see UpdateParticlePreview).
	ParticleSystem* particlePreviewSystem;

	// Properties-panel draft state for the selected ParticleSystem: the two
	// fields that are applied on a button press rather than live (a sprite
	// path being typed, and a capacity whose every intermediate value would
	// otherwise reallocate the GPU buffer). Reseeded from the component
	// whenever the selection changes, tracked by propertiesParticleSeededId.
	// Sprite Animation section state (the sheet being sliced), same
	// applied-on-a-button-press pattern as the particle sprite path above.
	std::string propertiesSheetPath;
	int propertiesSheetCols = 1;
	int propertiesSheetRows = 1;
	f32 propertiesSheetFps = 12.f;

	std::string propertiesParticleTexturePath;
	int32 propertiesParticleMax;
	uint32 propertiesParticleSeededId;

	// Particle System helpers, shared by the Add form and the Properties
	// panel's "change the sprite" button.
	//
	// Loads `path` (project-relative or absolute) as a particle sprite,
	// falling back to the editor's own assets/particle_default.png when the
	// path is empty or won't load - a ParticleSystem with no texture samples
	// an unbound unit and draws as untextured squares, which reads as a bug
	// rather than as "you forgot to pick a sprite".
	std::shared_ptr<Texture> LoadParticleTexture(const std::string& path);
	// Copies a sprite chosen from anywhere on disk into the open project's
	// assets/textures (so the scene stays portable), and returns the path
	// that should actually be loaded. A no-op passthrough with no project open.
	std::string ImportParticleTexture(const std::string& path);
	// Creates the component, registers it in the tree, and gives it a
	// viewport icon - the three things every caller wants together.
	SceneObject* AttachParticleSystem(GameObject* go, const ParticleSystemDesc& desc);
	// Seeds `desc` from AddForm_particlePreset (0 = Default/plain, 1 = Fire,
	// 2 = Smoke, 3 = Explosion/one-shot burst).
	static void ApplyParticlePreset(ParticleSystemDesc& desc, int32 preset);

	// Add Form
	void AddFormSubmit();
	f32 AddForm_w, AddForm_h, AddForm_d, AddForm_p, AddForm_q, AddForm_oc, AddForm_ic;
	f32 AddForm_mass;
	int32 AddForm_sw, AddForm_sh, AddForm_r, AddForm_hscale;
	bool AddForm_sn, AddForm_fn, AddForm_cgo, AddForm_hs, AddForm_oe, AddForm_cs;
	bool AddForm_ghost;
	bool AddForm_stream;
	bool AddForm_loop;
	bool AddForm_spatialized;
	f32 AddForm_volume;
	string AddForm_go;
	Vec3 AddForm_dir;
	Vec4 AddForm_color;
	void ShowAddForm();
	bool showingAddFrom;
	bool openAddFormTrigger;
	uint32 showingAddFormType;
	string AddForm_modelPath;
	string AddForm_soundPath;
	// Particle System. Everything else in ParticleSystemDesc is left at its
	// default here and tuned live in the Properties panel afterwards - only
	// the sprite, the capacity and the emission style are awkward (or, for
	// the sprite, impossible) to change after the fact, so those are what
	// the creation form asks for. `AddForm_particlePreset` seeds the rest.
	string AddForm_particleTexturePath;
	int32 AddForm_particleMax;
	int32 AddForm_particlePreset;
#ifdef LUA_BINDINGS
	// Draft paths for Properties panel InputText (not menus of script names).
	std::string propertiesScriptAttachPath;
	std::string propertiesMaterialAssignError;
	std::string propertiesNewGoScriptName;
	bool openNewGoScriptModal;
	std::string propertiesNewGoScriptError;
#endif
	// Nodes to force open in the Scene Tree on the next frame, so a selection
	// made somewhere else (the viewport, attaching a script) is actually
	// visible. A SET and not one id: this used to open only the immediate
	// parent, which is enough in a flat scene but not in a layered one - a
	// light under World > TorchLight stayed hidden inside a collapsed World,
	// and selecting it by clicking its icon looked like it had done nothing.
	std::set<uint32> hierarchyForceOpenIds;
	// Marks `sceneObjectId` and every ancestor of it as force-open.
	void RevealInHierarchy(uint32 sceneObjectId);
	// The GameObject a scene object belongs to: itself when it already is one,
	// otherwise the nearest ancestor that is. Components (a light, a sound
	// emitter) are scene objects in their own right, parented to the object
	// hosting them.
	SceneObject* OwningGameObject(SceneObject* so) const;

	bool showDir;

	// Icons
	Texture *icons;

	uint32 documentId;
	bool shutDownDone;
};

#endif	/* SCENEEDITOR_H */
