//============================================================================
// Name        : MaterialEditor.cpp
// Description : Material editor window content implementation. See
//               MaterialEditor.h for the split between this (drawing +
//               engine actions) and MaterialEditorDocument (data + JSON).
//============================================================================

#include "MaterialEditor.h"
#include "OpenDir.h"
#include "../CodeEditorDocument.h"
#include "../MaterialCodegen.h"
#include "../MaterialPreview.h"
#include "../UndoValueEdit.h"
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <imgui.h>
#include <imgui_internal.h> // ImGuiStorage - per-slot texture-browse toggle state
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>

using namespace p3d;

namespace {

// Commit-boundary helper for the many small widgets below that each touch
// the node graph or generic-material properties - call immediately after
// the widget, regardless of whether this frame's call returned true (it
// only acts on IsItemActivated()/IsItemDeactivatedAfterEdit() edges, same
// pattern as UndoValueEdit.h's scalar version, just backed by
// MaterialEditorDocument's own whole-graph BeginGraphEdit()/
// CommitGraphEdit() rather than a per-call-site value baseline).
void GraphUndoCommit(MaterialEditorDocument& doc, const char* description)
{
	if (ImGui::IsItemActivated())
		doc.BeginGraphEdit();
	if (ImGui::IsItemDeactivatedAfterEdit())
		doc.CommitGraphEdit(description);
}

// Commit-boundary helper for a value that lives on the live IMaterial
// itself (generic-material scalar/color fields) rather than in the node
// graph - `setter(doc, value)` re-fetches whatever typed material pointer
// it needs from doc->currentMaterial fresh each call, since that shared_ptr
// can be replaced with a freshly-constructed instance (see the render-
// options-changed block in DrawGenericMaterialInspector) between when this
// pushes a command and when Undo()/Redo() later runs it.
template<typename T>
void UndoMaterialValue(MaterialEditorDocument& doc, T& baseline, const T& liveValue,
	const std::function<void(MaterialEditorDocument*, const T&)>& setter, const char* description)
{
	MaterialEditorDocument* docPtr = &doc;
	UndoValueEdit<T>(baseline, liveValue, [docPtr, setter, description](const T& before, const T& after) {
		docPtr->undo.Push(std::make_unique<ApplyClosureCommand>(
			[docPtr, setter, before]() { setter(docPtr, before); },
			[docPtr, setter, after]() { setter(docPtr, after); },
			description));
	});
}



// A relative texture path like "brick.png" is resolved against
// assets/textures/; anything that already looks absolute (starts with '/'
// or a drive letter) is left as-is.
// Fixed a real bug here: the original `!relOrAbs[0] == '/'` negated the
// *char* (true only when it's '\0') before comparing to '/', so the
// "prepend assets/textures/" branch almost never triggered.
std::string ResolveTexturePath(const std::string& relOrAbs) {
	if (relOrAbs.empty()) return "";
	const bool looksAbsolute = (relOrAbs[0] == '/') || (relOrAbs.size() >= 2 && relOrAbs[1] == ':');
	return looksAbsolute ? relOrAbs : ("assets/textures/" + relOrAbs);
}

std::string JoinPath(const std::string& root, const std::string& rel) {
	if (root.empty()) return rel;
	std::string r = root;
	if (r.back() != '/' && r.back() != '\\') r += "/";
	return r + rel;
}

const char* kTextureExtensions = "png,jpg,jpeg,tga,bmp,gif,hdr,exr,webp";

bool FileExists(const std::string& path) {
	std::ifstream f(path);
	return f.good();
}

// A material only shows up in the scene's "Assign material asset" picker
// (or as an Assets-panel drag source) once it's a real file under
// assets/materials/ - the Assets panel's own "New Material" flow already
// writes one immediately (ProjectManager::CreateMaterial), but the toolbar's
// New Generic/Custom Material buttons and the Type combo (below) used to
// leave a freshly created material unsaved (doc.absolutePath empty) until
// the user explicitly hit Save, so it was invisible to both of those and
// couldn't be assigned to anything. Picks the first non-colliding
// "<baseName>[N].mat" so repeated creates don't silently overwrite an
// earlier unsaved one, and writes nameInOut back so the doc's own Name
// field matches what actually got saved to disk.
std::string UniqueMaterialPath(const std::string& projectRoot, std::string& nameInOut) {
	const std::string base = nameInOut;
	for (int suffix = 0; ; ++suffix) {
		const std::string candidateName = (suffix == 0) ? base : (base + std::to_string(suffix));
		const std::string relPath = "assets/materials/" + candidateName + ".mat";
		if (!FileExists(JoinPath(projectRoot, relPath))) {
			nameInOut = candidateName;
			return relPath;
		}
	}
}

// Called right after CreateNewMaterial() at every call site below, so a
// freshly created material is a real assets/materials/*.mat file the moment
// it exists - see UniqueMaterialPath's comment for why that matters.
void AutoSaveNewMaterial(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer) {
	if (!doc.absolutePath.empty()) return; // already has a path somehow - don't touch it
	const std::string relPath = UniqueMaterialPath(projectRoot, doc.materialName);
	doc.displayName = doc.materialName;
	// doc.SaveToFile() directly (not MaterialEditor::SaveToFile, which
	// Applies BEFORE writing) - ApplyGraphOrTextToLiveMaterial derives
	// generatedGlslPath from absolutePath and refuses to compile until
	// that's set, so establish the path/write the file first, then Apply.
	if (!doc.SaveToFile(JoinPath(projectRoot, relPath))) {
		doc.lastApplyError = "Could not auto-save new material to " + relPath;
		return;
	}
	if (doc.editKind == MaterialEditKind::Custom) {
		std::string err;
		if (!MaterialEditor::ApplyGraphOrTextToLiveMaterial(doc, projectRoot, deferredGBuffer, &err))
			doc.lastApplyError = err;
	}
}

void SetupGlslEditor(CodeEditorDocument* doc) {
	doc->SetupForGlsl();
}

void SeedDefaultCustomGraph(MaterialEditorDocument& doc) {
	doc.ClearNodes();
	const uint32_t colorId = doc.CreateNode(MaterialNode::Color, "Base Color", ImVec2(80.f, 140.f));
	const uint32_t metallicId = doc.CreateNode(MaterialNode::Float, "Metallic", ImVec2(80.f, 320.f));
	const uint32_t roughnessId = doc.CreateNode(MaterialNode::Float, "Roughness", ImVec2(80.f, 460.f));
	const uint32_t outputId = doc.CreateNode(MaterialNode::Output, "Output", ImVec2(560.f, 280.f));
	for (auto& n : doc.nodes) {
		if (n.id == colorId) n.userData = "1.000000,1.000000,1.000000,1.000000";
		if (n.id == metallicId) n.userData = "0.000000";
		if (n.id == roughnessId) n.userData = "0.500000";
	}
	MaterialConnection c1; c1.fromNode = colorId; c1.fromPinIndex = 4; c1.toNode = outputId; c1.toPinIndex = 0;
	MaterialConnection c2; c2.fromNode = metallicId; c2.fromPinIndex = 0; c2.toNode = outputId; c2.toPinIndex = 2;
	MaterialConnection c3; c3.fromNode = roughnessId; c3.fromPinIndex = 0; c3.toNode = outputId; c3.toPinIndex = 3;
	doc.connections.push_back(c1);
	doc.connections.push_back(c2);
	doc.connections.push_back(c3);
}

// A single-instance file-browse toggle keyed by the current ImGui ID scope -
// ImGui::_priv's own state (path/extension/selection) is itself a single
// global instance (see OpenDir.cpp), so per-call-site tracking only needs to
// remember each site's own open/closed bool, not a whole dialog's state.
bool DrawBrowseButton(const char* buttonLabel, const std::string& rootDir, const char* extensions, std::string& outPickedAbsolute) {
	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID key = ImGui::GetID("##browse_open");
	bool open = storage->GetBool(key, false);
	bool picked = false;
	if (ImGui::Button(buttonLabel))
		ImGui::_priv::OpenLocation(rootDir, extensions, &open);
	if (open) {
		std::string result;
		if (ImGui::FilePath("##browse_path", rootDir, extensions, &result, 1024, &open)) {
			if (!result.empty()) { outPickedAbsolute = result; picked = true; }
		}
	}
	storage->SetBool(key, open);
	return picked;
}

void DrawTextureSlot(const char* label, Texture* tex, const std::string& textureRoot,
	const std::function<void(std::shared_ptr<Texture>)>& setter) {
	ImGui::PushID(label);
	ImGui::Text("%s", label);
	std::string picked;
	if (DrawBrowseButton("Browse...", textureRoot, kTextureExtensions, picked) && setter) {
		auto newTex = std::make_shared<Texture>();
		if (newTex->LoadTexture(picked, TextureType::Texture))
			setter(newTex);
	}
	ImGui::SameLine();
	if (tex) {
		ImGui::TextDisabled("(loaded)");
		ImVec2 slotSize = ImVec2(ImGui::GetContentRegionAvail().x, 64.f);
		void* tid = GetActiveRenderDevice().GetImGuiTextureID(tex->GetBindID(), tex->GetTextureType());
		if (tid) ImGui::Image((ImTextureID)tid, slotSize);
		else ImGui::TextDisabled("[texture preview unavailable]");
	} else {
		ImGui::TextDisabled("(none)");
	}
	ImGui::PopID();
}

bool IsPinHovered(ImVec2 pinPos, float radius) {
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 delta(io.MousePos.x - pinPos.x, io.MousePos.y - pinPos.y);
	return (delta.x * delta.x + delta.y * delta.y) <= (radius * radius);
}

void HandleConnectionDrag(MaterialEditorDocument& doc, ImDrawList* drawList, const std::vector<PinPosition>& allPins) {
	if (!doc.isDraggingConnection) return;
	ImGuiIO& io = ImGui::GetIO();
	drawList->AddLine(doc.dragStartPos, io.MousePos, ImGui::GetColorU32(ImVec4(0.6f, 0.8f, 1.f, 0.9f)), 2.f);

	if (!ImGui::IsMouseDown(0)) {
		for (const auto& p : allPins) {
			if (!p.isOutput && p.nodeId != doc.dragFromNode && IsPinHovered(p.screenPos, 8.f)) {
				MaterialConnection conn;
				conn.fromNode = doc.dragFromNode;
				conn.fromPinIndex = doc.dragFromPinIndex;
				conn.toNode = p.nodeId;
				conn.toPinIndex = p.pinIndex;
				// Custom canvas gesture, not a standard ImGui widget - no
				// IsItemActivated/IsItemDeactivatedAfterEdit to hang a
				// commit boundary on, but the whole mutation happens in
				// this one frame, so a plain before/after capture is enough
				// (unlike node dragging below, which spans frames).
				const MaterialEditorDocument::GraphSnapshot before = doc.CaptureGraphSnapshot();
				// A pin accepts only one incoming connection - replace any existing one.
				doc.connections.erase(std::remove_if(doc.connections.begin(), doc.connections.end(),
					[&](const MaterialConnection& c) { return c.toNode == conn.toNode && c.toPinIndex == conn.toPinIndex; }),
					doc.connections.end());
				doc.connections.push_back(conn);
				doc.dirty = true;
				doc.undo.Push(std::make_unique<GraphEditCommand>(&doc, before, doc.CaptureGraphSnapshot(), "Connect Nodes"));
			}
		}
		doc.isDraggingConnection = false;
	}
}

void DrawAddNodeItem(MaterialEditorDocument& doc, MaterialNode::Type type, ImVec2 pos) {
	if (ImGui::MenuItem(MaterialNode::TypeToString(type))) {
		const MaterialEditorDocument::GraphSnapshot before = doc.CaptureGraphSnapshot();
		doc.CreateNode(type, MaterialNode::TypeToString(type), pos);
		doc.dirty = true;
		doc.undo.Push(std::make_unique<GraphEditCommand>(&doc, before, doc.CaptureGraphSnapshot(),
			std::string("Add ") + MaterialNode::TypeToString(type) + " Node"));
	}
}

} // namespace

std::shared_ptr<IMaterial> MaterialEditor::CreateNewMaterial(MaterialEditorDocument& doc, MaterialEditKind kind) {
	doc.ClearNodes();
	doc.currentMaterial.reset();
	doc.compiledShader.reset();
	doc.generatedGlslPath.clear();
	doc.absolutePath.clear();
	doc.editKind = kind;
	doc.materialName = "NewMaterial";
	doc.displayName = doc.materialName;
	doc.dirty = true;
	doc.lastApplyError.clear();

	if (kind == MaterialEditKind::Generic) {
		doc.editMode = MaterialEditMode::Inspector;
		auto mat = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse);
		mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
		doc.currentMaterial = mat;
	} else {
		doc.editMode = MaterialEditMode::NodeGraph;
		doc.currentMaterial = std::make_shared<CustomShaderMaterial>(std::string());
		SeedDefaultCustomGraph(doc);
	}
	return doc.currentMaterial;
}

bool MaterialEditor::LoadFromFile(MaterialEditorDocument& doc, const std::string& path, const std::string& projectRoot, bool deferredGBuffer) {
	if (!doc.LoadFromFile(path)) return false;
	doc.lastApplyError.clear();

	if (doc.editKind == MaterialEditKind::Custom) {
		if (doc.editMode == MaterialEditMode::Text) {
			if (!doc.codeDoc) doc.codeDoc = new CodeEditorDocument();
			SetupGlslEditor(doc.codeDoc);
			doc.codeDoc->editor.SetText(doc.simpleShaderText.empty() ? kDefaultSimpleShaderText : doc.simpleShaderText);
		}
		std::string err;
		if (!ApplyGraphOrTextToLiveMaterial(doc, projectRoot, deferredGBuffer, &err))
			doc.lastApplyError = err;
	}
	return true;
}

bool MaterialEditor::SaveToFile(MaterialEditorDocument& doc, const std::string& path, const std::string& projectRoot, bool deferredGBuffer) {
	if (doc.editKind == MaterialEditKind::Custom) {
		std::string err;
		if (!ApplyGraphOrTextToLiveMaterial(doc, projectRoot, deferredGBuffer, &err))
			doc.lastApplyError = err; // still save the graph/text below so in-progress work isn't lost
	}
	return doc.SaveToFile(path);
}

// Minimal, self-contained (no #includes, no node-graph dependency) solid
// magenta placeholder, built for exactly one branch. Compiled and bound
// whenever a material's real shader fails to (re)compile for a branch
// other than the one it's already running - see the callers below for why
// leaving the old cross-branch program bound is worse than this.
static std::string BuildErrorFallbackShaderGLSL(bool deferredGBuffer) {
	std::ostringstream out;
	out <<
		"#define varying_out out\n"
		"#define attribute_in in\n"
		"#ifdef VERTEX\n"
		"attribute_in vec3 aPosition;\n"
		"uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;\n"
		"void main() {\n"
		"\tgl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);\n"
		"}\n"
		"#endif\n"
		"#ifdef FRAGMENT\n";
	if (deferredGBuffer) {
		out <<
			"layout(location = 0) out vec4 FragData_r;\n"
			"layout(location = 1) out vec4 FragData_g;\n"
			"layout(location = 2) out vec4 FragData_b;\n"
			"layout(location = 3) out vec4 FragData_pbr;\n"
			"void main() {\n"
			"\tFragData_r = vec4(1.0, 0.0, 1.0, 1.0);\n"
			"\tFragData_g = vec4(1.0, 1.0, 1.0, 1.0);\n"
			"\tFragData_b = vec4(0.0, 0.0, 1.0, 1.0);\n"
			"\tFragData_pbr = vec4(0.5, 0.0, 0.0, 0.0);\n"
			"}\n";
	} else {
		out <<
			"varying_out vec4 FragColor;\n"
			"void main() {\n"
			"\tFragColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
			"}\n";
	}
	out << "#endif\n";
	return out.str();
}

// Always produces a branch-correct shader (matching the exact attribute/
// output contract `deferredGBuffer` requires) - the source above has no
// external dependencies, so this is only expected to fail on a genuinely
// broken graphics context, not on anything project-data-related.
static bool CompileFallbackErrorShader(bool deferredGBuffer, std::unique_ptr<Shader>* outShader) {
	return MaterialEditor::CompileMaterialShaderText(BuildErrorFallbackShaderGLSL(deferredGBuffer), deferredGBuffer, outShader, NULL);
}

bool MaterialEditor::ApplyGraphOrTextToLiveMaterial(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer, std::string* errorOut) {
	auto* cm = dynamic_cast<CustomShaderMaterial*>(doc.currentMaterial.get());
	if (!cm) { if (errorOut) *errorOut = "Not a Custom Shader material"; return false; }

	if (doc.generatedGlslPath.empty()) {
		if (doc.absolutePath.empty()) {
			if (errorOut) *errorOut = "Save the material (Save As...) before applying the shader";
			return false;
		}
		// doc.absolutePath is a full filesystem path; generatedGlslPath must
		// stay project-relative like every other call site here assumes
		// (they all do JoinPath(projectRoot, doc.generatedGlslPath)) - strip
		// the root prefix or this doubles up into a broken path.
		std::string base = doc.absolutePath;
		if (!projectRoot.empty() && base.compare(0, projectRoot.size(), projectRoot) == 0) {
			base = base.substr(projectRoot.size());
			while (!base.empty() && (base[0] == '/' || base[0] == '\\')) base.erase(0, 1);
		}
		const size_t dot = base.find_last_of('.');
		if (dot != std::string::npos) base = base.substr(0, dot);
		doc.generatedGlslPath = base + ".generated.glsl";
	}

	std::string glslText;
	std::vector<std::pair<uint32_t, std::string>> textureSamplers;

	if (doc.editMode == MaterialEditMode::Text) {
		if (!doc.codeDoc) { if (errorOut) *errorOut = "No shader text to compile"; return false; }
		doc.simpleShaderText = doc.codeDoc->editor.GetText();
		MaterialCodegenResult gen = GenerateGLSLFromSimpleText(doc.simpleShaderText);
		if (!gen.error.empty()) { if (errorOut) *errorOut = gen.error; return false; }
		glslText = gen.glsl;
		textureSamplers = gen.textureSamplers; // always empty - see GenerateGLSLFromSimpleText's doc comment
	} else {
		MaterialCodegenResult gen = GenerateGLSL(doc.nodes, doc.connections);
		if (!gen.error.empty()) { if (errorOut) *errorOut = gen.error; return false; }
		glslText = gen.glsl;
		textureSamplers = gen.textureSamplers;
		// codeDoc (if it exists from a previous visit to Text mode) is
		// deliberately left untouched here - it holds the user's own simple
		// snippet, an independent alternate representation of the material,
		// not a mirror of whatever the graph currently produces.
	}

	const std::string glslAbsPath = JoinPath(projectRoot, doc.generatedGlslPath);
	{
		std::ofstream f(glslAbsPath);
		if (!f) { if (errorOut) *errorOut = "Could not write " + glslAbsPath; return false; }
		f << glslText;
	}

	std::unique_ptr<Shader> newShader;
	if (!CompileMaterialShaderFile(glslAbsPath, deferredGBuffer, &newShader, errorOut))
	{
		// live material's previous shader is untouched, UNLESS it was
		// compiled for the other branch: running a Forward-branch shader's
		// real per-fragment lighting code inside a Deferred G-buffer MRT
		// pass (or vice versa) silently corrupts rendering - phantom
		// lighting baked from stale shared-UBO data, wrong output/
		// attachment count, wrong depth behavior. A branch-correct magenta
		// placeholder beats that. Same-branch failures (e.g. a typo while
		// iterating without switching renderers) keep the old behavior so
		// editing isn't disrupted mid-edit.
		if (!cm->HasKnownShaderBranch() || cm->IsShaderCompiledForDeferredGBuffer() != deferredGBuffer)
		{
			std::unique_ptr<Shader> fallback;
			if (CompileFallbackErrorShader(deferredGBuffer, &fallback))
			{
				cm->SetShader(fallback.get());
				doc.compiledShader = std::move(fallback);
				cm->MarkShaderBranch(deferredGBuffer);
			}
		}
		return false;
	}

	// SetShader() does NOT take ownership - only reset compiledShader
	// (which owns the previously swapped-in shader, if any) AFTER the swap,
	// so the material never points at a shader that's already been freed.
	cm->SetShader(newShader.get());
	doc.compiledShader = std::move(newShader);
	cm->MarkShaderBranch(deferredGBuffer);

	// Fixed uniform set, always (re-)issued. SendUniform silently skips any
	// name the active shader doesn't declare (GetUniformLocation returns -1
	// -> Shaders.cpp's `Handle > -1` guard), so over-issuing is harmless.
	cm->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
	cm->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
	cm->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
	cm->AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
	cm->AddUniform(Uniform("uCameraPosition", Uniforms::DataUsage::CameraPosition));
	cm->AddUniform(Uniform("uTime", Uniforms::DataUsage::Timer));
	// Forward branch only (harmless to also issue in Deferred mode - the
	// compiled shader simply doesn't declare these, so SendUniform's
	// GetUniformLocation()==-1 guard skips them).
	cm->AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
	cm->AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));

	WireSamplers(cm, textureSamplers, doc.nodes, projectRoot);

	if (errorOut) errorOut->clear();
	doc.lastApplyError.clear();
	doc.hasCompiledShader = true;
	doc.compiledForDeferredGBuffer = deferredGBuffer;
	doc.applyGeneration++;
	return true;
}

// Runs the VERTEX+FRAGMENT compile/link on a Shader that already has its
// source loaded (via LoadShaderFile or LoadShaderText). Returns false and
// sets *errorOut on failure. Shared by the file- and text-compile wrappers.
static bool CompileAndLinkShader(Shader* shader, bool deferredGBuffer, std::string* errorOut) {
	const std::string deferredDefine = deferredGBuffer ? "#define DEFERRED_GBUFFER\n" : "";
	std::string vErr, fErr, lErr;
	const bool vOk = shader->CompileShader(ShaderType::VertexShader, "#define VERTEX\n" + deferredDefine, &vErr);
	const bool fOk = shader->CompileShader(ShaderType::FragmentShader, "#define FRAGMENT\n" + deferredDefine, &fErr);
	const bool linkOk = (vOk && fOk) && shader->LinkProgram(&lErr);
	if (!vOk || !fOk || !linkOk || shader->ShaderProgram() == 0) {
		if (errorOut) {
			*errorOut = "Shader compile/link failed:";
			if (!vOk) *errorOut += "\n[vertex] " + vErr;
			if (!fOk) *errorOut += "\n[fragment] " + fErr;
			if (vOk && fOk && !linkOk) *errorOut += "\n[link] " + lErr;
		}
		return false; // caller's Shader cleans itself up; live material's previous shader is untouched
	}
	return true;
}

bool MaterialEditor::CompileMaterialShaderFile(const std::string& glslAbsPath, bool deferredGBuffer,
                                                std::unique_ptr<Shader>* outShader, std::string* errorOut) {
	std::unique_ptr<Shader> newShader(new Shader());
	newShader->LoadShaderFile(glslAbsPath.c_str());
	if (!CompileAndLinkShader(newShader.get(), deferredGBuffer, errorOut))
		return false; // newShader cleans itself up; live material's previous shader is untouched
	*outShader = std::move(newShader);
	return true;
}

bool MaterialEditor::CompileMaterialShaderText(const std::string& glslText, bool deferredGBuffer,
                                                std::unique_ptr<Shader>* outShader, std::string* errorOut) {
	if (glslText.empty()) { if (errorOut) *errorOut = "no shader text"; return false; }
	std::unique_ptr<Shader> newShader(new Shader());
	newShader->LoadShaderText(glslText);
	if (!CompileAndLinkShader(newShader.get(), deferredGBuffer, errorOut))
		return false; // newShader cleans itself up; live material's previous shader is untouched
	*outShader = std::move(newShader);
	return true;
}

void MaterialEditor::WireSamplers(CustomShaderMaterial* mat,
                                  const std::vector<std::pair<uint32_t, std::string>>& textureSamplers,
                                  const std::vector<MaterialNode>& nodes, const std::string& projectRoot) {
	if (!mat) return;
	mat->ClearSamplers();
	for (const auto& ts : textureSamplers) {
		const MaterialNode* node = nullptr;
		for (const auto& n : nodes) if (n.id == ts.first) { node = &n; break; }
		if (!node || node->texturePath.empty()) continue;
		auto tex = std::make_shared<Texture>();
		if (tex->LoadTexture(JoinPath(projectRoot, ResolveTexturePath(node->texturePath)), TextureType::Texture))
			mat->AddSampler(ts.second, tex);
	}
}

bool MaterialEditor::RecompileFromDisk(CustomShaderMaterial* mat, const std::string& projectRoot,
                                       bool deferredGBuffer, std::string* errorOut) {
	if (!mat) { if (errorOut) *errorOut = "no material"; return false; }

	std::unique_ptr<Shader> newShader;
	bool compiledOk;
	if (!mat->GetShaderFile().empty())
	{
		const std::string shaderPath = mat->GetShaderFile();
		const bool looksAbsolute = (shaderPath[0] == '/') || (shaderPath.size() >= 2 && shaderPath[1] == ':');
		const std::string glslAbsPath = looksAbsolute ? shaderPath : JoinPath(projectRoot, shaderPath);
		compiledOk = CompileMaterialShaderFile(glslAbsPath, deferredGBuffer, &newShader, errorOut);
	}
	else if (mat->GetShaderObject() && !mat->GetShaderObject()->GetShaderText().empty())
	{
		// No recoverable file - the usual case for a Material-Editor-created
		// material assigned to a scene, which SceneSerializer saves via its
		// embedded shaderSource and rebuilds from raw Shader* on load. The
		// cached text is the full generated GLSL (both branches), so it
		// recompiles for the other branch the same way the file path does.
		compiledOk = CompileMaterialShaderText(mat->GetShaderObject()->GetShaderText(), deferredGBuffer, &newShader, errorOut);
	}
	else
	{
		if (errorOut) *errorOut = "no shader file or source";
		compiledOk = false;
	}

	if (!compiledOk)
	{
		// Never leave a shader compiled for the OTHER branch bound - see
		// ApplyGraphOrTextToLiveMaterial's identical guard for why (stale
		// Forward per-fragment lighting running inside the Deferred
		// G-buffer pass or vice versa). A same-branch failure (e.g. the
		// disk source just has a genuine syntax error) keeps the old,
		// still branch-correct shader running instead.
		if (!mat->HasKnownShaderBranch() || mat->IsShaderCompiledForDeferredGBuffer() != deferredGBuffer)
		{
			std::unique_ptr<Shader> fallback;
			if (CompileFallbackErrorShader(deferredGBuffer, &fallback))
			{
				mat->SetShader(fallback.get());
				mat->AdoptShader(std::move(fallback));
				mat->MarkShaderBranch(deferredGBuffer);
			}
		}
		return false;
	}

	// No MaterialEditorDocument owns this material's shader, so adopt it
	// into the material itself - SetShader() alone doesn't take ownership
	// (same as the doc path, where doc.compiledShader plays this role).
	mat->SetShader(newShader.get());
	mat->AdoptShader(std::move(newShader));
	mat->MarkShaderBranch(deferredGBuffer);
	return true;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void DrawToolbar(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer) {
	if (!doc.currentMaterial) return;

	// Fixed-offset SameLine() right-alignment breaks in a narrow docked
	// window (Save button would land under/inside the combos above it) - so
	// every control gets its own row instead of trying to pack a variable-
	// width set of labeled widgets onto one line.
	ImGui::SetNextItemWidth(220.f);
	if (ImGui::InputText("Name", &doc.materialName)) {
		doc.displayName = doc.materialName;
		doc.dirty = true;
	}

	ImGui::SetNextItemWidth(220.f);
	static const char* kindLabels[] = { "Generic Shader", "Custom Shader" };
	int currentKind = (int)doc.editKind;
	if (ImGui::Combo("Type", &currentKind, kindLabels, IM_ARRAYSIZE(kindLabels))) {
		MaterialEditKind newKind = (MaterialEditKind)currentKind;
		if (newKind != doc.editKind) {
			MaterialEditor::CreateNewMaterial(doc, newKind);
			AutoSaveNewMaterial(doc, projectRoot, deferredGBuffer);
		}
	}

	if (doc.editKind == MaterialEditKind::Generic) {
		ImGui::TextDisabled("Inspector only");
	} else {
		ImGui::SetNextItemWidth(220.f);
		static const char* modeLabels[] = { "Inspector", "Text", "Node Graph" };
		int currentMode = (int)doc.editMode;
		if (ImGui::Combo("Edit Mode", &currentMode, modeLabels, IM_ARRAYSIZE(modeLabels))) {
			doc.editMode = (MaterialEditMode)currentMode;
			if (doc.editMode == MaterialEditMode::Text) {
				if (!doc.codeDoc) doc.codeDoc = new CodeEditorDocument();
				SetupGlslEditor(doc.codeDoc);
				doc.codeDoc->editor.SetText(doc.simpleShaderText.empty() ? kDefaultSimpleShaderText : doc.simpleShaderText);
			}
		}
	}

	const bool hasPath = !doc.absolutePath.empty();
	if (ImGui::Button(hasPath ? "Save" : "Save As...", ImVec2(120.f, 0.f))) {
		std::string path = doc.absolutePath;
		if (path.empty())
			path = JoinPath(projectRoot, "assets/materials/" + doc.materialName + ".mat");
		if (!MaterialEditor::SaveToFile(doc, path, projectRoot, deferredGBuffer))
			doc.lastApplyError = "Could not save to " + path;
	}
	if (hasPath) {
		ImGui::SameLine();
		ImGui::TextDisabled("%s", doc.absolutePath.c_str());
	}

	if (!doc.lastApplyError.empty())
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", doc.lastApplyError.c_str());
}

static void DrawTextureSlots(MaterialEditorDocument& doc, GenericShaderMaterial* gm, const std::string& textureRoot) {
	DrawTextureSlot("Color Map", gm->GetColorMap(), textureRoot, [&doc, gm](std::shared_ptr<Texture> t) { gm->SetColorMap(t); doc.dirty = true; });
	DrawTextureSlot("Normal Map", gm->GetNormalMap(), textureRoot, [&doc, gm](std::shared_ptr<Texture> t) { gm->SetNormalMap(t); doc.dirty = true; });
	DrawTextureSlot("Specular Map", gm->GetSpecularMap(), textureRoot, [&doc, gm](std::shared_ptr<Texture> t) { gm->SetSpecularMap(t); doc.dirty = true; });
	DrawTextureSlot("Env Map", gm->GetEnvMap(), textureRoot, [&doc, gm](std::shared_ptr<Texture> t) { gm->SetEnvMap(t); doc.dirty = true; });
	DrawTextureSlot("Metallic/Roughness Map", gm->GetMetallicRoughnessMap(), textureRoot, [&doc, gm](std::shared_ptr<Texture> t) { gm->SetMetallicRoughnessMap(t); doc.dirty = true; });
}

static void DrawGenericMaterialInspector(MaterialEditorDocument& doc, const std::string& projectRoot) {
	auto* gm = dynamic_cast<GenericShaderMaterial*>(doc.currentMaterial.get());
	if (!gm) return;

	// GenericShaderMaterial has no in-place "change options" API - the
	// ShaderUsage bitmask picks/compiles a shader permutation once, in the
	// constructor (see GetOptions()'s doc comment). Toggling a flag here
	// therefore has to construct a *new* GenericShaderMaterial and replace
	// doc.currentMaterial - which cannot propagate back onto a scene
	// object's RenderingMesh::Material this doc might be editing in place
	// (EditMaterialInline holds its own shared_ptr copy of the same
	// original object; reassigning doc.currentMaterial only changes this
	// document's copy). Warn for that case rather than silently doing
	// nothing, which is what the original inspector did.
	if (doc.absolutePath.empty()) {
		ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f),
			"Editing a live object's material - shader-option changes below create a new\n"
			"material object and will NOT reflect back onto the selected object.");
		ImGui::Spacing();
	}

	ImGui::Text("Shader Options (changing these recreates the shader)");
	const uint32 opts = gm->GetOptions();
	uint32 newOpts = opts;

	#define CHK(flag, label) do { bool v = (newOpts & ShaderUsage::flag) != 0; \
		if (ImGui::Checkbox(label, &v)) { \
			if (v) newOpts |= ShaderUsage::flag; else newOpts &= ~ShaderUsage::flag; \
		} ImGui::SameLine(); } while(0)

	CHK(Color, "Color");
	CHK(Texture, "Texture");
	CHK(PBR, "PBR");
	CHK(PBRMap, "PBR Map");
	ImGui::Separator();
	CHK(BumpMapping, "Normal Map");
	CHK(SpecularMap, "Specular Map");
	CHK(EnvMap, "Env Map");
	CHK(Refraction, "Refract");
	ImGui::Separator();
	CHK(CastShadows, "Cast Shadows");
	CHK(DirectionalShadow, "Dir Shadow");
	CHK(PointShadow, "Point Shadow");
	CHK(SpotShadow, "Spot Shadow");
	ImGui::Separator();
	CHK(AlphaTest, "Alpha Cutout");
	CHK(VertexWind, "Vertex Wind");

	#undef CHK
	ImGui::NewLine();

	if (newOpts != opts) {
		auto fresh = std::make_shared<GenericShaderMaterial>(newOpts);
		fresh->SetColor(gm->GetColor());
		fresh->SetSpecular(gm->GetSpecular());
		fresh->SetMetallic(gm->GetMetallic());
		fresh->SetRoughness(gm->GetRoughness());
		fresh->SetSSREnabled(gm->IsSSREnabled());
		fresh->SetAlphaCutoff(gm->GetAlphaCutoff());
		fresh->SetShininess(gm->GetShininess());
		fresh->SetReflectivity(gm->GetReflectivity());
		fresh->SetDisplacementHeight(gm->GetDisplacementHeight());
		// Only the color map is recoverable as a shared_ptr (GetColorMapShared) -
		// other slots only expose a raw Texture*, so they're lost across a
		// recreate. A pre-existing engine gap, not something to newly break.
		if (gm->GetColorMapShared()) fresh->SetColorMap(gm->GetColorMapShared());
		doc.currentMaterial = fresh;
		gm = fresh.get();
		doc.dirty = true;
	}

	ImGui::Spacing();
	ImGui::Text("Colors");

	// Undo here is a value-diff on the live GenericShaderMaterial, NOT a
	// GraphSnapshot (GraphUndoCommit) - Generic-kind materials have no node
	// graph, so a graph snapshot would be an empty no-op.
	#define GM_SETTER(Fn) [](MaterialEditorDocument* d, const auto& v) { \
		if (auto* g = dynamic_cast<GenericShaderMaterial*>(d->currentMaterial.get())) { g->Fn(v); d->dirty = true; } }

	Vec4 color = gm->GetColor();
	float c[4] = { color.x, color.y, color.z, color.w };
	if (ImGui::ColorEdit4("Base Color", c, ImGuiColorEditFlags_AlphaBar)) {
		gm->SetColor(Vec4(c[0], c[1], c[2], c[3]));
		doc.dirty = true;
	}
	{ static Vec4 baseline; UndoMaterialValue<Vec4>(doc, baseline, gm->GetColor(), GM_SETTER(SetColor), "Set Base Color"); }

	Vec4 spec = gm->GetSpecular();
	float s[4] = { spec.x, spec.y, spec.z, spec.w };
	if (ImGui::ColorEdit4("Specular Color", s, ImGuiColorEditFlags_AlphaBar)) {
		gm->SetSpecular(Vec4(s[0], s[1], s[2], s[3]));
		doc.dirty = true;
	}
	{ static Vec4 baseline; UndoMaterialValue<Vec4>(doc, baseline, gm->GetSpecular(), GM_SETTER(SetSpecular), "Set Specular Color"); }

	ImGui::Spacing();
	ImGui::Text("Textures");
	DrawTextureSlots(doc, gm, projectRoot.empty() ? std::string() : JoinPath(projectRoot, "assets/textures"));

	ImGui::Spacing();
	ImGui::Text("PBR");

	float metallic = gm->GetMetallic();
	if (ImGui::SliderFloat("Metallic", &metallic, 0.f, 1.f)) { gm->SetMetallic(metallic); doc.dirty = true; }
	{ static f32 baseline; UndoMaterialValue<f32>(doc, baseline, gm->GetMetallic(), GM_SETTER(SetMetallic), "Set Metallic"); }

	float roughness = gm->GetRoughness();
	if (ImGui::SliderFloat("Roughness", &roughness, 0.f, 1.f)) { gm->SetRoughness(roughness); doc.dirty = true; }
	{ static f32 baseline; UndoMaterialValue<f32>(doc, baseline, gm->GetRoughness(), GM_SETTER(SetRoughness), "Set Roughness"); }

	bool ssr = gm->IsSSREnabled();
	const bool ssrBefore = ssr;
	if (ImGui::Checkbox("Screen Space Reflections", &ssr)) {
		gm->SetSSREnabled(ssr);
		doc.dirty = true;
		MaterialEditorDocument* docPtr = &doc;
		docPtr->undo.Push(std::make_unique<ApplyClosureCommand>(
			[docPtr, ssrBefore]() { if (auto* g = dynamic_cast<GenericShaderMaterial*>(docPtr->currentMaterial.get())) { g->SetSSREnabled(ssrBefore); docPtr->dirty = true; } },
			[docPtr, ssr]() { if (auto* g = dynamic_cast<GenericShaderMaterial*>(docPtr->currentMaterial.get())) { g->SetSSREnabled(ssr); docPtr->dirty = true; } },
			"Toggle Screen Space Reflections"));
	}

	float alphaCutoff = gm->GetAlphaCutoff();
	if (ImGui::SliderFloat("Alpha Cutoff", &alphaCutoff, 0.f, 1.f)) { gm->SetAlphaCutoff(alphaCutoff); doc.dirty = true; }
	{ static f32 baseline; UndoMaterialValue<f32>(doc, baseline, gm->GetAlphaCutoff(), GM_SETTER(SetAlphaCutoff), "Set Alpha Cutoff"); }

	ImGui::Spacing();
	ImGui::Text("Lighting");

	float shininess = gm->GetShininess();
	if (ImGui::SliderFloat("Shininess", &shininess, 0.f, 128.f)) { gm->SetShininess(shininess); doc.dirty = true; }
	{ static f32 baseline; UndoMaterialValue<f32>(doc, baseline, gm->GetShininess(), GM_SETTER(SetShininess), "Set Shininess"); }

	float reflectivity = gm->GetReflectivity();
	if (ImGui::SliderFloat("Reflectivity", &reflectivity, 0.f, 1.f)) { gm->SetReflectivity(reflectivity); doc.dirty = true; }
	{ static f32 baseline; UndoMaterialValue<f32>(doc, baseline, gm->GetReflectivity(), GM_SETTER(SetReflectivity), "Set Reflectivity"); }

	float dispHeight = gm->GetDisplacementHeight();
	if (ImGui::DragFloat("Displacement Height", &dispHeight, 0.01f, -1.f, 5.f)) { gm->SetDisplacementHeight(dispHeight); doc.dirty = true; }
	{ static f32 baseline; UndoMaterialValue<f32>(doc, baseline, gm->GetDisplacementHeight(), GM_SETTER(SetDisplacementHeight), "Set Displacement Height"); }

	#undef GM_SETTER
}

static void DrawCustomMaterialInspector(MaterialEditorDocument& doc) {
	ImGui::Text("Shader File");
	ImGui::TextDisabled("%s", doc.generatedGlslPath.empty() ? "(not saved yet)" : doc.generatedGlslPath.c_str());
	ImGui::Spacing();
	ImGui::BulletText("Use the Text or Node Graph edit mode to author shader logic.");
	if (!doc.lastApplyError.empty()) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", doc.lastApplyError.c_str());
	}
}

static void DrawCommonMaterialSettings(MaterialEditorDocument& doc, IMaterial* mat) {
	if (!mat) return;
	MaterialEditorDocument* docPtr = &doc;
	// Same re-fetch-fresh rationale as DrawGenericMaterialInspector's
	// GM_SETTER - closures read docPtr->currentMaterial rather than
	// capturing `mat` directly.
	#define MAT_TOGGLE(before, EnableCall, DisableCall) \
		docPtr->undo.Push(std::make_unique<ApplyClosureCommand>( \
			[docPtr, before]() { if (docPtr->currentMaterial) { if (before) docPtr->currentMaterial->EnableCall; else docPtr->currentMaterial->DisableCall; docPtr->dirty = true; } }, \
			[docPtr, before]() { if (docPtr->currentMaterial) { if (!before) docPtr->currentMaterial->EnableCall; else docPtr->currentMaterial->DisableCall; docPtr->dirty = true; } }, \
			"Toggle " #EnableCall))

	ImGui::Text("Rendering");

	float opacity = mat->GetOpacity();
	if (ImGui::SliderFloat("Opacity", &opacity, 0.f, 1.f)) { mat->SetOpacity(opacity); doc.dirty = true; }
	{
		static f32 baseline;
		UndoMaterialValue<f32>(doc, baseline, mat->GetOpacity(),
			[](MaterialEditorDocument* d, const f32& v) { if (d->currentMaterial) { d->currentMaterial->SetOpacity(v); d->dirty = true; } },
			"Set Opacity");
	}

	bool transparent = mat->IsTransparent();
	if (ImGui::Checkbox("Transparent", &transparent)) {
		const bool before = !transparent;
		mat->SetTransparencyFlag(transparent);
		doc.dirty = true;
		docPtr->undo.Push(std::make_unique<ApplyClosureCommand>(
			[docPtr, before]() { if (docPtr->currentMaterial) { docPtr->currentMaterial->SetTransparencyFlag(before); docPtr->dirty = true; } },
			[docPtr, transparent]() { if (docPtr->currentMaterial) { docPtr->currentMaterial->SetTransparencyFlag(transparent); docPtr->dirty = true; } },
			"Toggle Transparent"));
	}

	bool blending = mat->IsBlendingEnabled();
	if (ImGui::Checkbox("Blending", &blending)) {
		const bool before = !blending;
		if (blending) mat->EnableBlending(); else mat->DisableBlending();
		doc.dirty = true;
		MAT_TOGGLE(before, EnableBlending(), DisableBlending());
	}

	bool depthTest = mat->IsDepthTesting();
	if (ImGui::Checkbox("Depth Test", &depthTest)) {
		const bool before = !depthTest;
		if (depthTest) mat->EnableDepthTest(); else mat->DisableDepthTest();
		doc.dirty = true;
		MAT_TOGGLE(before, EnableDepthTest(), DisableDepthTest());
	}

	bool depthWrite = mat->IsDepthWritting();
	if (ImGui::Checkbox("Depth Write", &depthWrite)) {
		const bool before = !depthWrite;
		if (depthWrite) mat->EnableDepthWrite(); else mat->DisableDepthWrite();
		doc.dirty = true;
		MAT_TOGGLE(before, EnableDepthWrite(), DisableDepthWrite());
	}

	uint32 cullFace = mat->GetCullFace();
	static const char* cullLabels[] = { "None", "Front", "Back" };
	int cullIdx = (int)cullFace;
	if (ImGui::Combo("Cull Face", &cullIdx, cullLabels, IM_ARRAYSIZE(cullLabels))) {
		const uint32 before = cullFace;
		mat->SetCullFace((uint32)cullIdx);
		doc.dirty = true;
		const uint32 after = (uint32)cullIdx;
		docPtr->undo.Push(std::make_unique<ApplyClosureCommand>(
			[docPtr, before]() { if (docPtr->currentMaterial) { docPtr->currentMaterial->SetCullFace(before); docPtr->dirty = true; } },
			[docPtr, after]() { if (docPtr->currentMaterial) { docPtr->currentMaterial->SetCullFace(after); docPtr->dirty = true; } },
			"Set Cull Face"));
	}

	bool wireframe = mat->IsWireFrame();
	if (ImGui::Checkbox("Wireframe", &wireframe)) {
		const bool before = !wireframe;
		if (wireframe) mat->StartRenderWireFrame(); else mat->StopRenderWireFrame();
		doc.dirty = true;
		MAT_TOGGLE(before, StartRenderWireFrame(), StopRenderWireFrame());
	}

	bool castShadows = mat->IsCastingShadows();
	if (ImGui::Checkbox("Cast Shadows", &castShadows)) {
		const bool before = !castShadows;
		if (castShadows) mat->EnableCastingShadows(); else mat->DisableCastingShadows();
		doc.dirty = true;
		MAT_TOGGLE(before, EnableCastingShadows(), DisableCastingShadows());
	}

	#undef MAT_TOGGLE
}

static void DrawInspectorTab(MaterialEditorDocument& doc, const std::string& projectRoot) {
	if (!doc.currentMaterial) return;
	if (doc.editKind == MaterialEditKind::Generic)
		DrawGenericMaterialInspector(doc, projectRoot);
	else
		DrawCustomMaterialInspector(doc);
	ImGui::Separator();
	DrawCommonMaterialSettings(doc, doc.currentMaterial.get());
}

static void DrawTextEditorTab(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer) {
	if (!doc.codeDoc) {
		doc.codeDoc = new CodeEditorDocument();
		SetupGlslEditor(doc.codeDoc);
		doc.codeDoc->editor.SetText(doc.simpleShaderText.empty() ? kDefaultSimpleShaderText : doc.simpleShaderText);
	}

	// No manual Save Shader button - MaterialEditor::DrawWindow debounce-
	// applies this text to the live material automatically as it changes.
	ImGui::TextDisabled("Set Albedo/Normal/Metallic/Roughness/Emissive/Occlusion - applies automatically. F1/Ctrl+Space for suggestions.");
	if (!doc.lastApplyError.empty())
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", doc.lastApplyError.c_str());

	ImGui::Separator();

	// Same drive sequence as Editor::DrawScriptEditorWindows() (Lua scripts)
	// - vim mode + inline completion are handled identically for both, see
	// CodeEditorDocument.h's completionUseGlsl comment for the one place
	// their behavior actually diverges.
	CodeEditorDocument* cd = doc.codeDoc;
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (focused || cd->completionOpen)
		cd->HandleEditorInput();

	const bool editorKeys = focused && !cd->completionOpen && !cd->completionBlockEditorKeys
		&& (!cd->vimEnabled || cd->vimMode == CodeEditorDocument::VimMode::Insert);
	cd->editor.SetHandleKeyboardInputs(editorKeys);
	ImVec2 avail = ImGui::GetContentRegionAvail();
	cd->editor.Render("##material_shader_editor", avail, true);
	cd->editor.SetHandleKeyboardInputs(true);
	cd->completionBlockEditorKeys = false;
	if (cd->editor.IsTextChanged())
		doc.dirty = true;
	cd->AfterEditorRender();
	if (cd->completionOpen)
		cd->DrawCompletionPopup();
}

static void DrawNodeBody(MaterialEditorDocument& doc, MaterialNode& node, const std::string& projectRoot) {
	static const float pinSpacing = 24.f;
	// The node is now its own clipped child window (see DrawNodeGraphTab) -
	// its own draw list is naturally clipped to its bounds, unlike the
	// shared canvas draw list previously passed in here.
	ImDrawList* bgDrawList = ImGui::GetWindowDrawList();

	switch (node.type) {
		case MaterialNode::Color: {
			float color[4] = {1, 1, 1, 1};
			if (!node.userData.empty())
				sscanf(node.userData.c_str(), "%f,%f,%f,%f", &color[0], &color[1], &color[2], &color[3]);
			if (ImGui::ColorEdit4("Value", color)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Color");
			node.userData = std::to_string(color[0]) + "," + std::to_string(color[1]) + "," +
				std::to_string(color[2]) + "," + std::to_string(color[3]);

			ImVec4 colVec(color[0], color[1], color[2], color[3]);
			float swatchWidth = ImGui::GetContentRegionAvail().x;
			int outPins = MaterialNode::GetOutputPinCount(MaterialNode::Color);
			float swatchHeight = (outPins - 1) * pinSpacing + 20.f;
			ImVec2 swatchPos = ImGui::GetCursorScreenPos();
			bgDrawList->AddRectFilled(swatchPos, ImVec2(swatchPos.x + swatchWidth, swatchPos.y + swatchHeight), ImGui::GetColorU32(colVec));
			break;
		}
		case MaterialNode::Float: {
			float val = 0.5f;
			if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f", &val);
			if (ImGui::DragFloat("Value", &val, 0.01f)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Value");
			node.userData = std::to_string(val);

			float barWidth = ImGui::GetContentRegionAvail().x;
			ImVec2 cursor = ImGui::GetCursorScreenPos();
			float t = std::min(std::max(val, 0.f), 1.f);
			bgDrawList->AddRectFilled(cursor, ImVec2(cursor.x + barWidth * t, cursor.y + 8.f), ImGui::GetColorU32(ImVec4(0.5f, 0.6f, 1.f, 1.f)));
			break;
		}
		case MaterialNode::Int: {
			int val = 0;
			if (!node.userData.empty()) sscanf(node.userData.c_str(), "%d", &val);
			if (ImGui::DragInt("Value", &val)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Value");
			node.userData = std::to_string(val);
			break;
		}
		case MaterialNode::Bool: {
			bool val = (node.userData == "1");
			if (ImGui::Checkbox("Value", &val)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Value");
			node.userData = val ? "1" : "0";
			break;
		}
		case MaterialNode::Vec2Type: {
			float v[2] = {0, 0};
			if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f", &v[0], &v[1]);
			if (ImGui::DragFloat2("Value", v, 0.01f)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Value");
			node.userData = std::to_string(v[0]) + "," + std::to_string(v[1]);
			break;
		}
		case MaterialNode::Vec3Type: {
			float v[3] = {0, 0, 0};
			if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f,%f", &v[0], &v[1], &v[2]);
			if (ImGui::DragFloat3("Value", v, 0.01f)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Value");
			node.userData = std::to_string(v[0]) + "," + std::to_string(v[1]) + "," + std::to_string(v[2]);
			break;
		}
		case MaterialNode::Vec4Type: {
			float v[4] = {0, 0, 0, 1};
			if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
			if (ImGui::DragFloat4("Value", v, 0.01f)) doc.dirty = true;
			GraphUndoCommit(doc, "Set Node Value");
			node.userData = std::to_string(v[0]) + "," + std::to_string(v[1]) + "," + std::to_string(v[2]) + "," + std::to_string(v[3]);
			break;
		}
		case MaterialNode::Texture: {
			const std::string textureRoot = JoinPath(projectRoot, "assets/textures");
			std::string picked;
			if (DrawBrowseButton("Load...", textureRoot, kTextureExtensions, picked)) {
				std::string rel = picked;
				const std::string prefix = textureRoot.empty() ? std::string() : (textureRoot + "/");
				if (!prefix.empty() && rel.compare(0, prefix.size(), prefix) == 0)
					rel = rel.substr(prefix.size());
				node.texturePath = rel;
				delete node.previewTex;
				node.previewTex = new Texture();
				node.previewTex->LoadTexture(picked, TextureType::Texture);
				doc.dirty = true;
			}

			if (!node.texturePath.empty()) {
				ImGui::TextDisabled("%s", node.texturePath.c_str());
				if (!node.previewTex) {
					node.previewTex = new Texture();
					node.previewTex->LoadTexture(JoinPath(projectRoot, ResolveTexturePath(node.texturePath)), TextureType::Texture);
				}
				ImVec2 slotSize(ImGui::GetContentRegionAvail().x, 64.f);
				void* tid = GetActiveRenderDevice().GetImGuiTextureID(node.previewTex->GetBindID(), node.previewTex->GetTextureType());
				if (tid) ImGui::Image((ImTextureID)tid, slotSize);
				else ImGui::TextDisabled("[preview unavailable]");
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL")) {
					const char* relPathC = reinterpret_cast<const char*>(payload->Data);
					std::string relPath(relPathC);
					const std::string texPrefix = "assets/textures/";
					if (relPath.compare(0, texPrefix.size(), texPrefix) == 0) {
						node.texturePath = relPath.substr(texPrefix.size());
						delete node.previewTex;
						node.previewTex = new Texture();
						node.previewTex->LoadTexture(JoinPath(projectRoot, relPath), TextureType::Texture);
						doc.dirty = true;
					}
				}
				ImGui::EndDragDropTarget();
			}
			break;
		}
		case MaterialNode::Output: {
			ImGui::Text("Material Output");
			Vec4 albedo(1.f, 1.f, 1.f, 1.f);
			for (const auto& conn : doc.connections) {
				if (conn.toNode == node.id && conn.toPinIndex == 0) {
					for (const auto& srcNode : doc.nodes) {
						if (srcNode.id == conn.fromNode) {
							albedo = srcNode.ComputePreviewValue(srcNode, doc.nodes, doc.connections);
							break;
						}
					}
				}
			}
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float previewSize = std::min(avail.x, 80.f);
			int inPins = MaterialNode::GetInputPinCount(MaterialNode::Output);
			float previewHeight = (inPins - 1) * pinSpacing + 20.f;
			ImVec2 basePos = ImGui::GetCursorScreenPos();
			ImVec2 previewPos(basePos.x + 10.f, basePos.y);
			bgDrawList->AddRectFilled(previewPos, ImVec2(previewPos.x + previewSize, previewPos.y + previewHeight),
				ImGui::GetColorU32(ImVec4(albedo.x, albedo.y, albedo.z, albedo.w)));
			break;
		}
		default: {
			Vec4 previewVal = node.ComputePreviewValue(node, doc.nodes, doc.connections);
			const char* opName = node.GetOpName();
			if (opName && opName[0] != '\0') ImGui::Text("%s", opName);
			else ImGui::TextDisabled("%s", node.name.c_str());

			float previewWidth = ImGui::GetContentRegionAvail().x;
			int totalPins = MaterialNode::GetInputPinCount(node.type) + MaterialNode::GetOutputPinCount(node.type);
			if (totalPins == 0) totalPins = 1;
			float previewHeight = std::max(16.f, (totalPins - 1) * pinSpacing + 20.f);
			ImVec2 previewPos = ImGui::GetCursorScreenPos();
			bgDrawList->AddRectFilled(previewPos, ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight),
				ImGui::GetColorU32(ImVec4(previewVal.x, previewVal.y, previewVal.z, previewVal.w)));

			char valStr[64];
			snprintf(valStr, sizeof(valStr), "%.2f", previewVal.x);
			bgDrawList->AddText(ImVec2(previewPos.x + 4.f, previewPos.y + 3.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.9f)), valStr);
			break;
		}
	}
}

// Fixed-ish size per node type (nested child windows can't auto-size to
// content the way a top-level ImGui::Begin(..., AlwaysAutoResize) window
// can), roughly matching how much room each type's widget + pin count need.
static ImVec2 EstimateNodeSize(const MaterialNode& node) {
	const int numInputs = MaterialNode::GetInputPinCount(node.type);
	const int numOutputs = MaterialNode::GetOutputPinCount(node.type);
	const int numPins = std::max(numInputs, numOutputs);
	float contentHeight;
	switch (node.type) {
		case MaterialNode::Color: contentHeight = 66.f; break;
		case MaterialNode::Texture: contentHeight = 112.f; break;
		case MaterialNode::Output: contentHeight = 44.f; break;
		default: contentHeight = 40.f; break;
	}
	const float pinsHeight = (numPins > 0) ? (float)(numPins - 1) * 24.f + 20.f : 0.f;
	const float height = 30.f /* header */ + std::max(contentHeight, pinsHeight) + 14.f;
	return ImVec2(190.f, height);
}

static void DrawNodeGraphTab(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer) {
	ImVec2 avail = ImGui::GetContentRegionAvail();
	if (avail.x < 10 || avail.y < 10) return;

	// No manual Apply button - MaterialEditor::DrawWindow debounce-applies
	// the graph to the live material automatically as it changes.
	if (!doc.lastApplyError.empty()) {
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", doc.lastApplyError.c_str());
		ImGui::Separator();
	}

	avail = ImGui::GetContentRegionAvail();
	if (avail.x < 10 || avail.y < 10) return;

	bool canvasHovered = false;
	ImGuiIO& io = ImGui::GetIO();
	static const float pinRadius = 5.f;

	// Everything below lives inside one clipped child - nodes are nested
	// children of THIS, not independent top-level windows, so nothing can
	// render or be dragged outside the Node Graph tab's own bounds.
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
	ImGui::BeginChild("##node_canvas", avail, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	ImDrawList* canvasDrawList = ImGui::GetWindowDrawList();
	ImVec2 canvasScreenPos = ImGui::GetCursorScreenPos();
	ImVec2 gridSize(40.f * doc.graphZoom, 40.f * doc.graphZoom);
	int startX = (int)(-doc.graphOffset.x / doc.graphZoom);
	int startY = (int)(-doc.graphOffset.y / doc.graphZoom);
	int endX = startX + (int)(avail.x / gridSize.x) + 2;
	int endY = startY + (int)(avail.y / gridSize.y) + 2;

	for (int x = startX; x <= endX; x++) {
		float sx = canvasScreenPos.x + doc.graphOffset.x + x * gridSize.x;
		canvasDrawList->AddLine(ImVec2(sx, canvasScreenPos.y), ImVec2(sx, canvasScreenPos.y + avail.y), ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.15f, 0.8f)), 1.f);
	}
	for (int y = startY; y <= endY; y++) {
		float sy = canvasScreenPos.y + doc.graphOffset.y + y * gridSize.y;
		canvasDrawList->AddLine(ImVec2(canvasScreenPos.x, sy), ImVec2(canvasScreenPos.x + avail.x, sy), ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.15f, 0.8f)), 1.f);
	}

	std::vector<PinPosition> allPins;

	for (auto& node : doc.nodes) {
		const ImVec2 screenPos(node.pos.x * doc.graphZoom + canvasScreenPos.x + doc.graphOffset.x,
			node.pos.y * doc.graphZoom + canvasScreenPos.y + doc.graphOffset.y);
		const ImVec2 nodeSize = EstimateNodeSize(node);

		ImGui::PushID((int)node.id);

		ImVec4 bgColor(0.15f, 0.15f, 0.18f, 0.95f);
		switch (node.type) {
			case MaterialNode::Color: bgColor = ImVec4(0.3f, 0.2f, 0.4f, 0.95f); break;
			case MaterialNode::Texture: bgColor = ImVec4(0.2f, 0.35f, 0.2f, 0.95f); break;
			case MaterialNode::Float: bgColor = ImVec4(0.2f, 0.25f, 0.4f, 0.95f); break;
			case MaterialNode::Output: bgColor = ImVec4(0.4f, 0.3f, 0.15f, 0.95f); break;
			default: break;
		}

		ImGui::SetCursorScreenPos(screenPos);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.05f, 0.05f, 0.06f, 1.f));

		// A real nested child - clipped to, and unable to move outside, the
		// canvas child above (unlike a top-level ImGui::Begin() window).
		ImGui::BeginChild("node", nodeSize, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.f));
		ImGui::TextUnformatted(node.name.c_str());
		ImGui::PopStyleColor();
		// An InvisibleButton laid directly over the title text, not
		// IsItemHovered() on the text itself: Text widgets have no notion
		// of "active" in ImGui, so the old check re-tested hover on the
		// title's rect every single frame of the drag - the instant the
		// cursor left that ~13px-tall label (trivially easy at normal drag
		// speed, since node.pos hasn't moved yet on the frame hover is
		// lost), the drag silently stopped applying delta. Net effect: the
		// node never visibly moved. IsItemActive() on a real button latches
		// from mouse-down to mouse-up regardless of where the cursor drifts
		// meanwhile - the same idiom ImGui's own drag/slider widgets use
		// internally, and what "drag a node's title to move it" needs.
		const ImVec2 titleMin = ImGui::GetItemRectMin();
		const ImVec2 titleSize = ImGui::GetItemRectSize();
		ImGui::SetCursorScreenPos(titleMin);
		ImGui::InvisibleButton("##drag_handle", titleSize);
		const bool headerHovered = ImGui::IsItemActive();
		// Must be read right here, immediately after the InvisibleButton -
		// IsItemActivated()/IsItemDeactivated() refer to the *last* ImGui
		// item, and DrawNodeBody()/the context-menu popup below draw many
		// more items before the drag logic that consumes these runs.
		const bool dragHandleActivated = ImGui::IsItemActivated();
		const bool dragHandleDeactivated = ImGui::IsItemDeactivated();
		ImGui::Separator();

		const bool nodeHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		if (nodeHovered) canvasHovered = true;

		DrawNodeBody(doc, node, projectRoot);

		if (nodeHovered && ImGui::IsMouseClicked(1))
			ImGui::OpenPopup("NodeContextMenu");

		if (ImGui::BeginPopup("NodeContextMenu")) {
			if (ImGui::MenuItem("Delete Node")) {
				const MaterialEditorDocument::GraphSnapshot before = doc.CaptureGraphSnapshot();
				const uint32_t deleteId = node.id;
				for (auto& n : doc.nodes) {
					if (n.id == deleteId && n.previewTex) { delete n.previewTex; n.previewTex = nullptr; break; }
				}
				doc.nodes.erase(std::remove_if(doc.nodes.begin(), doc.nodes.end(), [deleteId](const MaterialNode& n) { return n.id == deleteId; }), doc.nodes.end());
				doc.connections.erase(std::remove_if(doc.connections.begin(), doc.connections.end(), [deleteId](const MaterialConnection& c) { return c.fromNode == deleteId || c.toNode == deleteId; }), doc.connections.end());
				doc.dirty = true;
				doc.undo.Push(std::make_unique<GraphEditCommand>(&doc, before, doc.CaptureGraphSnapshot(), "Delete Node"));
			}
			ImGui::EndPopup();
		}

		// Node drag spans multiple frames (mouse held down), unlike the
		// one-shot mutations above - IsItemActivated()/IsItemDeactivated()
		// on the drag-handle InvisibleButton (captured right after it was
		// drawn, see dragHandleActivated/dragHandleDeactivated above) give
		// the same begin/end gesture boundary IsItemDeactivatedAfterEdit()
		// gives standard widgets (a plain InvisibleButton has no "edited"
		// concept, so the plain Deactivated edge is used instead).
		if (dragHandleActivated)
			doc.BeginGraphEdit();
		if (!doc.isDraggingConnection && headerHovered && ImGui::IsMouseDragging(0)) {
			ImVec2 delta(io.MouseDelta.x / doc.graphZoom, io.MouseDelta.y / doc.graphZoom);
			node.pos.x += delta.x;
			node.pos.y += delta.y;
			if (delta.x != 0.f || delta.y != 0.f) doc.dirty = true;
		}
		if (dragHandleDeactivated)
			doc.CommitGraphEdit("Move Node");

		ImGui::EndChild();
		ImGui::PopStyleColor(2);

		// Pins are drawn on the CANVAS's draw list (not the node's own,
		// which would clip a pin circle sitting right on the node's edge)
		// using the node's known screen rect - no need to query
		// GetWindowPos/Size since we already have screenPos/nodeSize.
		const float centerY = screenPos.y + nodeSize.y * 0.5f;
		const int numInputs = MaterialNode::GetInputPinCount(node.type);
		const int numOutputs = MaterialNode::GetOutputPinCount(node.type);

		for (int i = 0; i < numInputs; i++) {
			float y = centerY + (i - (numInputs - 1) * 0.5f) * 24.f;
			ImVec2 pinPos(screenPos.x, y);

			const char* label = MaterialNode::GetInputPinLabel(node.type, i);
			char lbl[8];
			if (!label) { snprintf(lbl, sizeof(lbl), "In%d", i + 1); label = lbl; }
			ImVec2 textSize = ImGui::CalcTextSize(label);
			canvasDrawList->AddText(ImVec2(pinPos.x - pinRadius - 4.f - textSize.x, y - 6.f), ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.f)), label);
			canvasDrawList->AddCircleFilled(pinPos, pinRadius, ImGui::GetColorU32(ImVec4(0.5f, 0.6f, 1.f, 1.f)));

			PinPosition pp; pp.nodeId = node.id; pp.pinIndex = i; pp.isOutput = false; pp.screenPos = pinPos;
			allPins.push_back(pp);
		}

		if (numOutputs > 0) {
			for (int i = 0; i < numOutputs; i++) {
				float y = centerY + (i - (numOutputs - 1) * 0.5f) * 24.f;
				ImVec2 pinPos(screenPos.x + nodeSize.x, y);
				canvasDrawList->AddCircleFilled(pinPos, pinRadius, ImGui::GetColorU32(ImVec4(0.5f, 0.8f, 0.5f, 1.f)));

				const char* label = MaterialNode::GetOutputPinLabel(node.type, i);
				char lbl[8];
				if (!label) { snprintf(lbl, sizeof(lbl), "Out%d", i + 1); label = lbl; }
				canvasDrawList->AddText(ImVec2(pinPos.x + pinRadius + 4.f, y - 6.f), ImGui::GetColorU32(ImVec4(0.7f, 0.8f, 0.5f, 1.f)), label);

				PinPosition pp; pp.nodeId = node.id; pp.pinIndex = i; pp.isOutput = true; pp.screenPos = pinPos;
				allPins.push_back(pp);

				if (IsPinHovered(pinPos, pinRadius + 2.f) && ImGui::IsMouseClicked(0)) {
					doc.isDraggingConnection = true;
					doc.dragFromNode = node.id;
					doc.dragFromPinIndex = i;
					doc.dragStartPos = pinPos;
				}
			}
		}

		ImGui::PopID();
	}

	ImDrawList* bgDrawList = canvasDrawList; // connections/drag-line drawing below is unchanged, just renamed
	for (const auto& conn : doc.connections) {
		ImVec2 fromPos, toPos;
		bool foundFrom = false, foundTo = false;
		for (const auto& p : allPins) {
			if (!foundFrom && p.nodeId == conn.fromNode && p.pinIndex == conn.fromPinIndex && p.isOutput) { fromPos = p.screenPos; foundFrom = true; }
			if (!foundTo && p.nodeId == conn.toNode && p.pinIndex == conn.toPinIndex && !p.isOutput) { toPos = p.screenPos; foundTo = true; }
		}
		if (foundFrom && foundTo) {
			float dx = (toPos.x - fromPos.x) * 0.5f;
			bgDrawList->AddBezierCubic(fromPos, ImVec2(fromPos.x + dx, fromPos.y), ImVec2(toPos.x - dx, toPos.y), toPos, ImGui::GetColorU32(ImVec4(0.4f, 0.6f, 0.9f, 0.7f)), 2.f);
		}
	}

	if (!canvasHovered && !doc.isDraggingConnection && ImGui::IsWindowHovered()) {
		if (io.MouseWheel != 0) {
			doc.graphZoom *= 1.f + io.MouseWheel * 0.1f;
			doc.graphZoom = std::max(0.25f, std::min(doc.graphZoom, 4.f));
		}
		if (ImGui::IsMouseDragging(1)) {
			doc.graphOffset.x += io.MouseDelta.x;
			doc.graphOffset.y += io.MouseDelta.y;
		}
		if (ImGui::IsMouseClicked(1))
			ImGui::OpenPopup("AddNodeMenu");
	}

	HandleConnectionDrag(doc, bgDrawList, allPins);

	if (ImGui::BeginPopup("AddNodeMenu")) {
		ImVec2 mouseGraphPos(
			(io.MousePos.x - canvasScreenPos.x - doc.graphOffset.x) / doc.graphZoom,
			(io.MousePos.y - canvasScreenPos.y - doc.graphOffset.y) / doc.graphZoom);

		if (ImGui::BeginMenu("Constants")) {
			DrawAddNodeItem(doc, MaterialNode::Color, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Float, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Texture, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Int, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Bool, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Vec2Type, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Vec3Type, mouseGraphPos);
			DrawAddNodeItem(doc, MaterialNode::Vec4Type, mouseGraphPos);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Math")) {
			for (auto t : { MaterialNode::Add, MaterialNode::Subtract, MaterialNode::Multiply, MaterialNode::Divide,
				MaterialNode::Power, MaterialNode::Modulo, MaterialNode::Negate, MaterialNode::Abs, MaterialNode::Sqrt,
				MaterialNode::Sin, MaterialNode::Cos, MaterialNode::Tan, MaterialNode::Min, MaterialNode::Max,
				MaterialNode::Clamp, MaterialNode::Lerp, MaterialNode::DotProduct, MaterialNode::CrossProduct,
				MaterialNode::Length, MaterialNode::Normalize, MaterialNode::Distance })
				DrawAddNodeItem(doc, t, mouseGraphPos);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Comparison / Logic")) {
			for (auto t : { MaterialNode::Equal, MaterialNode::NotEqual, MaterialNode::GreaterThan, MaterialNode::LessThan,
				MaterialNode::And, MaterialNode::Or, MaterialNode::Not, MaterialNode::Step, MaterialNode::SmoothStep })
				DrawAddNodeItem(doc, t, mouseGraphPos);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Vector")) {
			for (auto t : { MaterialNode::SplitVec2, MaterialNode::SplitVec3, MaterialNode::SplitVec4,
				MaterialNode::CombineVec2, MaterialNode::CombineVec3, MaterialNode::CombineVec4 })
				DrawAddNodeItem(doc, t, mouseGraphPos);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Special")) {
			for (auto t : { MaterialNode::ObjectPosition, MaterialNode::CameraPosition, MaterialNode::UVCoordinate,
				MaterialNode::NormalVector, MaterialNode::TimeValue })
				DrawAddNodeItem(doc, t, mouseGraphPos);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		DrawAddNodeItem(doc, MaterialNode::Output, mouseGraphPos);

		ImGui::EndPopup();
	}

	ImGui::EndChild(); // ##node_canvas
}

void MaterialEditor::DrawWindow(MaterialEditorDocument& doc, const std::string& projectRoot, bool deferredGBuffer) {
	DrawToolbar(doc, projectRoot, deferredGBuffer);
	ImGui::Separator();

	if (!doc.currentMaterial) {
		ImGui::TextDisabled("No material loaded.");
		ImGui::Spacing();
		if (ImGui::Button("New Generic Material")) {
			CreateNewMaterial(doc, MaterialEditKind::Generic);
			AutoSaveNewMaterial(doc, projectRoot, deferredGBuffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("New Custom Shader Material")) {
			CreateNewMaterial(doc, MaterialEditKind::Custom);
			AutoSaveNewMaterial(doc, projectRoot, deferredGBuffer);
		}
		return;
	}

	// Node graph / text content fills the whole remaining area - the preview
	// (below) floats over its top-right corner afterward instead of
	// stacking above it, so editing space isn't cut into a fixed-height
	// horizontal band. contentTopLeft is captured now (absolute screen
	// coords, so it survives whatever cursor/scroll state the mode content
	// leaves behind) purely to anchor that overlay once the content beneath
	// it has actually been drawn.
	const ImVec2 contentTopLeft = ImGui::GetCursorScreenPos();
	const float contentWidth = ImGui::GetContentRegionAvail().x;

	switch (doc.editMode) {
		case MaterialEditMode::Inspector: DrawInspectorTab(doc, projectRoot); break;
		case MaterialEditMode::Text: DrawTextEditorTab(doc, projectRoot, deferredGBuffer); break;
		case MaterialEditMode::NodeGraph: DrawNodeGraphTab(doc, projectRoot, deferredGBuffer); break;
	}

	// Debounced auto-apply (Custom kind only) - no separate Apply/Save
	// Shader click needed, the live material just stays in sync with
	// whatever's in the graph/text editor. Debounced on a content-changed
	// timer (not "mouse released"/IsAnyItemActive - the node canvas and the
	// vendored TextEditor don't reliably surface either) so a shader
	// recompile+link doesn't fire on every single frame of a slider drag or
	// keystroke, only once things hold still for a moment. `dirty` can't
	// double as this signal - Apply intentionally never clears it, since a
	// live-compiled material can still need an explicit File Save.
	if (doc.editKind == MaterialEditKind::Custom) {
		const std::string liveFingerprint = (doc.editMode == MaterialEditMode::Text)
			? (doc.codeDoc ? doc.codeDoc->editor.GetText() : std::string())
			: doc.AgentGetGraph().dump();
		if (liveFingerprint != doc.pendingFingerprint) {
			doc.pendingFingerprint = liveFingerprint;
			doc.pendingFingerprintSince = ImGui::GetTime();
		}
		constexpr double kAutoApplyDebounceSeconds = 0.35;
		const bool settled = (ImGui::GetTime() - doc.pendingFingerprintSince) > kAutoApplyDebounceSeconds;
		if (settled && (!doc.autoAppliedValid || doc.pendingFingerprint != doc.autoAppliedFingerprint)) {
			std::string err;
			MaterialEditor::ApplyGraphOrTextToLiveMaterial(doc, projectRoot, deferredGBuffer, &err);
			doc.lastApplyError = err;
			doc.autoAppliedFingerprint = doc.pendingFingerprint;
			doc.autoAppliedValid = true;
		}
	}

	// Live sphere preview (Custom kind only) - a genuine floating top-level
	// window (Begin, not BeginChild) positioned over the top-right corner
	// of whatever the node graph canvas / text editor just drew. This has
	// to be a real window, not a second overlapping BeginChild sibling of
	// the canvas: Dear ImGui only resolves hover/input ownership between
	// truly overlapping regions through its top-level window z-stack -
	// sibling child windows of the same parent don't get that, so orbit-
	// dragging the sphere could drag the canvas underneath instead (or vice
	// versa) depending on which one ImGui happened to treat as hovered.
	// AlwaysAutoResize (rather than a hand-computed fixed height) means the
	// "Lights" checkbox row below the image can never end up clipped by a
	// height guess that didn't leave quite enough room.
	if (doc.editKind == MaterialEditKind::Custom) {
		if (!doc.preview) doc.preview = std::make_unique<MaterialPreview>();
		doc.preview->EnsureInit(deferredGBuffer);
		doc.preview->SyncFromDoc(doc, projectRoot);

		const float margin = 8.f;
		const float pw = (float)doc.preview->width;
		ImGui::SetNextWindowPos(ImVec2(contentTopLeft.x + std::max(0.f, contentWidth - pw) - margin, contentTopLeft.y + margin), ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav
			| ImGuiWindowFlags_AlwaysAutoResize;
		ImGui::Begin("##MaterialPreviewFloat", nullptr, flags);
		// Being a real window (see the comment above) only fixes hover
		// ownership if it's actually topmost - being freshly Begin()'d
		// doesn't raise it in the display/hit-test order on its own, and
		// this window is never "appearing" (NoFocusOnAppearing, same ID
		// every frame) or clicked-to-focus (nothing here calls
		// SetWindowFocus, which would steal keyboard focus from the
		// node/text editor every single frame). BringWindowToDisplayFront
		// reorders it for rendering/hit-testing only, every frame,
		// independent of focus - exactly what's needed here.
		ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
		doc.preview->DrawAndUpdate();
		ImGui::End();
	}
}
