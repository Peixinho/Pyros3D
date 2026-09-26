// Compiles every shader the Material Editor can generate, the way each
// backend would, without a window or a device.
//
// MaterialCodegen is a string builder: a typo in one node's expression, or a
// template edit that breaks one #ifdef combination, produces a file that
// looks fine and only fails when someone Applies that exact graph under that
// exact renderer - or, for the GLES path, only in a browser. So this builds
// graphs that between them use every node type and every Output pin, and
// compiles each result six ways (Forward/Deferred/SHADOW_DEPTH x
// static/SKINNING) for
// three targets:
//
//   GL 4.1     - glslangValidator, with GLRenderDevice's `#version 410` prefix
//   GLES 3.00  - glslangValidator, with GLRenderDevice's WebGL2 preamble
//   Vulkan     - the engine's own SpirvShaderCompiler, AutoFix included, as
//                VulkanRenderDevice::CompileShaderStage does (Metal compiles
//                the same SPIR-V)
//
// It also checks the things a compile can't see: no mediump in the GLES
// output, and that nodes which cannot run per vertex are refused under
// Vertex Offset instead of generating something that silently misbehaves.
//
//   c++ -std=c++17 -DSPIRV_TOOLING -I include -I src/Pyros3D/Ext/imgui \
//       -I editor/src/editor $(pkg-config --cflags freetype2) \
//       tools/tests/material_codegen.cpp editor/src/editor/MaterialCodegen.cpp \
//       editor/src/editor/MaterialGraphTypes.cpp -o /tmp/material_codegen \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/material_codegen            # needs glslangValidator on PATH
//
// Exits 0 on PASS, 1 on FAIL.

#include "MaterialCodegen.h"
#include <Pyros3D/Rendering/SPIRV/ShaderCompiler.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace p3d;

namespace {

int failures = 0;

void Fail(const std::string& what) {
	std::fprintf(stderr, "FAIL: %s\n", what.c_str());
	failures++;
}

struct Graph {
	std::vector<MaterialNode> nodes;
	std::vector<MaterialConnection> connections;
	uint32_t nextId = 1;

	uint32_t Add(MaterialNode::Type type, const std::string& userData = "", const std::string& name = "") {
		MaterialNode n;
		n.id = nextId++;
		n.type = type;
		n.name = name.empty() ? MaterialNode::TypeToString(type) : name;
		n.userData = userData;
		nodes.push_back(n);
		return n.id;
	}
	void Link(uint32_t from, int fromPin, uint32_t to, int toPin) {
		MaterialConnection c;
		c.fromNode = from; c.fromPinIndex = fromPin; c.toNode = to; c.toPinIndex = toPin;
		connections.push_back(c);
	}
};

std::string ScratchDir() {
	const char* t = std::getenv("TMPDIR");
	return std::string(t ? t : "/tmp");
}

// glslangValidator reads the stage from the extension.
bool CompileWithGlslang(const std::string& source, bool vertex, std::string& log) {
	const std::string path = ScratchDir() + "/p3d_material_codegen." + (vertex ? "vert" : "frag");
	{
		std::ofstream f(path);
		f << source;
	}
	const std::string cmd = "glslangValidator " + path + " 2>&1";
	FILE* p = popen(cmd.c_str(), "r");
	if (!p) { log = "could not run glslangValidator"; return false; }
	char buf[512];
	log.clear();
	while (fgets(buf, sizeof(buf), p)) log += buf;
	return pclose(p) == 0;
}

const char* kGles3Preamble =
	"#version 300 es\n#define GLES3\n"
	"precision highp float;\nprecision highp int;\nprecision lowp sampler2D;\nprecision lowp samplerCube;\n"
	"precision highp sampler3D;\nprecision highp sampler2DShadow;\nprecision highp samplerCubeShadow;\n"
	"precision lowp sampler2DArray;\nprecision highp sampler2DArrayShadow;\n";

void CompileEverywhere(const std::string& label, const std::string& glsl) {
	for (int variant = 0; variant < 8; variant++) {
		const bool gbuffer = (variant & 1) != 0;
		const bool skinned = (variant & 2) != 0;
		const bool shadow = (variant & 4) != 0;
		if (gbuffer && shadow) continue; // the shadow pass is never a G-buffer pass
		std::string defines;
		if (gbuffer) defines += "#define DEFERRED_GBUFFER\n";
		if (skinned) defines += "#define SKINNING\n";
		if (shadow) defines += "#define SHADOW_DEPTH\n";
		const std::string what = label + (shadow ? " [shadow" : gbuffer ? " [deferred" : " [forward") + (skinned ? ", skinned]" : "]");

		for (int stage = 0; stage < 2; stage++) {
			const bool vertex = stage == 0;
			const std::string stageDefine = vertex ? "#define VERTEX\n" : "#define FRAGMENT\n";
			const std::string stageName = vertex ? " vertex" : " fragment";
			std::string log;

			if (!CompileWithGlslang("#version 410\n" + stageDefine + defines + " " + glsl, vertex, log))
				Fail(what + stageName + " GL 4.1:\n" + log);
			if (!CompileWithGlslang(std::string(kGles3Preamble) + stageDefine + defines + " " + glsl, vertex, log))
				Fail(what + stageName + " GLES 3.00:\n" + log);

			// VulkanRenderDevice::BuildShaderSource + CompileShaderStage.
			std::string src = "#version 450\n" + stageDefine + defines + " " + glsl;
			const uint32 spirvStage = vertex ? SpirvShaderStage::Vertex : SpirvShaderStage::Fragment;
			if (SpirvShaderCompiler::NeedsAutoFixForVulkan(src)) {
				SpirvAutoUboResult autoUbo;
				std::string err;
				if (!SpirvShaderCompiler::AutoFixForVulkan(src, spirvStage, 40, "AutoUBO_test", autoUbo, err)) {
					Fail(what + stageName + " Vulkan AutoFix: " + err);
					continue;
				}
			}
			std::vector<uint32> spirv;
			if (!SpirvShaderCompiler::Compile(src, spirvStage, spirv, log))
				Fail(what + stageName + " Vulkan:\n" + log);
		}
	}
}

void ExpectGraph(const std::string& label, const Graph& g) {
	MaterialCodegenResult r = GenerateGLSL(g.nodes, g.connections);
	if (!r.error.empty()) { Fail(label + ": codegen error: " + r.error); return; }
	if (r.glsl.find("mediump") != std::string::npos) Fail(label + ": generated source mentions mediump");
	CompileEverywhere(label, r.glsl);
}

void ExpectError(const std::string& label, const Graph& g, const std::string& fragment) {
	MaterialCodegenResult r = GenerateGLSL(g.nodes, g.connections);
	if (r.error.empty()) Fail(label + ": expected a codegen error, got none");
	else if (r.error.find(fragment) == std::string::npos) Fail(label + ": wrong error: " + r.error);
}

} // namespace

int main() {
	using N = MaterialNode;

	// 1. The seed graph every new material starts from.
	{
		Graph g;
		uint32_t color = g.Add(N::Color, "1,1,1,1");
		uint32_t metal = g.Add(N::Float, "0");
		uint32_t rough = g.Add(N::Float, "0.5");
		uint32_t out = g.Add(N::Output);
		g.Link(color, 4, out, N::OutAlbedo);
		g.Link(metal, 0, out, N::OutMetallic);
		g.Link(rough, 0, out, N::OutRoughness);
		ExpectGraph("seed graph", g);
		if (GenerateGLSL(g.nodes, g.connections).glsl.find("P3D_CUSTOM_SHADOW") != std::string::npos)
			Fail("seed graph: custom-shadow marker without offset or clip");
	}

	// 2. A realistic material: tiled + scrolling texture, normal map, fresnel
	// rim, parameters, cutout, SSR and a vertex wobble.
	{
		Graph g;
		uint32_t out = g.Add(N::Output);
		uint32_t uv = g.Add(N::UVCoordinate);
		uint32_t tiling = g.Add(N::FloatParameter, "4", "Tiling");
		uint32_t tiled = g.Add(N::Multiply);
		g.Link(uv, 0, tiled, 0); g.Link(tiling, 0, tiled, 1);
		uint32_t time = g.Add(N::TimeValue);
		uint32_t scroll = g.Add(N::Add);
		g.Link(tiled, 0, scroll, 0); g.Link(time, 0, scroll, 1);
		uint32_t albedoTex = g.Add(N::Texture);
		g.Link(scroll, 0, albedoTex, 0);
		uint32_t tint = g.Add(N::ColorParameter, "1,0.5,0.2,1", "Tint");
		uint32_t tinted = g.Add(N::Multiply);
		g.Link(albedoTex, 0, tinted, 0); g.Link(tint, 4, tinted, 1);
		g.Link(tinted, 0, out, N::OutAlbedo);
		uint32_t normalTex = g.Add(N::Texture);
		g.Link(tiled, 0, normalTex, 0);
		uint32_t nmap = g.Add(N::NormalMap);
		uint32_t strength = g.Add(N::Float, "0.8");
		g.Link(normalTex, 0, nmap, 0); g.Link(strength, 0, nmap, 1);
		g.Link(nmap, 0, out, N::OutNormal);
		uint32_t fres = g.Add(N::Fresnel);
		g.Link(nmap, 0, fres, 1);
		uint32_t rim = g.Add(N::Multiply);
		g.Link(fres, 0, rim, 0); g.Link(tint, 4, rim, 1);
		g.Link(rim, 0, out, N::OutEmissive);
		uint32_t noise = g.Add(N::Noise);
		uint32_t wp = g.Add(N::ObjectPosition);
		g.Link(wp, 0, noise, 0);
		uint32_t remap = g.Add(N::Remap);
		uint32_t lo = g.Add(N::Float, "0.2"), hi = g.Add(N::Float, "0.9");
		g.Link(noise, 0, remap, 0); g.Link(lo, 0, remap, 3); g.Link(hi, 0, remap, 4);
		g.Link(remap, 0, out, N::OutRoughness);
		g.Link(albedoTex, 4, out, N::OutOpacity);   // texture alpha
		uint32_t clip = g.Add(N::Float, "0.5");
		g.Link(clip, 0, out, N::OutAlphaClip);
		uint32_t refl = g.Add(N::FloatParameter, "0.3", "Shine");
		g.Link(refl, 0, out, N::OutReflection);
		// Vertex: offset along the normal by sin(time + noise(position)).
		uint32_t vn = g.Add(N::NormalVector);
		uint32_t vnoise = g.Add(N::Noise);
		uint32_t vpos = g.Add(N::ObjectPosition);
		g.Link(vpos, 0, vnoise, 0);
		uint32_t phase = g.Add(N::Add);
		g.Link(time, 0, phase, 0); g.Link(vnoise, 0, phase, 1);
		uint32_t wave = g.Add(N::Sin);
		g.Link(phase, 0, wave, 0);
		uint32_t amp = g.Add(N::FloatParameter, "0.05", "Wobble");
		uint32_t scaled = g.Add(N::Multiply);
		g.Link(wave, 0, scaled, 0); g.Link(amp, 0, scaled, 1);
		uint32_t offset = g.Add(N::Multiply);
		g.Link(vn, 0, offset, 0); g.Link(scaled, 0, offset, 1);
		g.Link(offset, 0, out, N::OutVertexOffset);

		ExpectGraph("full material", g);
		MaterialCodegenResult r = GenerateGLSL(g.nodes, g.connections);
		if (r.parameters.size() != 4) Fail("full material: expected 4 parameters, got " + std::to_string(r.parameters.size()));
		if (r.textureSamplers.size() != 2) Fail("full material: expected 2 samplers");
		if (r.glsl.find("discard;") == std::string::npos) Fail("full material: Alpha Clip connected but no discard");
		if (r.glsl.find("P3D_CUSTOM_SHADOW") == std::string::npos) Fail("full material: offset/clip but no custom-shadow marker");
	}

	// 3. Every node type, each feeding Albedo through a chain of Adds, so
	// every node's expression is compiled at least once - in both stages
	// where the node is allowed per vertex.
	for (int stage = 0; stage < 2; stage++) {
		const bool viaVertex = stage == 1;
		Graph g;
		uint32_t out = g.Add(N::Output);
		uint32_t acc = g.Add(N::Float, "0");
		for (int t = 0; t <= (int)N::CustomExpression; t++) {
			const N::Type type = (N::Type)t;
			if (type == N::Output) continue;
			if (viaVertex && (type == N::Texture || type == N::NormalMap)) continue;
			std::string data;
			if (type == N::CustomExpression) data = "a.xyz * b.x + vec3(c.y, d.z, 1.0)";
			const uint32_t n = g.Add(type, data, type == N::FloatParameter ? "P1" : type == N::ColorParameter ? "P2" : "");
			const int inputs = N::GetInputPinCount(type);
			for (int i = 0; i < inputs; i++) {
				const uint32_t c = g.Add(N::Float, "0.25");
				g.Link(c, 0, n, i);
			}
			const int outputs = N::GetOutputPinCount(type);
			for (int o = 0; o < outputs; o++) {
				const uint32_t sum = g.Add(N::Add);
				g.Link(acc, 0, sum, 0);
				g.Link(n, o, sum, 1);
				acc = sum;
			}
		}
		g.Link(acc, 0, out, viaVertex ? N::OutVertexOffset : N::OutAlbedo);
		ExpectGraph(viaVertex ? "every node (vertex stage)" : "every node (fragment stage)", g);
	}

	// 4. Graphs that must be refused.
	{
		Graph g;
		uint32_t out = g.Add(N::Output);
		uint32_t tex = g.Add(N::Texture);
		g.Link(tex, 0, out, N::OutVertexOffset);
		ExpectError("texture under Vertex Offset", g, "Texture");
	}
	{
		Graph g;
		uint32_t out = g.Add(N::Output);
		uint32_t a = g.Add(N::FloatParameter, "1", "Same");
		uint32_t b = g.Add(N::ColorParameter, "1,1,1,1", "Same");
		uint32_t add = g.Add(N::Add);
		g.Link(a, 0, add, 0); g.Link(b, 4, add, 1);
		g.Link(add, 0, out, N::OutAlbedo);
		ExpectError("parameter used as two types", g, "Same");
	}
	{
		Graph g;
		uint32_t out = g.Add(N::Output);
		uint32_t a = g.Add(N::Add), b = g.Add(N::Add);
		g.Link(a, 0, b, 0); g.Link(b, 0, a, 0);
		g.Link(a, 0, out, N::OutAlbedo);
		ExpectError("cycle", g, "Cycle");
	}

	// 5. Text mode: the seed snippet, and one using cutout + textures + built-ins.
	{
		MaterialCodegenResult r = GenerateGLSLFromSimpleText(kDefaultSimpleShaderText);
		if (!r.error.empty()) Fail("text seed: " + r.error);
		else CompileEverywhere("text seed", r.glsl);
		if (r.glsl.find("discard;") != std::string::npos) Fail("text seed: discard emitted without AlphaClip");

		const std::string body =
			"vec4 s = texture_2D(uAlbedoTex, vTexcoord * 2.0);\n"
			"vec3 Albedo = s.rgb;\n"
			"float Opacity = s.a;\n"
			"float AlphaClip = 0.5;\n"
			"vec3 Emissive = vec3(pow(1.0 - max(dot(p3d_N, p3d_V), 0.0), 4.0));\n";
		r = GenerateGLSLFromSimpleText(body, { "uAlbedoTex" });
		if (!r.error.empty()) Fail("text cutout: " + r.error);
		else CompileEverywhere("text cutout", r.glsl);
		if (r.glsl.find("discard;") == std::string::npos) Fail("text cutout: AlphaClip declared but no discard");
	}

	// 6. Saved names: the old ObjectPosition name still loads.
	{
		N::Type t;
		if (!N::TypeFromString("ObjectPosition", t) || t != N::ObjectPosition) Fail("ObjectPosition alias does not load");
		if (std::string(N::TypeToString(N::ObjectPosition)) != "WorldPosition") Fail("WorldPosition is not the saved name");
		for (int i = 0; i <= (int)N::CustomExpression; i++) {
			N::Type back;
			if (!N::TypeFromString(N::TypeToString((N::Type)i), back) || back != (N::Type)i)
				Fail(std::string("type name does not round-trip: ") + N::TypeToString((N::Type)i));
		}
	}

	if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
	std::printf("PASS\n");
	return 0;
}
