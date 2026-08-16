//============================================================================
// Name        : MaterialEditorDocument.cpp
// Description : Material document data + .mat JSON round-trip.
//============================================================================

#include "MaterialEditorDocument.h"
// Complete MaterialPreview type: ~MaterialEditorDocument() (below) destroys
// the unique_ptr<MaterialPreview> member, which needs the full definition.
#include "MaterialPreview.h"
#include "CodeEditorDocument.h"
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <fstream>

using json = nlohmann::json;
using namespace p3d;

namespace {

json SerializeNodes(const std::vector<MaterialNode>& nodes) {
	json arr = json::array();
	for (const auto& n : nodes) {
		json jn;
		jn["id"] = n.id;
		jn["type"] = MaterialNode::TypeToString(n.type);
		jn["name"] = n.name;
		jn["pos"] = { n.pos.x, n.pos.y };
		jn["userData"] = n.userData;
		jn["texturePath"] = n.texturePath;
		arr.push_back(jn);
	}
	return arr;
}

json SerializeConnections(const std::vector<MaterialConnection>& connections) {
	json arr = json::array();
	for (const auto& c : connections) {
		json jc;
		jc["fromNode"] = c.fromNode;
		jc["fromPinIndex"] = c.fromPinIndex;
		jc["toNode"] = c.toNode;
		jc["toPinIndex"] = c.toPinIndex;
		arr.push_back(jc);
	}
	return arr;
}

void DeserializeNodes(const json& arr, std::vector<MaterialNode>& outNodes, uint32_t& outNextId) {
	outNodes.clear();
	outNextId = 1;
	for (const auto& jn : arr) {
		MaterialNode n;
		n.id = jn.value("id", 0u);
		std::string typeStr = jn.value("type", "Float");
		if (!MaterialNode::TypeFromString(typeStr, n.type))
			n.type = MaterialNode::Float;
		n.name = jn.value("name", std::string());
		if (jn.find("pos") != jn.end() && jn["pos"].size() >= 2)
			n.pos = ImVec2(jn["pos"][0], jn["pos"][1]);
		n.userData = jn.value("userData", std::string());
		n.texturePath = jn.value("texturePath", std::string());
		outNodes.push_back(n);
		if (n.id >= outNextId) outNextId = n.id + 1;
	}
}

void DeserializeConnections(const json& arr, std::vector<MaterialConnection>& outConnections) {
	outConnections.clear();
	for (const auto& jc : arr) {
		MaterialConnection c;
		c.fromNode = jc.value("fromNode", 0u);
		c.fromPinIndex = jc.value("fromPinIndex", 0);
		c.toNode = jc.value("toNode", 0u);
		c.toPinIndex = jc.value("toPinIndex", 0);
		outConnections.push_back(c);
	}
}

void ApplyCommonMaterialSettings(const IMaterial* mat, json& j) {
	IMaterial* m = const_cast<IMaterial*>(mat);
	j["opacity"] = mat->GetOpacity();
	j["transparent"] = mat->IsTransparent();
	j["cullFace"] = mat->GetCullFace();
	j["wireframe"] = mat->IsWireFrame();
	j["castingShadows"] = m->IsCastingShadows();
	j["depthTest"] = m->IsDepthTesting();
	j["depthWrite"] = m->IsDepthWritting();
	j["blending"] = mat->IsBlendingEnabled();
}

void ReadCommonMaterialSettings(IMaterial* mat, const json& j) {
	if (j.find("opacity") != j.end()) mat->SetOpacity(j["opacity"]);
	if (j.value("transparent", false)) mat->SetTransparencyFlag(true);
	if (j.find("cullFace") != j.end()) mat->SetCullFace((uint32)j["cullFace"]);
	if (j.value("wireframe", false)) mat->StartRenderWireFrame();
	if (j.value("castingShadows", true)) mat->EnableCastingShadows(); else mat->DisableCastingShadows();
	if (j.value("depthTest", true)) mat->EnableDepthTest(); else mat->DisableDepthTest();
	if (j.value("depthWrite", true)) mat->EnableDepthWrite(); else mat->DisableDepthWrite();
	if (j.value("blending", false)) mat->EnableBlending();
}

} // namespace

MaterialEditorDocument::~MaterialEditorDocument() {
	delete codeDoc;
	codeDoc = nullptr;
	ClearNodes();
}

uint32_t MaterialEditorDocument::CreateNode(MaterialNode::Type type, const std::string& name, ImVec2 pos) {
	MaterialNode node;
	node.id = nextNodeId++;
	node.name = name;
	node.pos = pos;
	node.type = type;
	nodes.push_back(node);
	return node.id;
}

void MaterialEditorDocument::ClearNodes() {
	for (auto& n : nodes) {
		delete n.previewTex;
		n.previewTex = nullptr;
	}
	nodes.clear();
	connections.clear();
}

bool MaterialEditorDocument::LoadFromFile(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open()) return false;

	json j;
	try { f >> j; } catch (...) { return false; }

	const std::string kind = j.value("kind", "generic");
	materialName = j.value("name", path.substr(path.find_last_of("/\\") + 1));
	const std::string modeStr = j.value("editMode", "inspector");
	editMode = (modeStr == "text") ? MaterialEditMode::Text
		: (modeStr == "nodegraph") ? MaterialEditMode::NodeGraph
		: MaterialEditMode::Inspector;

	ClearNodes();

	if (kind == "generic") {
		editKind = MaterialEditKind::Generic;
		auto mat = std::make_shared<GenericShaderMaterial>(j.value("options", 0u));
		if (j.find("color") != j.end() && j["color"].size() >= 4)
			mat->SetColor(Vec4(j["color"][0], j["color"][1], j["color"][2], j["color"][3]));
		if (j.find("specular") != j.end() && j["specular"].size() >= 4)
			mat->SetSpecular(Vec4(j["specular"][0], j["specular"][1], j["specular"][2], j["specular"][3]));
		if (j.find("metallic") != j.end()) mat->SetMetallic(j["metallic"]);
		if (j.find("roughness") != j.end()) mat->SetRoughness(j["roughness"]);
		if (j.value("ssrEnabled", false)) mat->SetSSREnabled(true);
		if (j.find("alphaCutoff") != j.end()) mat->SetAlphaCutoff(j["alphaCutoff"]);
		if (j.find("shininess") != j.end()) mat->SetShininess(j["shininess"]);
		if (j.find("reflectivity") != j.end()) mat->SetReflectivity(j["reflectivity"]);
		if (j.find("displacementHeight") != j.end()) mat->SetDisplacementHeight(j["displacementHeight"]);
		ReadCommonMaterialSettings(mat.get(), j);
		currentMaterial = mat;
	} else {
		editKind = MaterialEditKind::Custom;
		generatedGlslPath = j.value("generatedGlslPath", std::string());
		if (j.find("nodes") != j.end())
			DeserializeNodes(j["nodes"], nodes, nextNodeId);
		if (j.find("connections") != j.end())
			DeserializeConnections(j["connections"], connections);
		// Placeholder - MaterialEditor::LoadFromFile compiles the real shader
		// (from generatedGlslPath or freshly codegen'd) right after this call.
		auto mat = std::make_shared<CustomShaderMaterial>(std::string());
		ReadCommonMaterialSettings(mat.get(), j);
		currentMaterial = mat;
	}

	absolutePath = path;
	displayName = materialName;
	dirty = false;
	return true;
}

bool MaterialEditorDocument::SaveToFile(const std::string& path) {
	if (!currentMaterial) return false;

	json j;
	j["name"] = materialName;
	j["editMode"] = (editMode == MaterialEditMode::Text) ? "text"
		: (editMode == MaterialEditMode::NodeGraph) ? "nodegraph" : "inspector";

	if (auto* gm = dynamic_cast<GenericShaderMaterial*>(currentMaterial.get())) {
		j["kind"] = "generic";
		j["options"] = gm->GetOptions();
		const auto& c = gm->GetColor();
		j["color"] = { c.x, c.y, c.z, c.w };
		const auto& s = gm->GetSpecular();
		j["specular"] = { s.x, s.y, s.z, s.w };
		j["metallic"] = gm->GetMetallic();
		j["roughness"] = gm->GetRoughness();
		j["ssrEnabled"] = gm->IsSSREnabled();
		j["alphaCutoff"] = gm->GetAlphaCutoff();
		j["shininess"] = gm->GetShininess();
		j["reflectivity"] = gm->GetReflectivity();
		j["displacementHeight"] = gm->GetDisplacementHeight();
	} else {
		j["kind"] = "custom";
		j["generatedGlslPath"] = generatedGlslPath;
		j["nodes"] = SerializeNodes(nodes);
		j["connections"] = SerializeConnections(connections);
	}

	ApplyCommonMaterialSettings(currentMaterial.get(), j);

	std::ofstream f(path);
	if (!f.is_open()) return false;
	f << j.dump(2);

	absolutePath = path;
	displayName = materialName;
	dirty = false;
	return true;
}

bool MaterialEditorDocument::AgentSetGraph(const json& nodesArr, const json& connectionsArr, std::string& errOut) {
	if (!nodesArr.is_array()) { errOut = "'nodes' must be an array"; return false; }
	if (!connectionsArr.is_null() && !connectionsArr.is_array()) { errOut = "'connections' must be an array"; return false; }

	std::vector<MaterialNode> newNodes;
	uint32_t newNextId = 1;
	DeserializeNodes(nodesArr, newNodes, newNextId);

	std::vector<MaterialConnection> newConnections;
	if (connectionsArr.is_array())
		DeserializeConnections(connectionsArr, newConnections);

	bool sawOutput = false;
	for (const auto& n : newNodes)
		if (n.type == MaterialNode::Output) { sawOutput = true; break; }
	if (!sawOutput) { errOut = "graph has no Output node - nothing would render"; return false; }

	ClearNodes(); // frees existing preview textures before nodes/connections are replaced
	nodes = newNodes;
	connections = newConnections;
	nextNodeId = newNextId;
	dirty = true;
	return true;
}

json MaterialEditorDocument::AgentGetGraph() const {
	json j;
	j["kind"] = (editKind == MaterialEditKind::Generic) ? "generic" : "custom";
	j["editMode"] = (editMode == MaterialEditMode::Text) ? "text"
		: (editMode == MaterialEditMode::NodeGraph) ? "nodegraph" : "inspector";
	j["generatedGlslPath"] = generatedGlslPath;
	j["nodes"] = SerializeNodes(nodes);
	j["connections"] = SerializeConnections(connections);
	return j;
}
