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
		// Depth, so the blur can stop at silhouettes - see the shader.
		UseRTT(RTT::Depth);

		texRes.Name = "uTexResolution";
		texRes.Type = Uniforms::DataType::Vec2;
		texRes.Usage = Uniforms::PostEffects::Other;
		Vec2 res = Vec2(Width, Height);
		texRes.SetValue(&res);
		AddUniform(texRes);
		
		// Initialize intensity
		intensity = 1.0f;
		uIntensityHandle = AddUniform(Uniform("uIntensity", Uniforms::DataType::Float, &intensity));
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));

		// See IEffect.h's comment on extraUniformsBinding - matches the
		// BlurSSAOParams block declared in FragmentShaderString below
		// (vec2 then float packs tight, std140 align 8 then 4; the second
		// vec2 rounds up to the next 8-byte boundary).
		// 36, not 25: 25 is BIND_DDGIUniforms (PyrosShader.glsl), and binding
		// points are one global registry - the same collision SSAOEffect's
		// comment on 24 describes, waiting for a scene with GI and SSAO.
		extraUniformsBinding = 36;
		extraUniformsBlockName = "BlurSSAOParams";
		extraUniformsSize = 24;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uTexResolution"] = 0;
		extraUniformOffsets["uIntensity"] = 8;
		extraUniformOffsets["uNearFar"] = 16;

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
									// - binding 36 (not 43, SSAOEffect's own
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
								"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
								"UBO_BINDING(36) uniform BlurSSAOParams {\n"
								"	vec2 uTexResolution;\n"
								"	float uIntensity;\n"
								"	vec2 uNearFar;\n"
								"};\n"
								"float LinearDepth(vec2 uv) {\n"
								"	float z = texture_2D(uTex1, uv).r;\n"
								"	return uNearFar.x * uNearFar.y / (uNearFar.y - z * (uNearFar.y - uNearFar.x));\n"
								"}\n"
								// Bilateral: a tap whose depth differs from the centre's by
								// more than 5% of that depth is on another surface and gets
								// no weight. A plain box averaged the dark AO of a crevice
								// straight onto the object in front of it, which drew a soft
								// dark halo around every silhouette. 4x4 still, to cancel
								// SSAOEffect's 4x4 rotation tile exactly.
								"const int blursize = 4;\n"
								"void main() {\n"
								"	vec2 texelSize = vec2(1.0,1.0) / uTexResolution;\n"
								"	float centerZ = LinearDepth(vTexcoord);\n"
								"	float result = 0.0;\n"
								"	float wsum = 0.0;\n"
								"	vec2 hlim = vec2(float(-blursize)*0.5 + 0.5);\n"
								"	for (int i=0;i<blursize;i++) {\n"
								"		for (int j=0;j<blursize;j++) {\n"
								"			vec2 offset = (hlim + vec2(float(i), float(j))) * texelSize * uIntensity;\n"
								"			float w = max(0.0, 1.0 - abs(LinearDepth(vTexcoord + offset) - centerZ) / (0.05 * centerZ));\n"
								"			result += texture_2D(uTex0, vTexcoord + offset).r * w;\n"
								"			wsum += w;\n"
								"		}\n"
								"	}\n"
								"	FragColor = vec4(wsum > 0.0 ? result / wsum : texture_2D(uTex0, vTexcoord).r);\n"
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
