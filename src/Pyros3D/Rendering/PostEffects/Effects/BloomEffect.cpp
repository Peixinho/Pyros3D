//============================================================================
// Name        : BloomEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See BloomEffect.h.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>

namespace p3d {

	namespace {
		// Every post-effect shader in this directory repeats this preamble.
		// Vulkan needs a static binding on every sampler and a location on
		// every fragment output; GL needs neither and rejects the layout
		// qualifiers on samplers, hence the macro pair rather than one
		// spelling. VULKAN is predefined by shaderc for a Vulkan-target
		// compile (see SpirvShaderCompiler::Compile).
		const char* kPreamble =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
#if defined(GLES3)
			// highp: bloom works on values above 1.0 by definition - that is
			// the whole point of a threshold - and mediump is fp16 on a lot
			// of mobile/WebGL hardware, which bands badly up there.
			"precision highp float;\n"
#endif
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
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n";
	}

	BloomBrightPassEffect::BloomBrightPassEffect(const uint32 Tex1, const uint32 Width, const uint32 Height)
		: IEffect(Width, Height)
	{
		UseRTT(Tex1);

		FragmentShaderString = std::string(kPreamble) +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			// 27, chosen from the free range in IEffect.h's binding registry
			// (25-31 are the post effects'; 25 BlurSSAO, 26 MotionBlur,
			// 28 BlurX, 29 BlurY, 30 DepthOfField). Binding points are one
			// global namespace shared with PyrosShader.glsl, not per-shader.
			"UBO_BINDING(27) uniform BloomBrightPassParams {\n"
			"	float uThreshold;\n"
			"	float uKnee;\n"
			"};\n"
			"void main() {\n"
			"	vec3 c = texture_2D(uTex0, vTexcoord).rgb;\n"
			// Rec. 709 luma. The old pass branched on .r alone, so a
			// saturated blue light never bloomed and a dull red one did.
			"	float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
			// Quadratic knee: nothing below (threshold - knee), a smooth
			// ramp across the knee, linear above it. A hard step here is
			// what makes bloom flicker on slowly brightening surfaces.
			"	float k = max(uKnee, 0.0001);\n"
			"	float soft = clamp((luma - uThreshold + k) / (2.0 * k), 0.0, 1.0);\n"
			"	float weight = max(luma - uThreshold, k * soft * soft) / max(luma, 0.0001);\n"
			"	FragColor = vec4(c * weight, 1.0);\n"
			"}";

		CompileShaders();

		threshold = 0.8f;
		knee = 0.35f;
		thresholdHandle = AddUniform(Uniform("uThreshold", Uniforms::DataType::Float, &threshold));
		kneeHandle = AddUniform(Uniform("uKnee", Uniforms::DataType::Float, &knee));

		extraUniformsBinding = 27;
		extraUniformsBlockName = "BloomBrightPassParams";
		// std140 rounds the block up to a vec4 even though two floats fit in
		// eight bytes; a short buffer is a dropped draw on WebGL2.
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uThreshold"] = 0;
		extraUniformOffsets["uKnee"] = 4;
	}

	void BloomBrightPassEffect::SetThreshold(const f32 &v) { threshold = v; thresholdHandle->SetValue(&threshold); }
	void BloomBrightPassEffect::SetKnee(const f32 &v) { knee = v; kneeHandle->SetValue(&knee); }

	BloomBrightPassEffect::~BloomBrightPassEffect() {}

	BloomCompositeEffect::BloomCompositeEffect(Texture* base, const uint32 Width, const uint32 Height)
		: IEffect(Width, Height)
	{
		// Order matters: uTex0/uTex1 are named after the order these are
		// declared, so the base has to come first in both constructors.
		UseCustomTexture(base);
		UseRTT(RTT::LastRTT);
		Build(Width, Height);
	}

	BloomCompositeEffect::BloomCompositeEffect(const uint32 baseRTT, const uint32 Width, const uint32 Height)
		: IEffect(Width, Height)
	{
		UseRTT(baseRTT);
		UseRTT(RTT::LastRTT);
		Build(Width, Height);
	}

	void BloomCompositeEffect::Build(const uint32 Width, const uint32 Height)
	{
		FragmentShaderString = std::string(kPreamble) +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
			"UBO_BINDING(31) uniform BloomCompositeParams {\n"
			"	float uIntensity;\n"
			"};\n"
			"void main() {\n"
			"	vec4 base = texture_2D(uTex0, vTexcoord);\n"
			"	vec3 bloom = texture_2D(uTex1, vTexcoord).rgb;\n"
			// Added, not screened or squared. The old pass squared the
			// blurred sum, which meant the bloom's own falloff was the
			// square of the blur kernel and its brightness scaled
			// quadratically with the source - a light twice as bright
			// bloomed four times as hard, so there was no setting that
			// worked for both a lamp and a sky.
			"	FragColor = vec4(base.rgb + bloom * uIntensity, base.a);\n"
			"}";

		CompileShaders();

		intensity = 1.0f;
		intensityHandle = AddUniform(Uniform("uIntensity", Uniforms::DataType::Float, &intensity));

		extraUniformsBinding = 31;
		extraUniformsBlockName = "BloomCompositeParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uIntensity"] = 0;
	}

	void BloomCompositeEffect::SetIntensity(const f32 &v) { intensity = v; intensityHandle->SetValue(&intensity); }

	BloomCompositeEffect::~BloomCompositeEffect() {}

};
