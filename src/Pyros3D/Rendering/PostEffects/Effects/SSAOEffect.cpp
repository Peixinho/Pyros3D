//============================================================================
// Name        : SSaoEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>

namespace p3d {

	SSAOEffect::SSAOEffect(const uint32 Tex1) : IEffect()
	{
		
		// Set RTT
		UseRTT(Tex1);
		
		// Use Sample
		rnm = new Texture();
		rnm->LoadTexture("../../../../resources/rnm.png", TextureType::Texture, false);
		UseCustomTexture(rnm);

		// Create Fragment Shader
		FragmentShaderString =
			"uniform sampler2D uTex0;\n"
			"uniform sampler2D uTex1;\n"
			"uniform vec2 uNearFar;\n"
			"uniform vec2 uScreen;\n"
			"uniform float uStrength;\n"
			"uniform float uBase;\n"
			"uniform float uArea;\n"
			"uniform int uSamples;\n"
			"uniform float uFalloff;\n"
			"uniform float uRadius;\n"
			"uniform float uScale;\n"
			"varying vec2 vTexcoord;\n"
			"uniform mat4 matProj;\n"
			"\n"
			"float DecodeLinearDepth(float z, vec4 z_info_local)\n"
			"{\n"
			"	return z_info_local.x - z * z_info_local.w;\n"
			"}\n"
			"\n"
			"float DecodeNativeDepth(float native_z, vec4 z_info_local)\n"
			"{\n"
			"	return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);\n"
			"}\n"
			"\n"
			"vec2 getPosViewSpace(vec2 uv, vec4 z_info_local, mat4 matProj_local, vec4 viewport_transform_local)\n"
			"{\n"
			"	vec2 screenPos = (uv + .5) * viewport_transform_local.zw - viewport_transform_local.xy;\n"
			"	vec2 screenSpaceRay = vec2(screenPos.x / matProj_local[1][1],-screenPos.y / matProj_local[2][2]);\n"
			"	return screenSpaceRay;\n"
			"}\n"
			"\n"
			"vec3 getPosViewSpace(float depth_sampled, vec2 uv, vec4 z_info_local, out vec3 vpos, mat4 matProj_local, vec4 viewport_transform_local)\n"
			"{\n"
			"	vec2 screenSpaceRay = getPosViewSpace(uv, z_info_local, matProj_local, viewport_transform_local);\n"
			"\n"
			"	float lDepth = DecodeNativeDepth(depth_sampled, z_info_local);\n"
			"	vpos.xy = lDepth * screenSpaceRay;\n"
			"	vpos.z = -lDepth;\n"
			"\n"
			"	return vec3(screenSpaceRay, -1);\n"
			"}\n"
			"\n"
			"void main() {\n"
			"\n"
			"	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);\n"
			"	vec2 ssaoOut = vec2(uScreen.x, uScreen.y);\n"
			"	vec4 ssao_vp = vec4(1.0, 1.0, 2.0/ssaoOut.x, 2.0/ssaoOut.y);\n"
			//"	ssao_vp = vp;\n"
			"	vec3 v1, v2, v3;\n"
			"	vec4 out_dim = vec4(uScreen.x, uScreen.y, 1.0/uScreen.x, 1.0/uScreen.y);\n"
			"	vec2 screenCoord = vec2(uScreen.x*vTexcoord.x, uScreen.y*vTexcoord.y);\n"
			"	getPosViewSpace(texture2D(uTex0, vTexcoord).r, screenCoord, z_info, v1, matProj, ssao_vp);\n"
			"	getPosViewSpace(texture2D(uTex0, vTexcoord + vec2(out_dim.z, 0)).r, screenCoord + vec2(1, 0), z_info, v2, matProj, ssao_vp);\n"
			"	getPosViewSpace(texture2D(uTex0, vTexcoord + vec2(0,out_dim.w)).r, screenCoord + vec2(0, 1), z_info, v3, matProj, ssao_vp);\n"
			"	vec3 vViewNormal = normalize(cross(v3 - v1, v2 - v1));\n"
			"	gl_FragColor = vec4(vViewNormal, 1.0);\n"
			"}";
			  
		CompileShaders();

		Uniform strength;
		Uniform Base;
		Uniform Area;
		Uniform Falloff;
		Uniform Radius;
		Uniform Samples;
		Uniform Scale;
		Uniform nearFarPlane;
		Uniform screen;
		Uniform matProj;

		total_strength = 1.0;
		base = 0.2;
		area = 0.075;
		falloff = 0.00001;
		radius = 0.1;
		samples = 16;
		scale = 0.005f;

		strength.Name = "uStrength";
		strength.Type = DataType::Float;
		strength.Usage = PostEffects::Other;
		strength.SetValue(&total_strength);
		AddUniform(strength);

		Base.Name = "uBase";
		Base.Type = DataType::Float;
		Base.Usage = PostEffects::Other;
		Base.SetValue(&base);
		AddUniform(Base);

		Area.Name = "uArea";
		Area.Type = DataType::Float;
		Area.Usage = PostEffects::Other;
		Area.SetValue(&area);
		AddUniform(Area);

		Falloff.Name = "uFalloff";
		Falloff.Type = DataType::Float;
		Falloff.Usage = PostEffects::Other;
		Falloff.SetValue(&falloff);
		AddUniform(Falloff);

		Radius.Name = "uRadius";
		Radius.Type = DataType::Float;
		Radius.Usage = PostEffects::Other;
		Radius.SetValue(&radius);
		AddUniform(Radius);

		Samples.Name = "uSamples";
		Samples.Type = DataType::Int;
		Samples.Usage = PostEffects::Other;
		Samples.SetValue(&samples);
		AddUniform(Samples);

		Scale.Name = "uScale";
		Scale.Type = DataType::Float;
		Scale.Usage = PostEffects::Other;
		Scale.SetValue(&scale);
		AddUniform(Scale);

		nearFarPlane.Name = "uNearFar";
		nearFarPlane.Type = DataType::Vec2;
		nearFarPlane.Usage = PostEffects::NearFarPlane;
		AddUniform(nearFarPlane);

		screen.Name = "uScreen";
		screen.Type = DataType::Vec2;
		screen.Usage = PostEffects::ScreenDimensions;
		AddUniform(screen);

		matProj.Name = "matProj";
		matProj.Type = DataType::Matrix;
		matProj.Usage = PostEffects::ProjectionFromScene;
		AddUniform(matProj);
	}

	SSAOEffect::~SSAOEffect()
	{
		delete rnm;
	}

};