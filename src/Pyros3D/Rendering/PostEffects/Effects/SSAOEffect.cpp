//============================================================================
// Name        : SSaoEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <cstdlib>
#include <time.h>
namespace p3d {

	SSAOEffect::SSAOEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

		// Use Sample
		rnm = new Texture();
		uint32 noiseSize = 16;
		std::vector<Vec3> noise; noise.resize(16);
		srand(time(NULL));
		for (int i = 0; i < noiseSize; ++i) {
			noise[i].x = (rand() % 200 - 100) * 0.01f;
			noise[i].y = (rand() % 200 - 100) * 0.01f;
			noise[i].z = 0.0f;
			noise[i].normalize();
		}
		rnm->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGB, 4, 4, false);
		rnm->UpdateData(&noise[0]);
		rnm->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
		UseCustomTexture(rnm);

		// Create Fragment Shader
		FragmentShaderString =
		"uniform sampler2D uTex0;\n"
		"uniform sampler2D uTex1;\n"
		"uniform vec2 uNearFar;\n"
		"uniform vec2 uScreen;\n"
		"uniform float uStrength;\n"
		"uniform int uSamples;\n"
		"uniform float uRadius;\n"
		"uniform float uScale;\n"
		"uniform float uTreshOld;\n"
		"varying vec2 vTexcoord;\n"
		"uniform mat4 matProj;\n"
		"uniform mat4 uInverseView;\n"
		"uniform mat4 uView;\n"
		"\n"
		"// Reconstruct Positions and Normals\n"
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
		"	float total_strength = uStrength;\n"
		"	float radius = uRadius;\n"
		"	int samples = uSamples;\n"
		"	vec3 sample_sphere[16];\n"
		"	sample_sphere[0] = vec3( 0.5381, 0.1856,0.4319);\n"
		"	sample_sphere[1] = vec3( 0.1379, 0.2486,0.4430);\n"
		"	sample_sphere[2] = vec3( 0.3371, 0.5679,0.57);\n"
		"	sample_sphere[3] = vec3(-0.6999,-0.451,0.19);\n"
		"	sample_sphere[4] = vec3( 0.0689,-0.1598,0.8547);\n"
		"	sample_sphere[5] = vec3( 0.0560, 0.69,0.1843);\n"
		"	sample_sphere[6] = vec3(-0.0146, 0.1402,0.762);\n"
		"	sample_sphere[7] = vec3( 0.0100,-0.1924,0.344);\n"
		"	sample_sphere[8] = vec3(-0.3577,-0.5301,0.4358);\n"
		"	sample_sphere[9] = vec3(-0.3169, 0.1063,0.158);\n"
		"	sample_sphere[10] = vec3( 0.103,-0.5869,0.46);\n"
		"	sample_sphere[11] = vec3(-0.897,-0.4940,0.3287);\n"
		"	sample_sphere[12] = vec3( 0.7119,-0.154,0.918);\n"
		"	sample_sphere[13] = vec3(-0.533, 0.596,0.5411);\n"
		"	sample_sphere[14] = vec3( 0.352,-0.631,0.5460);\n"
		"	sample_sphere[15] = vec3(-0.4776, 0.2847,0.271);\n"
		"\n"
		"	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);\n"
		"	vec2 ssaoOut = vec2(uScreen.x, uScreen.y);\n"
		"	vec4 ssao_vp = vec4(1.0, 1.0, 2.0/ssaoOut.x, 2.0/ssaoOut.y);\n"
		"	vec3 v1, v2, v3;\n"
		"	vec4 out_dim = vec4(uScreen.x, uScreen.y, 1.0/uScreen.x, 1.0/uScreen.y);\n"
		"	vec2 screenCoord = vec2(uScreen.x*vTexcoord.x, uScreen.y*vTexcoord.y);\n"
		"\n"
		" 	getPosViewSpace(texture2D(uTex0, vTexcoord).r, screenCoord, z_info, v1, matProj, ssao_vp);\n"
		"    getPosViewSpace(texture2D(uTex0, vTexcoord + vec2(out_dim.z, 0)).r, screenCoord + vec2(1, 0), z_info, v2, matProj, ssao_vp);\n"
		"    getPosViewSpace(texture2D(uTex0, vTexcoord + vec2(0,out_dim.w)).r, screenCoord + vec2(0, 1), z_info, v3, matProj, ssao_vp);\n"
		"	vec3 vViewNormal = normalize(cross(v1 - v2, v1 - v3));\n"
		"\n"
		"	vec4 vs_position = vec4(v1, 1.0);\n"
		"	vec4 ws_position = uInverseView * vec4(v1, 1.0);\n"
		"	vec4 cs_position = matProj * vec4(v1, 1.0);\n"
		"\n"
		"	float depth = texture2D(uTex0, vTexcoord.xy).r;\n"
		"	vec3 normal = vViewNormal;\n"
		"	vec3 rvec = vec3(texture2D(uTex1, (ws_position.xy + ws_position.z) * uScale).xy * 2.0 - 1.0, 0.0);\n"
		"	vec3 tangent = normalize(rvec - normal * dot(rvec, normal));\n"
		"	vec3 bitangent = cross(normal, tangent);\n"
		"	mat3 TBN = mat3(tangent, bitangent, normal);\n"
		"\n"
		"	float occlusion = 0.0;\n"
		"	\n"
		"	for(int i=0; i < samples; i++) {\n"
		"		vec3 samplePos = vs_position.xyz + (TBN * (sample_sphere[i] * radius));\n"
		"		vec4 offset = vec4(samplePos, 1.0);\n"
		"\n"
		"		float orig_offset = offset.z;\n"
		"\n"
		"		offset = matProj * offset;\n"
		"		offset.xy /= offset.w;\n"
		"		offset.xy = offset.xy * 0.5 + 0.5;\n"
		"		\n"
		"		float sampleDepth = texture2D(uTex0, offset.xy).r;\n"
		"		float zz = DecodeNativeDepth(sampleDepth, z_info);\n"
		"		//bool inside_wall = DecodeNativeDepth(sampleDepth, z_info) < -orig_offset;\n"
		"\n"
		"		float rangeCheck = -orig_offset - zz < radius + uTreshOld ? 1.0 : 0.0;\n"
		"		occlusion += (zz <= -orig_offset ? 1.0 : 0.0) * rangeCheck;\n"
		"		\n"
		"	}\n"
		"	float ao = (1.0 - (occlusion / samples) * total_strength);\n"
		"	gl_FragColor = vec4(ao);\n"
		"}";

		CompileShaders();

		total_strength = 1.5f;
		radius = .2f;
		samples = 16;
		scale = 100.f;
		treshOld = 2.0;

		AddUniform(Uniform("uSamples", Uniforms::DataType::Int, &samples));
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));
		AddUniform(Uniform("uScreen", Uniforms::PostEffects::ScreenDimensions));
		AddUniform(Uniform("matProj", Uniforms::PostEffects::ProjectionFromScene));
		uInverseViewMatrixUniform = AddUniform(Uniform("uInverseView", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		uViewMatrixUniform = AddUniform(Uniform("uView", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		uStrengthHandle = AddUniform(Uniform("uStrength", Uniforms::DataType::Float, &total_strength));
		uRadiusHandle = AddUniform(Uniform("uRadius", Uniforms::DataType::Float, &radius));
		uScaleHandle = AddUniform(Uniform("uScale", Uniforms::DataType::Float, &scale));
		uTreshOldHandle = AddUniform(Uniform("uTreshOld", Uniforms::DataType::Float, &treshOld));
	}

	SSAOEffect::~SSAOEffect()
	{
		delete rnm;
	}

};