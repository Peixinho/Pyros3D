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
								#if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
								"precision mediump float;\n"
								#endif
								"uniform sampler2D uTex0;"
								"uniform sampler2D uTex1;"
								"varying vec2 vTexcoord;"
								"void main() {"
									"if (vTexcoord.x<0.5) gl_FragColor = texture2D(uTex0,vTexcoord);\n"
									"else gl_FragColor = texture2D(uTex1,vTexcoord);"
								"}";
							
        CompileShaders();    
    }

    RTTDebug::~RTTDebug() {}

}