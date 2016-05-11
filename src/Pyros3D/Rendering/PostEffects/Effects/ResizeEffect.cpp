//============================================================================
// Name        : Resize.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Resize Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>

namespace p3d {

    ResizeEffect::ResizeEffect(const uint32 Tex1, const uint32 width, const uint32 height) : IEffect() 
    {

        // Set RTT
        UseRTT(Tex1);
        
		Width = width;
		Height = height;
		customDimensions = true;

        // Create Fragment Shader
		FragmentShaderString =
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