//============================================================================
// Name        : TonemapEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Tonemap Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <cmath>

namespace p3d {

	TonemapEffect::TonemapEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

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
								"#define UBO_BINDING(n) layout(std140, binding = n)\n"
								"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
								"#define IO_LOCATION(n) layout(location = n)\n"
								"#else\n"
								"#define UBO_BINDING(n) layout(std140)\n"
								"#define SAMPLER_BINDING(n)\n"
								"#define IO_LOCATION(n)\n"
								"#endif\n"
								// x the display's headroom over SDR white (1: SDR, ACES as ever), y z the HDR
								// curve's b c (worked out on the CPU from x - see PreDraw)
								"UBO_BINDING(49) uniform TonemapParams { vec4 uHDR; };\n"
								"IO_LOCATION(0) out vec4 FragColor;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								// ACES filmic fit (Narkowicz 2015) - compact,
								// no LUT, standard choice for real-time PBR.
								"vec3 ACESFilm(vec3 x) {\n"
								"	float a = 2.51;\n"
								"	float b = 0.03;\n"
								"	float c = 2.43;\n"
								"	float d = 0.59;\n"
								"	float e = 0.14;\n"
								"	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);\n"
								"}\n"
								// HDR output: Lottes' curve (2016) - mid grey where ACES puts it, highlights rolling
								// off to the display's peak instead of to 1.0, in units of SDR white
								"vec3 LottesHDR(vec3 x) {\n"
								"	const float a = 1.6;\n"
								"	const float d = 0.977;\n"
								"	vec3 p = pow(max(x, vec3(0.0)), vec3(a));\n"
								"	return uHDR.x * p / (pow(max(x, vec3(0.0)), vec3(a * d)) * uHDR.y + uHDR.z);\n"
								"}\n"
								"void main() {\n"
								"	vec3 x = texture_2D(uTex0, vTexcoord).rgb;\n"
								"	vec3 mapped = uHDR.x > 1.0001 ? clamp(LottesHDR(x), vec3(0.0), vec3(uHDR.x)) : ACESFilm(x);\n"
								// (the same encode either way: in extended sRGB, past 1.0 is brighter than SDR white)
								"	mapped = pow(mapped, vec3(1.0/2.2));\n"
								"	FragColor = vec4(mapped, 1.0);\n"
								"}\n";

		CompileShaders();

		// (half floats, not bytes: what follows the tonemap - the antialiasing - reads this, and in HDR
		// output it holds values past 1.0 that a byte would clip back to SDR white)
		UseHDRAttachment();

		hdr = Vec4(1.f, 1.f, 0.f, 0.f);
		hdrHandle = AddUniform(Uniform("uHDR", Uniforms::DataType::Vec4, &hdr));
		// (a block of its own shape and binding - see IEffect::extraUniformsBinding)
		extraUniformsBinding = 49;
		extraUniformsBlockName = "TonemapParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uHDR"] = 0;
	}

	// The display's headroom this frame (it moves with the brightness), and the curve fitted to it:
	// scene values up to kSceneMax reach the display's peak, and 0.18 lands where ACES puts it.
	void TonemapEffect::PreDraw()
	{
		const f32 H = IsActiveRenderDeviceSet() ? GetActiveRenderDevice().GetHDRHeadroom() : 1.f;
		if (H <= 1.0001f) { hdr = Vec4(1.f, 1.f, 0.f, 0.f); hdrHandle->SetValue(&hdr); return; }
		const f32 a = 1.6f, d = 0.977f, kSceneMax = 16.f, midIn = 0.18f;
		const f32 midOut = 0.267f / H;                       // (ACES(0.18), as a share of the peak)
		const f32 ma = powf(midIn, a), mad = powf(midIn, a * d), ha = powf(kSceneMax, a), had = powf(kSceneMax, a * d);
		const f32 b = (-ma + ha * midOut) / ((had - mad) * midOut);
		const f32 c = (had * ma - ha * mad * midOut) / ((had - mad) * midOut);
		hdr = Vec4(H, b, c, 0.f);
		hdrHandle->SetValue(&hdr);
	}

	TonemapEffect::~TonemapEffect() {}

};
