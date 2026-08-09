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

		// See IEffect.h's comment on extraUniformsBinding - matches the
		// BlurSSAOParams block declared in FragmentShaderString below
		// (vec2 then float packs tight, std140 align 8 then 4).
		extraUniformsBinding = 25;
		extraUniformsBlockName = "BlurSSAOParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uTexResolution"] = 0;
		extraUniformOffsets["uIntensity"] = 8;

		// No VertexShaderString override: IEffect's constructor already
		// installed the shared full-screen-quad vertex shader, and this
		// effect adds nothing to it (no extra varyings, no vertex-stage
		// UBO - unlike BlurX/BlurYEffect, whose own copies genuinely do).
		// It used to carry a byte-identical private copy, which quietly
		// stopped tracking the shared one when that gained its Metal
		// vTexcoord flip (see IEffect.cpp's comment): this pass then
		// sampled its input upside down on Metal, and being an *odd*
		// number of mirroring hops in the ssao -> blur -> composite chain,
		// nothing downstream cancelled it - the finished AO term arrived
		// at the composite vertically mirrored and got multiplied over an
		// upright scene, which is the doubled/ghosted look the SSAO demo
		// had on Metal. Inheriting the shared shader is what keeps that
		// from silently happening again.

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
									// See SSAOEffect.cpp's identical comment
									// - binding 25 (not 24, SSAOEffect's own
									// - see IEffect.h's comment on
									// extraUniformsBinding for why these
									// must all be globally distinct).
									"#if defined(VULKAN)\n"
									"#define UBO_BINDING(n) layout(std140, binding = n)\n"
									"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
									"#define IO_LOCATION(n) layout(location = n)\n"
									"#else\n"
									"#define UBO_BINDING(n) layout(std140)\n"
									"#define SAMPLER_BINDING(n)\n"
									"#define IO_LOCATION(n)\n"
									"#endif\n"
									"IO_LOCATION(0) out vec4 FragColor;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"UBO_BINDING(25) uniform BlurSSAOParams {\n"
								"	vec2 uTexResolution;\n"
								"	float uIntensity;\n"
								"};\n"
								"const int blursize = 4;\n"
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
