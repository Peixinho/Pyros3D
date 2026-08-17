//=============================================================================
// Name        : MaterialPreview.h
// Description : Live sphere preview for Custom Shader materials in the
//               Material Editor. Renders the doc's generated GLSL through
//               its own private renderer (Forward or Deferred, matching the
//               project's configured renderer type - see EnsureInit()) +
//               PostEffectsManager into an ImGui::Image, with mouse
//               orbit/pan/zoom.
//=============================================================================

#ifndef MATERIALPREVIEW_H
#define MATERIALPREVIEW_H

#include "MaterialEditorDocument.h"
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <memory>
#include <string>

namespace p3d { class RenderingComponent; }

struct MaterialPreview {
	// Lazily built once, on the first EnsureInit() call.
	p3d::IRenderer* renderer = nullptr;      // Forward- or DeferredRenderer, owned
	p3d::PostEffectsManager* effects = nullptr; // owned
	p3d::SceneGraph* scene = nullptr;        // owned, private scene
	// True once EnsureInit() has built a DeferredRenderer instead of a
	// ForwardRenderer. Fixed for the document's lifetime (see EnsureInit()'s
	// comment on why this isn't a live-toggleable setting).
	bool usingDeferred = false;
	// G-buffer for the Deferred case only (mirrors SceneEditor::BuildGBuffer);
	// left null when usingDeferred is false.
	p3d::Texture* gbufferDepth = nullptr;
	p3d::Texture* gbufferAlbedo = nullptr;
	p3d::Texture* gbufferSpecular = nullptr;
	p3d::Texture* gbufferNormal = nullptr;
	p3d::Texture* gbufferMatRough = nullptr;
	p3d::FrameBuffer* gbufferFBO = nullptr;
	std::shared_ptr<p3d::GameObject> sphereGO;
	// Non-owning - cached at CreateSphere() time purely so SyncFromDoc's
	// Generic-kind branch can update the sphere's material every call
	// without re-deriving it from sphereGO's component list each frame.
	p3d::RenderingComponent* sphereRC = nullptr;
	std::shared_ptr<p3d::GameObject> cameraGO;
	// The key light's GameObject. The "Lights" toggle in DrawAndUpdate()
	// Add()/Removes() it from the preview scene (see that method's comment
	// on why Enable()/Disable() alone wouldn't switch the light off).
	std::shared_ptr<p3d::GameObject> lightGO;
	bool lightsEnabled = true;
	// Custom kind: own copy of the doc's compiled material, deliberately NOT
	// the doc's own currentMaterial (a distinct CustomShaderMaterial
	// instance - see SyncFromDoc()), so this isolated offscreen render never
	// aliases whatever the live scene/other preview is doing with the same
	// shader. Generic kind needs no such isolation (no compile step, no
	// texture/uniform staging) - SyncFromDoc points this straight at the
	// doc's own GenericShaderMaterial instance instead, so slider edits in
	// the Inspector tab show up immediately. IMaterial-typed to hold either.
	std::shared_ptr<p3d::IMaterial> previewMaterial;

	// Kept small - this renders as a floating overlay in the corner of the
	// node graph/text editor (see MaterialEditor::DrawWindow), not a full
	// stacked panel, so it shouldn't dominate the window.
	int width = 220, height = 220;

	// Orbit camera state.
	float yaw = 0.6f, pitch = 0.35f, distance = 3.5f;
	p3d::Vec3 panTarget = p3d::Vec3(0, 0, 0);

	uint32_t lastSeenApplyGeneration = 0; // see SyncFromDoc
	// See DrawAndUpdate() - toggled every call, renders on alternating
	// calls only. Starts true so the very first call (flips to false)
	// actually renders immediately instead of showing an empty/garbage
	// texture for one frame.
	bool skipRenderThisCall = true;

	// Frees scene/camera/light/sphere first, then the FBO-backed
	// renderer/effects (their color texture is what a frame's ImGui draw
	// list may still reference - see Editor's deferred-destroy queue for
	// why destruction can be punted to after rasterization).
	~MaterialPreview();
	// useDeferred: the project's currently configured renderer type
	// (Editor::UseDeferredGBuffer()). Only consulted on the first call -
	// once renderer exists, later calls with a different value are ignored
	// (see .cpp for why: rebuilding it live hits the same mid-frame
	// WaitIdle() hazard SceneEditor::SwitchRenderer() defers via a queued
	// switch, which this preview doesn't need the machinery for).
	void EnsureInit(bool useDeferred);
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
	// Depth + 4 color attachments, matching SceneEditor::BuildGBuffer()'s
	// setup exactly - DeferredRenderer expects that exact layout.
	void BuildGBuffer(uint32_t width, uint32_t height);
	void DestroyGBuffer();
	// Builds the view matrix from yaw/pitch/distance/panTarget and runs the
	// pipeline (same call sequence as SceneEditor::RenderCameraPreview,
	// including the shared-UBO invalidation + WaitIdle around it).
	void RenderFrame();
};

#endif /* MATERIALPREVIEW_H */
