//============================================================================
// Name        : ScreenSpaceReflectionEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen Space Reflection Effect
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/ScreenSpaceReflectionEffect.h>
#include <Pyros3D/Core/Math/Matrix.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <cstdlib>
#include <time.h>
#include <vector>
#include <cstdio>

using namespace p3d::Math;

namespace p3d {

	ScreenSpaceReflectionEffect::ScreenSpaceReflectionEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		// Set RTT - Use Depth buffer as primary, Color buffer as secondary
		UseRTT(Tex1);  // Depth buffer
		UseRTT(RTT::Color);  // Color buffer for sampling reflections

		// Create Fragment Shader - Using SSAO-style depth decoding
		FragmentShaderString =
								"#define varying_in in\n"
								"#define varying_out out\n"
								"#define attribute_in in\n"
								"#define texture_2D texture\n"
								"#define texture_cube textureCube\n"
								#if defined(GLES3)
									"precision mediump float;\n"
								#endif
								// See SSAOEffect.cpp's identical comment -
								// Vulkan/SPIR-V needs UBO_BINDING/
								// SAMPLER_BINDING/IO_LOCATION on everything
								// non-local; GL needs none of it. Binding 31
								// - see IEffect.h's comment on
								// extraUniformsBinding for why this must be
								// globally distinct (24-30 already taken by
								// SSAOEffect/BlurSSAOEffect/MotionBlurEffect/
								// BlurXEffect/BlurYEffect/DepthOfFieldEffect).
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
								"UBO_BINDING(31) uniform SSRParams {\n"
								"	vec2 uNearFar;\n"
								"	vec2 uScreen;\n"
								"	mat4 matProj;\n"
								"	mat4 uInverseView;\n"
								"	int uMaxSteps;\n"
								"	float uMaxDistance;\n"
								"	float uReflectionStrength;\n"
								"};\n"
								"IO_LOCATION(0) varying_in vec2 vTexcoord;\n"
								"\n"
								"// Depth reconstruction functions\n"
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
								"vec3 getPosViewSpace(float depth_sampled, vec2 uv, vec4 z_info_local, out vec3 vpos, mat4 matProj_local, vec4 viewport_transform_local)\n"
								"{\n"
								"	vec2 screenPos = (uv + .5) * viewport_transform_local.zw - viewport_transform_local.xy;\n"
								"	vec2 screenSpaceRay = vec2(screenPos.x / matProj_local[0][0], screenPos.y / matProj_local[1][1]);\n"
								"	float lDepth = DecodeNativeDepth(depth_sampled, z_info_local);\n"
								"	vpos.xy = lDepth * screenSpaceRay;\n"
								"	vpos.z = -lDepth;\n"
								"	return vec3(screenSpaceRay, -1);\n"
								"}\n"
								"\n"
								"void main() {\n"
								"	// Copy SSAO reconstruction without hemisphere sampling\n"
								"	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);\n"
								"	vec2 ssaoOut = vec2(uScreen.x, uScreen.y);\n"
								"	vec4 ssao_vp = vec4(1.0, 1.0, 2.0/ssaoOut.x, 2.0/ssaoOut.y);\n"
								"	vec3 v1, v2, v3;\n"
								"	vec4 out_dim = vec4(uScreen.x, uScreen.y, 1.0/uScreen.x, 1.0/uScreen.y);\n"
								"	// Use original SSAO offsets for consistency\n"
								"	vec2 screenCoord = vec2(uScreen.x*vTexcoord.x, uScreen.y*vTexcoord.y);\n"
								"\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord).r, screenCoord, z_info, v1, matProj, ssao_vp);\n"
								"	\n"
								"	// Calculate normal using finite differences for smoother results\n"
								"	vec3 v2_x, v3_x, v2_y, v3_y;\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(out_dim.z, 0)).r, screenCoord + vec2(1, 0), z_info, v2_x, matProj, ssao_vp);\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(-out_dim.z, 0)).r, screenCoord + vec2(-1, 0), z_info, v3_x, matProj, ssao_vp);\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(0, out_dim.w)).r, screenCoord + vec2(0, 1), z_info, v2_y, matProj, ssao_vp);\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(0, -out_dim.w)).r, screenCoord + vec2(0, -1), z_info, v3_y, matProj, ssao_vp);\n"
								"	\n"
								"	vec3 dx = (v2_x - v3_x) * 0.5;\n"
								"	vec3 dy = (v2_y - v3_y) * 0.5;\n"
								"	vec3 normal = normalize(cross(dx, dy));\n"
								"	// Ensure normal points towards camera for smooth surfaces\n"
								"	if(dot(normal, -v1) < 0.0) normal = -normal;\n"
								"\n"
								"	// Calculate reflection direction in view space\n"
								"	vec3 viewDir = normalize(-v1);\n"
								"	vec3 reflectDir = reflect(-viewDir, normal);\n"
								"	\n"
								"	// Convert reflection direction to screen space\n"
								"	vec3 rayStart3D = v1;\n"
								"	vec3 rayEnd3D = v1 + reflectDir;\n"
								"	vec4 clipStart = matProj * vec4(rayStart3D, 1.0);\n"
								"	vec4 clipEnd = matProj * vec4(rayEnd3D, 1.0);\n"
								"	vec2 screenStart = clipStart.xy / clipStart.w;\n"
								"	vec2 screenEnd = clipEnd.xy / clipEnd.w;\n"
								"	vec2 screenReflectDir = normalize(screenEnd - screenStart);\n"
								"	\n"
								"	// Screen space ray marching\n"
								"	vec2 rayStart = vTexcoord + screenReflectDir * 0.01; // Offset to avoid self-intersection\n"
								"	vec2 rayDir = screenReflectDir;\n"
								"	vec2 rayStep = rayDir * 0.002; // Much smaller steps for smoother reflections\n"
								"	\n"
								"	vec2 hitPos = rayStart;\n"
								"	vec4 reflectionColor = vec4(0.0);\n"
								"	float bestHit = 999.0;\n"
								"	vec2 bestScreenPos = vec2(0.0);\n"
								"	\n"
								"	// Ray marching loop\n"
								"	for(int i = 0; i < int(uMaxSteps); i++) { // Use uniform for steps\n"
								"		hitPos += rayStep;\n"
								"		\n"
								"		// Check if we're still on screen with margin\n"
								"		if(hitPos.x < 0.01 || hitPos.x > 0.99 || hitPos.y < 0.01 || hitPos.y > 0.99) {\n"
								"			break;\n"
								"		}\n"
								"		\n"
								"		// Sample depth at this screen position\n"
								"		float sampleDepth = texture_2D(uTex0, hitPos).r;\n"
								"		float sampleLinearDepth = DecodeNativeDepth(sampleDepth, z_info);\n"
								"		\n"
								"		// Reconstruct 3D position at this screen point\n"
								"		vec3 samplePos;\n"
								"		getPosViewSpace(sampleDepth, hitPos * uScreen.xy, z_info, samplePos, matProj, ssao_vp);\n"
								"		\n"
								"		// Check if we hit something - distance from ray to surface\n"
								"		float hitDistance = abs(samplePos.z - v1.z);\n"
								"		// Also check distance from current pixel to avoid self-reflection\n"
								"		float pixelDistance = length(hitPos - vTexcoord);\n"
								"		// Simple hit detection\n"
								"		if(hitDistance < bestHit && hitDistance < uMaxDistance) {\n"
								"			bestHit = hitDistance;\n"
								"			bestScreenPos = hitPos;\n"
								"		}\n"
								"	}\n"
								"	\n"
								"	// Use the best hit found\n"
								"	if(bestHit < 999.0) {\n"
								"		reflectionColor = texture_2D(uTex1, bestScreenPos);\n"
								"		reflectionColor.rgb *= uReflectionStrength; // Use uniform for reflection strength\n"
								"	} else {\n"
								"		// Output zero for areas without reflections\n"
								"		reflectionColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
								"	}\n"
								"	\n"
								"	// Show reflections on ALL surfaces - not just floor\n"
								"	// Removed floor height restriction to make everything reflective\n"
								"	\n"
								"	FragColor = reflectionColor;\n"
								"    return;\n"
								"}";

		CompileShaders();

		// Default parameters - Simple SSR
		maxSteps = 256;
		maxDistance = 10.0f;
		thickness = 0.1f;
		reflectionStrength = 0.5f;

		// Add uniforms for SSR
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));
		AddUniform(Uniform("uScreen", Uniforms::PostEffects::ScreenDimensions));
		AddUniform(Uniform("matProj", Uniforms::PostEffects::ProjectionFromScene));
		uInverseViewMatrixUniform = AddUniform(Uniform("uInverseView", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		
		// Add SSR parameter uniforms
		uMaxStepsUniform = AddUniform(Uniform("uMaxSteps", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		uMaxDistanceUniform = AddUniform(Uniform("uMaxDistance", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		uReflectionStrengthUniform = AddUniform(Uniform("uReflectionStrength", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		
		// Initialize uniforms with default values
		if (uMaxStepsUniform) uMaxStepsUniform->SetValue(&maxSteps);
		if (uMaxDistanceUniform) uMaxDistanceUniform->SetValue(&maxDistance);
		if (uReflectionStrengthUniform) uReflectionStrengthUniform->SetValue(&reflectionStrength);

		// See SSAOEffect.cpp's comment on extraUniformsBinding - matches
		// the SSRParams block declared in FragmentShaderString above
		// exactly (std140: vec2/vec2 pack tight at 0/8, each mat4 rounds
		// up to the next 16-byte boundary and occupies 64 bytes, then the
		// int/float/float run packs tight at 4-byte alignment).
		extraUniformsBinding = 31;
		extraUniformsBlockName = "SSRParams";
		extraUniformsSize = 160;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uNearFar"] = 0;
		extraUniformOffsets["uScreen"] = 8;
		extraUniformOffsets["matProj"] = 16;
		extraUniformOffsets["uInverseView"] = 80;
		extraUniformOffsets["uMaxSteps"] = 144;
		extraUniformOffsets["uMaxDistance"] = 148;
		extraUniformOffsets["uReflectionStrength"] = 152;
	}

	ScreenSpaceReflectionEffect::~ScreenSpaceReflectionEffect()
	{
	}

}; 