#pragma once

#include <unordered_set>
#include <vector>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d {
class DebugRenderer;
class GameObject;
class IComponent;
class SceneGraph;
}

struct EditorCameraSettings {
	// Perspective by default - every scene camera that existed before this
	// was one, and a missing "orthographic" key in an older .editor.json
	// therefore reads back as exactly what it used to mean.
	bool orthographic = false;
	float fov = 70.f;
	// Half-height of the ortho view volume, in world units - the usual way
	// to size one, and the reason it is not derived from fov: an ortho
	// camera has no field of view, so reusing that number would mean
	// switching projection silently changed how much you could see.
	float orthoSize = 10.f;
	float nearPlane = 0.1f;
	float farPlane = 2000.f;
};

struct SceneCameraDebugEntry {
	p3d::GameObject* go;
	EditorCameraSettings settings;
	bool isViewCamera;
	// The aspect the frustum is drawn with. Not the editor viewport's: that
	// is whatever shape the dock happens to be, and a camera's frustum
	// changing when the panel was resized described nothing about the
	// camera. The caller passes the aspect its preview renders at.
	float aspect = 16.f / 9.f;
};

class EditorDebugDraw {
public:
	EditorDebugDraw();
	~EditorDebugDraw();

	void ToggleForComponent(p3d::IComponent* comp);
	bool IsOn(p3d::IComponent* comp) const;

	void ToggleForCamera(p3d::GameObject* camGO);
	bool IsCameraOn(p3d::GameObject* camGO) const;
	void ForgetCamera(p3d::GameObject* camGO);

	void ToggleNormalsForRenderingComponent(p3d::IComponent* comp);
	bool IsNormalsOn(p3d::IComponent* comp) const;
	void ForgetComponent(p3d::IComponent* comp);

	// Light gizmos and camera frustums follow the selection: a light's
	// volume/cone is drawn while that light COMPONENT is selected, a camera's
	// frustum while its GameObject is. Drawing every one of them all the
	// time buried a real scene - a lit interior has dozens of lights - so
	// these two switches are the opt-in "show them all" mode instead, and
	// the per-component eye buttons filter what that mode shows. Both
	// default off.
	void ToggleCameraFrustum(bool on) { showAllCameraFrustums = on; }
	bool IsCameraFrustumOn() const { return showAllCameraFrustums; }
	void ToggleLightGizmos(bool on) { showAllLightGizmos = on; }
	bool AreLightGizmosOn() const { return showAllLightGizmos; }
	// The master switch over both, selection included - for looking at a
	// lit scene with nothing drawn over it. Defaults on.
	void ToggleGizmoLines(bool on) { showGizmoLines = on; }
	bool AreGizmoLinesOn() const { return showGizmoLines; }

	void Draw(p3d::DebugRenderer* dbg, p3d::SceneGraph* scene, p3d::GameObject* viewCam,
		float fovDeg, float aspect, p3d::uint32 viewportHeight,
		p3d::GameObject* skipA = NULL, p3d::GameObject* skipB = NULL, p3d::GameObject* skipC = NULL,
		const std::vector<SceneCameraDebugEntry>* sceneCameras = NULL,
		p3d::IComponent* selectedComponent = NULL, p3d::GameObject* selectedCamera = NULL);

private:
	std::unordered_set<p3d::IComponent*> compsHidden;
	std::unordered_set<p3d::GameObject*> camerasHidden;
	std::unordered_set<p3d::IComponent*> renderingNormalsOn;
	bool showAllCameraFrustums = false;
	bool showAllLightGizmos = false;
	bool showGizmoLines = true;

	void drawLightGizmos(p3d::DebugRenderer* dbg, p3d::GameObject* viewCam, float fovDeg, float aspect,
		p3d::uint32 viewportHeight, p3d::SceneGraph* scene,
		p3d::GameObject* skipA, p3d::GameObject* skipB, p3d::GameObject* skipC,
		const std::vector<SceneCameraDebugEntry>* sceneCameras,
		p3d::IComponent* selectedComponent, p3d::GameObject* selectedCamera);
};
