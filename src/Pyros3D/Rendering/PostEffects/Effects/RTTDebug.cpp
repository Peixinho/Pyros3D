//============================================================================
// Name        : RTT Debug.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RTT Debug Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>

namespace p3d {

    RTTDebug::RTTDebug(const uint32& Tex1) : IEffect() 
    {
        // Set RTT
        UseRTT(Tex1);
        
        // Create Fragment Shader
        FragmentShaderString =      "uniform sampler2D uTex0;"
                                                "varying vec2 vTexcoord;"
                                                "void main() {"
                                                    "gl_FragColor = texture2D(uTex0,vTexcoord);"
                                                "}";
        
        CompileShaders();    
    }

    RTTDebug::~RTTDebug() {
    }

}