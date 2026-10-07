//============================================================================
// Name        : SharpUpscaleEffect.cpp
// Description : See SharpUpscaleEffect.h
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/SharpUpscaleEffect.h>
#include <algorithm>
#include <string>

namespace p3d {

	SharpUpscaleEffect::SharpUpscaleEffect(const uint32 Tex1, const uint32 Width, const uint32 Height, const f32 sharpness) : IEffect(Width, Height)
	{
		UseRTT(Tex1);

		const f32 s = std::max(0.f, std::min(1.f, sharpness));
		// (CAS: the weight of the four neighbours runs from -1/8 to -1/5)
		const std::string peak = std::to_string(-1.f / (8.f - 3.f * s));

		FragmentShaderString = std::string(
								"#define varying_in in\n"
								"#define varying_out out\n"
								"#define attribute_in in\n"
								"#define texture_2D texture\n"
								"#define texture_cube texture\n"
								#if defined(GLES3)
									"precision highp float;\n"
								#endif
								"#if defined(VULKAN)\n"
								"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
								"#define IO_LOCATION(n) layout(location = n)\n"
								"#else\n"
								"#define SAMPLER_BINDING(n)\n"
								"#define IO_LOCATION(n)\n"
								"#endif\n"
								"IO_LOCATION(0) out vec4 FragColor;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"void main(void) {\n"
								// The source's own size: nothing has to be told to this pass.
								"	vec2 size = vec2(textureSize(uTex0, 0));\n"
								"	vec2 inv = 1.0 / size;\n"
								// Catmull-Rom over 4x4 texels, as nine filtered taps.
								"	vec2 sp = vTexcoord * size;\n"
								"	vec2 tc = floor(sp - 0.5) + 0.5;\n"
								"	vec2 f = sp - tc;\n"
								"	vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));\n"
								"	vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);\n"
								"	vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));\n"
								"	vec2 w3 = f * f * (-0.5 + 0.5 * f);\n"
								"	vec2 w12 = w1 + w2;\n"
								"	vec2 t0 = (tc - 1.0) * inv;\n"
								"	vec2 t3 = (tc + 2.0) * inv;\n"
								"	vec2 t12 = (tc + w2 / w12) * inv;\n"
								"	vec3 c = texture_2D(uTex0, vec2(t0.x, t0.y)).rgb * (w0.x * w0.y)\n"
								"		+ texture_2D(uTex0, vec2(t12.x, t0.y)).rgb * (w12.x * w0.y)\n"
								"		+ texture_2D(uTex0, vec2(t3.x, t0.y)).rgb * (w3.x * w0.y)\n"
								"		+ texture_2D(uTex0, vec2(t0.x, t12.y)).rgb * (w0.x * w12.y)\n"
								"		+ texture_2D(uTex0, vec2(t12.x, t12.y)).rgb * (w12.x * w12.y)\n"
								"		+ texture_2D(uTex0, vec2(t3.x, t12.y)).rgb * (w3.x * w12.y)\n"
								"		+ texture_2D(uTex0, vec2(t0.x, t3.y)).rgb * (w0.x * w3.y)\n"
								"		+ texture_2D(uTex0, vec2(t12.x, t3.y)).rgb * (w12.x * w3.y)\n"
								"		+ texture_2D(uTex0, vec2(t3.x, t3.y)).rgb * (w3.x * w3.y);\n"
								// The neighbourhood, a source texel each way: what the
								// resample may not overshoot, and what the sharpening
								// reads its contrast from.
								"	vec4 m = texture_2D(uTex0, vTexcoord);\n"
								"	vec3 n = texture_2D(uTex0, vTexcoord + vec2(0.0, -inv.y)).rgb;\n"
								"	vec3 s = texture_2D(uTex0, vTexcoord + vec2(0.0, inv.y)).rgb;\n"
								"	vec3 e = texture_2D(uTex0, vTexcoord + vec2(inv.x, 0.0)).rgb;\n"
								"	vec3 w = texture_2D(uTex0, vTexcoord + vec2(-inv.x, 0.0)).rgb;\n"
								"	vec3 lo = min(m.rgb, min(min(n, s), min(e, w)));\n"
								"	vec3 hi = max(m.rgb, max(max(n, s), max(e, w)));\n"
								"	c = clamp(c, lo, hi);\n"
								// Contrast-adaptive sharpening: most where there is
								// little contrast to begin with, none where a pixel is
								// already at an extreme - so edges do not grow halos.
								"	vec3 amount = sqrt(clamp(min(lo, 2.0 - hi) / max(hi, vec3(1e-4)), 0.0, 1.0));\n"
								"	vec3 k = amount * (") + peak + std::string(");\n"
								"	vec3 sharp = (c + (n + s + e + w) * k) / (1.0 + 4.0 * k);\n"
								"	FragColor = vec4(max(sharp, vec3(0.0)), m.a);\n"
								"}\n");

		CompileShaders();
	}

	SharpUpscaleEffect::~SharpUpscaleEffect() {}

};
