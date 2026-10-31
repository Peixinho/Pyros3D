//============================================================================
// Name        : MaterialCodegen.cpp
// Description : Node graph -> GLSL codegen implementation. See MaterialCodegen.h.
//============================================================================

#include <fstream>
#include <iterator>
#include "MaterialCodegen.h"
#include <algorithm>
#include <Pyros3D/Rendering/ColourSpace.h>
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

	// Whether a Texture node holds data rather than a painted colour (see the Texture case).
	bool TextureNodeIsData(const MaterialNode& node) {
		if (node.userData == "linear") return true;
		if (node.userData == "srgb") return false;
		std::string n = node.texturePath;
		std::transform(n.begin(), n.end(), n.begin(), ::tolower);
		const size_t slash = n.find_last_of("/\\");
		if (slash != std::string::npos) n = n.substr(slash + 1);
		static const char* kData[] = { "nor", "nrm", "normal", "rough", "metal", "orm", "arm", "_ao", "occlusion", "mask", "height", "disp", "bump", "spec" };
		for (const char* k : kData) if (n.find(k) != std::string::npos) return true;
		return false;
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
	// the (skinned) world-space position, normal and texcoord the renderer
	// hands its Vertex() before the offset is applied - see BuildSurface.
	std::string UVExpr() const { return stage == Stage::Fragment ? "vec4(vTexcoord, 0.0, 1.0)" : "vec4(p3d_vertexUV, 0.0, 1.0)"; }
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
				// (a picked colour: decoded here, once - the renderer lights in linear light, ColourSpace.h)
				expr = Vec4Literal(p3d::SrgbToLinear(c[0]), p3d::SrgbToLinear(c[1]), p3d::SrgbToLinear(c[2]), c[3]);
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
				expr = isVector ? ("p3d_ToLinear(p_" + name + ")") : ("vec4(p_" + name + ")");    // (a colour parameter is picked: decoded)
				break;
			}
			case T::Texture: {
				if (stage == Stage::Vertex) { error = "A Texture node can't feed Vertex Offset (textures are sampled per fragment)"; break; }
				std::string sampler = "uTex" + std::to_string((int)textureSamplers.size());
				textureSamplers.push_back({node.id, sampler});
				expr = "texture_2D(" + sampler + ", (" + in(0, UVExpr().c_str()) + ").xy)";
				// A painted picture is sRGB and decoded as it is read; a data picture (normals, roughness,
				// metal, occlusion, masks, heights) is not. userData "linear" / "srgb" says which; without
				// it the file's name does (the names every texture tool gives its maps).
				if (!TextureNodeIsData(node)) expr = "p3d_ToLinear(" + expr + ")";
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

// Normal Map node: tangent-space `m` -> world space, around the geometric normal N, with the tangent frame solved
// from screen-space derivatives of position and UV (Schueler's cotangent frame); sign(det) keeps it right on every
// backend's screen-Y direction.
const char* const kPerturbNormalGLSL =
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
		"}\n";

// The functions graph nodes expand to - stage-neutral GLSL, no uniforms: both stages carry them
void EmitNodeHelpers(std::ostringstream& out) {
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
		// (a hash of whole numbers: exact on every card. The float one this replaced leaned on the low bits
		// of big products, and drew blocks and stray rectangles on some - NVIDIA - where the Mac showed none.
		// p3d_noise only hashes lattice points, so the quarter steps below are whole numbers too.)
		"float p3d_ih(ivec3 q) { uint n = uint(q.x) * 1597334673u ^ uint(q.y) * 3812015801u ^ uint(q.z) * 2798796415u; n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16; return float(n >> 8) * (1.0 / 16777216.0); }\n"
		"float p3d_hash(vec3 p) { return p3d_ih(ivec3(floor(p * 4.0 + 0.5))); }\n"
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

// The renderer's form of the material (CustomShaderMaterial::SetSurfaceSource): the graph's statements in a surface
// function filling SurfaceData; the renderer does the lighting, shadows and the rest. Parameters and textures are
// named in //@ lines for it to bind; uTime, uCameraPosition, vWorldPos, vTexcoord and uOpacity mean what they do here.
std::string BuildSurface(const TemplateInputs& in) {
	std::ostringstream out;
	out << "// Generated by the Pyros3D Material Editor's node graph - the renderer's surface\n";
	for (const auto& p : in.parameters) out << "//@param " << p.name << " " << (p.isVector ? "vec4" : "float") << "\n";
	for (const auto& n : in.samplerNames) out << "//@texture " << n << "\n";
	out << "vec4 p3d_ToLinear(vec4 c) {\n"
		"\tvec3 x = max(c.rgb, vec3(0.0));\n"
		"\treturn vec4(mix(x / 12.92, pow((x + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), x)), c.a);\n"
		"}\n";
	EmitNodeHelpers(out);
	// The Vertex Offset pin: the renderer's vertex stage for this material (surface.vert's CUSTOM_VERTEX), the world
	// position moved before it is projected. The same text is compiled for both stages; each keeps its own part.
	if (!in.vertexOffset.empty()) {
		out << "#ifdef P3D_STAGE_VERTEX\n"
			"void Vertex(inout VertexData v) {\n"
			"\tvec4 p3d_worldPos = vec4(v.position, 1.0);\n"
			"\tvec3 p3d_worldNormal = v.normal;\n"
			"\tvec2 p3d_vertexUV = v.uv;\n";
		for (const auto& st : in.vertexStatements) out << "\t" << st << "\n";
		out << "\tv.position += (" << in.vertexOffset << ").xyz;\n"
			"}\n"
			"#else\n";
	}
	out << kPerturbNormalGLSL << "\n";
	out << "void Surface(inout SurfaceData s) {\n"
		"\tvec3 p3d_N = s.normal;\n"
		"\tvec3 p3d_V = s.view;\n";
	for (const auto& st : in.statements) out << "\t" << st << "\n";
	out << "\ts.albedo = (" << in.albedo << ").xyz;\n"
		"\ts.metallic = (" << in.metallic << ").x;\n"
		"\ts.roughness = clamp((" << in.roughness << ").x, 0.03, 1.0);\n"
		"\ts.emission = (" << in.emissive << ").xyz;\n"
		"\ts.occlusion = (" << in.occlusion << ").x;\n"
		"\ts.alpha = clamp((" << in.opacity << ").x, 0.0, 1.0) * uOpacity;\n";
	if (in.alphaClipConnected) out << "\tif (s.alpha < (" << in.alphaClip << ").x) discard;\n";
	if (in.normalConnected) out << "\ts.normal = normalize((" << in.normal << ").xyz);\n";
	out << "}\n";
	if (!in.vertexOffset.empty()) out << "#endif\n";
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

MaterialCodegenResult GenerateSurfaceFromSimpleText(const std::string& userBody, const std::vector<std::string>& textureNames) {
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
	result.surface = BuildSurface(in);
	result.usesCameraPosition = true;
	result.usesTime = true;
	return result;
}

MaterialCodegenResult GenerateSurface(const std::vector<MaterialNode>& nodes, const std::vector<MaterialConnection>& connections) {
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

	result.surface = BuildSurface(in);
	result.textureSamplers = cg.textureSamplers;
	result.parameters = in.parameters;
	result.usesCameraPosition = cg.usesCameraPosition || vg.usesCameraPosition;
	result.usesTime = cg.usesTime || vg.usesTime;
	return result;
}
