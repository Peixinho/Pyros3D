//============================================================================
// Name        : FsrEffect.cpp
// Description : See FsrEffect.h
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/FsrEffect.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace p3d {

	namespace {
		// AMD's two headers, as shipped beside the engine's shaders.
		bool ReadText(const char* path, std::string &out)
		{
			std::ifstream f(path, std::ios::in | std::ios::binary);
			if (!f) return false;
			std::stringstream ss;
			ss << f.rdbuf();
			out = ss.str();
			return !out.empty();
		}
		const char* kPortability = "shaders/fsr/ffx_a.h";
		const char* kFsr = "shaders/fsr/ffx_fsr1.h";
	}

	bool FsrEffect::SourcesPresent()
	{
#if defined(GLES3) || defined(GLES2) || defined(__EMSCRIPTEN__)
		// (EASU reads through textureGather, which WebGL 2 does not have)
		return false;
#else
		// (PYROS_NO_FSR=1: the built-in filter, to compare the two)
		if (getenv("PYROS_NO_FSR") != NULL) return false;
		std::ifstream a(kPortability), b(kFsr);
		return (bool)a && (bool)b;
#endif
	}

	FsrEffect::FsrEffect(const Pass pass, const uint32 Tex1, const uint32 Width, const uint32 Height, const f32 sharpness) : IEffect(Width, Height)
	{
		UseRTT(Tex1);

		std::string portability, fsr;
		if (!ReadText(kPortability, portability) || !ReadText(kFsr, fsr)) return;

		std::string src =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#if defined(VULKAN)\n"
			"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
			"#define IO_LOCATION(n) layout(location = n)\n"
			"#else\n"
			"#define SAMPLER_BINDING(n)\n"
			"#define IO_LOCATION(n)\n"
			"#endif\n"
			"IO_LOCATION(0) out vec4 FragColor;\n"
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
			"#define A_GPU 1\n"
			"#define A_GLSL 1\n"
			"#define A_SKIP_EXT 1\n";
		src += portability;
		src += "\n";
		if (pass == EASU)
		{
			src +=
				"#define FSR_EASU_F 1\n"
				"AF4 FsrEasuRF(AF2 p) { return textureGather(uTex0, p, 0); }\n"
				"AF4 FsrEasuGF(AF2 p) { return textureGather(uTex0, p, 1); }\n"
				"AF4 FsrEasuBF(AF2 p) { return textureGather(uTex0, p, 2); }\n";
			src += fsr;
			src +=
				"\nvoid main(void) {\n"
				// What it reads is as big as it is; what it writes is as big as
				// one step of the texture coordinate across a pixel says. So the
				// pass is told nothing, and follows the render scale as it moves.
				"	vec2 inSize = vec2(textureSize(uTex0, 0));\n"
				"	vec2 outSize = floor(1.0 / max(abs(vec2(dFdx(vTexcoord.x), dFdy(vTexcoord.y))), vec2(1e-6)) + 0.5);\n"
				"	AU4 con0, con1, con2, con3;\n"
				"	FsrEasuCon(con0, con1, con2, con3, inSize.x, inSize.y, inSize.x, inSize.y, outSize.x, outSize.y);\n"
				// (the pixel being written, counted the way the texture's own
				// texels are - not gl_FragCoord, which runs the other way up on
				// some of the backends)
				"	AU2 ip = AU2(floor(vTexcoord * outSize));\n"
				"	AF3 c;\n"
				"	FsrEasuF(c, ip, con0, con1, con2, con3);\n"
				"	FragColor = vec4(c, 1.0);\n"
				"}\n";
		}
		else
		{
			// RCAS's sharpness is in stops: 0 the sharpest, each one more half of it.
			const f32 stops = 2.f * (1.f - std::max(0.f, std::min(1.f, sharpness)));
			src +=
				"#define FSR_RCAS_F 1\n"
				"AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(uTex0, p, 0); }\n"
				"void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}\n";
			src += fsr;
			src +=
				"\nvoid main(void) {\n"
				"	vec2 size = vec2(textureSize(uTex0, 0));\n"
				"	AU4 con;\n"
				"	FsrRcasCon(con, " + std::to_string(stops) + ");\n"
				"	AU2 ip = AU2(floor(vTexcoord * size));\n"
				"	AF3 c;\n"
				"	FsrRcasF(c.r, c.g, c.b, ip, con);\n"
				"	FragColor = vec4(c, 1.0);\n"
				"}\n";
		}
		FragmentShaderString = src;

		shader->LoadShaderText(VertexShaderString);
		bool ok = shader->CompileShader(ShaderType::VertexShader);
		shader->LoadShaderText(FragmentShaderString);
		ok = shader->CompileShader(ShaderType::FragmentShader) && ok;
		valid = shader->LinkProgram() && ok;
	}

	FsrEffect::~FsrEffect() {}

};
