//============================================================================
// Name        : BloomEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bloom Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>

namespace p3d {

    BloomEffect::BloomEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
    {   
        
        // Set RTT
        UseRTT(Tex1);
        
        // Create Fragment Shader
        FragmentShaderString =      
                                    #if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
                                    "precision mediump float;\n"
                                    #endif
                                    "const float BloomSize = 1.0/2048.0;"
                                    "uniform sampler2D uTex0;"
                                    "varying vec2 vTexcoord;"        
                                    "void main()"
                                    "{"
                                    "vec4 sum = vec4(0);"
                                    "vec2 texcoord = vTexcoord;"
                                    "int j;"
                                    "int i;"
                                    "for( i= -4 ;i < 4; i++)"
                                    "{"
                                            "for (j = -3; j < 3; j++)"
                                            "{"
                                                "sum += texture2D(uTex0, texcoord + vec2(j, i)*0.004) * 0.25;"
                                            "}"
                                    "}"
                                    "if (texture2D(uTex0, texcoord).r < 0.3)"
                                    "{"
                                    "gl_FragColor = sum*sum*0.012 + texture2D(uTex0, texcoord);"
                                    "}"
                                    "else"
                                    "{"
                                        "if (texture2D(uTex0, texcoord).r < 0.5)"
                                        "{"
                                            "gl_FragColor = sum*sum*0.009 + texture2D(uTex0, texcoord);"
                                        "}"
                                        "else"
                                        "{"
                                            "gl_FragColor = sum*sum*0.0075 + texture2D(uTex0, texcoord);"
                                        "}"
                                    "}"
                                "}";
        
        CompileShaders();
    }

    BloomEffect::~BloomEffect() {
    }

};