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
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include "UI/IUInterface.h"
#include "libgizmo/IGizmo.h"
#include "SceneObjects.h"
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include "Grid.h"
#include "AxisHelper.h"
#include "Helpers/LightHelper.h"
#include "Helpers/GameObjectHelper.h"
#include "SelectedMaterial.h"
#include "UI/OpenDir.h"
#include <memory>
//#include "../UI/OpenDir.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

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

        SceneEditor(bool *open, bool *openTree);
        virtual ~SceneEditor();

	virtual void Init(const uint32 width, const uint32 height);
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void Update(const f64 time);
	virtual void Show();
	virtual void ShowProperties();
	virtual void ShowMenubarOptions();
	virtual void ShowTools();
	virtual void Shutdown();

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

	void NewScene();
	bool SaveSceneToFile(const std::string &path);
	bool LoadSceneFromFile(const std::string &path);
	const std::string &GetScenePath() const { return scenePath; }
	// Drawn by Editor::DrawUI() unconditionally rather than from Show(), so
	// the modal survives the Scene View panel being closed.
	void DrawSceneFileDialog();

private:

	std::string scenePath;
	// Open Scene and Save Scene As share one modal.
	bool showingSceneDialog, sceneDialogIsSave, sceneDialogBrowse;
	std::string sceneDialogPath;
	std::string sceneDialogError;
	void CreateGameObject(const std::string &name = "GameObject");

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

	// Selected Mesh
	std::shared_ptr<SelectedMaterial> SelectedMeshMaterial;
	std::shared_ptr<RenderingComponent> SelectedRenderingComponent;
	std::shared_ptr<IMaterial> tempMaterial; RenderingMesh* SelectedMesh;
	void DeselectMesh();
	void SelectMesh(RenderingMesh* rmesh);

	// Debug Renderer
	DebugRenderer* debugRenderer;
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
	int32 AddForm_sw, AddForm_sh, AddForm_r, AddForm_hscale;
	bool AddForm_sn, AddForm_fn, AddForm_cgo, AddForm_hs, AddForm_oe, AddForm_cs;
	string AddForm_go;
	Vec3 AddForm_dir;
	Vec4 AddForm_color;
	void ShowAddForm();
	bool showingAddFrom;
	uint32 showingAddFormType;
	string AddForm_modelPath;

	bool showDir;

	bool showRightMenu;

	// Icons
	Texture *icons;

	// Window
	bool* Open;
	bool* OpenTree;
};

#endif	/* SCENEEDITOR_H */
