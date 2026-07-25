//============================================================================
// Name        : Resize.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Resize Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>

namespace p3d {

    ResizeEffect::ResizeEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height) 
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
								// See SSAOEffect.cpp's identical comment -
								// no non-sampler uniforms here, so only
								// the sampler/varying-location macros are
								// needed.
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
                                    "FragColor = texture_2D(uTex0, vTexcoord);\n"
                                "}\n";
        
        CompileShaders();
    }

    ResizeEffect::~ResizeEffect() {
    }

};
