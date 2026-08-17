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
#include "UndoStack.h"
#include <Pyros3D/Utils/Json/json.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct CodeEditorDocument;
struct MaterialPreview;
namespace p3d { class IMaterial; class Shader; class CustomShaderMaterial; }

struct MaterialEditorDocument {
	uint32_t id = 0;
	std::string absolutePath;      // empty => inline/unsaved (Properties-panel "Edit Material" case)
	std::string displayName = "NewMaterial";
	bool dirty = false;
	// True for a doc loaded purely to get a compiled IMaterial to assign
	// onto a scene object (Editor::AssignMaterialAsset picking an existing
	// asset from the Properties panel's list) - kept in Editor::materialDocs
	// so it stays alive for as long as something references its compiled
	// shader (see CustomShaderMaterial::SetShader's non-owning contract),
	// but Editor::DrawMaterialEditorWindows skips it, so assigning a
	// material never pops open a Material Editor tab as a side effect.
	// Cleared back to false the moment the user actually asks to edit it
	// (Editor::OpenMaterialDocument / EditMaterialInline).
	bool hiddenFromTabs = false;

	std::shared_ptr<p3d::IMaterial> currentMaterial;
	MaterialEditKind editKind = MaterialEditKind::Generic;
	MaterialEditMode editMode = MaterialEditMode::Inspector;
	std::string materialName = "NewMaterial";

	// Custom-shader-only state (node graph + GLSL text editing). codeDoc's
	// buffer holds a short user snippet (just the Albedo/Normal/Metallic/
	// Roughness/Emissive/Occlusion assignments) - NOT the full boilerplate/
	// dual-branch GLSL file, which stays generated (see MaterialCodegen::
	// GenerateGLSLFromSimpleText) and written to generatedGlslPath, never
	// shown to the user. simpleShaderText mirrors codeDoc's text so it
	// round-trips through the .mat JSON the same way nodes/connections do,
	// without needing codeDoc constructed just to reload a saved file.
	CodeEditorDocument* codeDoc = nullptr;
	std::string simpleShaderText;
	uint32_t nextNodeId = 1;
	std::vector<MaterialNode> nodes;
	std::vector<MaterialConnection> connections;

	// Text mode's named texture-uniform inputs (see MaterialTextureInput) -
	// independent of nodes/connections, since Text mode has no node to hang
	// a texturePath off of. Declared as `uniform sampler2D <name>;` and
	// sampled directly by name in the user's snippet (see
	// MaterialCodegen::GenerateGLSLFromSimpleText).
	uint32_t nextTextureInputId = 1;
	std::vector<MaterialTextureInput> textTextures;
	// Frees textTextures' previewTex pointers before clearing - mirrors
	// ClearNodes()'s ownership handling for MaterialNode::previewTex.
	void ClearTextTextures();
	ImVec2 graphOffset = ImVec2(0.f, 0.f);
	float graphZoom = 1.0f;
	bool isDraggingConnection = false;
	uint32_t dragFromNode = 0;
	int dragFromPinIndex = -1;
	ImVec2 dragStartPos = ImVec2(0, 0);

	// Per-document undo/redo history (see UndoStack.h / the undo/redo plan).
	// Custom-kind-only in practice (Generic kind has no graph, but the
	// stack is harmless and unused for it).
	UndoStack undo;

	// Coarse whole-graph snapshot used for undo: nodes+connections+
	// nextNodeId, with each node's previewTex reset to null - previewTex is
	// a raw *owning* pointer (see ClearNodes()/MaterialEditor.cpp's lazy
	// load sites), so a naive vector copy would alias it between the live
	// graph and the snapshot and double-free on the next ClearNodes(); the
	// lazy-load code already regenerates a null previewTex on next draw, so
	// dropping it here is free.
	struct GraphSnapshot {
		std::vector<MaterialNode> nodes;
		std::vector<MaterialConnection> connections;
		uint32_t nextNodeId = 1;
	};
	GraphSnapshot CaptureGraphSnapshot() const;
	// ClearNodes()'s then reassigns from `snap` - safe to call with a
	// snapshot captured from this same document at an earlier point.
	void RestoreGraphSnapshot(const GraphSnapshot& snap);

	// Commit-boundary pair for the many small ImGui widgets in
	// UI/MaterialEditor.cpp that each touch the graph (node value fields,
	// generic-material sliders/checkboxes): BeginGraphEdit() on
	// IsItemActivated(), CommitGraphEdit() on IsItemDeactivatedAfterEdit()
	// (see MaterialEditor.cpp's GraphUndoCommit() helper). One shared
	// pending baseline is enough for the whole document - only one ImGui
	// widget can be mid-gesture at a time, unlike UndoValueEdit's
	// per-call-site baseline (used where a scalar before/after pair is
	// cheap to keep per widget; a whole GraphSnapshot is not).
	GraphSnapshot pendingEditBaseline;
	bool pendingEditBaselineValid = false;
	void BeginGraphEdit();
	void CommitGraphEdit(const std::string& description);

	// Runtime-compiled shader (Custom kind only). SetShader() on
	// CustomShaderMaterial does NOT take ownership of what's handed to it -
	// this is what keeps the previously-swapped-in Shader alive/owned between
	// Applies. See MaterialEditor::ApplyGraphOrTextToLiveMaterial for the
	// swap ordering that makes this safe (reset() only AFTER SetShader()).
	std::unique_ptr<p3d::Shader> compiledShader;
	std::string generatedGlslPath; // assets/materials/<Name>.generated.glsl
	std::string lastApplyError;    // transient UI feedback, not serialized

	// Debounced auto-apply (Custom kind only - see MaterialEditor::
	// DrawWindow). pendingFingerprint/-Since track the live graph/text's
	// most recent content and when it last changed; once it's held still
	// for a short debounce window, DrawWindow calls
	// ApplyGraphOrTextToLiveMaterial and records the applied content in
	// autoAppliedFingerprint. None of this is `dirty` - Apply deliberately
	// never clears that (a live-compiled material can still need an
	// explicit File Save), so a separate marker is needed to know whether
	// the CURRENT content has already been compiled.
	std::string autoAppliedFingerprint;
	bool autoAppliedValid = false;
	std::string pendingFingerprint;
	double pendingFingerprintSince = 0.0;

	// Which branch compiledShader (if any) was last compiled with - set on
	// every successful ApplyGraphOrTextToLiveMaterial. Lets a live
	// Forward/Deferred renderer switch recompile only the docs whose
	// material is running the other branch (see
	// Editor::RecompileCustomMaterialsForRenderer).
	bool hasCompiledShader = false;
	bool compiledForDeferredGBuffer = false;
	// Bumped on every successful ApplyGraphOrTextToLiveMaterial.
	// MaterialPreview polls it to lazily recompile its own Forward-only
	// copy of the shader (no callback/hook needed at the Apply call sites).
	uint32_t applyGeneration = 0;
	// Last applyGeneration already pushed out to the objects in the open
	// scene that use this material - see Editor::DrawMaterialEditorWindows.
	// Polled the same way MaterialPreview polls applyGeneration, and for the
	// same reason: Apply is reached from the toolbar's Save, the debounced
	// auto-apply and the agent command, and none of them should have to know
	// the scene exists.
	uint32_t sceneSyncedApplyGeneration = 0;

	// Live sphere preview (Custom kind only), lazily constructed by
	// MaterialEditor::DrawWindow. Forward-declared here; complete type
	// only needed where the doc's destructor runs (see the .cpp include).
	// NOTE: Editor's close path pulls this out (preview.release()) and
	// destroys it after the frame's ImGui rasterization - see
	// Editor::deferredDestroyPreviewRenderers - because its FBO color
	// texture is still referenced by this frame's draw list.
	std::unique_ptr<MaterialPreview> preview;

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

// Reverses one discrete graph edit (a widget commit gesture, a node drag,
// a connection made/broken, or one AgentSetGraph call) by swapping which
// GraphSnapshot is live - coarse (whole-graph) rather than per-field, since
// node values can cascade through the graph and a whole-snapshot diff is
// cheap for graphs this size.
class GraphEditCommand : public IUndoableCommand {
public:
	GraphEditCommand(MaterialEditorDocument* doc, const MaterialEditorDocument::GraphSnapshot& before,
		const MaterialEditorDocument::GraphSnapshot& after, const std::string& description);

	void Undo() override;
	void Redo() override;
	std::string Description() const override { return description_; }

private:
	MaterialEditorDocument* doc_;
	MaterialEditorDocument::GraphSnapshot before_, after_;
	std::string description_;
};

#endif /* MATERIALEDITORDOCUMENT_H */
