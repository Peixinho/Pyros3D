//============================================================================
// Name        : AntiAliasingEffects.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : The full-screen passes behind AntiAliasingStage.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/AntiAliasingEffects.h>
#include <Pyros3D/Ext/smaa/SMAAShaderSource.inl>

namespace p3d {

	namespace {

		// Same preamble every effect writes out by hand - see VignetteEffect.
		// highp on GLES3: FXAA's edge search and SMAA's area lookups both
		// work in fractions of a texel, which fp16 cannot hold at any real
		// resolution.
		std::string FragmentPrelude()
		{
			return
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
				"IO_LOCATION(0) varying_in vec2 vTexcoord;\n";
		}

		// Engine-wide UBO bindings, unique per block shape - see
		// IEffect::extraUniformsBinding.
		const uint32 kFXAABinding = 46;
		const uint32 kSMAABinding = 47;
		const uint32 kTAABinding = 48;
	}

	// One vec2 block, shared shape for FXAA and the three SMAA passes.
	#define P3D_RESOLUTION_BLOCK(binding, blockName) \
		AddUniform(Uniform("uResolution", Uniforms::PostEffects::ScreenDimensions)); \
		extraUniformsBinding = binding; \
		extraUniformsBlockName = blockName; \
		extraUniformsSize = 16; \
		extraUniformsScratch.resize(extraUniformsSize, 0); \
		extraUniformOffsets["uResolution"] = 0;

	FXAAEffect::FXAAEffect(const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		P3D_RESOLUTION_BLOCK(kFXAABinding, "FXAAParams")
		UseRTT(RTT::Color);

		FragmentShaderString = FragmentPrelude() +
			"IO_LOCATION(0) out vec4 FragColor;\n"
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"UBO_BINDING(46) uniform FXAAParams {\n"
			"	vec2 uResolution;\n"
			"};\n"
			// Luma of the displayed value: the input may be linear HDR, and
			// an edge between 4.0 and 8.0 is not one anybody can see.
			"float FxaaLuma(vec2 p) { return dot(clamp(textureLod(uTex0, p, 0.0).rgb, 0.0, 1.0), vec3(0.299, 0.587, 0.114)); }\n"
			// A macro, not a function: the offset has to be a compile-time
			// constant, and a function parameter never is.
			"#define FxaaLumaOff(p, o) dot(clamp(textureLodOffset(uTex0, p, 0.0, o).rgb, 0.0, 1.0), vec3(0.299, 0.587, 0.114))\n"
			"void main() {\n"
			"	vec2 rcp = 1.0 / uResolution;\n"
			"	vec2 posM = vTexcoord;\n"
			"	vec4 rgbyM = textureLod(uTex0, posM, 0.0);\n"
			"	float lumaM = dot(clamp(rgbyM.rgb, 0.0, 1.0), vec3(0.299, 0.587, 0.114));\n"
			"	float lumaS = FxaaLumaOff(posM, ivec2( 0, 1));\n"
			"	float lumaE = FxaaLumaOff(posM, ivec2( 1, 0));\n"
			"	float lumaN = FxaaLumaOff(posM, ivec2( 0,-1));\n"
			"	float lumaW = FxaaLumaOff(posM, ivec2(-1, 0));\n"
			"	float rangeMax = max(max(lumaN, lumaW), max(max(lumaE, lumaS), lumaM));\n"
			"	float rangeMin = min(min(lumaN, lumaW), min(min(lumaE, lumaS), lumaM));\n"
			"	float range = rangeMax - rangeMin;\n"
			"	if (range < max(0.0312, rangeMax * 0.125)) { FragColor = vec4(rgbyM.rgb, 1.0); return; }\n"
			"	float lumaNW = FxaaLumaOff(posM, ivec2(-1,-1));\n"
			"	float lumaSE = FxaaLumaOff(posM, ivec2( 1, 1));\n"
			"	float lumaNE = FxaaLumaOff(posM, ivec2( 1,-1));\n"
			"	float lumaSW = FxaaLumaOff(posM, ivec2(-1, 1));\n"
			"	float lumaNS = lumaN + lumaS;\n"
			"	float lumaWE = lumaW + lumaE;\n"
			"	float subpixRcpRange = 1.0 / range;\n"
			"	float subpixNSWE = lumaNS + lumaWE;\n"
			"	float edgeHorz1 = (-2.0 * lumaM) + lumaNS;\n"
			"	float edgeVert1 = (-2.0 * lumaM) + lumaWE;\n"
			"	float lumaNESE = lumaNE + lumaSE;\n"
			"	float lumaNWNE = lumaNW + lumaNE;\n"
			"	float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;\n"
			"	float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;\n"
			"	float lumaNWSW = lumaNW + lumaSW;\n"
			"	float lumaSWSE = lumaSW + lumaSE;\n"
			"	float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);\n"
			"	float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);\n"
			"	float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;\n"
			"	float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;\n"
			"	float edgeHorz = abs(edgeHorz3) + edgeHorz4;\n"
			"	float edgeVert = abs(edgeVert3) + edgeVert4;\n"
			"	float subpixNWSWNESE = lumaNWSW + lumaNESE;\n"
			"	float lengthSign = rcp.x;\n"
			"	bool horzSpan = edgeHorz >= edgeVert;\n"
			"	float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;\n"
			"	if (!horzSpan) { lumaN = lumaW; lumaS = lumaE; }\n"
			"	if (horzSpan) lengthSign = rcp.y;\n"
			"	float subpixB = (subpixA * (1.0 / 12.0)) - lumaM;\n"
			"	float gradientN = lumaN - lumaM;\n"
			"	float gradientS = lumaS - lumaM;\n"
			"	float lumaNN = lumaN + lumaM;\n"
			"	float lumaSS = lumaS + lumaM;\n"
			"	bool pairN = abs(gradientN) >= abs(gradientS);\n"
			"	float gradient = max(abs(gradientN), abs(gradientS));\n"
			"	if (pairN) lengthSign = -lengthSign;\n"
			"	float subpixC = clamp(abs(subpixB) * subpixRcpRange, 0.0, 1.0);\n"
			"	vec2 posB = posM;\n"
			"	vec2 offNP = horzSpan ? vec2(rcp.x, 0.0) : vec2(0.0, rcp.y);\n"
			"	if (!horzSpan) posB.x += lengthSign * 0.5;\n"
			"	if (horzSpan) posB.y += lengthSign * 0.5;\n"
			// Preset 12's step schedule.
			"	const float steps[5] = float[5](1.0, 1.5, 2.0, 4.0, 12.0);\n"
			"	vec2 posN = posB - offNP * steps[0];\n"
			"	vec2 posP = posB + offNP * steps[0];\n"
			"	float subpixD = (-2.0 * subpixC) + 3.0;\n"
			"	float subpixE = subpixC * subpixC;\n"
			"	if (!pairN) lumaNN = lumaSS;\n"
			"	float gradientScaled = gradient * 0.25;\n"
			"	float lumaMM = lumaM - lumaNN * 0.5;\n"
			"	float subpixF = subpixD * subpixE;\n"
			"	bool lumaMLTZero = lumaMM < 0.0;\n"
			"	float lumaEndN = FxaaLuma(posN) - lumaNN * 0.5;\n"
			"	float lumaEndP = FxaaLuma(posP) - lumaNN * 0.5;\n"
			"	bool doneN = abs(lumaEndN) >= gradientScaled;\n"
			"	bool doneP = abs(lumaEndP) >= gradientScaled;\n"
			"	for (int i = 1; i < 5; i++) {\n"
			"		if (doneN && doneP) break;\n"
			"		if (!doneN) { posN -= offNP * steps[i]; lumaEndN = FxaaLuma(posN) - lumaNN * 0.5; doneN = abs(lumaEndN) >= gradientScaled; }\n"
			"		if (!doneP) { posP += offNP * steps[i]; lumaEndP = FxaaLuma(posP) - lumaNN * 0.5; doneP = abs(lumaEndP) >= gradientScaled; }\n"
			"	}\n"
			"	float dstN = horzSpan ? (posM.x - posN.x) : (posM.y - posN.y);\n"
			"	float dstP = horzSpan ? (posP.x - posM.x) : (posP.y - posM.y);\n"
			"	bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;\n"
			"	bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;\n"
			"	float spanLength = dstP + dstN;\n"
			"	bool directionN = dstN < dstP;\n"
			"	float dst = min(dstN, dstP);\n"
			"	bool goodSpan = directionN ? goodSpanN : goodSpanP;\n"
			"	float subpixG = subpixF * subpixF;\n"
			"	float pixelOffset = (dst * (-1.0 / spanLength)) + 0.5;\n"
			"	float subpixH = subpixG * 0.75;\n"
			"	float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;\n"
			"	float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);\n"
			"	if (!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;\n"
			"	if (horzSpan) posM.y += pixelOffsetSubpix * lengthSign;\n"
			"	FragColor = vec4(textureLod(uTex0, posM, 0.0).rgb, 1.0);\n"
			"}\n";

		CompileShaders();
	}

	namespace {
		// SMAA's own source, configured for GLSL. SMAA_RT_METRICS is
		// (1/w, 1/h, w, h) in its convention.
		std::string SMAAHeader()
		{
			std::string s = FragmentPrelude() +
				"IO_LOCATION(0) out vec4 FragColor;\n"
				"UBO_BINDING(47) uniform SMAAParams {\n"
				"	vec2 uResolution;\n"
				"};\n"
				"#define SMAA_GLSL_3\n"
				"#define SMAA_PRESET_HIGH\n"
				"#define SMAA_RT_METRICS vec4(1.0 / uResolution, uResolution)\n";
			// Only the helper half runs here - the reference splits its
			// passes into a VS that precomputes offsets and a PS that uses
			// them, and with a shared full-screen vertex shader the "VS" half
			// is just called at the top of main().
			s += "#define SMAA_INCLUDE_VS 1\n";
			s += "#define SMAA_INCLUDE_PS 1\n";
			s += kSMAAShaderSource;
			return s;
		}
	}

	SMAAEdgeEffect::SMAAEdgeEffect(const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		P3D_RESOLUTION_BLOCK(kSMAABinding, "SMAAParams")
		UseRTT(RTT::Color);

		FragmentShaderString = SMAAHeader() +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"void main() {\n"
			"	vec4 offset[3];\n"
			"	SMAAEdgeDetectionVS(vTexcoord, offset);\n"
			"	vec2 e = SMAALumaEdgeDetectionPS(vTexcoord, offset, uTex0);\n"
			"	FragColor = vec4(e, 0.0, 1.0);\n"
			"}\n";

		CompileShaders();
	}

	SMAAWeightEffect::SMAAWeightEffect(Texture* edges, Texture* areaTex, Texture* searchTex, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		P3D_RESOLUTION_BLOCK(kSMAABinding, "SMAAParams")
		UseCustomTexture(edges);
		UseCustomTexture(areaTex);
		UseCustomTexture(searchTex);

		FragmentShaderString = SMAAHeader() +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
			"SAMPLER_BINDING(2) uniform sampler2D uTex2;\n"
			"void main() {\n"
			"	vec2 pixcoord;\n"
			"	vec4 offset[3];\n"
			"	SMAABlendingWeightCalculationVS(vTexcoord, pixcoord, offset);\n"
			"	FragColor = SMAABlendingWeightCalculationPS(vTexcoord, pixcoord, offset, uTex0, uTex1, uTex2, vec4(0.0));\n"
			"}\n";

		CompileShaders();
	}

	SMAABlendEffect::SMAABlendEffect(Texture* weights, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		P3D_RESOLUTION_BLOCK(kSMAABinding, "SMAAParams")
		UseRTT(RTT::Color);
		UseCustomTexture(weights);

		FragmentShaderString = SMAAHeader() +
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n"
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n"
			"void main() {\n"
			"	vec4 offset;\n"
			"	SMAANeighborhoodBlendingVS(vTexcoord, offset);\n"
			"	FragColor = vec4(SMAANeighborhoodBlendingPS(vTexcoord, offset, uTex0, uTex1).rgb, 1.0);\n"
			"}\n";

		CompileShaders();
	}

	#undef P3D_RESOLUTION_BLOCK

	TAAResolveEffect::TAAResolveEffect(Texture* velocity, Texture* velocityDepth, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();

		Matrix identity;
		reprojectUniform = AddUniform(Uniform("uReproject", Uniforms::DataType::Matrix, &identity));
		Vec4 params(0.f, 0.f, 0.f, 0.f);
		paramsUniform = AddUniform(Uniform("uParams", Uniforms::DataType::Vec4, &params));
		extraUniformsBinding = kTAABinding;
		extraUniformsBlockName = "TAAParams";
		extraUniformsSize = 80;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uReproject"] = 0;
		extraUniformOffsets["uParams"] = 64;

		UseRTT(RTT::Color);
		UseCustomTexture(velocity);
		UseCustomTexture(velocityDepth);

		FragmentShaderString = FragmentPrelude() +
			"IO_LOCATION(0) out vec4 FragColor;\n"
			"SAMPLER_BINDING(0) uniform sampler2D uTex0;\n" // current frame, jittered
			"SAMPLER_BINDING(1) uniform sampler2D uTex1;\n" // velocity, UV/frame
			"SAMPLER_BINDING(2) uniform sampler2D uTex2;\n" // velocity pass depth
			"SAMPLER_BINDING(3) uniform sampler2D uTex3;\n" // history
			"UBO_BINDING(48) uniform TAAParams {\n"
			"	mat4 uReproject;\n"
			"	vec4 uParams;\n" // x: history valid
			"};\n"
			"vec3 ToYCoCg(vec3 c) { return vec3(0.25*c.r + 0.5*c.g + 0.25*c.b, 0.5*c.r - 0.5*c.b, -0.25*c.r + 0.5*c.g - 0.25*c.b); }\n"
			"vec3 FromYCoCg(vec3 c) { return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }\n"
			// Texture v against clip-space y. Rendered images put the top row
			// first on Vulkan and Metal and the bottom row first on GL - the
			// same split the editor's ImGui::Image UVs make.
			"#if defined(VULKAN)\n"
			"vec2 UVToNDC(vec2 uv) { return vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); }\n"
			"vec2 NDCToUV(vec2 n) { return vec2(n.x * 0.5 + 0.5, 0.5 - n.y * 0.5); }\n"
			"#else\n"
			"vec2 UVToNDC(vec2 uv) { return uv * 2.0 - 1.0; }\n"
			"vec2 NDCToUV(vec2 n) { return n * 0.5 + 0.5; }\n"
			"#endif\n"
			// The history, read through a Catmull-Rom filter (nine filtered taps for
			// its sixteen texels). It is read between texels every frame the
			// picture moves, and read plainly each of those readings is a blur
			// laid over the last: a turning camera wiped the grass smooth.
			"vec3 History(vec2 uv) {\n"
			"	vec2 size = vec2(textureSize(uTex3, 0));\n"
			"	vec2 inv = 1.0 / size;\n"
			"	vec2 sp = uv * size;\n"
			"	vec2 tc = floor(sp - 0.5) + 0.5;\n"
			"	vec2 f = sp - tc;\n"
			"	vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));\n"
			"	vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);\n"
			"	vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));\n"
			"	vec2 w3 = f * f * (-0.5 + 0.5 * f);\n"
			"	vec2 w12 = w1 + w2;\n"
			"	vec2 t0 = (tc - 1.0) * inv;\n"
			"	vec2 t3 = (tc + 2.0) * inv;\n"
			"	vec2 t12 = (tc + w2 / w12) * inv;\n"
			"	return texture(uTex3, vec2(t0.x, t0.y)).rgb * (w0.x * w0.y)\n"
			"		+ texture(uTex3, vec2(t12.x, t0.y)).rgb * (w12.x * w0.y)\n"
			"		+ texture(uTex3, vec2(t3.x, t0.y)).rgb * (w3.x * w0.y)\n"
			"		+ texture(uTex3, vec2(t0.x, t12.y)).rgb * (w0.x * w12.y)\n"
			"		+ texture(uTex3, vec2(t12.x, t12.y)).rgb * (w12.x * w12.y)\n"
			"		+ texture(uTex3, vec2(t3.x, t12.y)).rgb * (w3.x * w12.y)\n"
			"		+ texture(uTex3, vec2(t0.x, t3.y)).rgb * (w0.x * w3.y)\n"
			"		+ texture(uTex3, vec2(t12.x, t3.y)).rgb * (w12.x * w3.y)\n"
			"		+ texture(uTex3, vec2(t3.x, t3.y)).rgb * (w3.x * w3.y);\n"
			"}\n"
			"void main() {\n"
			"	vec2 uv = vTexcoord;\n"
			"	vec3 cur = max(texture(uTex0, uv).rgb, vec3(0.0));\n"
			"	if (uParams.x < 0.5) { FragColor = vec4(cur, 1.0); return; }\n"
			"	vec2 texel = 1.0 / vec2(textureSize(uTex0, 0));\n"
			// Neighbourhood statistics for the history clip, and the nearest
			// depth's texel for the velocity - dilating to the front-most
			// neighbour keeps a moving edge from dragging the background's
			// (zero) velocity across it.
			"	vec3 m1 = vec3(0.0), m2 = vec3(0.0);\n"
			"	vec3 mn = vec3(1e9), mx = vec3(-1e9);\n"
			"	float bestDepth = 2.0; vec2 bestUV = uv;\n"
			"	for (int y = -1; y <= 1; y++) {\n"
			"		for (int x = -1; x <= 1; x++) {\n"
			"			vec2 o = vec2(float(x), float(y)) * texel;\n"
			"			vec3 c = ToYCoCg(max(texture(uTex0, uv + o).rgb, vec3(0.0)));\n"
			"			m1 += c; m2 += c * c; mn = min(mn, c); mx = max(mx, c);\n"
			"			float d = texture(uTex2, uv + o).r;\n"
			"			if (d < bestDepth) { bestDepth = d; bestUV = uv + o; }\n"
			"		}\n"
			"	}\n"
			"	vec2 velocity;\n"
			"	if (bestDepth >= 0.99999) {\n"
			// Nothing drawn here in the velocity pass - sky or background.
			// Reproject the far plane with the camera's own motion instead
			// of reading a velocity nobody wrote.
			"		vec4 prev = uReproject * vec4(UVToNDC(uv), 1.0, 1.0);\n"
			"		velocity = (prev.w > 0.0) ? uv - NDCToUV(prev.xy / prev.w) : vec2(0.0);\n"
			"	} else {\n"
			"		velocity = texture(uTex1, bestUV).rg;\n"
			// The velocity shader differences clip-space UVs, which on Metal
			// run the opposite way to texture v (see IEffect's vertex shader).
			"#if defined(METAL)\n"
			"		velocity.y = -velocity.y;\n"
			"#endif\n"
			"	}\n"
			"	vec2 prevUV = uv - velocity;\n"
			"	if (any(lessThan(prevUV, vec2(0.0))) || any(greaterThan(prevUV, vec2(1.0)))) { FragColor = vec4(cur, 1.0); return; }\n"
			"	vec3 hist = ToYCoCg(max(History(prevUV), vec3(0.0)));\n"
			// Variance clip, bounded by the min/max box: tighter than the box
			// alone, so stale history from a disocclusion goes faster.
			"	vec3 mean = m1 / 9.0;\n"
			"	vec3 sigma = sqrt(max(m2 / 9.0 - mean * mean, vec3(0.0)));\n"
			"	vec3 lo = max(mn, mean - 1.25 * sigma);\n"
			"	vec3 hi = min(mx, mean + 1.25 * sigma);\n"
			"	hist = FromYCoCg(clamp(hist, lo, hi));\n"
			// Luma-weighted blend: in HDR one firefly texel otherwise
			// dominates the average and flickers for frames.
			"	float lc = dot(cur, vec3(0.299, 0.587, 0.114));\n"
			"	float lh = dot(hist, vec3(0.299, 0.587, 0.114));\n"
			// The share of this frame: a tenth standing still, up to nearly half
			// when the picture is moving under the pixel - a moving thing is
			// then made of fewer, newer frames and smears less.
			"	float moved = clamp(length(velocity * vec2(textureSize(uTex0, 0))) * 0.5, 0.0, 1.0);\n"
			"	float share = mix(0.1, 0.45, moved);\n"
			"	float wc = share / (1.0 + lc);\n"
			"	float wh = (1.0 - share) / (1.0 + lh);\n"
			"	FragColor = vec4((cur * wc + hist * wh) / (wc + wh), 1.0);\n"
			"}\n";

		CompileShaders();
	}

	void TAAResolveEffect::SetHistory(Texture* history)
	{
		UseCustomTexture(history);
	}

	void TAAResolveEffect::SetFrameParams(const Matrix &reproject, const bool historyValid)
	{
		reprojectUniform->SetValue((void*)&reproject);
		Vec4 params(historyValid ? 1.f : 0.f, 0.f, 0.f, 0.f);
		paramsUniform->SetValue(&params);
	}

	MSDepthResolveEffect::MSDepthResolveEffect(Texture* msDepth, Texture* target, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		// Depth-only target: the capture's single-sample depth itself.
		delete fbo;
		delete attachment;
		attachment = NULL;
		fbo = new FrameBuffer();
		fbo->SetDebugName("MSAA depth resolve");
		fbo->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, target);

		UseCustomTexture(msDepth);

		FragmentShaderString = FragmentPrelude() +
			"SAMPLER_BINDING(0) uniform sampler2DMS uTex0;\n"
			"void main() {\n"
			"	gl_FragDepth = texelFetch(uTex0, ivec2(gl_FragCoord.xy), 0).r;\n"
			"}\n";

		CompileShaders();
	}

}
