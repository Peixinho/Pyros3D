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
#include <string>

namespace MaterialEditor {

	// Toolbar + active tab content. Caller has already done ImGui::Begin().
	// deferredGBuffer selects which of the generated shader's two branches
	// (see MaterialCodegen.cpp) actually gets compiled - should reflect the
	// current project's Renderer setting (ProjectSettings::rendererType).
	void DrawWindow(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer);

	std::shared_ptr<p3d::IMaterial> CreateNewMaterial(MaterialEditorDocument& doc, MaterialEditKind kind);
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
