//============================================================================
// Name        : DisplayTextureEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Display Texture Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/DisplayTextureEffect.h>

namespace p3d {

	DisplayTextureEffect::DisplayTextureEffect(Texture* texture, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseCustomTexture(texture);

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
								"IO_LOCATION(0) out vec4 FragColor;"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;"
								"void main(void) {\n"
								"	FragColor = texture_2D(uTex0, vTexcoord);\n"
								"}\n";

		CompileShaders();
	}

	DisplayTextureEffect::~DisplayTextureEffect() {}

};
