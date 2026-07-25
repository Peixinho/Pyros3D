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
		f32 vel =  3.25f;
		//texResHandle = AddUniform(Uniform("uTexResolution", Uniforms::DataType::Vec2, &res));
		velHandle = AddUniform(Uniform("uVelocityScale", Uniforms::DataType::Float, &vel));

		// See SSAOEffect.cpp's comment on extraUniformsBinding - matches
		// the MotionBlurParams block declared in FragmentShaderString
		// below (vec2 then float packs tight, std140 align 8 then 4).
		// uTexResolution is never actually AddUniform()'d (see the
		// commented-out line above - a pre-existing gap on GL too, not
		// something this fix changes), so it just stays 0 in the UBO,
		// matching current GL behavior exactly.
		extraUniformsBinding = 26;
		extraUniformsBlockName = "MotionBlurParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uTexResolution"] = 0;
		extraUniformOffsets["uVelocityScale"] = 8;

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
									"#define IO_LOCATION(n) layout(location = n)\n"
									"#else\n"
									"#define IO_LOCATION(n)\n"
									"#endif\n"
								"IO_LOCATION(0) varying_out vec2 vTexcoord;\n"
								"void main() {\n"
									"gl_Position = vec4(-1.0 + vec2((gl_VertexID & 1) << 2, (gl_VertexID & 2) << 1), 0.0, 1.0);\n"
									"vTexcoord = (gl_Position.xy+1.0)*0.5;\n"
								"}";

		// Create Fragment Shader
		FragmentShaderString =
								"#define MAX_SAMPLES 32\n"
									"#define varying_in in\n"
									"#define varying_out out\n"
									"#define attribute_in in\n"
									"#define texture_2D texture\n"
									"#define texture_cube texture\n"
									#if defined(GLES3)
										"precision mediump float;\n"
									#endif
									// See SSAOEffect.cpp's identical comment.
									"#if defined(VULKAN)\n"
									"#define UBO_BINDING(n) layout(std140, binding = n)\n"
									"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
									"#define IO_LOCATION(n) layout(location = n)\n"
									"#else\n"
									"#define UBO_BINDING(n)\n"
									"#define SAMPLER_BINDING(n)\n"
									"#define IO_LOCATION(n)\n"
									"#endif\n"
									"IO_LOCATION(0) out vec4 FragColor;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
								"UBO_BINDING(26) uniform MotionBlurParams {\n"
								"	vec2 uTexResolution;\n"
								"	float uVelocityScale;\n"
								"};\n"
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
									"FragColor = oResult / float(nSamples);\n"
								"}";

		CompileShaders();
    }

    MotionBlurEffect::~MotionBlurEffect() {
    }

    void MotionBlurEffect::SetCurrentFPS(const f32 &currentfps) {
	    this->cfps = currentfps;
	    f32 v = cfps/tfps;
	    velHandle->SetValue(&v);
    }

    void MotionBlurEffect::SetTargetFPS(const f32 &targetfps) {
	    this->tfps = targetfps;
	    f32 v = cfps/tfps;
	    velHandle->SetValue(&v);
    }

};
