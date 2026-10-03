//============================================================================
// Name        : BloomEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See BloomEffect.h.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>

namespace p3d {

	namespace {
		// Every post-effect shader in this directory repeats this preamble.
		// Vulkan needs a static binding on every sampler and a location on
		// every fragment output; GL needs neither and rejects the layout
		// qualifiers on samplers, hence the macro pair rather than one
		// spelling. VULKAN is predefined by shaderc for a Vulkan-target
		// compile (see SpirvShaderCompiler::Compile).
		const char* kPreamble =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
#if defined(GLES3)
			// highp: bloom works on values above 1.0 by definition - that is
			// the whole point of a threshold - and mediump is fp16 on a lot
			// of mobile/WebGL hardware, which bands badly up there.
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
	}

	BloomBrightPassEffect::BloomBrightPassEffect(const uint32 Tex1, const uint32 Width, const uint32 Height)
		: IEffect(Width, Height)
	{
		UseRTT(Tex1);
		// HDR, or a light at 4.0 and a light at 1.2 become the same texel
		// before the blur ever sees them.
		UseHDRAttachment();

		FragmentShaderString = std::string(kPreamble) +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			// 27, chosen from the free range in IEffect.h's binding registry
			// (25-31 are the post effects'; 25 BlurSSAO, 26 MotionBlur,
			// 28 BlurX, 29 BlurY, 30 DepthOfField). Binding points are one
			// global namespace shared with PyrosShader.glsl, not per-shader.
			"UBO_BINDING(27) uniform BloomBrightPassParams {\n"
			"	float uThreshold;\n"
			"	float uKnee;\n"
			"};\n"
			"vec3 Prefilter(vec3 c) {\n"
			// Rec. 709 luma. The old pass branched on .r alone, so a
			// saturated blue light never bloomed and a dull red one did.
			"	float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
			// Quadratic knee: nothing below (threshold - knee), a smooth
			// ramp across the knee, linear above it. A hard step here is
			// what makes bloom flicker on slowly brightening surfaces.
			"	float k = max(uKnee, 0.0001);\n"
			"	float soft = clamp((luma - uThreshold + k) / (2.0 * k), 0.0, 1.0);\n"
			"	float weight = max(luma - uThreshold, k * soft * soft) / max(luma, 0.0001);\n"
			"	c *= weight;\n"
			// One texel far above the rest otherwise owns the whole mip
			// below it. Dividing by luma keeps the energy but stops the spike.
			"	return c / (1.0 + dot(c, vec3(0.2126, 0.7152, 0.0722)));\n"
			"}\n"
			"void main() {\n"
			// Four taps of the source, half a texel apart: this pass is half
			// the frame, and a single point sample aliases into a flicker
			// as the window size changes which source texel it lands on.
			"	vec2 texel = 0.5 / vec2(textureSize(uTex0, 0));\n"
			"	vec3 c = Prefilter(texture_2D(uTex0, vTexcoord + vec2(-texel.x, -texel.y)).rgb)\n"
			"		+ Prefilter(texture_2D(uTex0, vTexcoord + vec2( texel.x, -texel.y)).rgb)\n"
			"		+ Prefilter(texture_2D(uTex0, vTexcoord + vec2(-texel.x,  texel.y)).rgb)\n"
			"		+ Prefilter(texture_2D(uTex0, vTexcoord + vec2( texel.x,  texel.y)).rgb);\n"
			"	FragColor = vec4(c * 0.25, 1.0);\n"
			"}";

		CompileShaders();

		threshold = 0.8f;
		knee = 0.35f;
		thresholdHandle = AddUniform(Uniform("uThreshold", Uniforms::DataType::Float, &threshold));
		kneeHandle = AddUniform(Uniform("uKnee", Uniforms::DataType::Float, &knee));

		extraUniformsBinding = 27;
		extraUniformsBlockName = "BloomBrightPassParams";
		// std140 rounds the block up to a vec4 even though two floats fit in
		// eight bytes; a short buffer is a dropped draw on WebGL2.
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uThreshold"] = 0;
		extraUniformOffsets["uKnee"] = 4;
	}

	void BloomBrightPassEffect::SetThreshold(const f32 &v) { threshold = v; thresholdHandle->SetValue(&threshold); }
	void BloomBrightPassEffect::SetKnee(const f32 &v) { knee = v; kneeHandle->SetValue(&knee); }

	BloomBrightPassEffect::~BloomBrightPassEffect() {}

	BloomCompositeEffect::BloomCompositeEffect(Texture* base, const uint32 Width, const uint32 Height)
		: IEffect(Width, Height)
	{
		// Order matters: uTex0/uTex1 are named after the order these are
		// declared, so the base has to come first in both constructors.
		UseCustomTexture(base);
		UseRTT(RTT::LastRTT);
		Build(Width, Height);
	}

	BloomCompositeEffect::BloomCompositeEffect(const uint32 baseRTT, const uint32 Width, const uint32 Height)
		: IEffect(Width, Height)
	{
		UseRTT(baseRTT);
		UseRTT(RTT::LastRTT);
		Build(Width, Height);
	}

	void BloomCompositeEffect::Build(const uint32 Width, const uint32 Height)
	{
		UseHDRAttachment();
		FragmentShaderString = std::string(kPreamble) +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
			"UBO_BINDING(31) uniform BloomCompositeParams {\n"
			"	float uIntensity;\n"
			"};\n"
			"void main() {\n"
			"	vec4 base = texture_2D(uTex0, vTexcoord);\n"
			"	vec3 bloom = texture_2D(uTex1, vTexcoord).rgb;\n"
			// Added, not screened or squared. The old pass squared the
			// blurred sum, which meant the bloom's own falloff was the
			// square of the blur kernel and its brightness scaled
			// quadratically with the source - a light twice as bright
			// bloomed four times as hard, so there was no setting that
			// worked for both a lamp and a sky.
			"	FragColor = vec4(base.rgb + bloom * uIntensity, base.a);\n"
			"}";

		CompileShaders();

		intensity = 1.0f;
		intensityHandle = AddUniform(Uniform("uIntensity", Uniforms::DataType::Float, &intensity));

		extraUniformsBinding = 31;
		extraUniformsBlockName = "BloomCompositeParams";
		extraUniformsSize = 16;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uIntensity"] = 0;
	}

	void BloomCompositeEffect::SetIntensity(const f32 &v) { intensity = v; intensityHandle->SetValue(&intensity); }

	BloomCompositeEffect::~BloomCompositeEffect() {}

	namespace {

		uint32 MipSize(const uint32 full, const f32 scale)
		{
			const uint32 v = (uint32)((f32)full * scale);
			return v > 0 ? v : 1;
		}

		// Half of whatever came in. The four taps sit half a texel off the
		// centre, so together they cover the 2x2 that this pixel stands
		// for. A full-texel offset misses that box whenever the mip size
		// is not exactly half (integer truncation), and the miss lines up
		// differently at every window size - squares at one resolution, a
		// grid at the next. Karis weights keep one hot texel from owning
		// the whole tap; a plain average of equal neighbours is unchanged.
		class BloomDownsampleEffect : public IEffect {
		public:
			BloomDownsampleEffect(const uint32 width, const uint32 height) : IEffect(width, height)
			{
				UseRTT(RTT::LastRTT);
				UseHDRAttachment();
				FragmentShaderString = std::string(kPreamble) +
					"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
					"float Luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }\n"
					"void Tap(vec2 uv, inout vec3 sum, inout float wsum) {\n"
					"	vec3 c = texture_2D(uTex0, uv).rgb;\n"
					"	float w = 1.0 / (1.0 + Luma(c));\n"
					"	sum += c * w;\n"
					"	wsum += w;\n"
					"}\n"
					"void main() {\n"
					"	vec2 texel = 0.5 / vec2(textureSize(uTex0, 0));\n"
					"	vec3 sum = vec3(0.0);\n"
					"	float wsum = 0.0;\n"
					"	Tap(vTexcoord + texel * vec2(-1.0, -1.0), sum, wsum);\n"
					"	Tap(vTexcoord + texel * vec2( 1.0, -1.0), sum, wsum);\n"
					"	Tap(vTexcoord + texel * vec2(-1.0,  1.0), sum, wsum);\n"
					"	Tap(vTexcoord + texel * vec2( 1.0,  1.0), sum, wsum);\n"
					"	FragColor = vec4(sum / max(wsum, 0.0001), 1.0);\n"
					"}";
				CompileShaders();
			}
		};

		// The threshold image, stretched up with the same tent and nothing
		// added. Used for the last step so the half-resolution prefilter
		// itself is not composited: that mask is one texel per two screen
		// pixels, and once the highlight clips to white the bilinear ramp
		// disappears and the terminator comes back as blocks. The glow is
		// the blurred mips; the sharp highlight is already in the base.
		class BloomStretchEffect : public IEffect {
		public:
			BloomStretchEffect(Texture* src, const uint32 width, const uint32 height) : IEffect(width, height)
			{
				UseCustomTexture(src);
				UseHDRAttachment();
				FragmentShaderString = std::string(kPreamble) +
					"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
					"void main() {\n"
					"	vec2 t = 1.0 / vec2(textureSize(uTex0, 0));\n"
					"	vec2 uv = vTexcoord;\n"
					"	vec3 s = texture_2D(uTex0, uv + t * vec2(-1.0, -1.0)).rgb\n"
					"		+ texture_2D(uTex0, uv + t * vec2( 0.0, -1.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex0, uv + t * vec2( 1.0, -1.0)).rgb\n"
					"		+ texture_2D(uTex0, uv + t * vec2(-1.0,  0.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex0, uv).rgb * 4.0\n"
					"		+ texture_2D(uTex0, uv + t * vec2( 1.0,  0.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex0, uv + t * vec2(-1.0,  1.0)).rgb\n"
					"		+ texture_2D(uTex0, uv + t * vec2( 0.0,  1.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex0, uv + t * vec2( 1.0,  1.0)).rgb;\n"
					"	FragColor = vec4(s * (1.0 / 16.0), 1.0);\n"
					"}";
				CompileShaders();
			}
		};

		// finer + a 3x3 tent of the coarser mip. One bilinear tap of a mip
		// that is not exactly twice as small lands on a rectangle; the tent
		// overlaps those rectangles, which is what removes the blocks.
		class BloomUpsampleEffect : public IEffect {
		public:
			BloomUpsampleEffect(Texture* finer, Texture* coarser, const uint32 width, const uint32 height) : IEffect(width, height)
			{
				UseCustomTexture(finer);
				UseCustomTexture(coarser);
				UseHDRAttachment();
				FragmentShaderString = std::string(kPreamble) +
					"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
					"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
					"vec3 Tent(vec2 uv) {\n"
					"	vec2 t = 1.0 / vec2(textureSize(uTex1, 0));\n"
					"	vec3 s = texture_2D(uTex1, uv + t * vec2(-1.0, -1.0)).rgb\n"
					"		+ texture_2D(uTex1, uv + t * vec2( 0.0, -1.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex1, uv + t * vec2( 1.0, -1.0)).rgb\n"
					"		+ texture_2D(uTex1, uv + t * vec2(-1.0,  0.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex1, uv).rgb * 4.0\n"
					"		+ texture_2D(uTex1, uv + t * vec2( 1.0,  0.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex1, uv + t * vec2(-1.0,  1.0)).rgb\n"
					"		+ texture_2D(uTex1, uv + t * vec2( 0.0,  1.0)).rgb * 2.0\n"
					"		+ texture_2D(uTex1, uv + t * vec2( 1.0,  1.0)).rgb;\n"
					"	return s * (1.0 / 16.0);\n"
					"}\n"
					"void main() {\n"
					"	FragColor = vec4(texture_2D(uTex0, vTexcoord).rgb + Tent(vTexcoord), 1.0);\n"
					"}";
				CompileShaders();
			}
		};

	}

	void AppendBloom(PostEffectsManager &manager, const uint32 width, const uint32 height,
		const f32 threshold, const f32 knee, const f32 intensity)
	{
		// Whatever ran before us is what the bloom gets added to. On an
		// empty chain that is RTT::Color, the captured scene.
		IEffect* previous = manager.GetLastEffect();

		const f32 scales[4] = { 0.5f, 0.25f, 0.125f, 0.0625f };
		IEffect* level[4];

		BloomBrightPassEffect* pre = new BloomBrightPassEffect(RTT::LastRTT, MipSize(width, scales[0]), MipSize(height, scales[0]));
		pre->SetThreshold(threshold);
		pre->SetKnee(knee);
		pre->SetResizeScale(scales[0]);
		manager.AddEffect(pre);
		level[0] = pre;

		for (int i = 1; i < 4; i++)
		{
			BloomDownsampleEffect* down = new BloomDownsampleEffect(MipSize(width, scales[i]), MipSize(height, scales[i]));
			down->SetResizeScale(scales[i]);
			manager.AddEffect(down);
			level[i] = down;
		}

		// Fold the wide mips back up to the quarter buffer. Stopping there
		// leaves the half-resolution prefilter out of the composite - see
		// BloomStretchEffect.
		IEffect* up = level[3];
		for (int i = 2; i >= 1; i--)
		{
			BloomUpsampleEffect* u = new BloomUpsampleEffect(level[i]->GetTexture(), up->GetTexture(),
				MipSize(width, scales[i]), MipSize(height, scales[i]));
			u->SetResizeScale(scales[i]);
			manager.AddEffect(u);
			up = u;
		}

		BloomStretchEffect* stretch = new BloomStretchEffect(up->GetTexture(), MipSize(width, scales[0]), MipSize(height, scales[0]));
		stretch->SetResizeScale(scales[0]);
		manager.AddEffect(stretch);

		BloomCompositeEffect* composite = (previous != NULL)
			? new BloomCompositeEffect(previous->GetTexture(), width, height)
			: new BloomCompositeEffect(RTT::Color, width, height);
		composite->SetIntensity(intensity);
		// The second input is RTT::LastRTT, which at draw time is whatever
		// ran immediately before - the half-resolution upsample.
		manager.AddEffect(composite);
	}

};
