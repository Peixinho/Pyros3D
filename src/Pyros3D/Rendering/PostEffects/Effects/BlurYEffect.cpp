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
								"varying_out vec2 vblurTexCoords[6];\n"
								"uniform float uTexResolution;\n"
								"void main() {\n"
									"gl_Position = vec4(-1.0 + vec2((gl_VertexID & 1) << 2, (gl_VertexID & 2) << 1), 0.0, 1.0);\n"
									"vTexcoord = (gl_Position.xy+1.0)*0.5;\n"
									"vblurTexCoords[0] = vTexcoord + vec2(0.0, -3.0/uTexResolution);\n"
									"vblurTexCoords[1] = vTexcoord + vec2(0.0, -2.0/uTexResolution);\n"
									"vblurTexCoords[2] = vTexcoord + vec2(0.0, -1.0/uTexResolution);\n"
									"vblurTexCoords[3] = vTexcoord + vec2(0.0, 1.0/uTexResolution);\n"
									"vblurTexCoords[4] = vTexcoord + vec2(0.0, 2.0/uTexResolution);\n"
									"vblurTexCoords[5] = vTexcoord + vec2(0.0, 3.0/uTexResolution);\n"
								"}";

		// Create Fragment Shader
		FragmentShaderString =
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
									"vec4 FragColor;"
								#else
									"out vec4 FragColor;"
								#endif
								"varying_in vec2 vTexcoord;\n"
								"uniform sampler2D uTex0;\n"
								"varying_in vec2 vblurTexCoords[6];\n"
								"void main() {\n"
									"FragColor = texture2D(uTex0, vblurTexCoords[ 0])*0.00598;\n"
									"FragColor += texture2D(uTex0, vblurTexCoords[ 1])*0.060626;\n"
									"FragColor += texture2D(uTex0, vblurTexCoords[ 2])*0.241843;\n"
									"FragColor += texture2D(uTex0, vTexcoord)*0.383103;\n"
									"FragColor += texture2D(uTex0, vblurTexCoords[ 3])*0.241843;\n"
									"FragColor += texture2D(uTex0, vblurTexCoords[ 4])*0.060626;\n"
									"FragColor += texture2D(uTex0, vblurTexCoords[ 5])*0.00598;\n"
									#if defined(GLES2)
									"gl_FragColor = FragColor;\n"
									#endif
								"}";

		CompileShaders();
	}

	BlurYEffect::~BlurYEffect() {
	}

};
