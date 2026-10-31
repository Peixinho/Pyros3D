//=============================================================================
// Name        : MaterialPreview.h
// Description : Live sphere preview for materials in the Material Editor.
//               Draws the doc's material - a Custom one's surface from the
//               graph or the text, a Generic one as it is - through a renderer
//               of its own into a texture shown as an ImGui::Image, with mouse
//               orbit/pan/zoom.
//=============================================================================

#ifndef MATERIALPREVIEW_H
#define MATERIALPREVIEW_H

#include "MaterialEditorDocument.h"
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Rendering/Renderer/ImGuiRenderer.h>
#include <memory>
#include <string>

namespace p3d { class RenderingComponent; }

struct MaterialPreview {
public:
	// (the editor's - Editor::Init sets them)
	static p3d::gfx::Device* gfxDevice;
	static p3d::ImGuiRenderer* imguiRenderer;
	std::unique_ptr<p3d::SceneRenderer> sceneRenderer;
	p3d::gfx::Texture previewTarget; uint32_t previewTargetW = 0, previewTargetH = 0;
	p3d::SceneGraph* scene = nullptr;        // owned, private scene
	std::shared_ptr<p3d::GameObject> sphereGO;
	p3d::RenderingComponent* sphereRC = nullptr;
	std::shared_ptr<p3d::GameObject> cameraGO;
	std::shared_ptr<p3d::GameObject> lightGO;
	bool lightsEnabled = true;
	std::shared_ptr<p3d::IMaterial> previewMaterial;

	int width = 220, height = 220;

	float yaw = 0.6f, pitch = 0.35f, distance = 3.5f;
	p3d::Vec3 panTarget = p3d::Vec3(0, 0, 0);

	uint32_t lastSeenApplyGeneration = 0; // see SyncFromDoc

	~MaterialPreview();
	void EnsureInit();
	void SyncFromDoc(const MaterialEditorDocument& doc, const std::string& projectRoot);
	void DrawAndUpdate();
private:
	p3d::Vec3 ComputeEye() const;
	void CreateSphere();
	void RenderFrame();
};

#endif /* MATERIALPREVIEW_H */
