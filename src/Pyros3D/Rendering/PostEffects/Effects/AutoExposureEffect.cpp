//============================================================================
// Name        : AutoExposureEffect.cpp
// Description : The eye adjusting: exposure that follows how bright the view is.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/AutoExposureEffect.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <chrono>
#include <cmath>
#include <cstring>

namespace p3d {

	namespace {
		const uint32 kMeterW = 64, kMeterH = 36;

		const char* kPrelude =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
#if defined(GLES3)
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
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n";

		// Each texel of the small image: the mean log2 luminance of its block of the frame (4x4
		// samples), weighted towards the middle of the view.
		const char* kMeter = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;   // the frame so far
void main()
{
	vec2 size = vec2(textureSize(uTex0, 0));
	vec2 cell = vec2(1.0 / 64.0, 1.0 / 36.0);
	vec2 corner = vTexcoord - cell * 0.5;
	float sum = 0.0;
	for (int j = 0; j < 4; j++)
		for (int i = 0; i < 4; i++)
		{
			vec3 c = textureLod(uTex0, corner + cell * (vec2(float(i), float(j)) + 0.5) / 4.0, 0.0).rgb;
			sum += log2(max(dot(c, vec3(0.2126, 0.7152, 0.0722)), 1.0e-4));
		}
	vec2 d = (vTexcoord - 0.5) * vec2(1.6, 2.0);
	float w = exp(-dot(d, d) * 1.5) + 0.15;      // (centre-weighted, the edges still counting)
	FragColor = vec4(sum / 16.0 * w, w, 0.0, 1.0);
}
)GLSL";

		const char* kAdapt = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;   // the meter (64 x 36)
SAMPLER_BINDING(1) uniform sampler2D uTex1;   // last frame's adapted value
UBO_BINDING(44) uniform ExposureAdaptParams {
	vec4 uRate;    // dt, speed up, speed down
};
void main()
{
	ivec2 n = textureSize(uTex0, 0);
	float sum = 0.0, wsum = 0.0;
	for (int y = 0; y < n.y; y++)
		for (int x = 0; x < n.x; x++)
		{
			vec2 s = texelFetch(uTex0, ivec2(x, y), 0).rg;
			sum += s.r; wsum += s.g;
		}
	float target = clamp(sum / max(wsum, 1e-4), -14.0, 14.0);
	vec4 prev = texelFetch(uTex1, ivec2(0, 0), 0);
	float adapted = target;
	// (a == 1 marks a value made here; anything else is a target never written yet: start there)
	if (prev.a > 0.5 && prev.a < 1.5 && abs(prev.r) < 20.0)
	{
		float speed = target > prev.r ? uRate.y : uRate.z;
		adapted = prev.r + (target - prev.r) * (1.0 - exp(-uRate.x * speed));
	}
	FragColor = vec4(adapted, target, 0.0, 1.0);
}
)GLSL";

		const char* kApply = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;   // the frame
SAMPLER_BINDING(1) uniform sampler2D uTex1;   // the adapted log2 luminance (1x1)
UBO_BINDING(44) uniform ExposureApplyParams {
	vec4 uExposure;   // key, compensation (EV), least EV, most EV
};
void main()
{
	float adapted = texelFetch(uTex1, ivec2(0, 0), 0).r;
	float ev = log2(uExposure.x) - adapted + uExposure.y;
	ev = clamp(ev, uExposure.z, uExposure.w);
	vec4 c = texture(uTex0, vTexcoord);
	FragColor = vec4(c.rgb * exp2(ev), c.a);
}
)GLSL";

		f64 NowSeconds() { return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
		inline void Put(std::vector<uchar> &block, const uint32 offset, const f32 x, const f32 y, const f32 z, const f32 w)
		{
			const f32 v[4] = { x, y, z, w };
			memcpy(&block[offset], v, sizeof(v));
		}
	}

	// ------------------------------------------------------------------ meter

	ExposureMeterEffect::ExposureMeterEffect() : IEffect(kMeterW, kMeterH)
	{
		UseHDRAttachment();
		UseRTT(RTT::LastRTT);
		FragmentShaderString = std::string(kPrelude) + kMeter;
		CompileShaders();
	}

	// ------------------------------------------------------------------ adapt

	ExposureAdaptEffect::ExposureAdaptEffect() : IEffect(1, 1), speedUp(3.f), speedDown(1.2f), lastTime(-1.0)
	{
		UseHDRAttachment();
		attachment->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		// The second 1x1: read from while the other is drawn into, and the two swapped each frame.
		otherAttachment = new Texture();
		otherAttachment->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, 1, 1, false);
		otherAttachment->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		otherAttachment->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		otherFbo = new FrameBuffer();
		otherFbo->SetDebugName("Auto exposure (other)");
		otherFbo->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, otherAttachment);

		UseRTT(RTT::LastRTT);
		UseCustomTexture(otherAttachment);
		FragmentShaderString = std::string(kPrelude) + kAdapt;
		CompileShaders();
		extraUniformsBinding = 44;
		extraUniformsBlockName = "ExposureAdaptParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
	}

	ExposureAdaptEffect::~ExposureAdaptEffect()
	{
		delete otherFbo;
		delete otherAttachment;
	}

	void ExposureAdaptEffect::PreBind()
	{
		// (what this frame reads: the target it is not drawing into, which last frame drew)
		for (size_t k = 0; k < RTTOrder.size(); k++)
			if (RTTOrder[k].Type == RTT::CustomTexture) RTTOrder[k].texture = otherAttachment;
	}

	void ExposureAdaptEffect::PreDraw()
	{
		const f64 now = NowSeconds();
		f32 dt = lastTime < 0.0 ? 0.f : (f32)(now - lastTime);
		lastTime = now;
		if (dt > 0.25f) dt = 0.25f;                 // (a stall does not snap the eye)
		Put(extraUniformsScratch, 0, dt, speedUp, speedDown, 0.f);
		// Next frame draws into the one read now. (The manager bound this frame's target before
		// this ran, and takes its texture from that; the swap only changes what comes next.)
		std::swap(fbo, otherFbo);
		std::swap(attachment, otherAttachment);
	}

	// ------------------------------------------------------------------ apply

	ExposureApplyEffect::ExposureApplyEffect(const uint32 TexColor, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();
		UseRTT(TexColor);
		Build();
	}

	ExposureApplyEffect::ExposureApplyEffect(Texture* color, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();
		UseCustomTexture(color);
		Build();
	}

	void ExposureApplyEffect::Build()
	{
		key = 0.18f; compensation = 0.f; minEV = -2.f; maxEV = 2.f;
		UseRTT(RTT::LastRTT);
		FragmentShaderString = std::string(kPrelude) + kApply;
		CompileShaders();
		extraUniformsBinding = 44;
		extraUniformsBlockName = "ExposureApplyParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
	}

	void ExposureApplyEffect::PreDraw()
	{
		Put(extraUniformsScratch, 0, key > 1e-4f ? key : 1e-4f, compensation, minEV, maxEV);
	}
};
