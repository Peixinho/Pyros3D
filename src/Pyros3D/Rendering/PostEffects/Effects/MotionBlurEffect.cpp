//============================================================================
// Name        : MotionBlur.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : MotionBlur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.h>

namespace p3d {

    MotionBlurEffect::MotionBlurEffect(const uint32 Tex1, Texture* VelocityMap, const uint32 Width, const uint32 Height) : IEffect(Width, Height) 
    {

		// Set RTT
		UseRTT(Tex1);
		UseCustomTexture(VelocityMap);

		//Vec2 res = Vec2(Width, Height);
		f32 vel = 0.009f;
		//texResHandle = AddUniform(Uniform("uTexResolution", Uniforms::DataType::Vec2, &res));
		AddUniform(Uniform("uVelocityScale", Uniforms::DataType::Float, &vel));

		VertexShaderString =
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
								"varying_out vec2 vTexcoord;\n"
								"void main() {\n"
									"gl_Position = vec4(-1.0 + vec2((gl_VertexID & 1) << 2, (gl_VertexID & 2) << 1), 0.0, 1.0);\n"
									"vTexcoord = (gl_Position.xy+1.0)*0.5;\n"
								"}";

		// Create Fragment Shader
		FragmentShaderString =
								"#define MAX_SAMPLES 32\n"
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
									"vec4 FragColor;\n"
								#else
									"out vec4 FragColor;\n"
								#endif
								"varying_in vec2 vTexcoord;\n"
								"uniform sampler2D uTex0;\n"
								"uniform sampler2D uTex1;\n"
								"uniform vec2 uTexResolution;\n"
								"uniform float uVelocityScale;\n"
								"void main() {\n"
									"vec2 texelSize = 1.0 / vec2(textureSize(uTex0, 0));\n"
									"vec2 screenTexCoords = gl_FragCoord.xy * texelSize;\n"
									"vec2 velocity = texture(uTex1, screenTexCoords).rg;\n"
									"velocity *= uVelocityScale;\n"
									"float speed = length(velocity / texelSize);\n"
									"float nSamples = clamp(int(speed), 1, MAX_SAMPLES);\n"
									"vec4 oResult = texture(uTex0, screenTexCoords);\n"
									"for (int i = 1; i < nSamples; ++i) {\n"
									"      vec2 offset = velocity * (float(i) / float(nSamples - 1) - 0.5);\n"
									"      oResult += texture(uTex0, screenTexCoords + offset);\n"
									"}\n"
									"oResult /= float(nSamples);\n"
									"FragColor = oResult;\n"
									#if defined(GLES2)
									"gl_FragColor = FragColor;\n"
									#endif
								"}";

		CompileShaders();
    }

    MotionBlurEffect::~MotionBlurEffect() {
    }

};
