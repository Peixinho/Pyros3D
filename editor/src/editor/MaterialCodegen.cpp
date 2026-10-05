//============================================================================
// Name        : MaterialCodegen.cpp
// Description : Node graph -> GLSL codegen implementation. See MaterialCodegen.h.
//============================================================================

#include "MaterialCodegen.h"
#include <cstdio>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace {

// GLSL float literals need a decimal point ("1" is an int literal); %g alone
// can produce "1" for 1.0f, which some strict GLSL front-ends reject as an
// argument to a float-expecting constructor.
std::string FormatFloat(float v) {
	char buf[64];
	snprintf(buf, sizeof(buf), "%g", v);
	std::string s = buf;
	if (s.find('.') == std::string::npos && s.find('e') == std::string::npos
		&& s.find("inf") == std::string::npos && s.find("nan") == std::string::npos)
		s += ".0";
	return s;
}

std::string Vec4Literal(float x, float y, float z, float w) {
	return "vec4(" + FormatFloat(x) + ", " + FormatFloat(y) + ", " + FormatFloat(z) + ", " + FormatFloat(w) + ")";
}

// Walks the graph, emitting one memoized `vec4 nN = ...;` statement per
// visited node (everything is represented as vec4 for simplicity, matching
// MaterialNode::ComputePreviewValue's existing CPU-preview convention -
// scalar-producing nodes broadcast to all four components, and whichever
// consumer needs a scalar takes `.x`).
//
// One Codegen per shader stage: the Vertex Offset pin's subgraph runs in the
// vertex shader, everything else in the fragment shader, and the built-in
// inputs (UV, normal, position...) are spelled differently in each. A node
// reached from both stages is simply emitted twice, once per stage.
enum class Stage { Vertex, Fragment };

class Codegen {
public:
	Codegen(const std::vector<MaterialNode>& nodes, const std::vector<MaterialConnection>& connections, Stage stage)
		: nodes(nodes), connections(connections), stage(stage) {}

	const MaterialNode* FindNode(uint32_t id) const {
		for (const auto& n : nodes) if (n.id == id) return &n;
		return nullptr;
	}

	bool IsInputConnected(uint32_t nodeId, int pinIndex) const {
		for (const auto& c : connections)
			if (c.toNode == nodeId && c.toPinIndex == pinIndex) return true;
		return false;
	}

	// Resolves the GLSL expression feeding input pin `pinIndex` of `nodeId`,
	// falling back to `defaultExpr` if unconnected (or on error).
	std::string ResolveInput(uint32_t nodeId, int pinIndex, const std::string& defaultExpr) {
		for (const auto& c : connections) {
			if (c.toNode != nodeId || c.toPinIndex != pinIndex) continue;
			const MaterialNode* src = FindNode(c.fromNode);
			if (!src) return defaultExpr;
			std::string var = EmitNode(*src);
			if (!error.empty()) return defaultExpr;
			return ApplyOutputSwizzle(*src, c.fromPinIndex, var);
		}
		return defaultExpr;
	}

	// Per-output-pin swizzle for multi-output nodes (Color's R/G/B/A/RGBA,
	// Texture's RGBA/R/G/B/A, Split*'s X/Y/Z/W) - broadcasts the selected
	// scalar component back to a vec4 so every node's local var stays
	// uniformly vec4-typed.
	std::string ApplyOutputSwizzle(const MaterialNode& src, int fromPinIndex, const std::string& varName) {
		if (src.type == MaterialNode::Color || src.type == MaterialNode::ColorParameter) {
			switch (fromPinIndex) {
				case 0: return "vec4(" + varName + ".x)";
				case 1: return "vec4(" + varName + ".y)";
				case 2: return "vec4(" + varName + ".z)";
				case 3: return "vec4(" + varName + ".w)";
				default: return varName; // RGBA
			}
		}
		if (src.type == MaterialNode::Texture) {
			switch (fromPinIndex) {
				case 1: return "vec4(" + varName + ".x)";
				case 2: return "vec4(" + varName + ".y)";
				case 3: return "vec4(" + varName + ".z)";
				case 4: return "vec4(" + varName + ".w)";
				default: return varName; // RGBA
			}
		}
		if (src.type == MaterialNode::SplitVec2 || src.type == MaterialNode::SplitVec3 || src.type == MaterialNode::SplitVec4) {
			const char* comp = (fromPinIndex == 0) ? ".x" : (fromPinIndex == 1) ? ".y" : (fromPinIndex == 2) ? ".z" : ".w";
			return "vec4(" + varName + comp + ")";
		}
		return varName;
	}

	// Built-in inputs, spelled for this stage. The vertex stage works from
	// the (skinned) world-space position/normal main() computes before the
	// offset is applied - see BuildTemplate.
	std::string UVExpr() const { return stage == Stage::Fragment ? "vec4(vTexcoord, 0.0, 1.0)" : "vec4(aTexcoord, 0.0, 1.0)"; }
	std::string NormalExpr() const { return stage == Stage::Fragment ? "vec4(p3d_N, 0.0)" : "vec4(p3d_worldNormal, 0.0)"; }
	std::string PositionExpr() const { return stage == Stage::Fragment ? "vec4(vWorldPos.xyz, 1.0)" : "vec4(p3d_worldPos.xyz, 1.0)"; }
	std::string ViewDirExpr() const {
		return stage == Stage::Fragment ? "vec4(p3d_V, 0.0)" : "vec4(normalize(uCameraPosition - p3d_worldPos.xyz), 0.0)";
	}

	// Emits (memoized) the statement computing `node`'s value; returns its
	// local variable name, or "" with `error` set on failure.
	std::string EmitNode(const MaterialNode& node) {
		auto found = emitted.find(node.id);
		if (found != emitted.end()) return found->second;
		if (visiting.count(node.id)) { error = "Cycle detected in node graph at node " + std::to_string(node.id); return ""; }
		visiting.insert(node.id);

		const std::string var = std::string(stage == Stage::Vertex ? "vn" : "n") + std::to_string(node.id);
		std::string expr;
		auto in = [&](int pin, const char* def) { return ResolveInput(node.id, pin, def); };

		using T = MaterialNode::Type;
		switch (node.type) {
			case T::Color: {
				float c[4] = {1, 1, 1, 1};
				if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]);
				expr = Vec4Literal(c[0], c[1], c[2], c[3]);
				break;
			}
			case T::Float: {
				float v = 0.5f;
				if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f", &v);
				expr = "vec4(" + FormatFloat(v) + ")";
				break;
			}
			case T::Int: {
				int v = 0;
				if (!node.userData.empty()) sscanf(node.userData.c_str(), "%d", &v);
				expr = "vec4(float(" + std::to_string(v) + "))";
				break;
			}
			case T::Bool: {
				expr = (node.userData == "1") ? "vec4(1.0)" : "vec4(0.0)";
				break;
			}
			case T::Vec2Type: {
				float v[2] = {0, 0};
				if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f", &v[0], &v[1]);
				expr = Vec4Literal(v[0], v[1], 0.f, 0.f);
				break;
			}
			case T::Vec3Type: {
				float v[3] = {0, 0, 0};
				if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f,%f", &v[0], &v[1], &v[2]);
				expr = Vec4Literal(v[0], v[1], v[2], 0.f);
				break;
			}
			case T::Vec4Type: {
				float v[4] = {0, 0, 0, 1};
				if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
				expr = Vec4Literal(v[0], v[1], v[2], v[3]);
				break;
			}
			case T::FloatParameter: case T::ColorParameter: {
				const bool isVector = node.type == T::ColorParameter;
				const std::string name = MaterialNode::SanitizeParameterName(node.name);
				if (name.empty()) { error = "A parameter node has no name"; break; }
				auto existing = parameterIndex.find(name);
				if (existing != parameterIndex.end()) {
					if (parameters[existing->second].isVector != isVector) {
						error = "Parameter '" + name + "' is used as both a Float and a Color parameter";
						break;
					}
				} else {
					MaterialCodegenResult::Parameter p;
					p.name = name;
					p.isVector = isVector;
					if (isVector) {
						p.value[0] = p.value[1] = p.value[2] = p.value[3] = 1.f;
						if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f,%f,%f,%f", &p.value[0], &p.value[1], &p.value[2], &p.value[3]);
					} else {
						p.value[0] = 0.5f;
						if (!node.userData.empty()) sscanf(node.userData.c_str(), "%f", &p.value[0]);
					}
					parameterIndex[name] = parameters.size();
					parameters.push_back(p);
				}
				expr = isVector ? ("p_" + name) : ("vec4(p_" + name + ")");
				break;
			}
			case T::Texture: {
				if (stage == Stage::Vertex) { error = "A Texture node can't feed Vertex Offset (textures are sampled per fragment)"; break; }
				std::string sampler = "uTex" + std::to_string((int)textureSamplers.size());
				textureSamplers.push_back({node.id, sampler});
				expr = "texture_2D(" + sampler + ", (" + in(0, UVExpr().c_str()) + ").xy)";
				break;
			}
			case T::Add: expr = "(" + in(0,"vec4(0.0)") + " + " + in(1,"vec4(0.0)") + ")"; break;
			case T::Subtract: expr = "(" + in(0,"vec4(0.0)") + " - " + in(1,"vec4(0.0)") + ")"; break;
			case T::Multiply: expr = "(" + in(0,"vec4(1.0)") + " * " + in(1,"vec4(1.0)") + ")"; break;
			case T::Divide: expr = "(" + in(0,"vec4(0.0)") + " / (" + in(1,"vec4(1.0)") + " + vec4(0.0001)))"; break;
			case T::Power: expr = "pow(abs(" + in(0,"vec4(1.0)") + "), " + in(1,"vec4(1.0)") + ")"; break;
			case T::Modulo: expr = "mod(" + in(0,"vec4(0.0)") + ", max(" + in(1,"vec4(1.0)") + ", vec4(0.0001)))"; break;
			case T::Negate: expr = "(-" + in(0,"vec4(0.0)") + ")"; break;
			case T::Abs: expr = "abs(" + in(0,"vec4(0.0)") + ")"; break;
			case T::Sqrt: expr = "sqrt(abs(" + in(0,"vec4(0.0)") + "))"; break;
			case T::Sin: expr = "sin(" + in(0,"vec4(0.0)") + ")"; break;
			case T::Cos: expr = "cos(" + in(0,"vec4(0.0)") + ")"; break;
			case T::Tan: expr = "tan(" + in(0,"vec4(0.0)") + ")"; break;
			case T::Min: expr = "min(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + ")"; break;
			case T::Max: expr = "max(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + ")"; break;
			case T::Clamp: expr = "clamp(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + ", " + in(2,"vec4(1.0)") + ")"; break;
			// Per channel: a scalar T arrives broadcast, so this is the old
			// `.x` behaviour for every graph that fed it one.
			case T::Lerp: expr = "mix(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(1.0)") + ", " + in(2,"vec4(0.5)") + ")"; break;
			case T::DotProduct: expr = "vec4(dot(" + in(0,"vec4(0.0)") + ".xyz, " + in(1,"vec4(0.0)") + ".xyz))"; break;
			case T::CrossProduct: expr = "vec4(cross(" + in(0,"vec4(0.0)") + ".xyz, " + in(1,"vec4(0.0)") + ".xyz), 0.0)"; break;
			case T::Length: expr = "vec4(length(" + in(0,"vec4(0.0)") + ".xyz))"; break;
			case T::Normalize: expr = "vec4(normalize(" + in(0,"vec4(0.0,0.0,1.0,0.0)") + ".xyz), 0.0)"; break;
			case T::Distance: expr = "vec4(distance(" + in(0,"vec4(0.0)") + ".xyz, " + in(1,"vec4(0.0)") + ".xyz))"; break;
			case T::Equal: expr = "vec4(equal(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + "))"; break;
			case T::NotEqual: expr = "vec4(notEqual(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + "))"; break;
			case T::GreaterThan: expr = "vec4(greaterThan(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + "))"; break;
			case T::LessThan: expr = "vec4(lessThan(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + "))"; break;
			case T::And: expr = "(((" + in(0,"vec4(0.0)") + ".x > 0.5) && (" + in(1,"vec4(0.0)") + ".x > 0.5)) ? vec4(1.0) : vec4(0.0))"; break;
			case T::Or: expr = "(((" + in(0,"vec4(0.0)") + ".x > 0.5) || (" + in(1,"vec4(0.0)") + ".x > 0.5)) ? vec4(1.0) : vec4(0.0))"; break;
			case T::Not: expr = "((" + in(0,"vec4(0.0)") + ".x > 0.5) ? vec4(0.0) : vec4(1.0))"; break;
			case T::Step: expr = "step(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(0.0)") + ")"; break;
			case T::SmoothStep: expr = "smoothstep(" + in(0,"vec4(0.0)") + ", " + in(1,"vec4(1.0)") + ", " + in(2,"vec4(0.5)") + ")"; break;
			case T::SplitVec2: case T::SplitVec3: case T::SplitVec4:
				expr = in(0, "vec4(0.0)"); // pass-through; consumer applies the X/Y/Z/W swizzle
				break;
			case T::CombineVec2: expr = "vec4(" + in(0,"vec4(0.0)") + ".x, " + in(1,"vec4(0.0)") + ".x, 0.0, 0.0)"; break;
			case T::CombineVec3: expr = "vec4(" + in(0,"vec4(0.0)") + ".x, " + in(1,"vec4(0.0)") + ".x, " + in(2,"vec4(0.0)") + ".x, 0.0)"; break;
			case T::CombineVec4: expr = "vec4(" + in(0,"vec4(0.0)") + ".x, " + in(1,"vec4(0.0)") + ".x, " + in(2,"vec4(0.0)") + ".x, " + in(3,"vec4(1.0)") + ".x)"; break;
			case T::OneMinus: expr = "(vec4(1.0) - " + in(0,"vec4(0.0)") + ")"; break;
			case T::Saturate: expr = "clamp(" + in(0,"vec4(0.0)") + ", 0.0, 1.0)"; break;
			case T::Fract: expr = "fract(" + in(0,"vec4(0.0)") + ")"; break;
			case T::Floor: expr = "floor(" + in(0,"vec4(0.0)") + ")"; break;
			case T::Remap:
				expr = "(" + in(3,"vec4(0.0)") + " + (" + in(0,"vec4(0.0)") + " - " + in(1,"vec4(0.0)") + ") / p3d_nz("
					+ in(2,"vec4(1.0)") + " - " + in(1,"vec4(0.0)") + ") * (" + in(4,"vec4(1.0)") + " - " + in(3,"vec4(0.0)") + "))";
				break;
			case T::UVCoordinate: expr = UVExpr(); break;
			// World space, like every other position/direction here and like
			// the Normal pin that consumes it. Was the VIEW-space vNormal,
			// which fed straight into Normal lit the surface from a direction
			// that turned with the camera.
			case T::NormalVector: expr = NormalExpr(); break;
			case T::ObjectPosition: expr = PositionExpr(); break;
			case T::ObjectOrigin: usesModelMatrix = true; expr = "vec4(uModelMatrix[3].xyz, 1.0)"; break;
			case T::CameraPosition: usesCameraPosition = true; expr = "vec4(uCameraPosition, 1.0)"; break;
			case T::ViewDirection: usesCameraPosition = true; expr = ViewDirExpr(); break;
			case T::TimeValue: usesTime = true; expr = "vec4(uTime)"; break;
			case T::Fresnel: {
				usesCameraPosition = true;
				const std::string n = in(1, NormalExpr().c_str());
				expr = "vec4(pow(1.0 - clamp(dot(normalize(" + n + ".xyz), " + ViewDirExpr() + ".xyz), 0.0, 1.0), max(" + in(0,"vec4(5.0)") + ".x, 0.0001)))";
				break;
			}
			case T::Noise:
				expr = "vec4(p3d_noise(" + in(0, ("vec4(" + UVExpr() + ".xy, 0.0, 0.0)").c_str()) + ".xyz * " + in(1,"vec4(1.0)") + ".x))";
				break;
			case T::NormalMap: {
				if (stage == Stage::Vertex) { error = "A Normal Map node can't feed Vertex Offset"; break; }
				// A tangent-space sample -> world-space normal, with the tangent
				// frame rebuilt from screen-space derivatives: no tangent
				// attributes needed, so it works on every mesh (and skinned
				// ones) - see p3d_PerturbNormal in the template.
				expr = "vec4(p3d_PerturbNormal(p3d_N, vWorldPos.xyz, vTexcoord, " + in(0,"vec4(0.5, 0.5, 1.0, 1.0)") + ".xyz * 2.0 - 1.0, "
					+ in(1,"vec4(1.0)") + ".x), 0.0)";
				break;
			}
			case T::CustomExpression: {
				// The user's expression sees four vec4 inputs a/b/c/d and may
				// return a float or any vecN - p3d_v4 widens it. Emitted as a
				// block so a/b/c/d don't leak into main()'s scope.
				const std::string body = node.userData.empty() ? std::string("a") : node.userData;
				statements.push_back("vec4 " + var + "; { vec4 a = " + in(0,"vec4(0.0)") + "; vec4 b = " + in(1,"vec4(0.0)")
					+ "; vec4 c = " + in(2,"vec4(0.0)") + "; vec4 d = " + in(3,"vec4(0.0)") + "; " + var + " = p3d_v4(" + body + "); }");
				visiting.erase(node.id);
				emitted[node.id] = var;
				return error.empty() ? var : "";
			}
			case T::Output: expr = "vec4(0.0)"; break; // never actually emitted as a statement (it's the traversal root)
			default: expr = "vec4(0.5, 0.5, 0.5, 1.0)"; break;
		}

		if (!error.empty()) { visiting.erase(node.id); return ""; }
		statements.push_back("vec4 " + var + " = " + expr + ";");
		visiting.erase(node.id);
		emitted[node.id] = var;
		return var;
	}

	const std::vector<MaterialNode>& nodes;
	const std::vector<MaterialConnection>& connections;
	Stage stage;
	std::map<uint32_t, std::string> emitted;
	std::set<uint32_t> visiting;
	std::vector<std::string> statements;
	std::vector<std::pair<uint32_t, std::string>> textureSamplers;
	std::vector<MaterialCodegenResult::Parameter> parameters;
	std::map<std::string, size_t> parameterIndex;
	bool usesCameraPosition = false;
	bool usesTime = false;
	bool usesModelMatrix = false;
	std::string error;
};

// Generated shaders always contain BOTH branches, selected at compile time
// by whichever renderer the project actually uses (#define DEFERRED_GBUFFER
// passed in by MaterialEditor::ApplyGraphOrTextToLiveMaterial, mirroring how
// PyrosShader.glsl itself is a single dual-mode source file) - so the same
// .mat keeps working if the project's renderer setting changes, with no
// regeneration needed.
//
// The Forward branch is a real per-fragment PBR light loop (directional/
// point/spot, no shadow-map sampling yet), deliberately ported close to
// verbatim from PyrosShader.glsl's own non-deferred branch (LIGHT struct,
// buildLightFromMatrix, Attenuation, DualConeSpotLight, the Cook-Torrance
// CalculatePBRLighting/DistributionGGX/GeometrySchlickGGX/GeometrySmith/
// FresnelSchlick functions) rather than re-derived, so it matches how the
// engine's built-in materials actually light a surface.
// Everything the template needs from either front end (graph or Text mode).
// Every *Expr is a vec4-valued GLSL expression over the fragment stage's
// statements; vertexOffsetExpr over vertexStatements.
struct TemplateInputs {
	std::string albedo = "vec4(1.0)";
	bool normalConnected = false;
	std::string normal;               // world space, .xyz; only read when normalConnected
	std::string metallic = "vec4(0.0)";
	std::string roughness = "vec4(0.5)";
	std::string emissive = "vec4(0.0)";
	std::string occlusion = "vec4(1.0)";
	std::string opacity = "vec4(1.0)";
	bool alphaClipConnected = false;  // no discard at all otherwise - keeps early-Z
	std::string alphaClip = "vec4(0.0)";
	std::string reflection = "vec4(0.0)";
	std::string vertexOffset;         // empty = no offset
	std::vector<std::string> statements;
	std::vector<std::string> vertexStatements;
	std::vector<std::string> samplerNames;
	std::vector<MaterialCodegenResult::Parameter> parameters;
};

// Declarations shared by both stages: parameter uniforms and the small
// helper functions graph nodes expand to. Stage-neutral GLSL only - no
// derivatives, no samplers.
void EmitSharedPrelude(std::ostringstream& out, const TemplateInputs& in) {
	out << "uniform float uTime;\n";
	// The application's sixteen (IRenderer::SetShaderGlobal), for a material
	// whose text reads them - not declared otherwise, so no other material
	// carries them about.
	{
		bool reads = false;
		for (const auto& st : in.statements) if (st.find("uGlobals") != std::string::npos) reads = true;
		for (const auto& st : in.vertexStatements) if (st.find("uGlobals") != std::string::npos) reads = true;
		if (reads) out << "uniform vec4 uGlobals[16];\n";
	}
	for (const auto& p : in.parameters)
		out << "uniform " << (p.isVector ? "vec4" : "float") << " p_" << p.name << ";\n";
	out <<
		// Widens a Custom Expression's result, whatever its type, to the
		// vec4 every node value is.
		"vec4 p3d_v4(float x) { return vec4(x); }\n"
		"vec4 p3d_v4(vec2 v) { return vec4(v, 0.0, 0.0); }\n"
		"vec4 p3d_v4(vec3 v) { return vec4(v, 0.0); }\n"
		"vec4 p3d_v4(vec4 v) { return v; }\n"
		// Remap's divisor, kept away from zero without flipping its sign.
		"vec4 p3d_nz(vec4 v) { return mix(v, vec4(1e-5), step(abs(v), vec4(1e-5))); }\n"
		// Value noise over a hashed lattice, trilinear with a smoothstep
		// fade: cheap, stage-neutral and the same on every backend (no
		// texture, no bit ops, which GLES3 would allow but WebGL drivers
		// have been unreliable with). Returns [0,1].
		"float p3d_hash(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }\n"
		"float p3d_noise(vec3 x) {\n"
		"\tvec3 i = floor(x);\n"
		"\tvec3 f = fract(x);\n"
		"\tf = f * f * (3.0 - 2.0 * f);\n"
		"\treturn mix(mix(mix(p3d_hash(i + vec3(0.0, 0.0, 0.0)), p3d_hash(i + vec3(1.0, 0.0, 0.0)), f.x),\n"
		"\t               mix(p3d_hash(i + vec3(0.0, 1.0, 0.0)), p3d_hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),\n"
		"\t           mix(mix(p3d_hash(i + vec3(0.0, 0.0, 1.0)), p3d_hash(i + vec3(1.0, 0.0, 1.0)), f.x),\n"
		"\t               mix(p3d_hash(i + vec3(0.0, 1.0, 1.0)), p3d_hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);\n"
		"}\n";
}

std::string BuildTemplate(const TemplateInputs& in) {
	std::ostringstream out;
	out <<
		"#define varying_in in\n"
		"#define varying_out out\n"
		"#define attribute_in in\n"
		"#define texture_2D texture\n"
		"#define texture_cube texture\n"
		"#define MAX_LIGHTS 4\n"
		// Matches PyrosShader.glsl / IRenderer's PYROS_MAX_BONES.
		"#define MAX_BONES 60\n"
		// highp, never mediump: this line lands after the device's own
		// `precision highp float` preamble and silently wins. On Apple GPUs
		// mediump is fp16, world-space positions through the MVP overflow
		// it, and the whole mesh is dropped before rasterization - the
		// web-only "invisible geometry" bug every shader in
		// resources/shaders was fixed for.
		"#if defined(GLES3)\n"
		"\tprecision highp float;\n"
		"#endif\n"
		"\n"
		"// Generated by the Pyros3D Material Editor's node graph. Hand edits here\n"
		"// are preserved until the graph is re-Applied (which overwrites this file).\n"
		"// Contains both a Forward (real per-fragment lighting) and a Deferred\n"
		"// G-buffer branch - DEFERRED_GBUFFER picks which one compiles, set by\n"
		"// the project's Renderer setting at Apply time. SKINNING is defined for\n"
		"// skinned meshes by CustomShaderMaterial's variant cache.\n"
		"\n"
		;
	// Read by CustomShaderMaterial::HasCustomShadow(): only a graph that
	// moves vertices or cuts holes needs to cast its own shadow - every
	// other material keeps the renderer's shared (cheaper) shadow shader.
	if (!in.vertexOffset.empty() || in.alphaClipConnected)
		out << "// P3D_CUSTOM_SHADOW\n\n";
	out <<
		"#ifdef VERTEX\n"
		"attribute_in vec3 aPosition;\n"
		"attribute_in vec3 aNormal;\n"
		"attribute_in vec2 aTexcoord;\n"
		// A plain uniform array, not PyrosShader.glsl's BoneMatrices block:
		// that block is only uploaded for materials that SupportsUniformBlocks(),
		// which a CustomShaderMaterial does not. Fed by DataUsage::Skinning.
		"#ifdef SKINNING\n"
		"attribute_in vec4 aBonesID;\n"
		"attribute_in vec4 aBonesWeight;\n"
		"uniform mat4 uBoneMatrix[MAX_BONES];\n"
		"#endif\n"
		"uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;\n"
		"uniform vec3 uCameraPosition;\n";
	EmitSharedPrelude(out, in);
	out <<
		"varying_out vec2 vTexcoord;\n"
		"varying_out vec3 vNormal;\n"      // view-space - deferred G-buffer convention
		"varying_out vec3 vNormalWorld;\n" // world-space - forward lighting convention
		"varying_out vec4 vWorldPos;\n"
		// View-space position, which is what the shadow matrices
		// (uDirectionalDepthsMVP) are built to consume - see
		// lighting.glsl's header note and PyrosShader.glsl's identical
		// vWorldPositionShadow, whose name this keeps despite holding
		// view space, so the two stay comparable.
		"varying_out vec4 vWorldPositionShadow;\n"
		"void main() {\n"
		"\tvec4 p3d_localPos = vec4(aPosition, 1.0);\n"
		"\tvec3 p3d_localNormal = aNormal;\n"
		"#ifdef SKINNING\n"
		"\tmat4 p3d_skin = uBoneMatrix[int(aBonesID.x)] * aBonesWeight.x\n"
		"\t              + uBoneMatrix[int(aBonesID.y)] * aBonesWeight.y\n"
		"\t              + uBoneMatrix[int(aBonesID.z)] * aBonesWeight.z\n"
		"\t              + uBoneMatrix[int(aBonesID.w)] * aBonesWeight.w;\n"
		"\tp3d_localPos = p3d_skin * p3d_localPos;\n"
		"\tp3d_localNormal = (p3d_skin * vec4(aNormal, 0.0)).xyz;\n"
		"#endif\n"
		"\tvec4 p3d_worldPos = uModelMatrix * p3d_localPos;\n"
		"\tvec3 p3d_worldNormal = normalize((uModelMatrix * vec4(p3d_localNormal, 0.0)).xyz);\n";
	for (const auto& st : in.vertexStatements)
		out << "\t" << st << "\n";
	// World-space offset (Vertex Offset pin), before anything below derives
	// from the position. The shadow pass draws with the engine's own depth
	// material, so a displaced surface casts its undisplaced shadow.
	if (!in.vertexOffset.empty())
		out << "\tp3d_worldPos.xyz += (" << in.vertexOffset << ").xyz;\n";
	out <<
		"\tvTexcoord = aTexcoord;\n"
		"\tvNormalWorld = p3d_worldNormal;\n"
		"\tvNormal = (uViewMatrix * vec4(p3d_worldNormal, 0.0)).xyz;\n"
		"\tvWorldPos = p3d_worldPos;\n"
		"\tvWorldPositionShadow = uViewMatrix * p3d_worldPos;\n"
		"\tgl_Position = uProjectionMatrix * uViewMatrix * p3d_worldPos;\n"
		"}\n"
		"#endif\n"
		"\n"
		"#ifdef FRAGMENT\n"
		"uniform vec4 uAmbientLight;\n"
		// Environment ambient - see p3d_Ambient below.
		"uniform vec4 uAmbientSky;\n"
		"uniform vec4 uAmbientEquator;\n"
		"uniform vec4 uAmbientGround;\n"
		"uniform vec4 uAmbientParams;\n"
		"uniform vec4 uAmbientSH[9];\n"
		"uniform mat4 uViewMatrix;\n"
		"uniform mat4 uModelMatrix;\n"
		"uniform vec3 uCameraPosition;\n"
		// IMaterial's own opacity, multiplied into the Opacity pin.
		"uniform float uOpacity;\n";
	for (const auto& name : in.samplerNames)
		out << "uniform sampler2D " << name << ";\n";
	EmitSharedPrelude(out, in);
	out <<
		// The lit forward branch - neither the G-buffer write nor the shadow
		// pass's depth-only one (SHADOW_DEPTH, see CustomShaderMaterial's
		// shadow variant), which must never declare the shadow-map samplers:
		// it runs while those very maps are the render target.
		"#if !defined(DEFERRED_GBUFFER) && !defined(SHADOW_DEPTH)\n"
		"#define P3D_FORWARD_LIT\n"
		"#endif\n"
		"#ifdef P3D_FORWARD_LIT\n"
		"uniform mat4 uLights[MAX_LIGHTS];\n"
		"uniform int uNumberOfLights;\n"
		// Directional shadow receiving. Forward only: under Deferred the
		// light passes do their own PCF against the G-buffer
		// (lighting.glsl / secondpass*.glsl), so a G-buffer-writing
		// material gets shadows without knowing anything about them.
		// Declared as plain uniforms rather than PyrosShader.glsl's
		// UBO_BINDING/SAMPLER_BINDING blocks to match how uLights above
		// is already declared here - IRenderer feeds all four through the
		// per-material Uniforms::DataUsage path.
		"uniform sampler2DShadow uDirectionalShadowMaps;\n"
		"uniform mat4 uDirectionalDepthsMVP[4];\n"
		"uniform vec4 uDirectionalShadowFar[4];\n"
		"uniform int uNumberOfDirectionalShadows;\n"
		"uniform samplerCube uPointShadowMaps[4];\n"
		"uniform mat4 uPointDepthsMVP[8];\n"
		"uniform int uNumberOfPointShadows;\n"
		"uniform sampler2DShadow uSpotShadowMaps[4];\n"
		"uniform mat4 uSpotDepthsMVP[4];\n"
		"uniform int uNumberOfSpotShadows;\n"
		"varying_in vec4 vWorldPositionShadow;\n"
		"#endif\n"
		"varying_in vec2 vTexcoord;\n"
		"varying_in vec3 vNormal;\n"
		"varying_in vec3 vNormalWorld;\n"
		"varying_in vec4 vWorldPos;\n"
		"\n"
		"#if defined(DEFERRED_GBUFFER)\n"
		"layout(location = 0) out vec4 FragData_r;\n"
		"layout(location = 1) out vec4 FragData_g;\n"
		"layout(location = 2) out vec4 FragData_b;\n"
		"layout(location = 3) out vec4 FragData_pbr;\n"
		"#elif defined(SHADOW_DEPTH)\n"
		"out vec4 FragColor;\n"
		"#else\n"
		"out vec4 FragColor;\n"
		"\n"
		"struct LIGHT { vec4 Color; vec3 Direction; vec3 Position; float Radius; vec2 Cones; float Type; bool HaveShadowMap; float PCFTexelSize; int ShadowMap; };\n"
		"void buildLightFromMatrix(mat4 Light, inout LIGHT L) {\n"
		"\tL.Color = Light[0];\n"
		"\tL.Position = vec3(Light[1][0], Light[1][1], Light[1][2]);\n"
		"\tL.Direction = vec3(Light[1][3], Light[2][0], Light[2][1]);\n"
		"\tL.Radius = Light[2][2];\n"
		"\tL.Cones = vec2(Light[2][3], Light[3][0]);\n"
		"\tL.Type = Light[3][1];\n"
		// Same packing IRenderer writes and lighting.glsl reads: a
		// negative shadow-map slot means this light has none.
		"\tL.HaveShadowMap = (Light[3][3] >= 0.0);\n"
		"\tL.PCFTexelSize = Light[3][2];\n"
		// Which shadow map this light owns - point and spot only;
		// directional has a single map and ignores it.
		"\tL.ShadowMap = int(Light[3][3]);\n"
		"}\n"
		"#ifndef DEFERRED_GBUFFER\n"
		// Shadow filtering - the same functions as PyrosShader.glsl's (see there for
		// the reasoning) and secondpass*.glsl's. L.PCFTexelSize carries
		// ILightComponent::GetShadowFilterPacked(): filter radius in texels in the
		// integer part, normal bias / 8 in the fraction.
		"int ShadowFilterRadius(float packed) { return int(clamp(floor(packed), 0.0, 3.0)); }\n"
		"float ShadowNormalBiasTexels(float packed) { return fract(packed) * 8.0; }\n"
		"float ShadowTexelWorld(mat4 M, vec4 pos, float texels) {\n"
		"\tvec3 rowX = vec3(M[0][0], M[1][0], M[2][0]);\n"
		"\tvec3 rowW = vec3(M[0][3], M[1][3], M[2][3]);\n"
		"\tfloat w = dot(vec4(M[0][3], M[1][3], M[2][3], M[3][3]), pos);\n"
		"\treturn abs(w) / (texels * max(length(rowX - 0.5 * rowW), 1e-8));\n"
		"}\n"
		// The interpolated view-space normal, turned toward the camera.
		"vec3 ShadowReceiverNormal() {\n"
		"\tvec3 n = normalize(vNormal);\n"
		"\treturn dot(n, vWorldPositionShadow.xyz) > 0.0 ? -n : n;\n"
		"}\n"
		"float ShadowPCF2D(sampler2DShadow map, vec3 uvz, int K, vec2 size, vec4 rect) {\n"
		"\tvec2 st = uvz.xy * size - 0.5;\n"
		"\tvec2 base = floor(st);\n"
		"\tvec2 f = st - base;\n"
		"\tfloat sum = 0.0;\n"
		"\tfor (int j = -K; j <= K + 1; j++) {\n"
		"\t\tfloat wy = (j == -K) ? 1.0 - f.y : ((j == K + 1) ? f.y : 1.0);\n"
		"\t\tfor (int i = -K; i <= K + 1; i++) {\n"
		"\t\t\tfloat wx = (i == -K) ? 1.0 - f.x : ((i == K + 1) ? f.x : 1.0);\n"
		"\t\t\tvec2 uv = clamp((base + vec2(float(i), float(j)) + 0.5) / size, rect.xy, rect.zw);\n"
		"\t\t\tsum += wx * wy * texture(map, vec3(uv, uvz.z));\n"
		"\t\t}\n"
		"\t}\n"
		"\tfloat n = float(2 * K + 1);\n"
		"\treturn sum / (n * n);\n"
		"}\n"
		"float ShadowCascadeFar(vec4 splits, int c) {\n"
		"\tif (c == 0) return splits.x;\n"
		"\tif (c == 1) return splits.y;\n"
		"\tif (c == 2) return splits.z;\n"
		"\treturn splits.w;\n"
		"}\n"
		"float ShadowDirectionalCascade(int c, bool multi, float packed, vec4 pos, vec3 n) {\n"
		"\tmat4 M = uDirectionalDepthsMVP[c];\n"
		"\tvec2 size = vec2(textureSize(uDirectionalShadowMaps, 0));\n"
		"\tfloat tile = multi ? size.x * 0.5 : size.x;\n"
		"\tvec4 coord = M * vec4(pos.xyz + n * (ShadowNormalBiasTexels(packed) * ShadowTexelWorld(M, pos, tile)), 1.0);\n"
		"\tvec4 rect = vec4(0.0, 0.0, 1.0, 1.0);\n"
		"\tif (multi) {\n"
		"\t\tvec2 off = vec2((c == 1 || c == 3) ? 0.5 : 0.0, (c >= 2) ? 0.5 : 0.0);\n"
		"\t\tcoord.xy = coord.xy * 0.5 + off;\n"
		"\t\trect = vec4(off + 0.5 / size, off + 0.5 - 0.5 / size);\n"
		"\t}\n"
		"\treturn ShadowPCF2D(uDirectionalShadowMaps, coord.xyz, ShadowFilterRadius(packed), size, rect);\n"
		"}\n"
		// uDirectionalShadowFar[0] holds the cascades' linear view-space far
		// distances; cross-faded over the last 10% of each.
		"float DirectionalShadowFactor(LIGHT L) {\n"
		"\tif (!L.HaveShadowMap || uNumberOfDirectionalShadows <= 0) return 1.0;\n"
		"\tvec4 splits = uDirectionalShadowFar[0];\n"
		"\tbool multi = splits.y > 0.0;\n"
		"\tint count = !multi ? 1 : (splits.w > 0.0 ? 4 : (splits.z > 0.0 ? 3 : 2));\n"
		"\tvec4 pos = vWorldPositionShadow;\n"
		"\tvec3 n = ShadowReceiverNormal();\n"
		"\tfloat depth = -pos.z;\n"
		"\tint c = count;\n"
		"\tfor (int i = 3; i >= 0; i--)\n"
		"\t\tif (i < count && depth < ShadowCascadeFar(splits, i)) c = i;\n"
		"\tif (c >= count) return 1.0;\n"
		"\tfloat cFar = ShadowCascadeFar(splits, c);\n"
		"\tfloat t = clamp((depth - cFar * 0.9) / (cFar * 0.1), 0.0, 1.0);\n"
		"\tfloat s = ShadowDirectionalCascade(c, multi, L.PCFTexelSize, pos, n);\n"
		"\tif (t > 0.0) {\n"
		"\t\tfloat next = (c + 1 < count) ? ShadowDirectionalCascade(c + 1, multi, L.PCFTexelSize, pos, n) : 1.0;\n"
		"\t\ts = mix(s, next, t);\n"
		"\t}\n"
		"\treturn s;\n"
		"}\n"
		// Point and spot both sample element [0] of their sampler array and index
		// only the *matrix* by the light's slot. That mirrors PyrosShader.glsl
		// exactly: indexing a sampler array by a value that varies with the light
		// loop is not a constant expression, which core GLSL 3.30 does not allow.
		"float PCFPoint(mat4 m1, mat4 m2, float packed, vec4 pos, vec3 n) {\n"
		"\tvec3 ls = (m2 * pos).xyz;\n"
		"\tfloat size = float(textureSize(uPointShadowMaps[0], 0).x);\n"
		"\tvec3 a = abs(ls);\n"
		"\tfloat ma = max(a.x, max(a.y, a.z));\n"
		"\tls += mat3(m2) * n * (ShadowNormalBiasTexels(packed) * 2.0 * ma / size);\n"
		"\ta = abs(ls);\n"
		"\tma = max(a.x, max(a.y, a.z));\n"
		"\t// m1 already carries the device's shadow-bias/projection remap, so no\n"
		"\t// extra *0.5+0.5 here. 0.02 is PointLight's default bias scale - this\n"
		"\t// path has no per-light value for it.\n"
		"\tvec4 clip = m1 * vec4(0.0, 0.0, -ma * 0.98, 1.0);\n"
		"\tfloat depth = clip.z / clip.w;\n"
		"\tvec3 axis, t, b;\n"
		"\tif (a.x >= a.y && a.x >= a.z) { axis = vec3(sign(ls.x), 0.0, 0.0); t = vec3(0.0, 0.0, 1.0); b = vec3(0.0, 1.0, 0.0); }\n"
		"\telse if (a.y >= a.z)          { axis = vec3(0.0, sign(ls.y), 0.0); t = vec3(1.0, 0.0, 0.0); b = vec3(0.0, 0.0, 1.0); }\n"
		"\telse                          { axis = vec3(0.0, 0.0, sign(ls.z)); t = vec3(1.0, 0.0, 0.0); b = vec3(0.0, 1.0, 0.0); }\n"
		"\tvec2 st = (vec2(dot(ls, t), dot(ls, b)) / ma * 0.5 + 0.5) * size - 0.5;\n"
		"\tvec2 base = floor(st);\n"
		"\tvec2 f = st - base;\n"
		"\tint K = ShadowFilterRadius(packed);\n"
		"\tfloat sum = 0.0;\n"
		"\tfor (int j = -K; j <= K + 1; j++) {\n"
		"\t\tfloat wy = (j == -K) ? 1.0 - f.y : ((j == K + 1) ? f.y : 1.0);\n"
		"\t\tfor (int i = -K; i <= K + 1; i++) {\n"
		"\t\t\tfloat wx = (i == -K) ? 1.0 - f.x : ((i == K + 1) ? f.x : 1.0);\n"
		"\t\t\tvec2 uv = (base + vec2(float(i), float(j)) + 0.5) / size * 2.0 - 1.0;\n"
		"\t\t\tsum += wx * wy * ((texture(uPointShadowMaps[0], axis + t * uv.x + b * uv.y).r >= depth) ? 1.0 : 0.0);\n"
		"\t\t}\n"
		"\t}\n"
		"\tfloat nk = float(2 * K + 1);\n"
		"\treturn sum / (nk * nk);\n"
		"}\n"
		"float PCFSpot(mat4 sMatrix, float packed, vec4 pos, vec3 n) {\n"
		"\tvec2 size = vec2(textureSize(uSpotShadowMaps[0], 0));\n"
		"\tvec4 coord = sMatrix * vec4(pos.xyz + n * (ShadowNormalBiasTexels(packed) * ShadowTexelWorld(sMatrix, pos, size.x)), 1.0);\n"
		"\tcoord.xyz /= coord.w;\n"
		"\treturn ShadowPCF2D(uSpotShadowMaps[0], coord.xyz, ShadowFilterRadius(packed), size, vec4(0.0, 0.0, 1.0, 1.0));\n"
		"}\n"
		// Slot clamped before use: nothing validates the packed index, and an
		// out-of-range read of uPointDepthsMVP/uSpotDepthsMVP is undefined rather
		// than merely wrong.
		"float PointShadowFactor(LIGHT L) {\n"
		"\tif (!L.HaveShadowMap || uNumberOfPointShadows <= 0) return 1.0;\n"
		"\tint slot = clamp(L.ShadowMap, 0, 3);\n"
		"\treturn PCFPoint(uPointDepthsMVP[slot * 2], uPointDepthsMVP[slot * 2 + 1], L.PCFTexelSize, vWorldPositionShadow, ShadowReceiverNormal());\n"
		"}\n"
		"float SpotShadowFactor(LIGHT L) {\n"
		"\tif (!L.HaveShadowMap || uNumberOfSpotShadows <= 0) return 1.0;\n"
		"\tint slot = clamp(L.ShadowMap, 0, 3);\n"
		"\treturn PCFSpot(uSpotDepthsMVP[slot], L.PCFTexelSize, vWorldPositionShadow, ShadowReceiverNormal());\n"
		"}\n"
		"#endif\n"
		"float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius) {\n"
		"\tif (Radius > 0.0) {\n"
		"\t\tfloat d = distance(Vertex, LightPosition);\n"
		"\t\treturn clamp(1.0 - (1.0 / Radius) * d, 0.0, 1.0);\n"
		"\t}\n"
		"\treturn 1.0;\n"
		"}\n"
		"float DualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone) {\n"
		"\tif (cosOutterCone > 0.0 || cosInnerCone > 0.0) {\n"
		"\t\tvec3 to_light = normalize(SpotLightPosition - Vertex);\n"
		"\t\tfloat angle = dot(-to_light, normalize(SpotLightDirection));\n"
		"\t\tfloat funcX = 1.0 / (cosInnerCone - cosOutterCone);\n"
		"\t\tfloat funcY = -funcX * cosOutterCone;\n"
		"\t\treturn clamp(angle * funcX + funcY, 0.0, 1.0);\n"
		"\t}\n"
		"\treturn 0.0;\n"
		"}\n"
		"const float PBR_PI = 3.14159265359;\n"
		"float DistributionGGX(vec3 N, vec3 H, float roughness) {\n"
		"\tfloat a = roughness * roughness;\n"
		"\tfloat a2 = a * a;\n"
		"\tfloat NdotH = max(dot(N, H), 0.0);\n"
		"\tfloat denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);\n"
		"\treturn a2 / max(PBR_PI * denom * denom, 1e-6);\n"
		"}\n"
		"float GeometrySchlickGGX(float NdotV, float roughness) {\n"
		"\tfloat r = (roughness + 1.0);\n"
		"\tfloat k = (r * r) / 8.0;\n"
		"\treturn NdotV / (NdotV * (1.0 - k) + k);\n"
		"}\n"
		"float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {\n"
		"\tfloat NdotV = max(dot(N, V), 0.0);\n"
		"\tfloat NdotL = max(dot(N, L), 0.0);\n"
		"\treturn GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);\n"
		"}\n"
		"vec3 FresnelSchlick(float cosTheta, vec3 F0) {\n"
		"\treturn F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);\n"
		"}\n"
		"vec3 CalculatePBRLighting(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness) {\n"
		"\tvec3 H = normalize(V + L);\n"
		"\tvec3 F0 = mix(vec3(0.04), albedo, metallic);\n"
		"\tfloat NDF = DistributionGGX(N, H, roughness);\n"
		"\tfloat G = GeometrySmith(N, V, L, roughness);\n"
		"\tvec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);\n"
		"\tvec3 numerator = NDF * G * F;\n"
		"\tfloat denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;\n"
		"\tvec3 specularTerm = numerator / denom;\n"
		"\tvec3 kS = F;\n"
		"\tvec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);\n"
		// radiance * PI: the engine's light colour is irradiance/PI, not
		// irradiance - see resources/shaders/PyrosShader.glsl's identical
		// comment. Without it a graph material lit through this BRDF was
		// exactly PI (3.14x) darker than a Generic Diffuse material under
		// the same light, which has no 1/PI at all.
		"\tvec3 irradiance = radiance * PBR_PI;\n"
		"\tfloat NdotL = max(dot(N, L), 0.0);\n"
		"\treturn (kD * albedo / PBR_PI + specularTerm) * irradiance * NdotL;\n"
		"}\n"
		"#endif\n"
		"\n"
		// The ambient term, the same three modes as PyrosShader.glsl's
		// AmbientAt() for a material without GLOBALILLUMINATION: 0 flat,
		// 1 sky/equator/ground gradient, 2+ order-2 SH (a DDGI scene, mode
		// 3, lands on SH exactly as a Generic material without the GI flag
		// does). Before this every custom material used the flat colour in
		// every mode. Returns irradiance/PI, like AmbientAt() - see there.
		//
		// SHIrradiance is a THIRD copy of that formula (C++
		// SphericalHarmonicsL2::Irradiance and PyrosShader.glsl being the
		// other two). Index order is (l,m) -> l*(l+1)+m in all three.
		"vec3 p3d_SHIrradiance(vec3 n) {\n"
		"\tconst float c1 = 0.429043, c2 = 0.511664, c3 = 0.743125, c4 = 0.886227, c5 = 0.247708;\n"
		"\treturn uAmbientSH[8].rgb * (c1 * (n.x * n.x - n.y * n.y))\n"
		"\t     + uAmbientSH[6].rgb * (c3 * n.z * n.z)\n"
		"\t     + uAmbientSH[0].rgb * c4\n"
		"\t     - uAmbientSH[6].rgb * c5\n"
		"\t     + uAmbientSH[4].rgb * (2.0 * c1 * n.x * n.y)\n"
		"\t     + uAmbientSH[7].rgb * (2.0 * c1 * n.x * n.z)\n"
		"\t     + uAmbientSH[5].rgb * (2.0 * c1 * n.y * n.z)\n"
		"\t     + uAmbientSH[3].rgb * (2.0 * c2 * n.x)\n"
		"\t     + uAmbientSH[1].rgb * (2.0 * c2 * n.y)\n"
		"\t     + uAmbientSH[2].rgb * (2.0 * c2 * n.z);\n"
		"}\n"
		"vec3 p3d_Ambient(vec3 n) {\n"
		"\tif (uAmbientParams.x >= 1.5)\n"
		"\t\treturn max(p3d_SHIrradiance(n) * (1.0 / 3.14159265359), vec3(0.0));\n"
		"\tif (uAmbientParams.x >= 0.5) {\n"
		"\t\tfloat y = clamp(n.y, -1.0, 1.0);\n"
		"\t\treturn (y >= 0.0) ? mix(uAmbientEquator.rgb, uAmbientSky.rgb, y)\n"
		"\t\t                  : mix(uAmbientEquator.rgb, uAmbientGround.rgb, -y);\n"
		"\t}\n"
		"\treturn uAmbientLight.rgb;\n"
		"}\n"
		// Normal Map node: tangent-space `m` -> world space, around the
		// geometric normal N, with the tangent frame solved from screen-space
		// derivatives of position and UV (Schueler's cotangent frame). The
		// sign(det) term is what makes it independent of the backend's
		// screen-Y direction: without it the frame mirrors on Vulkan/Metal,
		// whose framebuffer Y runs the other way from GL's, and every bump
		// lights inside out.
		"vec3 p3d_PerturbNormal(vec3 N, vec3 p, vec2 uv, vec3 m, float strength) {\n"
		"\tvec3 dp1 = dFdx(p);\n"
		"\tvec3 dp2 = dFdy(p);\n"
		"\tvec2 duv1 = dFdx(uv);\n"
		"\tvec2 duv2 = dFdy(uv);\n"
		"\tvec3 dp2perp = cross(dp2, N);\n"
		"\tvec3 dp1perp = cross(N, dp1);\n"
		"\tvec3 T = dp2perp * duv1.x + dp1perp * duv2.x;\n"
		"\tvec3 B = dp2perp * duv1.y + dp1perp * duv2.y;\n"
		"\tfloat det = dot(dp1, dp2perp);\n"
		"\tfloat s = (det < 0.0) ? -1.0 : 1.0;\n"
		"\tfloat invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-20));\n"
		"\tm.xy *= strength;\n"
		"\treturn normalize(mat3(T * (invmax * s), B * (invmax * s), N) * m);\n"
		"}\n"
		"\n"
		"void main() {\n"
		// World normal and view direction, available to every node as
		// World Normal / View Direction / Fresnel's default.
		"\tvec3 p3d_N = normalize(vNormalWorld);\n"
		"\tvec3 p3d_V = normalize(uCameraPosition - vWorldPos.xyz);\n";
	for (const auto& st : in.statements)
		out << "\t" << st << "\n";
	out <<
		"\tvec3 albedo = (" << in.albedo << ").xyz;\n"
		"\tfloat metallic = (" << in.metallic << ").x;\n"
		"\tfloat roughness = clamp((" << in.roughness << ").x, 0.03, 1.0);\n"
		"\tvec3 emissive = (" << in.emissive << ").xyz;\n"
		"\tfloat occlusion = (" << in.occlusion << ").x;\n"
		"\tfloat opacity = clamp((" << in.opacity << ").x, 0.0, 1.0) * uOpacity;\n"
		"\tfloat reflection = clamp((" << in.reflection << ").x, 0.0, 1.0);\n";
	// Cutout: in both branches, so a clipped fragment never reaches the
	// G-buffer either. Only when the pin is used - a shader containing
	// `discard` at all loses early depth testing on some GPUs.
	if (in.alphaClipConnected)
		out << "\tif (opacity < (" << in.alphaClip << ").x) discard;\n";
	if (in.normalConnected)
		out << "\tvec3 N = normalize((" << in.normal << ").xyz);\n";
	else
		out << "\tvec3 N = p3d_N;\n";
	out <<
		"\n"
		// Shadow pass: the offset position and the cutout above are all it
		// needs. Depth in R, like PyrosShader.glsl's CASTSHADOWS - a point
		// light's cube map is an R32F colour target that stores exactly
		// this; the directional/spot targets are depth-only and drop it.
		"#if defined(SHADOW_DEPTH)\n"
		"\tFragColor = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);\n"
		"#elif defined(DEFERRED_GBUFFER)\n"
		"\tvec3 normalOut = normalize((uViewMatrix * vec4(N, 0.0)).xyz);\n"
		// Emissive goes into the albedo channel, and the three alphas
		// carry that same value scaled by the ambient.
		//
		// A previous attempt moved emissive out of the albedo channel and
		// into the alphas alone, on the theory that anything in albedo
		// gets attenuated by N.L in the light passes. Measured, that
		// theory is wrong about where deferred actually reads emissive
		// from: a pure-emissive material (Albedo 0, Emissive 0.8) renders
		// at 204/255 in Forward and, with this write, 204/255 in
		// Deferred - but with the alpha-only packing it disappeared
		// completely, not one lit pixel. The alphas alone do not get
		// emissive to the screen. Keep it here.
		"\tvec3 color = albedo * occlusion + emissive;\n"
		"\tvec3 ambient = p3d_Ambient(N);\n"
		"\tFragData_r = vec4(color, color.x * ambient.x);\n"
		"\tFragData_g = vec4(1.0, 1.0, 1.0, color.y * ambient.y);\n"
		"\tFragData_b = vec4(normalOut, color.z * ambient.z);\n"
		// z/w: SSR opt-in and strength, the same pair PyrosShader.glsl writes
		// from uSSRReflective/uReflectivity - lastPass.glsl reflects only
		// where z is set.
		"\tFragData_pbr = vec4(roughness, metallic, reflection > 0.0 ? 1.0 : 0.0, reflection);\n"
		"#else\n"
		"\tvec3 Position = vWorldPos.xyz;\n"
		"\tvec3 V = p3d_V;\n"
		"\tvec3 pbrColor = vec3(0.0);\n"
		"\tfor (int i = 0; i < MAX_LIGHTS; i++) {\n"
		"\t\tif (i < uNumberOfLights) {\n"
		"\t\t\tLIGHT L;\n"
		"\t\t\tbuildLightFromMatrix(uLights[i], L);\n"
		"\t\t\tvec3 Ldir = vec3(0.0);\n"
		"\t\t\tfloat atten = 1.0;\n"
		"\t\t\tfloat spotEffect = 1.0;\n"
		// 1.0 = fully lit. Set per light type below; all three casters
		// are handled, each against its own shadow map.
		"\t\t\tfloat shadowFactor = 1.0;\n"
		"\t\t\tif (L.Type == 1.0) {\n"
		"\t\t\t\tLdir = normalize(-L.Direction);\n"
		"\t\t\t\tshadowFactor = DirectionalShadowFactor(L);\n"
		"\t\t\t} else if (L.Type == 2.0) {\n"
		"\t\t\t\tLdir = normalize(L.Position - Position);\n"
		"\t\t\t\tatten = Attenuation(Position, L.Position, L.Radius);\n"
		// Only worth a shadow lookup where the light actually reaches,
		// same gate PyrosShader.glsl uses.
		"\t\t\t\tif (atten > 0.0) shadowFactor = PointShadowFactor(L);\n"
		"\t\t\t} else if (L.Type == 3.0) {\n"
		"\t\t\t\tLdir = normalize(L.Position - Position);\n"
		"\t\t\t\tatten = Attenuation(Position, L.Position, L.Radius);\n"
		"\t\t\t\tspotEffect = 1.0 - DualConeSpotLight(Position, L.Position, L.Direction, L.Cones.x, L.Cones.y);\n"
		"\t\t\t\tif (atten > 0.0 && spotEffect > 0.0) shadowFactor = SpotShadowFactor(L);\n"
		"\t\t\t}\n"
		"\t\t\tpbrColor += CalculatePBRLighting(N, V, Ldir, L.Color.rgb, albedo, metallic, roughness) * atten * spotEffect * shadowFactor;\n"
		"\t\t}\n"
		"\t}\n"
		"\tvec3 ambientPBR = albedo * occlusion * (1.0 - metallic) * p3d_Ambient(N);\n"
		"\tvec3 color = pbrColor + ambientPBR + emissive;\n"
		"\tFragColor = vec4(color, opacity);\n"
		"#endif\n"
		"}\n"
		"#endif\n";
	return out.str();
}

} // namespace

const char* const kDefaultSimpleShaderText =
	"vec3 Albedo = vec3(1.0, 1.0, 1.0);\n"
	"float Metallic = 0.0;\n"
	"float Roughness = 0.5;\n"
	"vec3 Emissive = vec3(0.0, 0.0, 0.0);\n"
	"float Occlusion = 1.0;\n"
	"float Opacity = 1.0;\n"
	"// SSR strength, 0..1 (Deferred renderer only).\n"
	"float Reflection = 0.0;\n"
	"\n"
	"// Leave Normal at (0,0,0) to use the surface's own normal. World space.\n"
	"vec3 Normal = vec3(0.0, 0.0, 0.0);\n"
	"\n"
	"// Declare `float AlphaClip = 0.5;` to discard fragments whose Opacity is\n"
	"// below it. Available here: p3d_N (world normal), p3d_V (view direction),\n"
	"// vWorldPos, vTexcoord, uTime, uCameraPosition.\n";

namespace {
// Crude but sufficient word-boundary check for "<type> <name>" appearing
// anywhere in the user's snippet - used only to decide whether a default
// declaration would collide with one the user already wrote themselves.
bool DeclaresVar(const std::string& body, const char* type, const char* name) {
	std::regex re(std::string("\\b") + type + "\\s+" + name + "\\b");
	return std::regex_search(body, re);
}
// Comments stripped, so the seed text's own "Declare `float AlphaClip`"
// hint does not count as a declaration.
std::string StripComments(const std::string& body) {
	std::string out;
	for (size_t i = 0; i < body.size(); i++) {
		if (body.compare(i, 2, "//") == 0) { while (i < body.size() && body[i] != '\n') i++; if (i < body.size()) out += '\n'; continue; }
		if (body.compare(i, 2, "/*") == 0) { size_t e = body.find("*/", i + 2); i = (e == std::string::npos) ? body.size() : e + 1; out += ' '; continue; }
		out += body[i];
	}
	return out;
}
} // namespace

MaterialCodegenResult GenerateGLSLFromSimpleText(const std::string& userBody, const std::vector<std::string>& textureNames) {
	MaterialCodegenResult result;
	const std::string code = StripComments(userBody);

	std::vector<std::string> statements;
	// Only inject a default declaration for a name the user's own text
	// doesn't already declare. kDefaultSimpleShaderText - the seed text
	// every fresh/mode-switched Text document starts from - declares most
	// of them itself (with real types, not just assignments), so
	// unconditionally injecting a second declaration of the same name in the
	// same scope used to be a guaranteed GLSL "already defined" error on
	// first use of Text mode. A snippet that only touches e.g. Roughness
	// still compiles, since the rest fall back to their injected defaults.
	std::ostringstream defaults;
	if (!DeclaresVar(code, "vec3", "Albedo"))      defaults << "vec3 Albedo = vec3(1.0); ";
	if (!DeclaresVar(code, "vec3", "Normal"))      defaults << "vec3 Normal = vec3(0.0); ";
	if (!DeclaresVar(code, "float", "Metallic"))   defaults << "float Metallic = 0.0; ";
	if (!DeclaresVar(code, "float", "Roughness"))  defaults << "float Roughness = 0.5; ";
	if (!DeclaresVar(code, "vec3", "Emissive"))    defaults << "vec3 Emissive = vec3(0.0); ";
	if (!DeclaresVar(code, "float", "Occlusion"))  defaults << "float Occlusion = 1.0; ";
	if (!DeclaresVar(code, "float", "Opacity"))    defaults << "float Opacity = 1.0; ";
	if (!DeclaresVar(code, "float", "Reflection")) defaults << "float Reflection = 0.0; ";
	if (!defaults.str().empty()) statements.push_back(defaults.str());
	statements.push_back(userBody);

	TemplateInputs in;
	in.statements = statements;
	in.samplerNames = textureNames;
	in.albedo = "vec4(Albedo, 1.0)";
	// Normal is always "connected" here (unlike the node-graph path, there's
	// no separate isPinConnected signal) - a zero vector is the sentinel for
	// "not overridden", falling back to the geometric normal.
	in.normalConnected = true;
	in.normal = "vec4(dot(Normal, Normal) > 0.0001 ? Normal : p3d_N, 0.0)";
	in.metallic = "vec4(Metallic)";
	in.roughness = "vec4(Roughness)";
	in.emissive = "vec4(Emissive, 0.0)";
	in.occlusion = "vec4(Occlusion)";
	in.opacity = "vec4(Opacity)";
	in.reflection = "vec4(Reflection)";
	// Cutout only when the snippet asks for it - see TemplateInputs.
	if (DeclaresVar(code, "float", "AlphaClip")) {
		in.alphaClipConnected = true;
		in.alphaClip = "vec4(AlphaClip)";
	}
	result.glsl = BuildTemplate(in);
	result.usesCameraPosition = true;
	result.usesTime = true;
	return result;
}

MaterialCodegenResult GenerateGLSL(const std::vector<MaterialNode>& nodes, const std::vector<MaterialConnection>& connections) {
	MaterialCodegenResult result;

	const MaterialNode* outputNode = nullptr;
	int outputCount = 0;
	for (const auto& n : nodes) {
		if (n.type == MaterialNode::Output) { outputNode = &n; outputCount++; }
	}
	if (outputCount == 0) { result.error = "Graph has no Output node"; return result; }
	if (outputCount > 1) { result.error = "Graph has more than one Output node"; return result; }

	Codegen cg(nodes, connections, Stage::Fragment);
	Codegen vg(nodes, connections, Stage::Vertex);

	auto resolveOutputPin = [&](Codegen& gen, int pinIndex, const std::string& defaultExpr) -> std::string {
		return gen.ResolveInput(outputNode->id, pinIndex, defaultExpr);
	};
	auto isPinConnected = [&](int pinIndex) { return cg.IsInputConnected(outputNode->id, pinIndex); };

	TemplateInputs in;
	using O = MaterialNode;
	in.albedo    = resolveOutputPin(cg, O::OutAlbedo, "vec4(1.0)");
	in.normalConnected = isPinConnected(O::OutNormal);
	if (in.normalConnected) in.normal = resolveOutputPin(cg, O::OutNormal, "vec4(p3d_N, 0.0)");
	in.metallic  = resolveOutputPin(cg, O::OutMetallic, "vec4(0.0)");
	in.roughness = resolveOutputPin(cg, O::OutRoughness, "vec4(0.5)");
	in.emissive  = resolveOutputPin(cg, O::OutEmissive, "vec4(0.0)");
	in.occlusion = resolveOutputPin(cg, O::OutOcclusion, "vec4(1.0)");
	in.opacity   = resolveOutputPin(cg, O::OutOpacity, "vec4(1.0)");
	in.alphaClipConnected = isPinConnected(O::OutAlphaClip);
	if (in.alphaClipConnected) in.alphaClip = resolveOutputPin(cg, O::OutAlphaClip, "vec4(0.0)");
	in.reflection = resolveOutputPin(cg, O::OutReflection, "vec4(0.0)");
	if (!cg.error.empty()) { result.error = cg.error; return result; }

	if (isPinConnected(O::OutVertexOffset)) {
		in.vertexOffset = resolveOutputPin(vg, O::OutVertexOffset, "vec4(0.0)");
		if (!vg.error.empty()) { result.error = vg.error; return result; }
	}

	// One parameter list across both stages, the vertex stage's merged in
	// behind the fragment's with the same Float-vs-Color check.
	in.parameters = cg.parameters;
	for (const auto& p : vg.parameters) {
		bool found = false;
		for (const auto& q : in.parameters) {
			if (q.name != p.name) continue;
			found = true;
			if (q.isVector != p.isVector) {
				result.error = "Parameter '" + p.name + "' is used as both a Float and a Color parameter";
				return result;
			}
		}
		if (!found) in.parameters.push_back(p);
	}

	in.statements = cg.statements;
	in.vertexStatements = vg.statements;
	for (const auto& ts : cg.textureSamplers) in.samplerNames.push_back(ts.second);

	result.glsl = BuildTemplate(in);
	result.textureSamplers = cg.textureSamplers;
	result.parameters = in.parameters;
	result.usesCameraPosition = cg.usesCameraPosition || vg.usesCameraPosition;
	result.usesTime = cg.usesTime || vg.usesTime;
	return result;
}
