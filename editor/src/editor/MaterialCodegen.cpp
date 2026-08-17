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
// scalar-producing nodes just leave y/z/w unused and get a `.x` taken by
// whichever consumer needs a scalar).
class Codegen {
public:
	Codegen(const std::vector<MaterialNode>& nodes, const std::vector<MaterialConnection>& connections)
		: nodes(nodes), connections(connections) {}

	const MaterialNode* FindNode(uint32_t id) const {
		for (const auto& n : nodes) if (n.id == id) return &n;
		return nullptr;
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
	// Split*'s X/Y/Z/W) - broadcasts the selected scalar component back to a
	// vec4 so every node's local var stays uniformly vec4-typed.
	std::string ApplyOutputSwizzle(const MaterialNode& src, int fromPinIndex, const std::string& varName) {
		if (src.type == MaterialNode::Color) {
			switch (fromPinIndex) {
				case 0: return "vec4(" + varName + ".x)";
				case 1: return "vec4(" + varName + ".y)";
				case 2: return "vec4(" + varName + ".z)";
				case 3: return "vec4(" + varName + ".w)";
				default: return varName; // RGBA
			}
		}
		if (src.type == MaterialNode::SplitVec2 || src.type == MaterialNode::SplitVec3 || src.type == MaterialNode::SplitVec4) {
			const char* comp = (fromPinIndex == 0) ? ".x" : (fromPinIndex == 1) ? ".y" : (fromPinIndex == 2) ? ".z" : ".w";
			return "vec4(" + varName + comp + ")";
		}
		return varName;
	}

	// Emits (memoized) the statement computing `node`'s value; returns its
	// local variable name, or "" with `error` set on failure.
	std::string EmitNode(const MaterialNode& node) {
		auto found = emitted.find(node.id);
		if (found != emitted.end()) return found->second;
		if (visiting.count(node.id)) { error = "Cycle detected in node graph at node " + std::to_string(node.id); return ""; }
		visiting.insert(node.id);

		const std::string var = "n" + std::to_string(node.id);
		std::string expr;

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
			case T::Texture: {
				std::string sampler = "uTex" + std::to_string((int)textureSamplers.size());
				textureSamplers.push_back({node.id, sampler});
				expr = "texture_2D(" + sampler + ", vTexcoord)";
				break;
			}
			case T::Add: expr = "(" + ResolveInput(node.id,0,"vec4(0.0)") + " + " + ResolveInput(node.id,1,"vec4(0.0)") + ")"; break;
			case T::Subtract: expr = "(" + ResolveInput(node.id,0,"vec4(0.0)") + " - " + ResolveInput(node.id,1,"vec4(0.0)") + ")"; break;
			case T::Multiply: expr = "(" + ResolveInput(node.id,0,"vec4(1.0)") + " * " + ResolveInput(node.id,1,"vec4(1.0)") + ")"; break;
			case T::Divide: expr = "(" + ResolveInput(node.id,0,"vec4(0.0)") + " / (" + ResolveInput(node.id,1,"vec4(1.0)") + " + vec4(0.0001)))"; break;
			case T::Power: expr = "pow(abs(" + ResolveInput(node.id,0,"vec4(1.0)") + "), " + ResolveInput(node.id,1,"vec4(1.0)") + ")"; break;
			case T::Modulo: expr = "mod(" + ResolveInput(node.id,0,"vec4(0.0)") + ", max(" + ResolveInput(node.id,1,"vec4(1.0)") + ", vec4(0.0001)))"; break;
			case T::Negate: expr = "(-" + ResolveInput(node.id,0,"vec4(0.0)") + ")"; break;
			case T::Abs: expr = "abs(" + ResolveInput(node.id,0,"vec4(0.0)") + ")"; break;
			case T::Sqrt: expr = "sqrt(abs(" + ResolveInput(node.id,0,"vec4(0.0)") + "))"; break;
			case T::Sin: expr = "sin(" + ResolveInput(node.id,0,"vec4(0.0)") + ")"; break;
			case T::Cos: expr = "cos(" + ResolveInput(node.id,0,"vec4(0.0)") + ")"; break;
			case T::Tan: expr = "tan(" + ResolveInput(node.id,0,"vec4(0.0)") + ")"; break;
			case T::Min: expr = "min(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + ")"; break;
			case T::Max: expr = "max(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + ")"; break;
			case T::Clamp: expr = "clamp(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + ", " + ResolveInput(node.id,2,"vec4(1.0)") + ")"; break;
			case T::Lerp: expr = "mix(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(1.0)") + ", " + ResolveInput(node.id,2,"vec4(0.5)") + ".x)"; break;
			case T::DotProduct: expr = "vec4(dot(" + ResolveInput(node.id,0,"vec4(0.0)") + ".xyz, " + ResolveInput(node.id,1,"vec4(0.0)") + ".xyz))"; break;
			case T::CrossProduct: expr = "vec4(cross(" + ResolveInput(node.id,0,"vec4(0.0)") + ".xyz, " + ResolveInput(node.id,1,"vec4(0.0)") + ".xyz), 0.0)"; break;
			case T::Length: expr = "vec4(length(" + ResolveInput(node.id,0,"vec4(0.0)") + ".xyz))"; break;
			case T::Normalize: expr = "vec4(normalize(" + ResolveInput(node.id,0,"vec4(0.0,0.0,1.0,0.0)") + ".xyz), 0.0)"; break;
			case T::Distance: expr = "vec4(distance(" + ResolveInput(node.id,0,"vec4(0.0)") + ".xyz, " + ResolveInput(node.id,1,"vec4(0.0)") + ".xyz))"; break;
			case T::Equal: expr = "vec4(equal(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + "))"; break;
			case T::NotEqual: expr = "vec4(notEqual(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + "))"; break;
			case T::GreaterThan: expr = "vec4(greaterThan(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + "))"; break;
			case T::LessThan: expr = "vec4(lessThan(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + "))"; break;
			case T::And: expr = "(((" + ResolveInput(node.id,0,"vec4(0.0)") + ".x > 0.5) && (" + ResolveInput(node.id,1,"vec4(0.0)") + ".x > 0.5)) ? vec4(1.0) : vec4(0.0))"; break;
			case T::Or: expr = "(((" + ResolveInput(node.id,0,"vec4(0.0)") + ".x > 0.5) || (" + ResolveInput(node.id,1,"vec4(0.0)") + ".x > 0.5)) ? vec4(1.0) : vec4(0.0))"; break;
			case T::Not: expr = "((" + ResolveInput(node.id,0,"vec4(0.0)") + ".x > 0.5) ? vec4(0.0) : vec4(1.0))"; break;
			case T::Step: expr = "step(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(0.0)") + ")"; break;
			case T::SmoothStep: expr = "smoothstep(" + ResolveInput(node.id,0,"vec4(0.0)") + ", " + ResolveInput(node.id,1,"vec4(1.0)") + ", " + ResolveInput(node.id,2,"vec4(0.5)") + ")"; break;
			case T::SplitVec2: case T::SplitVec3: case T::SplitVec4:
				expr = ResolveInput(node.id, 0, "vec4(0.0)"); // pass-through; consumer applies the X/Y/Z/W swizzle
				break;
			case T::CombineVec2: expr = "vec4(" + ResolveInput(node.id,0,"vec4(0.0)") + ".x, " + ResolveInput(node.id,1,"vec4(0.0)") + ".x, 0.0, 0.0)"; break;
			case T::CombineVec3: expr = "vec4(" + ResolveInput(node.id,0,"vec4(0.0)") + ".x, " + ResolveInput(node.id,1,"vec4(0.0)") + ".x, " + ResolveInput(node.id,2,"vec4(0.0)") + ".x, 0.0)"; break;
			case T::CombineVec4: expr = "vec4(" + ResolveInput(node.id,0,"vec4(0.0)") + ".x, " + ResolveInput(node.id,1,"vec4(0.0)") + ".x, " + ResolveInput(node.id,2,"vec4(0.0)") + ".x, " + ResolveInput(node.id,3,"vec4(1.0)") + ".x)"; break;
			case T::UVCoordinate: expr = "vec4(vTexcoord, 0.0, 1.0)"; break;
			case T::NormalVector: expr = "vec4(normalize(vNormal), 0.0)"; break;
			case T::ObjectPosition: expr = "vWorldPos"; break;
			case T::CameraPosition: usesCameraPosition = true; expr = "vec4(uCameraPosition, 1.0)"; break;
			case T::TimeValue: usesTime = true; expr = "vec4(uTime)"; break;
			case T::Output: expr = "vec4(0.0)"; break; // never actually emitted as a statement (it's the traversal root)
			default: expr = "vec4(0.5, 0.5, 0.5, 1.0)"; break;
		}

		statements.push_back("vec4 " + var + " = " + expr + ";");
		visiting.erase(node.id);
		emitted[node.id] = var;
		return var;
	}

	const std::vector<MaterialNode>& nodes;
	const std::vector<MaterialConnection>& connections;
	std::map<uint32_t, std::string> emitted;
	std::set<uint32_t> visiting;
	std::vector<std::string> statements;
	std::vector<std::pair<uint32_t, std::string>> textureSamplers;
	bool usesCameraPosition = false;
	bool usesTime = false;
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
std::string BuildTemplate(const std::string& albedoExpr, bool normalConnected, const std::string& normalConnectedExpr,
                           const std::string& metallicExpr, const std::string& roughnessExpr,
                           const std::string& emissiveExpr, const std::string& occlusionExpr,
                           const std::vector<std::string>& statements,
                           const std::vector<std::string>& samplerNames,
                           bool usesTime) {
	std::ostringstream out;
	out <<
		"#define varying_in in\n"
		"#define varying_out out\n"
		"#define attribute_in in\n"
		"#define texture_2D texture\n"
		"#define texture_cube texture\n"
		"#define MAX_LIGHTS 4\n"
		"#if defined(GLES3)\n"
		"\tprecision mediump float;\n"
		"#endif\n"
		"\n"
		"// Generated by the Pyros3D Material Editor's node graph. Hand edits here\n"
		"// are preserved until the graph is re-Applied (which overwrites this file).\n"
		"// Contains both a Forward (real per-fragment lighting) and a Deferred\n"
		"// G-buffer branch - DEFERRED_GBUFFER picks which one compiles, set by\n"
		"// the project's Renderer setting at Apply time.\n"
		"\n"
		"#ifdef VERTEX\n"
		"attribute_in vec3 aPosition;\n"
		"attribute_in vec3 aNormal;\n"
		"attribute_in vec2 aTexcoord;\n"
		"uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;\n"
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
		"\tvTexcoord = aTexcoord;\n"
		"\tvWorldPositionShadow = uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);\n"
		"\tvNormal = (uViewMatrix * uModelMatrix * vec4(aNormal, 0.0)).xyz;\n"
		"\tvNormalWorld = (uModelMatrix * vec4(aNormal, 0.0)).xyz;\n"
		"\tvWorldPos = uModelMatrix * vec4(aPosition, 1.0);\n"
		"\tgl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);\n"
		"}\n"
		"#endif\n"
		"\n"
		"#ifdef FRAGMENT\n"
		"uniform vec4 uAmbientLight;\n"
		"uniform mat4 uViewMatrix;\n"
		"uniform vec3 uCameraPosition;\n";
	for (const auto& name : samplerNames)
		out << "uniform sampler2D " << name << ";\n";
	if (usesTime) out << "uniform float uTime;\n";
	out <<
		"#ifndef DEFERRED_GBUFFER\n"
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
		"#ifdef DEFERRED_GBUFFER\n"
		"layout(location = 0) out vec4 FragData_r;\n"
		"layout(location = 1) out vec4 FragData_g;\n"
		"layout(location = 2) out vec4 FragData_b;\n"
		"layout(location = 3) out vec4 FragData_pbr;\n"
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
		// 4x4 PCF over the cascade, straight from PyrosShader.glsl's
		// own PCFDIRECTIONAL - the modern (non-GLLEGACY) form, where
		// texture() on a sampler2DShadow already returns the compared
		// result.
		"float PCFDirectional(mat4 sMatrix, float offX, float offY, float scale, vec4 pos, bool moreThanOneCascade) {\n"
		"\tvec4 coord = sMatrix * pos;\n"
		"\tif (moreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(offX, offY);\n"
		"\tfloat shadow = 0.0;\n"
		"\tfor (float y = -1.5; y <= 1.5; y += 1.0)\n"
		"\t\tfor (float x = -1.5; x <= 1.5; x += 1.0)\n"
		"\t\t\tshadow += texture(uDirectionalShadowMaps, coord.xyz + vec3(vec2(x, y) * scale, 0.0));\n"
		"\treturn shadow / 16.0;\n"
		"}\n"
		// Cascade selection by view depth, mirroring PyrosShader.glsl's
		// chain: uDirectionalShadowFar[0] is a single vec4 of the four
		// cascade far distances, not a 4-element array.
		"float DirectionalShadowFactor(LIGHT L) {\n"
		"\tif (!L.HaveShadowMap || uNumberOfDirectionalShadows <= 0) return 1.0;\n"
		"\tbool multi = (uDirectionalShadowFar[0].y > 0.0);\n"
		"\tif (gl_FragCoord.z < uDirectionalShadowFar[0].x) return PCFDirectional(uDirectionalDepthsMVP[0], 0.0, 0.0, L.PCFTexelSize, vWorldPositionShadow, multi);\n"
		"\telse if (gl_FragCoord.z < uDirectionalShadowFar[0].y) return PCFDirectional(uDirectionalDepthsMVP[1], 0.5, 0.0, L.PCFTexelSize, vWorldPositionShadow, multi);\n"
		"\telse if (gl_FragCoord.z < uDirectionalShadowFar[0].z) return PCFDirectional(uDirectionalDepthsMVP[2], 0.0, 0.5, L.PCFTexelSize, vWorldPositionShadow, multi);\n"
		"\telse if (gl_FragCoord.z < uDirectionalShadowFar[0].w) return PCFDirectional(uDirectionalDepthsMVP[3], 0.5, 0.5, L.PCFTexelSize, vWorldPositionShadow, multi);\n"
		"\treturn 1.0;\n"
		"}\n"
		// Point and spot both sample element [0] of their sampler array
		// and index only the *matrix* by the light's slot. That mirrors
		// PyrosShader.glsl exactly, and is not an oversight worth
		// "fixing" here: indexing a sampler array by a value that varies
		// with the light loop is not a constant expression, which core
		// GLSL 3.30 does not allow. Diverging would also make a custom
		// material shadow differently from a Generic one.
		"float PCFPoint(mat4 m1, mat4 m2, float scale, vec4 pos) {\n"
		"\tvec4 position_ls = m2 * pos;\n"
		"\tposition_ls.xyz /= position_ls.w;\n"
		"\tvec4 abs_position = abs(position_ls);\n"
		"\tfloat fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));\n"
		"\tvec4 clip = m1 * vec4(0.0, 0.0, fs_z, 1.0);\n"
		// m1 already carries the device's shadow-bias/projection remap,
		// so no extra *0.5+0.5 here - see PyrosShader.glsl's PCFPOINT,
		// where re-applying it double-transformed Vulkan's Z.
		"\tfloat depth = clip.z / clip.w;\n"
		"\tfloat shadow = 0.0;\n"
		"\tfor (float y = -1.5; y <= 1.5; y += 1.0)\n"
		"\t\tfor (float x = -1.5; x <= 1.5; x += 1.0)\n"
		"\t\t\tshadow += (texture(uPointShadowMaps[0], position_ls.xyz + vec3(vec2(x, y) * scale, 0.0)).r >= depth) ? 1.0 : 0.0;\n"
		"\treturn shadow / 16.0;\n"
		"}\n"
		// Single tap, matching PyrosShader.glsl's PCFSPOT - its 4x4 loop
		// is commented out there, and widening it here would make spot
		// shadows softer on custom materials than on Generic ones.
		"float PCFSpot(mat4 sMatrix, float scale, vec4 pos) {\n"
		"\tvec4 coord = sMatrix * pos;\n"
		"\tcoord.xyz /= coord.w;\n"
		"\treturn texture(uSpotShadowMaps[0], coord.xyz);\n"
		"}\n"
		// Slot clamped before use: nothing validates the packed index,
		// and an out-of-range read of uPointDepthsMVP/uSpotDepthsMVP is
		// undefined rather than merely wrong.
		"float PointShadowFactor(LIGHT L) {\n"
		"\tif (!L.HaveShadowMap || uNumberOfPointShadows <= 0) return 1.0;\n"
		"\tint slot = clamp(L.ShadowMap, 0, 3);\n"
		"\treturn PCFPoint(uPointDepthsMVP[slot * 2], uPointDepthsMVP[slot * 2 + 1], L.PCFTexelSize, vWorldPositionShadow);\n"
		"}\n"
		"float SpotShadowFactor(LIGHT L) {\n"
		"\tif (!L.HaveShadowMap || uNumberOfSpotShadows <= 0) return 1.0;\n"
		"\tint slot = clamp(L.ShadowMap, 0, 3);\n"
		"\treturn PCFSpot(uSpotDepthsMVP[slot], L.PCFTexelSize, vWorldPositionShadow);\n"
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
		"\tfloat NdotL = max(dot(N, L), 0.0);\n"
		"\treturn (kD * albedo / PBR_PI + specularTerm) * radiance * NdotL;\n"
		"}\n"
		"#endif\n"
		"\n"
		"void main() {\n";
	for (const auto& s : statements)
		out << "\t" << s << "\n";
	out <<
		"\tvec3 albedo = (" << albedoExpr << ").xyz;\n"
		"\tfloat metallic = (" << metallicExpr << ").x;\n"
		"\tfloat roughness = clamp((" << roughnessExpr << ").x, 0.03, 1.0);\n"
		"\tvec3 emissive = (" << emissiveExpr << ").xyz;\n"
		"\tfloat occlusion = (" << occlusionExpr << ").x;\n"
		"\n"
		"#ifdef DEFERRED_GBUFFER\n";
	if (normalConnected)
		out << "\tvec3 normalOut = normalize((uViewMatrix * vec4(normalize((" << normalConnectedExpr << ")), 0.0)).xyz);\n";
	else
		out << "\tvec3 normalOut = normalize(vNormal);\n";
	out <<
		// Emissive is NOT folded into the albedo channel. Whatever goes
		// there is what the light passes multiply by N.L, so an emissive
		// term packed into it gets attenuated - which is why an
		// emissive-heavy material used to look bright under the forward
		// renderer (where the branch below adds it after lighting) and
		// dark under deferred. It rides in the three alpha channels
		// instead: those carry one finished additive term that
		// secondpassAmbient.glsl outputs as-is, so anything added here
		// lands after lighting, matching forward. The ambient part gets
		// albedo and (1-metallic) applied on this side for the same
		// reason (see that shader's comment).
		// Caveat worth knowing: the albedo and specular G-buffer targets
		// are 8-bit UNORM, so emissive clamps at 1.0 in the R and G
		// channels. Only B (on the RGBA32F normal target) keeps HDR
		// range.
		"\tvec3 litAlbedo = albedo * occlusion;\n"
		"\tvec3 addTerm = litAlbedo * uAmbientLight.rgb + emissive;\n"
		"\tFragData_r = vec4(litAlbedo, addTerm.x);\n"
		"\tFragData_g = vec4(1.0, 1.0, 1.0, addTerm.y);\n"
		"\tFragData_b = vec4(normalOut, addTerm.z);\n"
		"\tFragData_pbr = vec4(roughness, metallic, 0.0, 0.0);\n"
		"#else\n";
	if (normalConnected)
		out << "\tvec3 N = normalize((" << normalConnectedExpr << "));\n";
	else
		out << "\tvec3 N = normalize(vNormalWorld);\n";
	out <<
		"\tvec3 Position = vWorldPos.xyz;\n"
		"\tvec3 V = normalize(uCameraPosition - Position);\n"
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
		"\tvec3 ambientPBR = albedo * occlusion * (1.0 - metallic) * uAmbientLight.rgb;\n"
		"\tvec3 color = pbrColor + ambientPBR + emissive;\n"
		"\tFragColor = vec4(color, 1.0);\n"
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
	"\n"
	"// Leave Normal at (0,0,0) to use the surface's own normal.\n"
	"vec3 Normal = vec3(0.0, 0.0, 0.0);\n";

namespace {
// Crude but sufficient word-boundary check for "<type> <name>" appearing
// anywhere in the user's snippet - used only to decide whether a default
// declaration would collide with one the user already wrote themselves.
bool DeclaresVar(const std::string& body, const char* type, const char* name) {
	std::regex re(std::string("\\b") + type + "\\s+" + name + "\\b");
	return std::regex_search(body, re);
}
} // namespace

MaterialCodegenResult GenerateGLSLFromSimpleText(const std::string& userBody, const std::vector<std::string>& textureNames) {
	MaterialCodegenResult result;

	std::vector<std::string> statements;
	// Only inject a default declaration for a name the user's own text
	// doesn't already declare. kDefaultSimpleShaderText - the seed text
	// every fresh/mode-switched Text document starts from - declares all six
	// itself (with real types, not just assignments), so unconditionally
	// injecting a second declaration of the same name in the same scope
	// used to be a guaranteed GLSL "already defined" error on first use of
	// Text mode. A snippet that only touches e.g. Roughness still compiles,
	// since the other five fall back to their injected defaults.
	std::ostringstream defaults;
	if (!DeclaresVar(userBody, "vec3", "Albedo"))     defaults << "vec3 Albedo = vec3(1.0); ";
	if (!DeclaresVar(userBody, "vec3", "Normal"))     defaults << "vec3 Normal = vec3(0.0); ";
	if (!DeclaresVar(userBody, "float", "Metallic"))  defaults << "float Metallic = 0.0; ";
	if (!DeclaresVar(userBody, "float", "Roughness")) defaults << "float Roughness = 0.5; ";
	if (!DeclaresVar(userBody, "vec3", "Emissive"))   defaults << "vec3 Emissive = vec3(0.0); ";
	if (!DeclaresVar(userBody, "float", "Occlusion")) defaults << "float Occlusion = 1.0; ";
	if (!defaults.str().empty()) statements.push_back(defaults.str());
	statements.push_back(userBody);

	// Normal is always "connected" here (unlike the node-graph path, there's
	// no separate isPinConnected signal) - a zero vector is the sentinel for
	// "not overridden", falling back to the geometric normal. Mathematically
	// identical to the unconnected-Normal branch in both Forward and
	// Deferred (vNormalWorld transformed by uViewMatrix == vNormal).
	const std::string normalExpr = "(dot(Normal, Normal) > 0.0001 ? Normal : vNormalWorld)";

	result.glsl = BuildTemplate("vec4(Albedo, 1.0)", /*normalConnected=*/true, normalExpr,
		"vec4(Metallic)", "vec4(Roughness)", "vec4(Emissive, 0.0)", "vec4(Occlusion)",
		statements, textureNames, /*usesTime=*/true);
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

	Codegen cg(nodes, connections);

	auto resolveOutputPin = [&](int pinIndex, const std::string& defaultExpr) -> std::string {
		for (const auto& c : connections) {
			if (c.toNode != outputNode->id || c.toPinIndex != pinIndex) continue;
			const MaterialNode* src = cg.FindNode(c.fromNode);
			if (!src) return defaultExpr;
			std::string var = cg.EmitNode(*src);
			if (!cg.error.empty()) return defaultExpr;
			return cg.ApplyOutputSwizzle(*src, c.fromPinIndex, var);
		}
		return defaultExpr;
	};
	auto isPinConnected = [&](int pinIndex) {
		for (const auto& c : connections)
			if (c.toNode == outputNode->id && c.toPinIndex == pinIndex) return true;
		return false;
	};

	const std::string albedoExpr    = resolveOutputPin(0, "vec4(1.0)");
	const bool normalConnected      = isPinConnected(1);
	const std::string normalConnectedExpr = normalConnected ? (resolveOutputPin(1, "vec4(0.0,0.0,1.0,0.0)") + ".xyz") : std::string();
	const std::string metallicExpr  = resolveOutputPin(2, "vec4(0.0)");
	const std::string roughnessExpr = resolveOutputPin(3, "vec4(0.5)");
	const std::string emissiveExpr  = resolveOutputPin(4, "vec4(0.0)");
	const std::string occlusionExpr = resolveOutputPin(5, "vec4(1.0)");

	if (!cg.error.empty()) { result.error = cg.error; return result; }

	std::vector<std::string> samplerNames;
	samplerNames.reserve(cg.textureSamplers.size());
	for (const auto& ts : cg.textureSamplers) samplerNames.push_back(ts.second);

	result.glsl = BuildTemplate(albedoExpr, normalConnected, normalConnectedExpr, metallicExpr, roughnessExpr, emissiveExpr, occlusionExpr,
		cg.statements, samplerNames, cg.usesTime);
	result.textureSamplers = cg.textureSamplers;
	result.usesCameraPosition = cg.usesCameraPosition;
	result.usesTime = cg.usesTime;
	return result;
}
