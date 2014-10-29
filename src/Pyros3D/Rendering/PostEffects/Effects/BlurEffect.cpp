//============================================================================
// Name        : BlurEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/BlurEffect.h>

namespace p3d {

    BlurEffect::BlurEffect(const uint32 &Tex1) : IEffect() 
    {
        
        // Set RTT
        UseRTT(Tex1);
        
        // Create Fragment Shader
        FragmentShaderString =      "#define KERNEL_SIZE 9\n"
                                                "varying vec2 vTexcoord;"
                                                "\n"
                                                "// Gaussian kernel\n"
                                                "// 1 2 1\n"
                                                "// 2 4 2\n"
                                                "// 1 2 1	\n"
                                                "uniform sampler2D uTex0, uTex1;\n"
                                                "const float width = 1024.0;\n"
                                                "const float height = 768.0;\n"
                                                "\n"
                                                "const float step_w = 1.0/1024.0;\n"
                                                "const float step_h = 1.0/768.0;\n"
                                                "\n"
                                                "void main(void)\n"
                                                "{\n"
                                                    "float kernel[9];\n"
                                                    "kernel[0] = 1.0/16.0;\n"
                                                    "kernel[1] = 2.0/16.0;\n"
                                                    "kernel[2] = 1.0/16.0;\n"
                                                    "kernel[3] = 2.0/16.0;\n"
                                                    "kernel[4] = 4.0/16.0;\n"
                                                    "kernel[5] = 2.0/16.0;\n"
                                                    "kernel[6] = 1.0/16.0;\n"
                                                    "kernel[7] = 2.0/16.0;\n"
                                                    "kernel[8] = 1.0/16.0;\n"
                                                    "\n"
                                                    "vec2 offset[9];\n"
                                                    "offset[0] = vec2(-step_w, -step_h);\n"
                                                    "offset[1] = vec2(0.0, -step_h);\n"
                                                    "offset[2] = vec2(step_w, -step_h);\n"
                                                    "offset[3] = vec2(-step_w, 0.0);\n"
                                                    "offset[4] = vec2(0.0, 0.0);\n"
                                                    "offset[5] = vec2(step_w, 0.0);\n"
                                                    "offset[6] = vec2(-step_w, step_h);\n"
                                                    "offset[7] = vec2(0.0, step_h);\n"
                                                    "offset[8] = vec2(step_w, step_h);\n"
                                                    "int i = 0;\n"
                                                    "vec4 sum = vec4(0.0);\n"
                                                    "\n"
                                                    "for( i=0; i<KERNEL_SIZE; i++ )\n"
                                                    "{\n"
                                                        "vec4 tmp = texture2D(uTex0, vTexcoord + offset[i]);\n"
                                                        "sum += tmp * kernel[i];\n"
                                                    "}\n"
                                                    "gl_FragColor = sum;\n"
                                                "}";
        
        CompileShaders();
    }

    BlurEffect::~BlurEffect() {
    }

};