//============================================================================
// Name        : DepthOfFieldEffect.cpp
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/DepthOfFieldEffect.h>

namespace p3d {

	DepthOfFieldEffect::DepthOfFieldEffect(Texture* texture1, Texture* texture2, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseCustomTexture(texture1);
		UseCustomTexture(texture2);
		UseRTT(RTT::Color);
		UseRTT(RTT::Depth);

		FragmentShaderString =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
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
			"float DecodeNativeDepth(float native_z, vec4 z_info_local)\n"
			"{\n"
			"return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);\n"
			"}\n"
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
			"SAMPLER_BINDING(2) uniform sampler2D uTex2;\n"
			"SAMPLER_BINDING(3) uniform sampler2D uTex3;\n"
			"UBO_BINDING(30) uniform DepthOfFieldParams {\n"
			"	vec2 uNearFar;\n"
			"	float uFocalPosition;\n"
			"	float uFocalRange;\n"
			"	float uRatioL;\n"
			"	float uRatioH;\n"
			"};\n"
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
			"void main() {\n"
			"float ratioL = uRatioL;\n"
			"float ratioH = uRatioH;\n"
			"float focalPosition = uFocalPosition;\n"
			"float focalRange = uFocalRange;\n"
			"vec4 z_info_local = vec4(uNearFar.x,uNearFar.y,uNearFar.x*uNearFar.y,uNearFar.x-uNearFar.y);\n"
			"float depth = texture_2D(uTex3, vTexcoord).x;\n"
			"float linearDepth = DecodeNativeDepth(depth, z_info_local);\n"
			"float ratio = clamp(abs(focalPosition-linearDepth)-focalRange, 0.0, ratioL);\n"
			"if (ratio < 0.4) FragColor = mix(texture_2D(uTex2, vTexcoord), texture_2D(uTex1, vTexcoord), ratio / (ratioL - ratioH));\n"
			"else FragColor =  mix(texture_2D(uTex1, vTexcoord), texture_2D(uTex0, vTexcoord), (ratio-ratioH) / (ratioL - ratioH));\n"
			"}";

		CompileShaders();

		Uniform nearFarPlane;
		nearFarPlane.Name = "uNearFar";
		nearFarPlane.Type = Uniforms::DataType::Vec2;
		nearFarPlane.Usage = Uniforms::PostEffects::NearFarPlane;
		AddUniform(nearFarPlane);

		fPosition = 20.f;
		fRange = 2.f;
		rL = 3.1f;
		rH = 1.0f;

		focalPositionHandle = AddUniform(Uniform("uFocalPosition", Uniforms::DataType::Float, &fPosition));
		focalRangeHandle = AddUniform(Uniform("uFocalRange", Uniforms::DataType::Float, &fRange));
		ratioLowHandle = AddUniform(Uniform("uRatioL", Uniforms::DataType::Float, &rL));
		ratioHighHandle = AddUniform(Uniform("uRatioH", Uniforms::DataType::Float, &rH));

		extraUniformsBinding = 30;
		extraUniformsBlockName = "DepthOfFieldParams";
		extraUniformsSize = 24;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uNearFar"] = 0;
		extraUniformOffsets["uFocalPosition"] = 8;
		extraUniformOffsets["uFocalRange"] = 12;
		extraUniformOffsets["uRatioL"] = 16;
		extraUniformOffsets["uRatioH"] = 20;
	}

	void DepthOfFieldEffect::SetFocalPosition(const f32 &v) { fPosition = v; focalPositionHandle->SetValue(&fPosition); }
	void DepthOfFieldEffect::SetFocalRange(const f32 &v)    { fRange = v;    focalRangeHandle->SetValue(&fRange); }
	void DepthOfFieldEffect::SetRatioLow(const f32 &v)      { rL = v;        ratioLowHandle->SetValue(&rL); }
	void DepthOfFieldEffect::SetRatioHigh(const f32 &v)     { rH = v;        ratioHighHandle->SetValue(&rH); }

	DepthOfFieldEffect::~DepthOfFieldEffect() {}

}
