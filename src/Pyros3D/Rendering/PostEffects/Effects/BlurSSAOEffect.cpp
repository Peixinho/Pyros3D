//============================================================================
// Name        : BlurSSAEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.h>

namespace p3d {

	BlurSSAOEffect::BlurSSAOEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

		texRes.Name = "uTexResolution";
		texRes.Type = Uniforms::DataType::Vec2;
		texRes.Usage = Uniforms::PostEffects::Other;
		Vec2 res = Vec2(Width, Height);
		texRes.SetValue(&res);
		AddUniform(texRes);

		VertexShaderString =
								#if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
								"precision mediump float;\n"
								#endif
								"attribute vec3 aPosition;\n"
								"attribute vec2 aTexcoord;\n"
								"varying vec2 vTexcoord;\n"
								"varying vec2 vblurTexCoords[6];\n"
								"uniform float uTexResolution;\n"
								"void main() {\n"
									"gl_Position = vec4(aPosition,1.0);\n"
									"vTexcoord = aTexcoord;\n"
								"}";

		// Create Fragment Shader
		FragmentShaderString =
								#if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
								"precision mediump float;\n"
								#endif
								"varying vec2 vTexcoord;\n"
								"uniform sampler2D uTex0;\n"
								"const int blursize = 4;\n"
								"uniform vec2 uTexResolution;\n"
								"void main() {\n"
									"vec2 texelSize = 1.0 / uTexResolution;\n"
									"float result = 0.0;\n"
									"vec2 hlim = vec2(float(-blursize)*0.5 + 0.5);\n"
									"for (int i=0;i<blursize;i++) {\n"
										"for (int j=0;j<blursize;j++) {\n"
											"vec2 offset = (hlim + vec2(float(i), float(j))) * texelSize;\n"
											"result += texture2D(uTex0, vTexcoord + offset).r;\n"
										"}\n"
									"}\n"
									"gl_FragColor = vec4(result/float(blursize*blursize));\n"
								"}";

		CompileShaders();
	}

	BlurSSAOEffect::~BlurSSAOEffect() {
	}

};