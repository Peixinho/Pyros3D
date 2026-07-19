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
		
		// Initialize intensity
		intensity = 1.0f;
		uIntensityHandle = AddUniform(Uniform("uIntensity", Uniforms::DataType::Float, &intensity));

		VertexShaderString =
									"#define varying_in in\n"
									"#define varying_out out\n"
									"#define attribute_in in\n"
									"#define texture_2D texture\n"
									"#define texture_cube texture\n"
									#if defined(GLES3)
										"precision mediump float;\n"
									#endif
								"varying_out vec2 vTexcoord;\n"
								"varying_out vec2 vblurTexCoords[6];\n"
								"void main() {\n"
									"gl_Position = vec4(-1.0 + vec2((gl_VertexID & 1) << 2, (gl_VertexID & 2) << 1), 0.0, 1.0);\n"
									"vTexcoord = (gl_Position.xy+1.0)*0.5;\n"
								"}";

		// Create Fragment Shader
		FragmentShaderString =
									"#define varying_in in\n"
									"#define varying_out out\n"
									"#define attribute_in in\n"
									"#define texture_2D texture\n"
									"#define texture_cube texture\n"
									#if defined(GLES3)
										"precision mediump float;\n"
									#endif
									"out vec4 FragColor;\n"
								"varying_in vec2 vTexcoord;\n"
								"uniform sampler2D uTex0;\n"
								"uniform float uIntensity;\n"
								"const int blursize = 4;\n"
								"uniform vec2 uTexResolution;\n"
								"void main() {\n"
									"vec2 texelSize = vec2(1.0,1.0) / uTexResolution;\n"
									"float result = 0.0;\n"
									"vec2 hlim = vec2(float(-blursize)*0.5 + 0.5);\n"
									"for (int i=0;i<blursize;i++) {\n"
										"for (int j=0;j<blursize;j++) {\n"
											"vec2 offset = (hlim + vec2(float(i), float(j))) * texelSize * uIntensity;\n"
											"result += texture_2D(uTex0, vTexcoord + offset).r;\n"
										"}\n"
									"}\n"
									"FragColor = vec4(result/float(blursize*blursize));\n"
								"}";

		CompileShaders();
	}

	BlurSSAOEffect::~BlurSSAOEffect() {
	}

	void BlurSSAOEffect::SetIntensity(const f32 intensity) {
		this->intensity = intensity;
		uIntensityHandle->SetValue(&this->intensity);
	}

};
