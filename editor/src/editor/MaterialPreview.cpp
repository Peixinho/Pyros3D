//=============================================================================
// Name        : MaterialPreview.cpp
// Description : Live sphere preview for Custom Shader materials. See
//               MaterialPreview.h for the class contract.
//
// Design note: the preview ALWAYS compiles its own Forward-only shader
// (never #define DEFERRED_GBUFFER), regardless of the project's live
// renderer. Building a second full G-buffer + DeferredRenderer pipeline
// just to preview one sphere is a lot of machinery for no visual benefit -
// MaterialCodegen.cpp's Forward and Deferred branches compute identical PBR
// shading math, they only differ in how the final result is written to
// render targets. The preview is an isolated offscreen render, not part of
// the scene's G-buffer pass, so it should never need to match the project's
// mode.
//=============================================================================

#include "MaterialPreview.h"
#include "UI/MaterialEditor.h"
#include "MaterialCodegen.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>

using namespace p3d;

namespace {

std::string JoinPath(const std::string& root, const std::string& rel) {
	if (root.empty()) return rel;
	std::string r = root;
	if (r.back() != '/' && r.back() != '\\') r += "/";
	return r + rel;
}

} // namespace

MaterialPreview::~MaterialPreview() {
	// Drop the GameObject refs first, then the scene that owns them, then
	// the FBO-backed pipeline objects (their color texture is what a
	// frame's ImGui draw list samples - the same mid-frame-free hazard
	// Editor::deferredDestroyPreviews guards against).
	sphereGO.reset();
	cameraGO.reset();
	lightGO.reset();
	previewMaterial.reset();
	delete scene;
	delete effects;
	delete renderer;
	scene = nullptr;
	effects = nullptr;
	renderer = nullptr;
}

void MaterialPreview::EnsureInit() {
	if (renderer) return;

	renderer = new p3d::ForwardRenderer((uint32)width, (uint32)height);
	effects = new p3d::PostEffectsManager((uint32)width, (uint32)height);
	scene = new p3d::SceneGraph();

	cameraGO = std::make_shared<p3d::GameObject>();
	scene->Add(cameraGO);

	// One fixed key light, pointing down-and-toward-the-default-camera.
	lightGO = std::make_shared<p3d::GameObject>();
	lightGO->Add(std::make_shared<p3d::DirectionalLight>(p3d::Vec4(1.f, 1.f, 1.f, 1.f), p3d::Vec3(-0.45f, -1.f, -0.35f)));
	scene->Add(lightGO);

	// Modest ambient so unlit areas aren't pure black (mirrors
	// SceneEditor::ambientLightColor / Renderer->SetGlobalLight).
	renderer->SetBackground(p3d::Vec4(0.18f, 0.18f, 0.2f, 1.f));
	renderer->SetGlobalLight(p3d::Vec4(0.3f, 0.3f, 0.35f, 1.f));

	if (previewMaterial)
		CreateSphere();
	scene->Update(0);
}

void MaterialPreview::CreateSphere() {
	if (sphereGO || !scene) return;
	sphereGO = std::make_shared<p3d::GameObject>();
	auto mesh = std::make_shared<p3d::Sphere>(1.0f, 32, 24, /*smooth=*/true);
	auto rc = std::make_shared<p3d::RenderingComponent>(mesh, previewMaterial);
	rc->DisableCastShadows();
	sphereGO->Add(rc);
	scene->Add(sphereGO);
	scene->Update(0);
}

p3d::Vec3 MaterialPreview::ComputeEye() const {
	const p3d::Vec3 offset(
		distance * cosf(pitch) * sinf(yaw),
		distance * sinf(pitch),
		distance * cosf(pitch) * cosf(yaw));
	return panTarget + offset;
}

void MaterialPreview::SyncFromDoc(const MaterialEditorDocument& doc, const std::string& projectRoot) {
	if (doc.editKind != MaterialEditKind::Custom) return;
	// Deliberately NOT gated on applyGeneration != 0 (as this used to be) -
	// a doc opened via the Scene Tree's "Edit Material" button
	// (Editor::EditMaterialInline) never calls
	// ApplyGraphOrTextToLiveMaterial, so applyGeneration stays 0 for the
	// entire life of that doc. That left this function returning before
	// ever compiling previewMaterial/creating sphereGO, so the preview
	// permanently showed only the background clear colour - no error, no
	// sphere, nothing to click on to fix it. Compiling once from whatever's
	// already available is what "set up the preview" means the first time
	// regardless of which flow opened the doc; a real Apply later still
	// bumps applyGeneration and re-syncs normally via the check below.
	if (previewMaterial && doc.applyGeneration == lastSeenApplyGeneration) return;

	std::unique_ptr<p3d::Shader> newShader;
	std::string err;
	bool compiled = false;
	// Prefer the on-disk generated file when there is one (keeps the
	// preview in step with unsaved edits to it). Same "no recoverable
	// file -> fall back to the material's own cached shader text" split
	// MaterialEditor::RecompileFromDisk uses: EditMaterialInline-opened
	// docs for a material assigned via the Scene/Assign-material flow
	// (rather than created fresh in this editor) have no generatedGlslPath
	// at all - SceneSerializer saves those with an embedded shaderSource
	// and no file, since there's nothing on disk to point at. Without this
	// fallback the preview stayed permanently empty for every material
	// that reached the Material Editor that way, which - since dragging a
	// mesh's assigned material open via "Edit Material" is the ordinary
	// way to land here - was effectively always.
	if (!doc.generatedGlslPath.empty()) {
		const std::string glslAbsPath = JoinPath(projectRoot, doc.generatedGlslPath);
		compiled = MaterialEditor::CompileMaterialShaderFile(glslAbsPath, /*deferredGBuffer=*/false, &newShader, &err);
	} else if (auto* cm = dynamic_cast<p3d::CustomShaderMaterial*>(doc.currentMaterial.get())) {
		if (cm->GetShaderObject() && !cm->GetShaderObject()->GetShaderText().empty())
			compiled = MaterialEditor::CompileMaterialShaderText(cm->GetShaderObject()->GetShaderText(), /*deferredGBuffer=*/false, &newShader, &err);
	}
	if (!compiled)
		return; // keep showing the last-good preview material on failure

	if (!previewMaterial)
		previewMaterial = std::make_shared<p3d::CustomShaderMaterial>(newShader.get());
	else
		previewMaterial->SetShader(newShader.get());
	// The Shader* constructor and SetShader() don't take ownership - adopt
	// explicitly (same recurring gotcha as every other path in this code).
	previewMaterial->AdoptShader(std::move(newShader));

	// Fixed uniforms - same set ApplyGraphOrTextToLiveMaterial issues.
	// SendUniform skips names the active shader doesn't declare, so
	// over-issuing is harmless.
	previewMaterial->AddUniform(p3d::Uniform("uProjectionMatrix", p3d::Uniforms::DataUsage::ProjectionMatrix));
	previewMaterial->AddUniform(p3d::Uniform("uViewMatrix", p3d::Uniforms::DataUsage::ViewMatrix));
	previewMaterial->AddUniform(p3d::Uniform("uModelMatrix", p3d::Uniforms::DataUsage::ModelMatrix));
	previewMaterial->AddUniform(p3d::Uniform("uAmbientLight", p3d::Uniforms::DataUsage::GlobalAmbientLight));
	previewMaterial->AddUniform(p3d::Uniform("uCameraPosition", p3d::Uniforms::DataUsage::CameraPosition));
	previewMaterial->AddUniform(p3d::Uniform("uTime", p3d::Uniforms::DataUsage::Timer));
	previewMaterial->AddUniform(p3d::Uniform("uLights", p3d::Uniforms::DataUsage::Lights));
	previewMaterial->AddUniform(p3d::Uniform("uNumberOfLights", p3d::Uniforms::DataUsage::NumberOfLights));

	// Sampler wiring - same texture-node walk as the live-apply path.
	MaterialCodegenResult gen = GenerateGLSL(doc.nodes, doc.connections);
	MaterialEditor::WireSamplers(previewMaterial.get(), gen.textureSamplers, doc.nodes, projectRoot);

	if (!sphereGO)
		CreateSphere();
	lastSeenApplyGeneration = doc.applyGeneration;
}

void MaterialPreview::RenderFrame() {
	if (!renderer || !effects || !scene || !cameraGO) return;

	const p3d::Vec3 eye = ComputeEye();
	p3d::Matrix view;
	view.LookAt(eye, panTarget, p3d::Vec3::UP);
	// GameObject stores world transform, not view.
	cameraGO->SetTransformationMatrix(view.Inverse());
	scene->Update(0);

	p3d::Projection proj;
	proj.Perspective(45.0f, (f32)width / (f32)height, 0.05f, 100.0f);

	// Same call sequence as SceneEditor::RenderCameraPreview (including its
	// shared-UBO invalidation), which is the established "second live
	// offscreen render interleaved into the same frame as the main
	// viewport" pattern in this codebase.
	p3d::IRenderer::InvalidateSharedUniformCaches();
	renderer->Resize((uint32)width, (uint32)height);
	effects->Resize((uint32)width, (uint32)height);
	effects->ProcessPostEffects(&proj);
	renderer->ResetViewPort();
	renderer->SetViewPort(0, 0, (uint32)width, (uint32)height);
	renderer->PreRender(cameraGO.get(), scene);
	renderer->ApplyBackgroundClearColor();
	effects->CaptureFrame();
	renderer->RenderScene(proj, cameraGO.get(), scene);
	effects->EndCapture();
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	// Preview and the main viewport share one GlobalMatrices UBO. Without
	// waiting here the main pass can overwrite view/proj on the GPU while
	// preview draws are still in flight, causing the two cameras to
	// visibly alternate/flicker. Do not skip this call.
	GetActiveRenderDevice().WaitIdle();
#endif
	p3d::IRenderer::InvalidateSharedUniformCaches();
}

void MaterialPreview::DrawAndUpdate() {
	if (!renderer || !effects || !scene || !cameraGO) {
		ImGui::TextDisabled("Preview unavailable (no renderer)");
		return;
	}

	// Toggle row, kept inside this same child region so it can't steal
	// mouse handling from the node graph / text editor below.
	//
	// The renderer packs every light in Scene->GetLights() WITHOUT checking
	// the light's IComponent::active flag, so Enable()/Disable() would not
	// actually switch the light off. Removing/re-adding the light's
	// GameObject does: Remove() unregisters it from the scene's light list
	// (and Add() re-registers, since Remove() nulls GO->Scene). With it off
	// only the ambient term remains, isolating the light's contribution.
	if (ImGui::Checkbox("Lights", &lightsEnabled) && scene && lightGO) {
		if (lightsEnabled) scene->Add(lightGO);
		else scene->Remove(lightGO);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("left-drag orbit · right-drag pan · wheel zoom");
	ImGui::Spacing();

	RenderFrame();

	p3d::Texture* color = effects->GetViewportColor();
	if (!color) {
		ImGui::TextDisabled("No preview texture");
		return;
	}
	void* tid = GetActiveRenderDevice().GetImGuiTextureID(color->GetBindID(), color->GetTextureType());
	if (!tid) {
		ImGui::TextDisabled("[preview texture unavailable]");
		return;
	}
	ImGui::Image((ImTextureID)tid, ImVec2((f32)width, (f32)height));

	// Mouse orbit/pan/zoom - plain ImGui polling (same style the node-graph
	// canvas uses), gated on IsItemHovered so it can't leak into the
	// node-graph canvas below, and contained to this widget's child region
	// by the caller.
	const bool hovered = ImGui::IsItemHovered();
	if (!hovered) return;

	ImGuiIO& io = ImGui::GetIO();
	if (io.MouseWheel != 0.f) {
		distance -= io.MouseWheel * 0.3f;
		distance = std::max(0.6f, std::min(distance, 20.f));
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { // orbit
		yaw   -= io.MouseDelta.x * 0.01f;
		pitch += io.MouseDelta.y * 0.01f;
		pitch = std::max(-1.5f, std::min(pitch, 1.5f)); // avoid gimbal flip at poles
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) { // pan
		const p3d::Vec3 eye = ComputeEye();
		const p3d::Vec3 fwd = (panTarget - eye).normalize();
		const p3d::Vec3 right = (fwd.cross(p3d::Vec3::UP)).normalize();
		const p3d::Vec3 up = (right.cross(fwd)).normalize();
		const float panSpeed = distance * 0.0015f; // scale with zoom so pan feels consistent
		panTarget += (right * (-io.MouseDelta.x * panSpeed)) + (up * (io.MouseDelta.y * panSpeed));
	}
}
