//============================================================================
// Name        : TonemapEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Tonemap Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>

namespace p3d {

	TonemapEffect::TonemapEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

		// Create Fragment Shader
		FragmentShaderString =
								"#define varying_in in\n"
								"#define varying_out out\n"
								"#define attribute_in in\n"
								"#define texture_2D texture\n"
								"#define texture_cube texture\n"
								#if defined(GLES3)
									"precision mediump float;\n"
								#endif
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
								// ACES filmic fit (Narkowicz 2015) - compact,
								// no LUT, standard choice for real-time PBR.
								"vec3 ACESFilm(vec3 x) {\n"
								"	float a = 2.51;\n"
								"	float b = 0.03;\n"
								"	float c = 2.43;\n"
								"	float d = 0.59;\n"
								"	float e = 0.14;\n"
								"	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);\n"
								"}\n"
								"void main() {\n"
								"	vec3 x = texture_2D(uTex0, vTexcoord).rgb;\n"
								"	vec3 mapped = ACESFilm(x);\n"
								"	mapped = pow(mapped, vec3(1.0/2.2));\n"
								"	FragColor = vec4(mapped, 1.0);\n"
								"}\n";

		CompileShaders();
	}

	TonemapEffect::~TonemapEffect() {}

};
