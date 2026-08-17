//============================================================================
// Name        : MaterialEditor.h
// Description : Material editor window content (Inspector / Text / Node
//               Graph). Stateless drawer over MaterialEditorDocument - the
//               window itself (Begin/End, docking, multi-doc lifecycle) is
//               owned by Editor::DrawMaterialEditorWindows(), the same split
//               Editor uses for Lua script documents.
//============================================================================

#ifndef MATERIALEDITOR_H
#define MATERIALEDITOR_H

#include "../MaterialEditorDocument.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MaterialEditor {

	// Toolbar + active tab content. Caller has already done ImGui::Begin().
	// deferredGBuffer selects which of the generated shader's two branches
	// (see MaterialCodegen.cpp) actually gets compiled - should reflect the
	// current project's Renderer setting (ProjectSettings::rendererType).
	//
	// Draws the document's own editing surface only: its header/Save row and
	// the graph or text canvas. Anything that is a property of the *material*
	// rather than of the document belongs to DrawProperties() below and is
	// deliberately not drawn here.
	void DrawWindow(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer);

	// The material's property sheet, for the Properties panel rather than the
	// Material Editor window: the Generic inspector (Generic kind), Text
	// mode's named sampler inputs, and the Rendering/blending settings every
	// kind shares. Caller has already done ImGui::Begin(); Editor routes the
	// focused material document here (Editor::DrawActiveMaterialProperties).
	// Node Graph mode contributes no texture section - its textures are
	// Texture nodes on the canvas, not a flat named list.
	void DrawProperties(MaterialEditorDocument& doc, const std::string& projectRoot);

	// Compiles a generated GLSL file (one containing both a Forward and a
	// Deferred branch, see MaterialCodegen.cpp) into a linked Shader, with
	// or without #define DEFERRED_GBUFFER - pick it to match the renderer
	// the material will actually run under. Returns false (and sets
	// *errorOut) on compile/link failure; on success *outShader owns the
	// new Shader. Shared by the live-apply path and the scene-level
	// recompile so the compile/link block lives in exactly one place.
	bool CompileMaterialShaderFile(const std::string& glslAbsPath, bool deferredGBuffer,
	                               std::unique_ptr<p3d::Shader>* outShader, std::string* errorOut = nullptr);

	// Same as CompileMaterialShaderFile but from shader text already in
	// memory - for CustomShaderMaterials saved into a scene via its
	// embedded shaderSource (SceneSerializer writes that when the material
	// has no recoverable file path, which is the normal case for
	// Material-Editor-created materials): their shaderString is the full
	// generated GLSL, so it recompiles for the other branch the same way.
	bool CompileMaterialShaderText(const std::string& glslText, bool deferredGBuffer,
	                               std::unique_ptr<p3d::Shader>* outShader, std::string* errorOut = nullptr);

	// Clears a material's existing samplers, then re-adds one per (sampler
	// uniform name, texture path) pair with a non-empty path - the
	// texture-loading loop ApplyGraphOrTextToLiveMaterial used to own
	// inline, now shared with MaterialPreview's sync path and both edit
	// modes (see the two builders below).
	void WireSamplers(p3d::CustomShaderMaterial* mat,
	                  const std::vector<std::pair<std::string, std::string>>& samplerNameToTexturePath,
	                  const std::string& projectRoot);

	// Builds WireSamplers' input list from the node graph's texture-node
	// walk (NodeGraph mode) - looks each (nodeId, sampler name) pair up in
	// `nodes` for its texturePath.
	std::vector<std::pair<std::string, std::string>> BuildNodeSamplerList(
		const std::vector<std::pair<uint32_t, std::string>>& textureSamplers, const std::vector<MaterialNode>& nodes);

	// Builds WireSamplers' input list from Text mode's named texture-input
	// list (MaterialEditorDocument::textTextures).
	std::vector<std::pair<std::string, std::string>> BuildTextSamplerList(
		const std::vector<MaterialTextureInput>& textTextures);

	// Recompiles mat's generated GLSL with a new DEFERRED_GBUFFER define
	// and hot-swaps it in place - from mat->GetShaderFile() when set, else
	// from mat->GetShaderObject()->GetShaderText() (the scene-save
	// shaderSource case: editor-created materials have no file path, and
	// SceneSerializer embeds their full generated text). For
	// CustomShaderMaterials assigned to a scene GameObject with no open
	// MaterialEditorDocument - there's no doc.compiledShader to update, so
	// ownership goes through CustomShaderMaterial::AdoptShader instead.
	// Returns false if neither a file nor a non-empty source text is
	// available (nothing to recompile from).
	bool RecompileFromDisk(p3d::CustomShaderMaterial* mat, const std::string& projectRoot,
	                       bool deferredGBuffer, std::string* errorOut = nullptr);

	// customMode only matters for kind == Custom (Text vs NodeGraph, locked
	// permanently onto the doc - see DrawToolbar); ignored for Generic.
	std::shared_ptr<p3d::IMaterial> CreateNewMaterial(MaterialEditorDocument& doc, MaterialEditKind kind,
	                                                   MaterialEditMode customMode = MaterialEditMode::NodeGraph);
	// Loads the .mat JSON and, for Custom kind, compiles+wires the live
	// shader (via ApplyGraphOrTextToLiveMaterial) so the document is
	// immediately renderable.
	bool LoadFromFile(MaterialEditorDocument& doc, const std::string& path, const std::string& projectRoot, bool deferredGBuffer);
	bool SaveToFile(MaterialEditorDocument& doc, const std::string& path, const std::string& projectRoot, bool deferredGBuffer);

	// Regenerates GLSL (NodeGraph mode: from the graph; Text mode: the raw
	// edited buffer), compiles it, and hot-swaps it onto the live
	// CustomShaderMaterial in place. On failure the material's previous
	// shader is left untouched and `errorOut` is set. Derives
	// doc.generatedGlslPath from doc.absolutePath if not already set -
	// fails if neither is known yet (material was never saved).
	bool ApplyGraphOrTextToLiveMaterial(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer, std::string* errorOut = nullptr);
}

#endif /* MATERIALEDITOR_H */
