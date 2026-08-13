//============================================================================
// Name        : Scene.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ( ͡° ͜ʖ ͡°)
// Description : Pyros Scene
//============================================================================

#ifndef SCENEEDITOR_H
#define	SCENEEDITOR_H

#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
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
#include "SelectedMaterial.h"
#include "UI/OpenDir.h"
#include "ProjectManager.h"
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

	virtual void Init(const uint32 width, const uint32 height);
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void Update(const f64 time);
	virtual void Show();
	void ShowViewport();
	void ShowHierarchy();
	virtual void ShowProperties();
	virtual void ShowMenubarOptions();
	void ShowViewOptions();
	virtual void ShowTools();
	virtual void Shutdown();

	uint32 GetDocumentId() const { return documentId; }
	// Filename stem, or "Untitled" when unsaved/new.
	std::string GetSceneDisplayName() const;
	// Rebind process-wide gizmo/physics debug hooks to this document.
	void BindSharedEditorHooks();

    private:

	void ShowRightMenu();
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

public:

	void NewScene(bool applyProjectDefaults = true);
	bool SaveSceneToFile(const std::string &path);
	bool LoadSceneFromFile(const std::string &path);
	const std::string &GetScenePath() const { return scenePath; }
	bool IsSceneDirty() const { return sceneDirty; }
	void MarkSceneDirty() { sceneDirty = true; }
	void ClearSceneDirty() { sceneDirty = false; }
	bool IsShowingSaveDialog() const { return showingSceneDialog && sceneDialogIsSave; }
	void OpenSaveSceneDialog();
	bool TrySaveCurrentScene(); // true if saved now; false if Save As dialog opened
	bool HasUnsavedWork() const;
	// Gate: if dirty, open modal and remember `action`. Returns true if caller may proceed now.
	bool ConfirmUnsavedThen(int action, const std::string& path = std::string());
	void DrawUnsavedChangesModal();
	void SetHostCallbacks(void (*onCloseProject)(), void (*onQuitApp)(),
		void (*onNewProject)() = NULL, void (*onOpenProject)() = NULL,
		void (*onQuitDiscardingUnsaved)() = NULL);
	void SetHostDocumentCallbacks(void (*onActivate)(SceneEditor*), void (*onRequestClose)(SceneEditor*),
		void (*onNewSceneDocument)() = NULL, void (*onOpenSceneDocument)(const std::string&) = NULL,
		void (*onOpenLuaScript)(const std::string&) = NULL);
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
	void (*hostOpenSceneDocument)(const std::string&);
	void (*hostOpenLuaScript)(const std::string&);
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
	// Owner GO for the current selection (component → GetOwner()).
	GameObject* GetSelectedOwnerGameObject() const;
	// Offscreen render used by EnsureModelThumbnail (RGBA8 PNG on disk).
	bool RenderModelPreviewToRGBA8(const std::string& p3dmPath, std::vector<unsigned char>& outRGBA,
		uint32& outW, uint32& outH);
	std::string TreeLabel(SceneObject* obj) const;
	void DrawSceneViewportIcons(const ImVec2& imgMin, const ImVec2& imgSize, GameObject* viewCam);
	bool TryPickViewportIcon(const Vec2& viewportMouse, uint32& outSceneObjectId);
	void OpenAddFormOnGameObject(uint32 goId, uint32 formType);
	void AddQuickLightOnGameObject(uint32 goId, uint32 formType);
	void ShowAddComponentMenu(uint32 goId);
	void DeleteGameObjectById(uint32 objId);
	void DeleteComponentById(uint32 objId);
	void DeleteSelected();
	uint32 DuplicateSelected();
	void CaptureGizmoBaseline();
	void PrepareGizmoForDraw(GameObject* viewCam);
	void HandleViewportGizmoInput(GameObject* viewCam);
	void ApplyGizmoTransformToObject();
	void ViewportPickAtMouse();
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

	void UseTranslationManipulator() { if (gizmo!=NULL) delete gizmo; gizmo = CreateMoveGizmo(); gizmo->SetLocation((localTransform?IGizmo::LOCATE_LOCAL:IGizmo::LOCATE_WORLD)); GizmoInUse = GizmoFunction::TRANSLATION; }
        void UseRotationManipulator() { if (gizmo!=NULL) delete gizmo; gizmo = CreateRotateGizmo(); gizmo->SetLocation((localTransform?IGizmo::LOCATE_LOCAL:IGizmo::LOCATE_WORLD)); GizmoInUse = GizmoFunction::ROTATION; }
        void UseScaleManipulator() { if (gizmo!=NULL) delete gizmo; gizmo = CreateScaleGizmo(); gizmo->SetLocation(IGizmo::LOCATE_LOCAL); GizmoInUse = GizmoFunction::SCALE; }
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
        // Renderer
        ForwardRenderer* Renderer;
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
	bool RebuildSceneMainScriptInstance();
	void InitSceneMainScript();
	void UpdateSceneMainScript(f64 time);
	void ResetSceneMainScriptLifecycle();
	void DrawGameObjectScriptProperties(uint32 goId);
	// Combo of assets/lua scripts only (not scenes/*.lua) + ASSET_REL drag-drop.
	void DrawScriptAssetPicker(const char* id, std::string& pathBuf);
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
	Matrix gizmoBaselineLocal;
	Matrix gizmoBaselineParentWorld;
	Matrix gizmoBaselineWorld;

	struct PlayModeObjectSnapshot {
		Vec3 position, rotation, scale;
		Matrix localTransform, scaleTransform, globalRotation;
	};
	bool playMode;
	bool showPhysicsDebug;
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
	AxisHelper* axisHelper;
	f32 l, r, t, b;

	std::vector<uint64> selection; // Multiple selection
	f32 sub_selection; // No multiple selection

	int32 draggin_id;
	int32 droppin_id;
	int32 node_clicked;
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
	uint32 showingAddFormType;
	string AddForm_modelPath;
	string AddForm_soundPath;
#ifdef LUA_BINDINGS
	// Draft paths for Properties panel InputText (not menus of script names).
	std::string propertiesScriptAttachPath;
	std::string propertiesNewGoScriptName;
	bool openNewGoScriptModal;
	std::string propertiesNewGoScriptError;
	// After attaching a script, keep this GameObject selected and expand it in the tree.
	uint32 hierarchyForceOpenId;
#endif

	bool showDir;

	// Icons
	Texture *icons;

	uint32 documentId;
	bool shutDownDone;
};

#endif	/* SCENEEDITOR_H */
