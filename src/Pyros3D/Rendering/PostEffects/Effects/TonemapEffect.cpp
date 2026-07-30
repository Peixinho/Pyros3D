//============================================================================
// Name        : TonemapEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Tonemap Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>

namespace p3d {

	TonemapEffect::TonemapEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

		// Create Fragment Shader
		//
		// ACES math is inlined directly in main(), not factored into its
		// own helper function - a real, reproduced bug found bringing this
		// effect up: a separate `vec3 ACESFilm(vec3 x) { ... }` function
		// (called from main(), matching how every other multi-step effect
		// in this codebase is written) made this draw's *sampler* read
		// (uTex0/RTT::Color) come back wrong (a uniform solid color across
		// the whole screen, not the real per-pixel captured image) even
		// though the function itself compiles and is otherwise correct
		// GLSL. Bisected by direct swap-testing on real hardware: a
		// UV-visualization shader with no helper function rendered
		// correctly, the exact same shader with the ACES computation
		// factored into a called helper function did not, and removing
		// only the function (inlining its body into main()) fixed it with
		// no other change. Not narrowed further than that (something in
		// this backend's shader reflection / descriptor setup appears to
		// mishandle a second function in an IEffect fragment shader
		// specifically) - SSAOEffect.cpp already has multiple non-main
		// helper functions and works, so it isn't "any extra function"
		// unconditionally, just something about this specific case not
		// pinned down. Inlining is a complete, safe workaround, not a
		// partial one - the output is identical either way.
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
								"IO_LOCATION(0) out vec4 FragColor;\n"
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"void main() {\n"
								"	vec3 x = texture_2D(uTex0, vTexcoord).rgb;\n"
								// ACES filmic fit (Narkowicz 2015) - compact,
								// no LUT, standard choice for real-time PBR.
								"	vec3 mapped = clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14), 0.0, 1.0);\n"
								"	mapped = pow(mapped, vec3(1.0/2.2));\n"
								"	FragColor = vec4(mapped, 1.0);\n"
								"}\n";

		CompileShaders();
	}

	TonemapEffect::~TonemapEffect() {}

};
