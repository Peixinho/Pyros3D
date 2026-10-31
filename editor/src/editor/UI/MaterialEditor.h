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
#include "../MaterialCodegen.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MaterialEditor {

	// Toolbar + active tab content. Caller has already done ImGui::Begin().
	//
	// Draws the document's own editing surface only: its header/Save row and
	// the graph or text canvas. Anything that is a property of the *material*
	// rather than of the document belongs to DrawProperties() below and is
	// deliberately not drawn here.
	void DrawWindow(MaterialEditorDocument& doc, const std::string& projectRoot);

	// The material's property sheet, for the Properties panel rather than the
	// Material Editor window: the Generic inspector (Generic kind), Text
	// mode's named sampler inputs, and the Rendering/blending settings every
	// kind shares. Caller has already done ImGui::Begin(); Editor routes the
	// focused material document here (Editor::DrawActiveMaterialProperties).
	// Node Graph mode contributes no texture section - its textures are
	// Texture nodes on the canvas, not a flat named list.
	void DrawProperties(MaterialEditorDocument& doc, const std::string& projectRoot);

	// Rewrites every Custom material's surface (<name>.generated.glsl.surface)
	// under assets/materials/ whose text differs from what the current codegen
	// produces for its .mat. That file is only written on Apply, so a change
	// to the codegen otherwise never reaches an existing project. Run on
	// project open, before the scene loads. Returns how many were rewritten.
	int RegenerateStaleGeneratedShaders(const std::string& projectRoot);

	// Clears a material's existing samplers, then re-adds one per (sampler
	// uniform name, texture path) pair with a non-empty path - the
	// texture-loading loop ApplyGraphOrTextToLiveMaterial used to own
	// inline, now shared with MaterialPreview's sync path and both edit
	// modes (see the two builders below).
	void WireSamplers(p3d::CustomShaderMaterial* mat,
	                  const std::vector<std::pair<std::string, std::string>>& samplerNameToTexturePath,
	                  const std::string& projectRoot);

	// Makes `mat`'s named parameters exactly the graph's Parameter nodes:
	// declares each with the node's value (its type from the node), and
	// removes any the graph no longer has.
	void SyncParameters(p3d::CustomShaderMaterial* mat, const std::vector<MaterialCodegenResult::Parameter>& params);

	// Builds WireSamplers' input list from the node graph's texture-node
	// walk (NodeGraph mode) - looks each (nodeId, sampler name) pair up in
	// `nodes` for its texturePath.
	std::vector<std::pair<std::string, std::string>> BuildNodeSamplerList(
		const std::vector<std::pair<uint32_t, std::string>>& textureSamplers, const std::vector<MaterialNode>& nodes);

	// Builds WireSamplers' input list from Text mode's named texture-input
	// list (MaterialEditorDocument::textTextures).
	std::vector<std::pair<std::string, std::string>> BuildTextSamplerList(
		const std::vector<MaterialTextureInput>& textTextures);

	// Project-relative form of a path for display/log text, or the path
	// unchanged when it lives outside the project.
	std::string DisplayPath(const std::string& path, const std::string& projectRoot);

	// Reads mat's surface again from beside its file (GetShaderFile()), checks it and sets it - a .mat applied
	// elsewhere, or a codegen change. False, with the compile error, when it does not compile.
	bool RecompileFromDisk(p3d::CustomShaderMaterial* mat, const std::string& projectRoot, std::string* errorOut = nullptr);

	// customMode only matters for kind == Custom (Text vs NodeGraph, locked
	// permanently onto the doc - see DrawToolbar); ignored for Generic.
	std::shared_ptr<p3d::IMaterial> CreateNewMaterial(MaterialEditorDocument& doc, MaterialEditKind kind,
	                                                   MaterialEditMode customMode = MaterialEditMode::NodeGraph);
	// Loads the .mat JSON and, for Custom kind, compiles+wires the live
	// shader (via ApplyGraphOrTextToLiveMaterial) so the document is
	// immediately renderable.
	bool LoadFromFile(MaterialEditorDocument& doc, const std::string& path, const std::string& projectRoot);
	// assets/materials/<name>.mat for a material never saved before, with
	// the name made file-safe and a number added rather than overwriting.
	std::string DefaultSavePath(const MaterialEditorDocument& doc, const std::string& projectRoot);
	// Save to the document's own file, or DefaultSavePath for one never
	// saved. Sets lastApplyError on failure.
	bool SaveDocument(MaterialEditorDocument& doc, const std::string& projectRoot);
	bool SaveToFile(MaterialEditorDocument& doc, const std::string& path, const std::string& projectRoot);

	// Regenerates the surface (NodeGraph mode: from the graph; Text mode: the
	// raw edited buffer), checks it, and sets it on the live
	// CustomShaderMaterial in place. On failure the material's previous
	// surface is left untouched and `errorOut` is set. Derives
	// doc.generatedGlslPath from doc.absolutePath if not already set -
	// fails if neither is known yet (material was never saved).
	bool ApplyGraphOrTextToLiveMaterial(MaterialEditorDocument& doc, const std::string& projectRoot, std::string* errorOut = nullptr);
}

#endif /* MATERIALEDITOR_H */
