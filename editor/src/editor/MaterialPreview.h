//=============================================================================
// Name        : MaterialPreview.h
// Description : Live sphere preview for Custom Shader materials in the
//               Material Editor. Renders the doc's generated GLSL (always
//               the Forward branch - see MaterialPreview.cpp's design note)
//               through its own private ForwardRenderer + PostEffectsManager
//               into an ImGui::Image, with mouse orbit/pan/zoom.
//=============================================================================

#ifndef MATERIALPREVIEW_H
#define MATERIALPREVIEW_H

#include "MaterialEditorDocument.h"
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <memory>
#include <string>

struct MaterialPreview {
	// Lazily built once, on the first EnsureInit() call.
	p3d::IRenderer* renderer = nullptr;      // ForwardRenderer, owned
	p3d::PostEffectsManager* effects = nullptr; // owned
	p3d::SceneGraph* scene = nullptr;        // owned, private scene
	std::shared_ptr<p3d::GameObject> sphereGO;
	std::shared_ptr<p3d::GameObject> cameraGO;
	// The key light's GameObject. The "Lights" toggle in DrawAndUpdate()
	// Add()/Removes() it from the preview scene (see that method's comment
	// on why Enable()/Disable() alone wouldn't switch the light off).
	std::shared_ptr<p3d::GameObject> lightGO;
	bool lightsEnabled = true;
	// Forward-only copy of the doc's compiled material. Deliberately NOT
	// the doc's own currentMaterial: that one tracks the project's live
	// renderer, this one never does (isolated offscreen render).
	std::shared_ptr<p3d::CustomShaderMaterial> previewMaterial;

	int width = 300, height = 300;

	// Orbit camera state.
	float yaw = 0.6f, pitch = 0.35f, distance = 3.5f;
	p3d::Vec3 panTarget = p3d::Vec3(0, 0, 0);

	uint32_t lastSeenApplyGeneration = 0; // see SyncFromDoc

	// Frees scene/camera/light/sphere first, then the FBO-backed
	// renderer/effects (their color texture is what a frame's ImGui draw
	// list may still reference - see Editor's deferred-destroy queue for
	// why destruction can be punted to after rasterization).
	~MaterialPreview();
	void EnsureInit();
	// Recompiles previewMaterial from doc's already-written generated GLSL
	// (doc.generatedGlslPath) if doc.applyGeneration has advanced since
	// last call. Re-wires texture samplers from doc.nodes the same way
	// ApplyGraphOrTextToLiveMaterial does (shared WireSamplers helper).
	void SyncFromDoc(const MaterialEditorDocument& doc, const std::string& projectRoot);
	// Renders the frame, draws an ImGui::Image of its color target, and
	// handles mouse orbit/pan/zoom while hovered. Call once per frame from
	// MaterialEditor::DrawWindow while the document is visible.
	void DrawAndUpdate();
private:
	p3d::Vec3 ComputeEye() const;
	void CreateSphere();
	// Builds the view matrix from yaw/pitch/distance/panTarget and runs the
	// pipeline (same call sequence as SceneEditor::RenderCameraPreview,
	// including the shared-UBO invalidation + WaitIdle around it).
	void RenderFrame();
};

#endif /* MATERIALPREVIEW_H */
