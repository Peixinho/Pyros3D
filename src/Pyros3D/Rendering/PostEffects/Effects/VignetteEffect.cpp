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

		screenDimensions = AddUniform(Uniform("uResolution", Uniforms::DataType::Vec2, Uniforms::DataUsage::ScreenDimensions));
        
		f32 r = radius;
		radiusUniform = AddUniform(Uniform("uRADIUS", Uniforms::DataType::Float));
		radiusUniform->SetValue(&r);

		f32 s = softness;
		softnessUniform = AddUniform(Uniform("uSOFTNESS", Uniforms::DataType::Float));
		softnessUniform->SetValue(&softnessUniform);

        // Set RTT
        UseRTT(Tex1);
        
        // Create Fragment Shader
		FragmentShaderString =
								#if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
								"precision mediump float;\n"
								#endif
								"uniform sampler2D uTex0;\n"
								"uniform vec2 uResolution;\n"
								"uniform float uRADIUS;\n"
								"uniform float uSOFTNESS;\n"
								"varying vec2 vTexcoord;"
								"void main(void) {\n"
									"vec2 resolution = uResolution;\n"
									"vec2 pos = resolution / 2.0;\n"
									"vec4 aColor = texture2D(uTex0, vTexcoord);\n"
									"vec2 position = (gl_FragCoord.xy / resolution.xy) - vec2(0.5,0.5);\n"
									"position.x *= resolution.x / resolution.y;\n"
									"float len = length(position);\n"
									"float vignette = smoothstep(uRADIUS, uRADIUS - uSOFTNESS, len);\n"
									"vec3 texColor = mix(aColor.rgb, aColor.rgb * vignette, 1.0);\n"
									"gl_FragColor = vec4(texColor, 1.0);\n"
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