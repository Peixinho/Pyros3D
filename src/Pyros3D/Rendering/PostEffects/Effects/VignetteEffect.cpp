//============================================================================
// Name        : VIGNETTE.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Blur Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/VignetteEffect.h>

namespace p3d {

    VignetteEffect::VignetteEffect(const uint32 Tex1, const uint32 Width, const uint32 Height, const f32 radius, const f32 softness) : IEffect(Width, Height)
    {

		// All three of these were miswired, and the effect did nothing at all
		// as a result. Uniform's three-argument form is (name, USAGE, type),
		// so passing a DataType where the usage goes made uResolution ask for
		// the projection matrix (DataType::Vec2 == PostEffects::
		// ProjectionFromScene) and made uRADIUS ask for the screen dimensions
		// (DataType::Float == PostEffects::ScreenDimensions) - with a radius
		// of ~1000 against a length of at most ~1, smoothstep returned 1
		// everywhere and the vignette was uniformly absent. uSOFTNESS then
		// took the address of its own Uniform* as its value.
		//
		// An auto-filled uniform is Uniform(name, usage); a plain value is
		// Uniform(name, type, &value) - which is what SSAOEffect and
		// DepthOfFieldEffect do, and what this now does.
		screenDimensions = AddUniform(Uniform("uResolution", Uniforms::PostEffects::ScreenDimensions));

		f32 r = radius;
		radiusUniform = AddUniform(Uniform("uRADIUS", Uniforms::DataType::Float, &r));

		f32 s = softness;
		softnessUniform = AddUniform(Uniform("uSOFTNESS", Uniforms::DataType::Float, &s));

		// An explicit block, like every other parameterised effect. Loose
		// non-opaque uniforms are rejected outright on Vulkan; the device
		// wraps them automatically, but nothing then creates the buffer that
		// block needs, and the first draw walks off a null descriptor inside
		// MoltenVK. std140: two floats pack tight from 0, the vec2 aligns to
		// 8, and the block rounds up to 16.
		extraUniformsBinding = 45;
		extraUniformsBlockName = "VignetteParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uRADIUS"] = 0;
		extraUniformOffsets["uSOFTNESS"] = 4;
		extraUniformOffsets["uResolution"] = 8;

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
									// highp, not mediump: this samples
									// gl_FragCoord, which fp16 cannot hold at
									// any real resolution.
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
								"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
								"UBO_BINDING(45) uniform VignetteParams {\n"
								"	float uRADIUS;\n"
								"	float uSOFTNESS;\n"
								"	vec2 uResolution;\n"
								"};\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"void main(void) {\n"
									"vec2 resolution = uResolution;\n"
									"vec2 pos = resolution / 2.0;\n"
									"vec4 aColor = texture_2D(uTex0, vTexcoord);\n"
									"vec2 position = (gl_FragCoord.xy / resolution.xy) - vec2(0.5,0.5);\n"
									"position.x *= resolution.x / resolution.y;\n"
									"float len = length(position);\n"
									"float vignette = smoothstep(uRADIUS, uRADIUS - uSOFTNESS, len);\n"
									"vec3 texColor = mix(aColor.rgb, aColor.rgb * vignette, 1.0);\n"
									"FragColor = vec4(texColor, 1.0);\n"
								"}\n";
        
        CompileShaders();
    }

    VignetteEffect::~VignetteEffect() {}

	void VignetteEffect::SetRadius(const f32 radius)
	{
		f32 r = radius;
		radiusUniform->SetValue(&r);
	}

	void VignetteEffect::SetSoftness(const f32 softness)
	{
		f32 s = softness;
		softnessUniform->SetValue(&s);
	}

};
