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

		// See BlurXEffect.cpp's identical comment - binding 29, not 28
		// (BlurXEffect's own - see SSAOEffect.cpp's comment on
		// extraUniformsBinding for why these must all be distinct).
		extraUniformsBinding = 29;
		extraUniformsSize = 4;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uTexResolution"] = 0;

		VertexShaderString =
									"#define varying_in in\n"
									"#define varying_out out\n"
									"#define attribute_in in\n"
									"#define texture_2D texture\n"
									"#define texture_cube texture\n"
									#if defined(GLES3)
										"precision mediump float;\n"
									#endif
									"#if defined(VULKAN)\n"
									"#define gl_VertexID gl_VertexIndex\n"
									"#define UBO_BINDING(n) layout(std140, binding = n)\n"
									"#define IO_LOCATION(n) layout(location = n)\n"
									"#else\n"
									"#define UBO_BINDING(n)\n"
									"#define IO_LOCATION(n)\n"
									"#endif\n"
								"IO_LOCATION(0) varying_out vec2 vTexcoord;\n"
								"IO_LOCATION(1) varying_out vec2 vblurTexCoords[6];\n"
								"UBO_BINDING(29) uniform BlurYParams {\n"
								"	float uTexResolution;\n"
								"};\n"
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
									"#define varying_in in\n"
									"#define varying_out out\n"
									"#define attribute_in in\n"
									"#define texture_2D texture\n"
									"#define texture_cube texture\n"
									#if defined(GLES3)
										"precision mediump float;\n"
									#endif
									"#if defined(VULKAN)\n"
									"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
									"#define IO_LOCATION(n) layout(location = n)\n"
									"#else\n"
									"#define SAMPLER_BINDING(n)\n"
									"#define IO_LOCATION(n)\n"
									"#endif\n"
									"IO_LOCATION(0) out vec4 FragColor;"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"IO_LOCATION(1) varying_in vec2 vblurTexCoords[6];\n"
								"void main() {\n"
									"FragColor = texture_2D(uTex0, vblurTexCoords[ 0])*0.00598;\n"
									"FragColor += texture_2D(uTex0, vblurTexCoords[ 1])*0.060626;\n"
									"FragColor += texture_2D(uTex0, vblurTexCoords[ 2])*0.241843;\n"
									"FragColor += texture_2D(uTex0, vTexcoord)*0.383103;\n"
									"FragColor += texture_2D(uTex0, vblurTexCoords[ 3])*0.241843;\n"
									"FragColor += texture_2D(uTex0, vblurTexCoords[ 4])*0.060626;\n"
									"FragColor += texture_2D(uTex0, vblurTexCoords[ 5])*0.00598;\n"
								"}";

		CompileShaders();
	}

	BlurYEffect::~BlurYEffect() {
	}

};
