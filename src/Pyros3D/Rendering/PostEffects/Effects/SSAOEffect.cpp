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
#include <algorithm>
#include <cmath>
namespace p3d {

	SSAOEffect::SSAOEffect(const uint32 Tex1, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{

		// Set RTT
		UseRTT(Tex1);

		// Use Sample
		rnm = new Texture();
		uint32 noiseSize = 16;
		// Bytes, encoded as v*0.5+0.5 - the shader decodes with *2-1. This
		// used to hand floats (negative ones included) to an RGB8 texture,
		// so the kernel rotations were the bit patterns of those floats.
		// Evenly spaced angles, shuffled, rather than rand(): sixteen
		// random directions clump, and the clumps show up as blotches.
		std::vector<uchar> noise(noiseSize * 3);
		srand(time(NULL));
		std::vector<int> order(noiseSize);
		for (uint32 i = 0; i < noiseSize; ++i) order[i] = i;
		for (uint32 i = noiseSize - 1; i > 0; --i) std::swap(order[i], order[rand() % (i + 1)]);
		for (uint32 i = 0; i < noiseSize; ++i) {
			const f32 a = (order[i] + 0.5f) / noiseSize * 2.0f * PI;
			noise[i * 3 + 0] = (uchar)((cosf(a) * 0.5f + 0.5f) * 255.0f + 0.5f);
			noise[i * 3 + 1] = (uchar)((sinf(a) * 0.5f + 0.5f) * 255.0f + 0.5f);
			noise[i * 3 + 2] = 0;
		}
		rnm->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGB8, 4, 4, false);
		rnm->UpdateData(&noise[0]);
		rnm->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
		// Nearest: each pixel of the 4x4 tile is one rotation, and the
		// blur relies on seeing all sixteen, not a bilinear mix of them.
		rnm->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
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
								"UBO_BINDING(43) uniform SSAOParams {\n"
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
								"	float d0 = texture_2D(uTex0, vTexcoord).r;\n"
								"	if (d0 >= 0.9999) { FragColor = vec4(1.0); return; }\n"
								"\n"
								"	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);\n"
								"	vec4 ssao_vp = vec4(1.0, 1.0, 2.0/uScreen.x, 2.0/uScreen.y);\n"
								"	vec2 px = 1.0 / uScreen;\n"
								"	vec2 sc = uScreen * vTexcoord;\n"
								"\n"
								"	vec3 P, Pr, Pl, Pu, Pd;\n"
								"	getPosViewSpace(d0, sc, z_info, P, matProj, ssao_vp);\n"
								// Far away the sampling radius is smaller than a pixel and the
								// depth buffer is coarser than the radius: the normal rebuilt from
								// neighbouring depths is noise, and so is the occlusion - open
								// ground at a distance came out blotched and banded. The effect
								// fades out as the radius shrinks towards a pixel on screen, and
								// pixels past that are not sampled at all.
								"	float radiusPx = uRadius * abs(matProj[1][1]) / max(-P.z, 0.001) * uScreen.y * 0.5;\n"
								"	float fade = smoothstep(2.0, 6.0, radiusPx);\n"
								"	if (fade <= 0.0) { FragColor = vec4(1.0); return; }\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(px.x, 0.0)).r, sc + vec2(1.0, 0.0), z_info, Pr, matProj, ssao_vp);\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord - vec2(px.x, 0.0)).r, sc - vec2(1.0, 0.0), z_info, Pl, matProj, ssao_vp);\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord + vec2(0.0, px.y)).r, sc + vec2(0.0, 1.0), z_info, Pu, matProj, ssao_vp);\n"
								"	getPosViewSpace(texture_2D(uTex0, vTexcoord - vec2(0.0, px.y)).r, sc - vec2(0.0, 1.0), z_info, Pd, matProj, ssao_vp);\n"
								// Pr steps one texel along +u (right on screen on both
								// backends), Pu one texel along +v - which is *up* the screen
								// on GL and *down* on Metal (v=0 at the top). The two edge
								// vectors therefore come out in opposite handedness, so the
								// cross product's operands have to swap or the reconstructed
								// normal points into the surface - and the kernel is a
								// hemisphere oriented along that normal, so every sample would
								// land inside the geometry and read as fully occluded.
								// Each axis takes whichever neighbour is nearer in depth: a
								// one-sided difference across a silhouette mixes two surfaces
								// and bends the normal, which reads as a dark rim on the edge.
								"	vec3 dx = abs(Pr.z - P.z) < abs(P.z - Pl.z) ? Pr - P : P - Pl;\n"
								"	vec3 dy = abs(Pu.z - P.z) < abs(P.z - Pd.z) ? Pu - P : P - Pd;\n"
								"#if defined(METAL)\n"
								"	vec3 normal = normalize(cross(dy, dx));\n"
								"#else\n"
								"	vec3 normal = normalize(cross(dx, dy));\n"
								"#endif\n"
								"\n"
								// A 4x4 tile of random rotations, repeated in screen space so the
								// 4x4 blur that follows cancels it exactly. It used to be looked up
								// by world position, which crawled across surfaces as the camera
								// moved and never lined up with the blur.
								"	vec3 rvec = vec3(texture_2D(uTex1, gl_FragCoord.xy * 0.25).xy * 2.0 - 1.0, 0.0);\n"
								"	vec3 tangent = normalize(rvec - normal * dot(rvec, normal));\n"
								"	vec3 bitangent = cross(normal, tangent);\n"
								"	mat3 TBN = mat3(tangent, bitangent, normal);\n"
								"\n"
								"	float radius = uRadius;\n"
								"	// Keeps a flat surface from occluding itself through depth-buffer\n"
								"	// quantization; grows with distance, where that quantization does.\n"
								// ...which is the square of the distance over the near plane: a
								// 24-bit buffer resolves about z*z / (near * 2^24) metres.
								"	float bias = 0.03 * radius + 0.0005 * (-P.z) + 3.0 * P.z * P.z / (max(uNearFar.x, 0.0001) * 16777216.0);\n"
								"	float occlusion = 0.0;\n"
								"	int samples = uSamples;\n"
								"	for (int i = 0; i < 64; i++) {\n"
								"		if (i >= samples) break;\n"
								"		float fi = float(i) + 0.5;\n"
								"		float t = fi / float(samples);\n"
								"		// Cosine-weighted hemisphere on a golden-angle spiral, with the\n"
								"		// sample distance on an independent sequence packed towards the\n"
								"		// centre: close occluders are what contact shadows are made of.\n"
								"		float phi = fi * 2.39996;\n"
								"		float sr = sqrt(t);\n"
								"		vec3 dir = vec3(cos(phi) * sr, sin(phi) * sr, sqrt(1.0 - t));\n"
								"		float h = fract(fi * 0.618034);\n"
								"		float dist = mix(0.1, 1.0, h * h);\n"
								"		vec3 samplePos = P + TBN * dir * (radius * dist);\n"
								"\n"
								"		vec4 offset = matProj * vec4(samplePos, 1.0);\n"
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
								"		float sceneZ = -DecodeNativeDepth(texture_2D(uTex0, offset.xy).r, z_info);\n"
								"		// Soft range check: an occluder much nearer the camera than this\n"
								"		// pixel (a teapot in front of the floor) is not occluding it. The\n"
								"		// old binary check counted anything up to radius + threshold,\n"
								"		// which is what drew a thick black outline around every object.\n"
								"		float range = 1.0 - smoothstep(radius, radius + uTreshOld, abs(P.z - sceneZ));\n"
								"		occlusion += (sceneZ >= samplePos.z + bias ? 1.0 : 0.0) * range;\n"
								"	}\n"
								"	float ao = clamp(1.0 - (occlusion / float(samples)) * uStrength, 0.0, 1.0);\n"
								"	FragColor = vec4(mix(1.0, ao, fade));\n"
								"}";

		CompileShaders();

		total_strength = 2.0f;
		radius = .5f;
		samples = 16;
		scale = 100.f;
		treshOld = 0.5f;

		AddUniform(Uniform("uSamples", Uniforms::DataType::Int, &samples));
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));
		AddUniform(Uniform("uScreen", Uniforms::PostEffects::ScreenDimensions));
		AddUniform(Uniform("matProj", Uniforms::PostEffects::ProjectionFromScene));
		// PostEffects::, not DataUsage:: - these are post-effect usages, and
		// the value in that slot is what ProcessPostEffects switches on. The
		// manager fills them from SetViewMatrix() when it has one, so an SSAO
		// in a scene's chain needs nobody to push the matrix in by hand; a
		// caller that does push it (the Lua helper's ssaoSetViewMatrix) still
		// wins, because the manager leaves these alone until told.
		uInverseViewMatrixUniform = AddUniform(Uniform("uInverseView", Uniforms::PostEffects::InverseViewFromScene));
		uViewMatrixUniform = AddUniform(Uniform("uView", Uniforms::PostEffects::ViewFromScene));
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
		// 43, not 24. 24 is BIND_Occluders2D's, which PyrosShader.glsl
		// declares for 2D shadows - binding points are one global registry
		// (see IEffect.h's comment on extraUniformsBinding) and this one was
		// picked without noticing. Two differently-shaped blocks on one point
		// survive only while the buffer is re-bound before every draw that
		// needs it; the occluder UBO is uploaded once and then never re-bound,
		// so from the second frame on SSAO's own, much smaller buffer is what
		// sits there - and WebGL2 drops every draw whose block is bigger than
		// the buffer under it, which is the whole class of bug that made lit
		// meshes vanish on the web.
		extraUniformsBinding = 43;
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
