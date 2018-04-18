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
									#if defined(GLES2)
										"#define varying_in varying\n"
										"#define varying_out varying\n"
										"#define attribute_in attribute\n"
										"#define texture_2D texture2D\n"
										"#define texture_cube textureCube\n"
										"precision mediump float;"
									#else
										"#define varying_in in\n"
										"#define varying_out out\n"
										"#define attribute_in in\n"
										"#define texture_2D texture\n"
										"#define texture_cube texture\n"
										#if defined(GLES3)
											"precision mediump float;\n"
										#endif
									#endif
									#if defined(GLES2)
										"vec4 FragColor;"
									#else
										"out vec4 FragColor;"
									#endif
                                    "const float BloomSize = 1.0/2048.0;"
                                    "uniform sampler2D uTex0;"
                                    "varying_in vec2 vTexcoord;" 
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
													"sum += texture_2D(uTex0, texcoord + vec2(j, i)*0.004) * 0.25;"
												"}"
										"}"
										"if (texture_2D(uTex0, texcoord).r < 0.3)"
										"{"
											"FragColor = sum*sum*0.012 + texture_2D(uTex0, texcoord);"
										"}"
										"else"
										"{"
											"if (texture_2D(uTex0, texcoord).r < 0.5)"
											"{"
												"FragColor = sum*sum*0.009 + texture_2D(uTex0, texcoord);"
											"}"
											"else"
											"{"
												"FragColor = sum*sum*0.0075 + texture2D(uTex0, texcoord);"
											"}"
										"}"
										#if defined(GLES2)
										"gl_FragColor = FragColor;"
										#endif
									"}";
        
        CompileShaders();
    }

    BloomEffect::~BloomEffect() {
    }

};
