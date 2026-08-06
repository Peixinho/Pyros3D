//============================================================================
// Name        : SSAOCompositeEffect.cpp
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/SSAOCompositeEffect.h>

namespace p3d {

	SSAOCompositeEffect::SSAOCompositeEffect(const uint32 TexColor, const uint32 TexSSAO, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseRTT(TexColor);
		UseRTT(TexSSAO);

		FragmentShaderString =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
			"#if defined(VULKAN)\n"
			"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
			"#define IO_LOCATION(n) layout(location = n)\n"
			"#else\n"
			"#define SAMPLER_BINDING(n)\n"
			"#define IO_LOCATION(n)\n"
			"#endif\n"
			"IO_LOCATION(0) out vec4 FragColor;\n"
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
			"void main() {\n"
			"FragColor = texture_2D(uTex0, vTexcoord)*texture_2D(uTex1, vTexcoord);\n"
			"}";

		CompileShaders();
	}

	SSAOCompositeEffect::~SSAOCompositeEffect() {}

}
