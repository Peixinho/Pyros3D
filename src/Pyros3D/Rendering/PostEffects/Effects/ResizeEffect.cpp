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
								"out vec4 FragColor;"
                                "uniform sampler2D uTex0;\n"
                                "varying_in vec2 vTexcoord;"
                                "void main(void) {\n"
                                    "FragColor = texture_2D(uTex0, vTexcoord);\n"
                                "}\n";
        
        CompileShaders();
    }

    ResizeEffect::~ResizeEffect() {
    }

};
