//============================================================================
// Name        : NoEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : No Effect - Simply Show Color Buffer in a Quad
//============================================================================

#include "NoEffect.h"

namespace Fishy3D {

    NoEffect::NoEffect(const unsigned &Tex1) : IEffect() 
    {
     
        UseRTT(Tex1);
        
        // Create Fragment Shader
        FragmentShaderString =      "uniform sampler2D uTex0;"
                                                "varying vec2 vTexcoord;"
                                                "void main() {"
                                                    "gl_FragColor = texture2D(uTex0,vTexcoord);"
                                                "}";
        
        CompileShaders();
    }

    NoEffect::~NoEffect() {
    }

};