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
	float fov = 70.f;
	float nearPlane = 0.1f;
	float farPlane = 2000.f;
};

struct SceneCameraDebugEntry {
	p3d::GameObject* go;
	EditorCameraSettings settings;
	bool isViewCamera;
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

	void ToggleCameraFrustum(bool on) { showCameraFrustum = on; }
	bool IsCameraFrustumOn() const { return showCameraFrustum; }

	void Draw(p3d::DebugRenderer* dbg, p3d::SceneGraph* scene, p3d::GameObject* viewCam,
		float fovDeg, float aspect, p3d::uint32 viewportHeight,
		p3d::GameObject* skipA = NULL, p3d::GameObject* skipB = NULL, p3d::GameObject* skipC = NULL,
		const std::vector<SceneCameraDebugEntry>* sceneCameras = NULL);

private:
	std::unordered_set<p3d::IComponent*> compsHidden;
	std::unordered_set<p3d::GameObject*> camerasHidden;
	std::unordered_set<p3d::IComponent*> renderingNormalsOn;
	bool showCameraFrustum = false;

	void drawLightGizmos(p3d::DebugRenderer* dbg, p3d::GameObject* viewCam, float fovDeg, float aspect,
		p3d::uint32 viewportHeight, p3d::SceneGraph* scene,
		p3d::GameObject* skipA, p3d::GameObject* skipB, p3d::GameObject* skipC,
		const std::vector<SceneCameraDebugEntry>* sceneCameras);
};
