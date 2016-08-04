//============================================================================
// Name        : BlurYEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>

namespace p3d {

	BlurYEffect::BlurYEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

		texRes.Name = "uTexResolution";
		texRes.Type = Uniforms::DataType::Float;
		texRes.Usage = Uniforms::PostEffects::Other;
		f32 res = (f32)Height;
		texRes.SetValue(&res);
		AddUniform(texRes);

		VertexShaderString =
								"attribute vec3 aPosition;\n"
								"attribute vec2 aTexcoord;\n"
								"varying vec2 vTexcoord;\n"
								"varying vec2 vblurTexCoords[6];\n"
								"uniform float uTexResolution;\n"
								"void main() {\n"
									"gl_Position = vec4(aPosition,1.0);\n"
									"vTexcoord = aTexcoord;\n"
									"vblurTexCoords[0] = vTexcoord + vec2(0.0, -3.0/uTexResolution);\n"
									"vblurTexCoords[1] = vTexcoord + vec2(0.0, -2.0/uTexResolution);\n"
									"vblurTexCoords[2] = vTexcoord + vec2(0.0, -1.0/uTexResolution);\n"
									"vblurTexCoords[3] = vTexcoord + vec2(0.0, 1.0/uTexResolution);\n"
									"vblurTexCoords[4] = vTexcoord + vec2(0.0, 2.0/uTexResolution);\n"
									"vblurTexCoords[5] = vTexcoord + vec2(0.0, 3.0/uTexResolution);\n"
								"}";

		// Create Fragment Shader
		FragmentShaderString =
								"varying vec2 vTexcoord;\n"
								"uniform sampler2D uTex0;\n"
								"varying vec2 vblurTexCoords[6];\n"
								"void main() {\n"
									"gl_FragColor += texture2D(uTex0, vblurTexCoords[ 0])*0.00598;\n"
									"gl_FragColor += texture2D(uTex0, vblurTexCoords[ 1])*0.060626;\n"
									"gl_FragColor += texture2D(uTex0, vblurTexCoords[ 2])*0.241843;\n"
									"gl_FragColor += texture2D(uTex0, vTexcoord)*0.383103;\n"
									"gl_FragColor += texture2D(uTex0, vblurTexCoords[ 3])*0.241843;\n"
									"gl_FragColor += texture2D(uTex0, vblurTexCoords[ 4])*0.060626;\n"
									"gl_FragColor += texture2D(uTex0, vblurTexCoords[ 5])*0.00598;\n"
								"}";

		CompileShaders();
	}

	BlurYEffect::~BlurYEffect() {
	}

};