//============================================================================
// Name        : RTT Debug.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RTT Debug Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>

namespace p3d {

    RTTDebug::RTTDebug(const uint32 Tex1,const uint32 Tex2, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
    {
        // Set RTT
        UseRTT(Tex1);
		UseRTT(Tex2);
        
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
								"uniform sampler2D uTex0;"
								"uniform sampler2D uTex1;"
								"varying_in vec2 vTexcoord;"
								"void main() {"
									"if (vTexcoord.x<0.5) FragColor = texture_2D(uTex0,vTexcoord);\n"
									"else FragColor = texture_2D(uTex1,vTexcoord);"
									#if defined(GLES2)
									"gl_FragColor = FragColor;\n"
									#endif
								"}";
							
        CompileShaders();    
    }

    RTTDebug::~RTTDebug() {}

}
