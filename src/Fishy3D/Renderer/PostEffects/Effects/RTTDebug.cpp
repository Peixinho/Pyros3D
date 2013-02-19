//============================================================================
// Name        : RTT Debug.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RTT Debug Effect
//============================================================================


#include "RTTDebug.h"
#include "IEffect.h"

namespace Fishy3D {

    RTTDebug::RTTDebug(const unsigned& Tex1, const unsigned& Tex2, const unsigned& Tex3, const unsigned& Tex4) : IEffect() 
    {
        // Set RTT
        UseRTT(Tex1);
        UseRTT(Tex2);
        UseRTT(Tex3);
        UseRTT(Tex4);
        
        // Create Fragment Shader
        FragmentShaderString =      "uniform sampler2D uTex0,uTex1,uTex2, uTex3;"
                                                "varying vec2 vTexcoord;"
                                                "void main() {"
                                                    "if (vTexcoord.x<0.5 && vTexcoord.y<0.5) gl_FragColor = texture2D(uTex0,vTexcoord);"
                                                    "if (vTexcoord.x<0.5 && vTexcoord.y>0.5) gl_FragColor = vec4(texture2D(uTex1,vTexcoord).xyz,1.0);"
                                                    "if (vTexcoord.x>0.5 && vTexcoord.y<0.5) gl_FragColor = texture2D(uTex2,vTexcoord);"
                                                    "if (vTexcoord.x>0.5 && vTexcoord.y>0.5) gl_FragColor = vec4(texture2D(uTex3,vTexcoord).w);"
                                                "}";
        
        CompileShaders();    
    }

    RTTDebug::~RTTDebug() {
    }

}