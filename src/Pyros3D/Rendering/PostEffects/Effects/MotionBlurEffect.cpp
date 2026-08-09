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

		cfps = 60.0f;
		tfps = 60.0f;
		strength = 3.25f;
		f32 vel = strength * (cfps / tfps);
		velHandle = AddUniform(Uniform("uVelocityScale", Uniforms::DataType::Float, &vel));

		// Must stay in lockstep with the fragment shader's own `#if
		// defined(VULKAN)` below, and those two macros are NOT the same
		// thing: VULKAN_BACKEND/METAL_BACKEND are C++ build flags
		// (cmake/PyrosBackend.cmake), while VULKAN is predefined by
		// shaderc for *any* SPIR-V target - which includes the Metal
		// backend, since it compiles GLSL through shaderc before handing
		// the SPIR-V to SPIRV-Cross. Guarding this half on VULKAN_BACKEND
		// alone (as it did, from before the Metal backend existed) left
		// Metal taking the UBO branch in the shader and the loose-uniform
		// branch here: nothing ever created or filled MotionBlurParams, so
		// uVelocityScale.x read back as 0, velocity scaled to nothing and
		// nSamples collapsed to 1 - the effect ran every frame as an exact
		// pass-through. The velocity map itself was fine; only the scale
		// was missing (confirmed by rendering both to screen).
#if defined(VULKAN_BACKEND) || defined(METAL_BACKEND)
		// Vulkan/Metal reject loose floats - deliver scale via UBO (vec4.x).
		extraUniformsBinding = 26;
		extraUniformsBlockName = "MotionBlurParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uVelocityScale"] = 0;
#else
		// GL: plain uniform + SendUniform. Avoids macOS driver std140
		// packing mismatches that left the scale at 0 (identity blur).
		extraUniformsBinding = 0;
#endif

		// Fragment: VULKAN (shaderc) uses a UBO; GL uses a loose float.
		// Sample with vTexcoord like every other post effect.
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
								"#if defined(VULKAN)\n"
								"UBO_BINDING(26) uniform MotionBlurParams {\n"
								"	vec4 uVelocityScale;\n"
								"};\n"
								"#else\n"
								"uniform float uVelocityScale;\n"
								"#endif\n"
								"void main() {\n"
									"vec2 texelSize = 1.0 / vec2(textureSize(uTex0, 0));\n"
									"vec2 screenTexCoords = vTexcoord;\n"
									"vec2 velocity = texture(uTex1, screenTexCoords).rg;\n"
									"#if defined(VULKAN)\n"
									"velocity *= uVelocityScale.x;\n"
									"#else\n"
									"velocity *= uVelocityScale;\n"
									"#endif\n"
									"float speed = length(velocity / texelSize);\n"
									"int nSamples = int(clamp(speed, 1.0, float(MAX_SAMPLES)));\n"
									"vec4 oResult = texture(uTex0, screenTexCoords);\n"
									"for (int i = 1; i < nSamples; ++i) {\n"
									"      vec2 offset = velocity * (float(i) / float(max(nSamples - 1, 1)) - 0.5);\n"
									"      oResult += texture(uTex0, screenTexCoords + offset);\n"
									"}\n"
									"FragColor = oResult / float(nSamples);\n"
								"}";

		CompileShaders();
    }

    MotionBlurEffect::~MotionBlurEffect() {
    }

    void MotionBlurEffect::SetCurrentFPS(const f32 &currentfps) {
	    this->cfps = currentfps > 1.0f ? currentfps : 1.0f;
	    f32 v = strength * (cfps / tfps);
	    velHandle->SetValue(&v);
    }

    void MotionBlurEffect::SetTargetFPS(const f32 &targetfps) {
	    this->tfps = targetfps > 1.0f ? targetfps : 1.0f;
	    f32 v = strength * (cfps / tfps);
	    velHandle->SetValue(&v);
    }

};
