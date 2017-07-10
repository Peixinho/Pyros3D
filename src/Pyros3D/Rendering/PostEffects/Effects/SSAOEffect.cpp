//============================================================================
// Name        : SSaoEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>

namespace p3d {

	SSAOEffect::SSAOEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		
		// Set RTT
		UseRTT(Tex1);
		
		// Use Sample
		rnm = new Texture();
		rnm->LoadTexture("../resources/rnm.png", TextureType::Texture, false);
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
			"uniform mat4 uInverseView;\n"
			"\n"
			// Reconstruct Positions and Normals
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
			"	vec2 screenSpaceRay = vec2(screenPos.x / matProj_local[0][0],screenPos.y / matProj_local[1][1]);\n"
   		        "	vec2 screenSpaceRay = vec2(screenPos.x / matProj_local[0][0], screenPos.y / matProj_local[1][1]);\n"
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
			"	vec3 v1, v2, v3;\n"
			"	vec4 out_dim = vec4(uScreen.x, uScreen.y, 1.0/uScreen.x, 1.0/uScreen.y);\n"
			"	vec2 screenCoord = vec2(uScreen.x*vTexcoord.x, uScreen.y*vTexcoord.y);\n"
			"	getPosViewSpace(texture2D(uTex0, vTexcoord).r, screenCoord, z_info, v1, matProj, ssao_vp);\n"
			"	getPosViewSpace(texture2D(uTex0, vTexcoord + vec2(out_dim.z, 0)).r, screenCoord + vec2(1, 0), z_info, v2, matProj, ssao_vp);\n"
			"	getPosViewSpace(texture2D(uTex0, vTexcoord + vec2(0,out_dim.w)).r, screenCoord + vec2(0, 1), z_info, v3, matProj, ssao_vp);\n"
			"	vec3 vViewNormal = normalize(cross(v3 - v1, v2 - v1));\n"
			//"	gl_FragColor = vec4(vViewNormal, 1.0);\n"
			// End Reconstruction
			"	float total_strength = uStrength;\n"
			"	float base = uBase;\n"
			"	float area = uArea;\n"
			"	float falloff = uFalloff;\n"
			"	float radius = uRadius;\n"
			"	int samples = uSamples;\n"
			"	float samplesf = float(uSamples);\n"
			"	vec3 sample_sphere[16];\n"
			"	sample_sphere[0] = vec3( 0.5381, 0.1856,-0.4319);\n"
			"	sample_sphere[1] = vec3( 0.1379, 0.2486, 0.4430);\n"
			"	sample_sphere[2] = vec3( 0.3371, 0.5679,-0.0057);\n"
			"	sample_sphere[3] = vec3(-0.6999,-0.0451,-0.0019);\n"
			"	sample_sphere[4] = vec3( 0.0689,-0.1598,-0.8547);\n"
			"	sample_sphere[5] = vec3( 0.0560, 0.0069,-0.1843);\n"
			"	sample_sphere[6] = vec3(-0.0146, 0.1402, 0.0762);\n"
			"	sample_sphere[7] = vec3( 0.0100,-0.1924,-0.0344);\n"
			"	sample_sphere[8] = vec3(-0.3577,-0.5301,-0.4358);\n"
			"	sample_sphere[9] = vec3(-0.3169, 0.1063, 0.0158);\n"
			"	sample_sphere[10] = vec3( 0.0103,-0.5869, 0.0046);\n"
			"	sample_sphere[11] = vec3(-0.0897,-0.4940, 0.3287);\n"
			"	sample_sphere[12] = vec3( 0.7119,-0.0154,-0.0918);\n"
			"	sample_sphere[13] = vec3(-0.0533, 0.0596,-0.5411);\n"
			"	sample_sphere[14] = vec3( 0.0352,-0.0631, 0.5460);\n"
			"	sample_sphere[15] = vec3(-0.4776, 0.2847,-0.0271);\n"
			"	vec4 _v1 = (uInverseView*vec4(v1,0.0));\n"
			"	vec3 random = normalize( texture2D(uTex1, vec2(_v1.x, _v1.y)).rgb);\n"
			"	float depth = DecodeNativeDepth(texture2D(uTex0, vTexcoord).r,z_info);\n"
			"	vec3 position = vec3(vTexcoord, depth);\n"
			"	vec3 normal = vViewNormal;\n"
			"	float radius_depth = radius/depth;\n"
			"	float occlusion = 0.0;\n"
			"	for(int i=0; i < 16; i++) {\n"
			"		vec3 ray = radius_depth * reflect(sample_sphere[i], random);\n"
			"		vec3 hemi_ray = position + sign(dot(ray,normal)) * ray;\n"
			"		float occ_depth = DecodeNativeDepth(texture2D(uTex0, vec2(clamp(hemi_ray.x,0.0,1.0),clamp(hemi_ray.y,0.0,1.0))).r,z_info);\n"
			"		float difference = depth - occ_depth;\n"
			"		occlusion += step(falloff, difference) * (1.0-smoothstep(falloff, area, difference));\n"
			"	}\n"
			"	float ao = 1.0 - total_strength * occlusion * (1.0 / samplesf);\n"
			"	gl_FragColor = vec4(clamp(ao + base,0.0,1.0));\n"
			"}";
			  
		CompileShaders();

		total_strength = 1.0f;
		base = 0.01f;
		area = 1.0f;
		falloff = 0.01f;
		radius = 0.02f;
		samples = 16;
		scale = 0.0005f;

		AddUniform(Uniform("uStrength",Uniforms::DataType::Float, &total_strength));
		AddUniform(Uniform("uBase", Uniforms::DataType::Float, &base));
		AddUniform(Uniform("uArea", Uniforms::DataType::Float, &area));
		AddUniform(Uniform("uFalloff", Uniforms::DataType::Float, &falloff));
		AddUniform(Uniform("uRadius", Uniforms::DataType::Float, &radius));
		AddUniform(Uniform("uSamples", Uniforms::DataType::Int, &samples));
		AddUniform(Uniform("uScale", Uniforms::DataType::Float, &scale));
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));
		AddUniform(Uniform("uScreen", Uniforms::PostEffects::ScreenDimensions));
		AddUniform(Uniform("matProj", Uniforms::PostEffects::ProjectionFromScene));
		uInverseViewMatrixUniform = AddUniform(Uniform("uInverseView", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
	}

	SSAOEffect::~SSAOEffect()
	{
		delete rnm;
	}

};
