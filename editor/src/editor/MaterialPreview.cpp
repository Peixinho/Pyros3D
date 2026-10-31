//=============================================================================
// Name        : MaterialPreview.cpp
// Description : Live sphere preview for materials. See MaterialPreview.h for
//               the class contract.
//=============================================================================

#include "MaterialPreview.h"
#include "UI/MaterialEditor.h"
#include "MaterialCodegen.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
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
	// Drop the GameObject refs first, then the scene that owns them, then what was drawn with (its texture is what
	// a frame's ImGui draw list samples: freed by the device once those frames are done)
	sphereGO.reset();
	sphereRC = nullptr; // non-owning, freed along with sphereGO above
	cameraGO.reset();
	lightGO.reset();
	previewMaterial.reset();
	delete scene;
	scene = nullptr;
	sceneRenderer.reset();
	if (previewTarget && gfxDevice) { if (imguiRenderer) imguiRenderer->Forget(previewTarget); gfxDevice->Destroy(previewTarget); }
	previewTarget = p3d::gfx::Texture();
}

p3d::gfx::Device* MaterialPreview::gfxDevice = nullptr;
p3d::ImGuiRenderer* MaterialPreview::imguiRenderer = nullptr;

void MaterialPreview::EnsureInit() {
	if (sceneRenderer || !gfxDevice) return;
	sceneRenderer.reset(new p3d::SceneRenderer(gfxDevice));
	scene = new p3d::SceneGraph();

	cameraGO = std::make_shared<p3d::GameObject>();
	scene->Add(cameraGO);

	// One fixed key light, pointing down-and-toward-the-default-camera.
	lightGO = std::make_shared<p3d::GameObject>();
	lightGO->Add(std::make_shared<p3d::DirectionalLight>(p3d::Vec4(1.f, 1.f, 1.f, 1.f), p3d::Vec3(-0.45f, -1.f, -0.35f)));
	scene->Add(lightGO);

	// Modest ambient so unlit areas aren't pure black
	sceneRenderer->SetName("Material preview");
	sceneRenderer->SetBackground(p3d::Vec3(0.027f, 0.027f, 0.033f));
	sceneRenderer->SetAmbient(p3d::Vec3(0.3f, 0.3f, 0.35f));

	if (previewMaterial)
		CreateSphere();
	scene->Update(0);
}

void MaterialPreview::CreateSphere() {
	if (sphereGO || !scene) return;
	sphereGO = std::make_shared<p3d::GameObject>();
	// With tangents: a normal-mapped material reads them
	auto mesh = std::make_shared<p3d::Sphere>(1.0f, 32, 24, /*smooth=*/true, /*HalfSphere=*/false, /*flip=*/false, /*TangentBitangent=*/true);
	auto rc = std::make_shared<p3d::RenderingComponent>(mesh, previewMaterial);
	rc->DisableCastShadows();
	sphereRC = rc.get();
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
	if (doc.editKind == MaterialEditKind::Generic) {
		// No compile step, no texture/uniform staging - point the preview
		// sphere straight at the doc's own live GenericShaderMaterial
		// instance every call (cheap - just a pointer assign) so Inspector
		// slider edits, which mutate that instance directly, show up
		// immediately with no separate Apply/sync step.
		previewMaterial = doc.currentMaterial;
		if (!sphereGO) CreateSphere();
		if (sphereRC && !sphereRC->GetMeshes().empty())
			sphereRC->GetMeshes()[0]->Material = doc.currentMaterial;
		return;
	}
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
	// Opacity/transparency/culling are material settings, not shader
	// source - edited from the Properties panel without an Apply - so they
	// are mirrored every sync, ahead of the recompile gate.
	if (previewMaterial && doc.currentMaterial) {
		previewMaterial->SetOpacity(doc.currentMaterial->GetOpacity());
		previewMaterial->SetTransparencyFlag(doc.currentMaterial->IsTransparent());
		previewMaterial->SetCullFace(doc.currentMaterial->GetCullFace());
	}
	if (previewMaterial && doc.applyGeneration == lastSeenApplyGeneration) return;

	// The material's surface from the same codegen, the graph's or the text's
	MaterialCodegenResult gen2 = doc.editMode == MaterialEditMode::Text
		? GenerateSurfaceFromSimpleText(doc.simpleShaderText, [&]() { std::vector<std::string> n; for (const auto &t : MaterialEditor::BuildTextSamplerList(doc.textTextures)) n.push_back(t.first); return n; }())
		: GenerateSurface(doc.nodes, doc.connections);
	if (gen2.surface.empty())
		return; // keep showing the last-good preview material on failure

	// previewMaterial is IMaterial-typed (shared with the Generic-kind branch above)
	std::shared_ptr<p3d::CustomShaderMaterial> customPreview = std::dynamic_pointer_cast<p3d::CustomShaderMaterial>(previewMaterial);
	if (!customPreview) {
		customPreview = std::make_shared<p3d::CustomShaderMaterial>();
		previewMaterial = customPreview;
	}
	customPreview->SetSurfaceSource(gen2.surface);

	// The settings mirrored above, for a freshly created preview material
	if (doc.currentMaterial) {
		customPreview->SetOpacity(doc.currentMaterial->GetOpacity());
		customPreview->SetTransparencyFlag(doc.currentMaterial->IsTransparent());
		customPreview->SetCullFace(doc.currentMaterial->GetCullFace());
	}

	// Sampler wiring - same source split as the live-apply path
	// (MaterialEditor::ApplyGraphOrTextToLiveMaterial): NodeGraph mode walks
	// Texture nodes reachable from Output, Text mode uses the document's
	// named texture-input list.
	std::vector<std::pair<std::string, std::string>> samplerList;
	if (doc.editMode == MaterialEditMode::Text) {
		samplerList = MaterialEditor::BuildTextSamplerList(doc.textTextures);
	} else {
		MaterialCodegenResult gen = GenerateSurface(doc.nodes, doc.connections);
		samplerList = MaterialEditor::BuildNodeSamplerList(gen.textureSamplers, doc.nodes);
		MaterialEditor::SyncParameters(customPreview.get(), gen.parameters);
	}
	MaterialEditor::WireSamplers(customPreview.get(), samplerList, projectRoot);

	if (!sphereGO)
		CreateSphere();
	lastSeenApplyGeneration = doc.applyGeneration;
}

void MaterialPreview::RenderFrame() {
	if (!sceneRenderer || !scene || !cameraGO) return;
	const p3d::Vec3 eye = ComputeEye();
	p3d::Matrix view;
	view.LookAt(eye, panTarget, p3d::Vec3::UP);
	// GameObject stores world transform, not view.
	cameraGO->SetTransformationMatrix(view.Inverse());
	// Real elapsed time, not 0: what a surface reads as uTime is the scene's clock, and a time-driven material -
	// anything scrolling, pulsing or flowing - would otherwise show one frozen frame here
	scene->Update(ImGui::GetTime());
	const uint32_t w = (uint32_t)std::max(width, 1), h = (uint32_t)std::max(height, 1);
	if (!previewTarget || w != previewTargetW || h != previewTargetH)
	{
		if (previewTarget) { if (imguiRenderer) imguiRenderer->Forget(previewTarget); gfxDevice->Destroy(previewTarget); }
		p3d::gfx::TextureDesc d; d.width = w; d.height = h; d.format = gfxDevice->BackbufferFormat();
		d.usage = p3d::gfx::TexColorTarget | p3d::gfx::TexSampled | p3d::gfx::TexCopySrc; d.debugName = "Editor.MaterialPreview";
		previewTarget = gfxDevice->CreateTexture(d); previewTargetW = w; previewTargetH = h;
	}
	p3d::View v;
	v.cameraWorld = view.Inverse(); v.fovY = 45.f; v.nearZ = 0.05f;
	v.width = w; v.height = h; v.target = previewTarget; v.hostFrame = true;     // (the editor's frame, presented by it)
	sceneRenderer->Render(scene, v);
}

void MaterialPreview::DrawAndUpdate() {
	// Drawn every frame - its own renderer, nothing shared with the Scene View's to race
	RenderFrame();
	void* tid = previewTarget ? (void*)(uintptr_t)p3d::ImGuiRenderer::Id(previewTarget) : NULL;
	if (!tid) {
		ImGui::TextDisabled("[preview texture unavailable]");
		return;
	}
	ImGui::Image((ImTextureID)tid, ImVec2((f32)width, (f32)height));
	// Captured immediately after Image - IsItemHovered() always refers to
	// the LAST submitted widget, so this has to happen before the Checkbox
	// below (or it'd report the checkbox's hover state instead of the
	// image's, which is exactly what silently broke orbit-drag: hovering
	// the actual sphere never registered as "hovered" once the checkbox
	// became the last item, so the drag code below never ran).
	const bool hovered = ImGui::IsItemHovered();

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

	// Mouse orbit/pan/zoom - plain ImGui polling (same style the node-graph
	// canvas uses), gated on `hovered` so it can't leak into the node-graph
	// canvas below, and contained to this widget's child region by the caller.
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
