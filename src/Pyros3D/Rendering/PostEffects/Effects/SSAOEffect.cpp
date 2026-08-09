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
		rnm->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGB8, 4, 4, false);
		rnm->UpdateData(&noise[0]);
		rnm->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
		UseCustomTexture(rnm);

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
								// Vulkan/SPIR-V rejects non-opaque uniforms
								// outside a block outright, and needs a
								// static layout(binding=) on every UBO/
								// sampler - GL needs neither (matches by
								// name/unit at runtime). VULKAN is
								// predefined by shaderc itself for any
								// Vulkan-target compile (see
								// SpirvShaderCompiler::Compile()'s
								// comment) - same pattern as
								// PyrosShader.glsl's own IO_LOCATION/
								// UBO_BINDING/SAMPLER_BINDING macros.
								// Binding 24 (not 0-23) - see IEffect.h's
								// comment on extraUniformsBinding: binding
								// points are a single global registry
								// shared with PyrosShader.glsl's own
								// UBOs, not per-shader.
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
								"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
								"UBO_BINDING(24) uniform SSAOParams {\n"
								"	vec2 uNearFar;\n"
								"	vec2 uScreen;\n"
								"	float uStrength;\n"
								"	int uSamples;\n"
								"	float uRadius;\n"
								"	float uScale;\n"
								"	float uTreshOld;\n"
								"	mat4 matProj;\n"
								"	mat4 uInverseView;\n"
								"	mat4 uView;\n"
								"};\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
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
								// Same NDC-vs-texture-origin mismatch secondpass*.glsl's
								// identical getPosViewSpace() already compensates for, reached
								// from the other direction: there `uv` comes from
								// gl_FragCoord, here from vTexcoord, and IEffect's own
								// full-screen-quad vertex shader already flips vTexcoord.y on
								// Metal (NDC Y up like GL, but render-target v=0 at the top
								// like Vulkan - see its comment). That flip is right for
								// *sampling* the scene textures and wrong for treating the
								// same value as an NDC coordinate: screenPos.y comes out as
								// -NDC_y, so the reconstructed view-space ray points the wrong
								// way vertically and the whole AO field ends up computed
								// against a scene mirrored about the horizon.
								"#if defined(METAL)\n"
								"	screenPos.y = -screenPos.y;\n"
								"#endif\n"
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
								" 	getPosViewSpace(texture_2D(uTex0, vTexcoord).r, screenCoord, z_info, v1, matProj, ssao_vp);\n"
								"    getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(out_dim.z, 0)).r, screenCoord + vec2(1, 0), z_info, v2, matProj, ssao_vp);\n"
								"    getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(0,out_dim.w)).r, screenCoord + vec2(0, 1), z_info, v3, matProj, ssao_vp);\n"
								// v2 steps one texel along +u (right on screen on both
								// backends), v3 one texel along +v - which is *up* the screen
								// on GL and *down* on Metal (v=0 at the top). The two edge
								// vectors therefore come out in opposite handedness, so the
								// cross product's operands have to swap or the reconstructed
								// normal points into the surface - and sample_sphere is a
								// hemisphere oriented along that normal, so every sample would
								// land inside the geometry and read as fully occluded.
								"#if defined(METAL)\n"
								"	vec3 vViewNormal = normalize(cross(v1 - v3, v1 - v2));\n"
								"#else\n"
								"	vec3 vViewNormal = normalize(cross(v1 - v2, v1 - v3));\n"
								"#endif\n"
								"\n"
								"	vec4 vs_position = vec4(v1, 1.0);\n"
								"	vec4 ws_position = uInverseView * vec4(v1, 1.0);\n"
								"	vec4 cs_position = matProj * vec4(v1, 1.0);\n"
								"\n"
								"	float depth = texture_2D(uTex0, vTexcoord.xy).r;\n"
								"	vec3 normal = vViewNormal;\n"
								"	vec3 rvec = vec3(texture_2D(uTex1, (ws_position.xy + ws_position.z) * uScale).xy * 2.0 - 1.0, 0.0);\n"
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
								// offset.xy is now a GL-convention texcoord (v=0 at NDC
								// y=-1, the bottom). uTex0's v=0 row is the *top* on Metal,
								// so the kernel sample would read the vertically mirrored
								// pixel - the depth comparison below then tests each sample
								// against unrelated geometry. Same flip IEffect's vertex
								// shader applies to vTexcoord, applied here because this
								// texcoord is computed in the shader rather than interpolated.
								"#if defined(METAL)\n"
								"		offset.y = 1.0 - offset.y;\n"
								"#endif\n"
								"		\n"
								"		float sampleDepth = texture_2D(uTex0, offset.xy).r;\n"
								"		float zz = DecodeNativeDepth(sampleDepth, z_info);\n"
								"		//bool inside_wall = DecodeNativeDepth(sampleDepth, z_info) < -orig_offset;\n"
								"\n"
								"		float rangeCheck = -orig_offset - zz < radius + uTreshOld ? 1.0 : 0.0;\n"
								"		occlusion += (zz <= -orig_offset ? 1.0 : 0.0) * rangeCheck;\n"
								"		\n"
								"	}\n"
								"	float ao = (1.0 - (occlusion / float(samples)) * total_strength);\n"
								"	FragColor = vec4(ao);\n"
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

		// See IEffect.h's comment on extraUniformsBinding - matches the
		// SSAOParams block declared in FragmentShaderString above exactly
		// (member order/types), std140 layout computed by hand: vec2/vec2
		// pack tight (align 8), the float/int run packs tight (align 4),
		// then each mat4 rounds up to the next 16-byte boundary (36->48)
		// and occupies 64 bytes (4 std140-aligned vec4 columns).
		extraUniformsBinding = 24;
		extraUniformsBlockName = "SSAOParams";
		extraUniformsSize = 240;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uNearFar"] = 0;
		extraUniformOffsets["uScreen"] = 8;
		extraUniformOffsets["uStrength"] = 16;
		extraUniformOffsets["uSamples"] = 20;
		extraUniformOffsets["uRadius"] = 24;
		extraUniformOffsets["uScale"] = 28;
		extraUniformOffsets["uTreshOld"] = 32;
		extraUniformOffsets["matProj"] = 48;
		extraUniformOffsets["uInverseView"] = 112;
		extraUniformOffsets["uView"] = 176;
	}

	SSAOEffect::~SSAOEffect()
	{
		delete rnm;
	}

};
