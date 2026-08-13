//============================================================================
// Name        : GammaEncodeEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Linear HDR -> display-referred LDR (pow 1/2.2)
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/GammaEncodeEffect.h>

namespace p3d {

	GammaEncodeEffect::GammaEncodeEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseRTT(Tex1);

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
								"IO_LOCATION(0) out vec4 FragColor;"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;"
								"void main(void) {\n"
								"	vec4 c = texture_2D(uTex0, vTexcoord);\n"
								"	FragColor = vec4(pow(max(c.rgb, vec3(0.0)), vec3(1.0/2.2)), c.a);\n"
								"}\n";

		CompileShaders();
	}

	GammaEncodeEffect::~GammaEncodeEffect() {}

};
