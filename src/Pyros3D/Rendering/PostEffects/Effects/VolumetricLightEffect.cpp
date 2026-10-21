//============================================================================
// Name        : VolumetricLightEffect.cpp
// Description : Light in the air: height fog lit by the sun through its shadow.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/VolumetricLightEffect.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <chrono>
#include <cmath>
#include <cstring>

namespace p3d {

	namespace {

		enum {
			OffProj = 0,
			OffInverseView = 64,
			OffNearFar = 128,
			OffShadowM = 144,     // mat4[4]: world to the sun's shadow map, a cascade each
			OffSun = 400,         // to the sun, cascades (0: no shadow)
			OffSunLight = 416,    // the sun's light (irradiance), scattering
			OffFog = 432,         // density, height falloff, base height, anisotropy
			OffAmbient = 448,     // the sky's light in the fog, max distance
			OffMarch = 464,       // steps, time, 0, 0
			BlockSize = 480
		};

		const char* kPrelude =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
#if defined(GLES3)
			"precision highp float;\n"
			"precision highp sampler2DShadow;\n"
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
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n";

		const char* kMarch = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;         // scene depth
SAMPLER_BINDING(1) uniform sampler2DShadow uTex1;   // the sun's shadow map
UBO_BINDING(44) uniform VolLightParams {
	mat4 matProj;
	mat4 uInverseView;
	vec4 uNearFar;
	mat4 uShadowM[4];
	vec4 uSun;        // to the sun, cascades (0: none - the fog is lit as if in sun)
	vec4 uSunLight;   // irradiance, scattering
	vec4 uFog;        // density, height falloff, base height, anisotropy
	vec4 uAmbient;    // the sky's light in the fog, max distance
	vec4 uMarch;      // steps, time
};

// Where the sun reaches p (1) or not (0), from the first cascade that holds it.
float sunAt(vec3 p)
{
	int count = int(uSun.w + 0.5);
	if (count == 0) return 1.0;
	for (int c = 0; c < 4; c++)
	{
		if (c >= count) break;
		vec4 q = uShadowM[c] * vec4(p, 1.0);
		vec3 s = q.xyz / q.w;
		if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z < 0.0 || s.z > 1.0) continue;
		if (count > 1)
		{
			// (the cascades lie 2x2 in one map - as secondpassDirectional reads them)
			vec2 off = vec2((c == 1 || c == 3) ? 0.5 : 0.0, (c >= 2) ? 0.5 : 0.0);
			s.xy = s.xy * 0.5 + off;
		}
		return texture(uTex1, s);
	}
	return 1.0;
}

// Henyey-Greenstein: how much of the light goes off at angle (cos) from its way, per steradian.
float phase(float cosTheta, float g)
{
	float g2 = g * g;
	return (1.0 - g2) / (12.566371 * pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5));
}

// Interleaved gradient noise: a different start along the ray for each pixel (and frame), so
// the steps' banding becomes grain the composite's blur takes out.
float ign(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

void main()
{
	float n = uNearFar.x, f = uNearFar.y;
	// (the same uv -> view ray as VolumetricSmokeEffect: on Vulkan and Metal the top row is v = 0)
#if defined(VULKAN)
	vec2 ndc = vec2(vTexcoord.x * 2.0 - 1.0, 1.0 - vTexcoord.y * 2.0);
#else
	vec2 ndc = vTexcoord * 2.0 - 1.0;
#endif
	vec3 viewRay = vec3(ndc.x / matProj[0][0], ndc.y / matProj[1][1], -1.0);
	float rayScale = length(viewRay);
	vec3 rd = normalize(mat3(uInverseView) * viewRay);
	vec3 ro = uInverseView[3].xyz;

	float depth = texture(uTex0, vTexcoord).r;
	float tScene = 1.0e6;
	if (depth < 0.99999) tScene = (n * f) / (depth * (n - f) + f) * rayScale;
	float tEnd = min(tScene, uAmbient.w);

	int steps = int(uMarch.x);
	float dt = tEnd / float(steps);
	float j = ign(gl_FragCoord.xy + vec2(uMarch.y * 61.0, uMarch.y * 17.0));
	vec3 sun = normalize(uSun.xyz);
	float ph = phase(dot(rd, sun), uFog.w);
	vec3 sunLight = uSunLight.rgb * uSunLight.w * ph;
	vec3 skyLight = uAmbient.rgb * 0.0795775;          // (an even sky: 1/4pi of it each way)

	vec3 L = vec3(0.0);
	float T = 1.0;
	for (int i = 0; i < 128; i++)
	{
		if (i >= steps) break;
		float t = (float(i) + j) * dt;
		vec3 p = ro + rd * t;
		float sigma = uFog.x * exp(-uFog.y * max(p.y - uFog.z, -40.0));
		if (sigma <= 0.0) continue;
		float a = exp(-sigma * dt);
		// (energy-conserving: what this step lets out of the light it scatters, seen through
		// what is in front of it - Hillaire's integration, albedo 1)
		vec3 S = sunLight * sunAt(p) + skyLight;
		L += T * S * (1.0 - a);
		T *= a;
		if (T < 0.003) break;
	}
	FragColor = vec4(L, T);
}
)GLSL";

		const char* kComposite = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;   // the frame
SAMPLER_BINDING(1) uniform sampler2D uTex1;   // the march, half resolution
SAMPLER_BINDING(2) uniform sampler2D uTex2;   // scene depth
UBO_BINDING(44) uniform VolLightCompositeParams {
	vec4 uNearFar;
};

float linearDepth(float d)
{
	return (uNearFar.x * uNearFar.y) / (d * (uNearFar.x - uNearFar.y) + uNearFar.y);
}

void main()
{
	vec4 scene = texture(uTex0, vTexcoord);
	ivec2 isz = textureSize(uTex1, 0);
	vec2 sz = vec2(isz);
	vec2 pos = vTexcoord * sz - 0.5;
	float here = linearDepth(texture(uTex2, vTexcoord).r);
	// 4x4 half-res texels under a tent, each weighed down by how far its depth is from this
	// pixel's (VolumetricSmokeCompositeEffect's upsampling): the grain goes, the edges stay.
	ivec2 corner = ivec2(floor(pos)) - 1;
	vec4 acc = vec4(0.0);
	float wsum = 0.0;
	for (int k = 0; k < 16; k++) {
		ivec2 o = ivec2(k - (k / 4) * 4, k / 4);
		ivec2 tc = clamp(corner + o, ivec2(0), isz - 1);
		float there = linearDepth(texture(uTex2, (vec2(tc) + 0.5) / sz).r);
		vec2 d = abs(vec2(tc) - pos);
		float w = max(0.0, 1.0 - d.x * 0.5) * max(0.0, 1.0 - d.y * 0.5);
		w = (w + 0.001) / (0.01 + abs(there - here) / here);
		acc += texelFetch(uTex1, tc, 0) * w;
		wsum += w;
	}
	vec4 air = acc / wsum;
	FragColor = vec4(scene.rgb * air.a + air.rgb, scene.a);
}
)GLSL";

		f64 NowSeconds() { return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

		inline void Put(std::vector<uchar> &block, const uint32 offset, const f32 x, const f32 y, const f32 z, const f32 w)
		{
			const f32 v[4] = { x, y, z, w };
			memcpy(&block[offset], v, sizeof(v));
		}
	}

	VolumetricLightEffect::VolumetricLightEffect(const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		// rgb is light, and scenes are lit in HDR.
		UseHDRAttachment();
		UseRTT(RTT::Depth);

		// The slot is a shadow sampler, so what is bound there must be a depth texture compared
		// on reading - this one when no sun casts (the same make as DeferredRenderer's dummyShadow2D).
		noShadow = new Texture();
		noShadow->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, 4, 4, false);
		noShadow->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		noShadow->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		UseCustomTexture(noShadow);

		density = 0.012f; falloff = 0.08f; baseHeight = 0.f;
		scattering = 1.f; anisotropy = 0.6f;
		ambient = 0.25f; ambientColour = Vec3(0.55f, 0.65f, 0.8f);
		maxDistance = 200.f; steps = 32.f;
		startTime = NowSeconds();

		FragmentShaderString = std::string(kPrelude) + kMarch;
		CompileShaders();

		AddUniform(Uniform("matProj", Uniforms::PostEffects::ProjectionFromScene));
		AddUniform(Uniform("uInverseView", Uniforms::PostEffects::InverseViewFromScene));
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));

		extraUniformsBinding = 44;
		extraUniformsBlockName = "VolLightParams";
		extraUniformsSize = BlockSize;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["matProj"] = OffProj;
		extraUniformOffsets["uInverseView"] = OffInverseView;
		extraUniformOffsets["uNearFar"] = OffNearFar;
	}

	VolumetricLightEffect::~VolumetricLightEffect() { delete noShadow; }

	void VolumetricLightEffect::PreBind()
	{
		// This frame's map (or the stand-in): bound next. (Set in PreDraw it was bound a frame
		// late, and a light deleted in between left a freed texture to bind.)
		const IRenderer::SunShadow &sun = IRenderer::GetSunShadow();
		Texture* map = (sun.valid && sun.map) ? sun.map : noShadow;
		for (size_t k = 0; k < RTTOrder.size(); k++)
			if (RTTOrder[k].Type == RTT::CustomTexture) RTTOrder[k].texture = map;
	}

	void VolumetricLightEffect::PreDraw()
	{
		const IRenderer::SunShadow &sun = IRenderer::GetSunShadow();
		std::vector<uchar> &b = extraUniformsScratch;
		uint32 cascades = 0;
		Vec3 toSun, light;
		if (sun.valid && sun.map)
		{
			cascades = sun.cascades;
			for (uint32 c = 0; c < cascades && c < 4; c++) memcpy(&b[OffShadowM + c * 64], &sun.worldToMap[c].m[0], 64);
			toSun = sun.toSun;
			// (the light's colour is irradiance / pi: back to irradiance)
			light = sun.radiance * 3.14159265f;
		}
		else
		{
			// No sun shadow this frame: lit by the scene's sun all the same, unshadowed.
			f32 strength = 0.f;
			IRenderer::GetSunHint(toSun, strength);
			light = Vec3(strength, strength, strength) * 3.14159265f;
		}
		const f32 time = (f32)fmod(NowSeconds() - startTime, 1000.0);
		Put(b, OffSun, toSun.x, toSun.y, toSun.z, (f32)cascades);
		Put(b, OffSunLight, light.x, light.y, light.z, scattering);
		Put(b, OffFog, density, falloff, baseHeight, anisotropy < -0.95f ? -0.95f : (anisotropy > 0.95f ? 0.95f : anisotropy));
		Put(b, OffAmbient, ambientColour.x * ambient, ambientColour.y * ambient, ambientColour.z * ambient, maxDistance);
		Put(b, OffMarch, steps < 4.f ? 4.f : (steps > 128.f ? 128.f : steps), time, 0.f, 0.f);
	}

	VolumetricLightCompositeEffect::VolumetricLightCompositeEffect(const uint32 TexColor, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();
		UseRTT(TexColor);
		Build();
	}

	VolumetricLightCompositeEffect::VolumetricLightCompositeEffect(Texture* color, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();
		UseCustomTexture(color);
		Build();
	}

	void VolumetricLightCompositeEffect::Build()
	{
		UseRTT(RTT::LastRTT);
		UseRTT(RTT::Depth);
		FragmentShaderString = std::string(kPrelude) + kComposite;
		CompileShaders();
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));
		extraUniformsBinding = 44;
		extraUniformsBlockName = "VolLightCompositeParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uNearFar"] = 0;
	}
};
