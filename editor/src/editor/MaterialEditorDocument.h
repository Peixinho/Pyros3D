//============================================================================
// Name        : MaterialEditorDocument.h
// Description : Material documents (dockable peer windows to Scene View,
//               same convention as CodeEditorDocument/Lua scripts). Pure
//               data + JSON round-trip; ImGui drawing and engine shader
//               compilation live in MaterialEditor.h/.cpp.
//============================================================================

#ifndef MATERIALEDITORDOCUMENT_H
#define MATERIALEDITORDOCUMENT_H

#include "MaterialGraphTypes.h"
#include <Pyros3D/Utils/Json/json.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct CodeEditorDocument;
namespace p3d { class IMaterial; class Shader; }

struct MaterialEditorDocument {
	uint32_t id = 0;
	std::string absolutePath;      // empty => inline/unsaved (Properties-panel "Edit Material" case)
	std::string displayName = "NewMaterial";
	bool dirty = false;

	std::shared_ptr<p3d::IMaterial> currentMaterial;
	MaterialEditKind editKind = MaterialEditKind::Generic;
	MaterialEditMode editMode = MaterialEditMode::Inspector;
	std::string materialName = "NewMaterial";

	// Custom-shader-only state (node graph + GLSL text editing)
	CodeEditorDocument* codeDoc = nullptr;
	uint32_t nextNodeId = 1;
	std::vector<MaterialNode> nodes;
	std::vector<MaterialConnection> connections;
	ImVec2 graphOffset = ImVec2(0.f, 0.f);
	float graphZoom = 1.0f;
	bool isDraggingConnection = false;
	uint32_t dragFromNode = 0;
	int dragFromPinIndex = -1;
	ImVec2 dragStartPos = ImVec2(0, 0);

	// Runtime-compiled shader (Custom kind only). SetShader() on
	// CustomShaderMaterial does NOT take ownership of what's handed to it -
	// this is what keeps the previously-swapped-in Shader alive/owned between
	// Applies. See MaterialEditor::ApplyGraphOrTextToLiveMaterial for the
	// swap ordering that makes this safe (reset() only AFTER SetShader()).
	std::unique_ptr<p3d::Shader> compiledShader;
	std::string generatedGlslPath; // assets/materials/<Name>.generated.glsl
	std::string lastApplyError;    // transient UI feedback, not serialized

	~MaterialEditorDocument();

	uint32_t CreateNode(MaterialNode::Type type, const std::string& name, ImVec2 pos);
	// Deletes node preview textures before clearing - used before both a
	// fresh CreateNewMaterial() and before DeserializeGraph() repopulates.
	void ClearNodes();

	// .mat JSON round-trip: name/kind/mode/common IMaterial settings, and for
	// Generic kind the full scalar property set; for Custom kind the node
	// graph (nodes/connections) and generatedGlslPath. Always leaves
	// currentMaterial non-null on success (Custom kind gets a placeholder
	// CustomShaderMaterial - MaterialEditor::LoadFromFile is responsible for
	// actually compiling/wiring it via ApplyGraphOrTextToLiveMaterial).
	bool LoadFromFile(const std::string& path);
	bool SaveToFile(const std::string& path);

	// Agent/MCP bridge: bulk-replace nodes+connections from JSON (same
	// "nodes"/"connections" array schema SaveToFile/LoadFromFile use).
	bool AgentSetGraph(const nlohmann::json& nodesArr, const nlohmann::json& connectionsArr, std::string& errOut);
	// Agent/MCP bridge: serialize the current graph back to JSON for inspection.
	nlohmann::json AgentGetGraph() const;
};

#endif /* MATERIALEDITORDOCUMENT_H */
