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
                                #if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
                                "precision mediump float;\n"
                                #endif
                                "uniform sampler2D uTex0;\n"
                                "varying vec2 vTexcoord;"
                                "void main(void) {\n"
                                    "gl_FragColor = texture2D(uTex0, vTexcoord);\n"
                                "}\n";
        
        CompileShaders();
    }

    ResizeEffect::~ResizeEffect() {
    }

};