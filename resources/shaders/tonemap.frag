#version 450
#include "common.glsl"

layout(set = 1, binding = 1) uniform texture2D uScene;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(set = 1, binding = 3) uniform texture2D uBloom;       // (half resolution, the bloom chain's top)
layout(set = 1, binding = 4) uniform texture2D uExposure;    // (1x1: this frame's exposure - auto, or as set)
layout(set = 1, binding = 5) uniform texture2D uDepth;       // (the prepass's: reversed Z, 0 where nothing was drawn)

layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 g0;              // the grade: exposure (stops), highlight shoulder, temperature, tint
	vec4 g1;              // contrast, its pivot, saturation, vibrance
	vec4 lift, gamma, gain;            // rgb, and w for all
	vec4 shadowTint, highlightTint;    // rgb, w strength
	vec4 g2;              // vignette, grain, chromatic aberration, 1 when the scene has a grade
	vec4 g3;              // x: the bloom's share of the picture; yzw the background where nothing is drawn (a scene with no sky's, linear)
	vec4 g4;              // x: the bloom's threshold, y its knee - in exposed light
} uDraw;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// A colourist's grade on the finished picture - the old renderer's GradeEffect, as it was: linear light 0..1 in (past
// the tone curve), graded in a perceptual space (gamma 2.2), linear out
float LumaOf(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float HashG(vec2 p) { vec3 q = fract(vec3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return fract((q.x + q.y) * q.z); }
vec3 Grade(vec3 c)
{
	c *= exp2(uDraw.g0.x);
	float shoulder = uDraw.g0.y;
	if (shoulder > 0.0)
	{
		// (exposure raised on a picture that already reaches white clipped a sunlit street flat: above this, slowly)
		vec3 over = max(c - shoulder, 0.0);
		c = min(c, vec3(shoulder)) + (1.0 - shoulder) * (1.0 - exp(-over / (1.0 - shoulder)));
	}
	// white balance: warm lifts red and lowers blue, tint trades green
	float temperature = uDraw.g0.z, tint = uDraw.g0.w;
	c *= vec3(1.0 + 0.12 * temperature - 0.04 * tint, 1.0 + 0.08 * tint, 1.0 - 0.12 * temperature - 0.04 * tint);
	vec3 g = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
	// lift, gamma, gain
	vec3 lift = uDraw.lift.rgb + uDraw.lift.w, gain = uDraw.gain.rgb * uDraw.gain.w, gam = max(uDraw.gamma.rgb * uDraw.gamma.w, vec3(0.05));
	g = gain * (g + lift * (1.0 - g));
	g = pow(max(g, vec3(0.0)), 1.0 / gam);
	// the S: each side of the pivot bent by the contrast, never leaving 0..1 - film's toe and shoulder
	float contrast = uDraw.g1.x, pivot = clamp(uDraw.g1.y, 0.05, 0.95);
	vec3 x = clamp(g, 0.0, 1.0);
	vec3 lo = pivot * pow(x / pivot, vec3(contrast));
	vec3 hi = 1.0 - (1.0 - pivot) * pow((1.0 - x) / (1.0 - pivot), vec3(contrast));
	g = mix(lo, hi, step(pivot, x)) + max(g - 1.0, vec3(0.0));
	// split toning, by where the pixel sits between black and white
	float l = LumaOf(g);
	float ws = 1.0 - smoothstep(0.0, 0.55, l), wh = smoothstep(0.45, 1.0, l);
	g += (uDraw.shadowTint.rgb - 0.5 * LumaOf(uDraw.shadowTint.rgb)) * uDraw.shadowTint.w * ws;
	g += (uDraw.highlightTint.rgb - 0.5 * LumaOf(uDraw.highlightTint.rgb)) * uDraw.highlightTint.w * wh;
	// saturation, and vibrance: the muted more than the vivid
	l = LumaOf(g);
	float mx = max(g.r, max(g.g, g.b)), mn = min(g.r, min(g.g, g.b));
	float sat = (mx - mn) / max(mx, 1e-4);
	g = mix(vec3(l), g, uDraw.g1.z * (1.0 + uDraw.g1.w * (1.0 - sat)));
	// the vignette, round on the screen whatever its shape
	float aspect = uFrame.viewport.x / max(uFrame.viewport.y, 1.0);
	vec2 v = (vUV - 0.5) * vec2(aspect, 1.0) / max(aspect, 1.0) * 2.0;
	g *= 1.0 - uDraw.g2.x * smoothstep(0.45, 1.35, length(v));
	// grain: new every frame, a touch more in the shadows
	float n = HashG(gl_FragCoord.xy + fract(uFrame.sky.z * 7.31) * 1931.0) - 0.5;
	g += n * uDraw.g2.y * (1.0 - 0.6 * l);
	return pow(max(g, vec3(0.0)), vec3(2.2));
}

// ACES filmic, Stephen Hill's fit of the RRT + ODT (sRGB primaries in, display-referred out)
vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}
vec3 ACESFitted(vec3 c)
{
	const mat3 inM = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
	const mat3 outM = mat3(1.60475, -0.10208, -0.00327, -0.53108, 1.10813, -0.07276, -0.07367, -0.00605, 1.07602);
	return clamp(outM * RRTAndODTFit(inM * c), 0.0, 1.0);
}

void Tonemap()
{
	vec3 hdr = Sane(texture(sampler2D(uScene, uLinearClamp), vUV).rgb);
	if (uDraw.g2.w > 0.5 && uDraw.g2.z > 0.0)
	{
		// (chromatic aberration: a cheap lens's fringes - red pushed out, blue pulled in, more toward the corners)
		vec2 off = (vUV - 0.5) * uDraw.g2.z * dot(vUV - 0.5, vUV - 0.5) * 4.0;
		hdr.r = texture(sampler2D(uScene, uLinearClamp), vUV + off).r;
		hdr.b = texture(sampler2D(uScene, uLinearClamp), vUV - off).b;
	}
	float exposure = texelFetch(sampler2D(uExposure, uLinearClamp), ivec2(0, 0), 0).r;
	// (frame.y negative: no curve, no bloom - the light as it is, exposed and clamped: a bake's picture, a colour map)
	if (uFrame.frame.y < 0.0) { outColor = vec4(clamp(hdr * exposure * -uFrame.frame.y, 0.0, 1.0), 1.0); return; }
	// (a scene with no sky: where nothing was drawn, its background as it was set - a backdrop's colour, not light the
	// scene is exposed for: lifted with a dim room's exposure it turned grey, a teal sea pale)
	// ... but only the background: what is laid in the air over it - particles, smoke, a beam, its glow - is light like
	// the rest and exposed with it (taken whole, a spark or a plume against the backdrop came out several times darker
	// than the same one against a wall)
	vec3 backdrop = vec3(0.0);
	if (uFrame.frame.z < 0.5)
	{
		ivec2 size = textureSize(sampler2D(uDepth, uLinearClamp), 0);
		float depth = texelFetch(sampler2D(uDepth, uLinearClamp), clamp(ivec2(vUV * vec2(size)), ivec2(0), size - 1), 0).r;
		if (depth <= 0.0)
		{
			backdrop = min(uDraw.g3.yzw, hdr);
			hdr -= backdrop;
			if (max(hdr.r, max(hdr.g, hdr.b)) < 1e-5) { outColor = vec4(clamp(backdrop, 0.0, 1.0), 1.0); return; }     // (nothing over it)
		}
	}
	// The bloom: only what of the spread light is past the scene's threshold, exposed (Bloom's uThreshold, with a soft
	// knee), added. Mixed in whole, every scene was a tenth blurred into itself - a haze over everything, and a speck one
	// frame brighter flashed a halo round it; thresholded after the blur, a single bright pixel is spread too thin to
	// pass, and only real highlights - the sun on water, a lamp - glow
	vec3 bloom = texture(sampler2D(uBloom, uLinearClamp), vUV).rgb;
	if (uDraw.g3.x > 0.0)
	{
		float b = max(bloom.r, max(bloom.g, bloom.b)) * exposure * uFrame.frame.y;
		float t = uDraw.g4.x, k = max(uDraw.g4.y * t, 1e-4);
		float soft = clamp(b - t + k, 0.0, 2.0 * k); soft = soft * soft / (4.0 * k);
		float pass = max(soft, b - t) / max(b, 1e-4);
		hdr += bloom * pass * clamp(uDraw.g3.x * 0.6, 0.0, 0.12);     // (a faint glow round what is really bright - a lamp, the sun on water - not a halo over everything)
	}
	vec3 x = hdr * exposure * uFrame.frame.y;
	vec3 sdr = ACESFitted(x);
	// (an HDR screen: the same picture up to the mid-tones, the highlights let past SDR white toward the headroom - the
	// curve's white raised to it, blended in over the upper half so nothing below shifts)
	float headroom = max(uFrame.weather.w, 1.0);
	if (headroom > 1.001)
	{
		vec3 extended = ACESFitted(x / headroom) * headroom;
		sdr = mix(sdr, max(extended, sdr), smoothstep(0.4, 0.95, sdr));
	}
	if (uDraw.g2.w > 0.5) sdr = Grade(sdr);     // (the scene's grade, on the finished picture)
	// (the background, as it was set, under what was drawn over it)
	sdr += clamp(backdrop, 0.0, 1.0) * (1.0 - clamp(max(sdr.r, max(sdr.g, sdr.b)), 0.0, 1.0));
	outColor = vec4(sdr, 1.0);     // (the backbuffer is sRGB, or HDR linear floats: the store or the screen encodes it)
}

void main()
{
	Tonemap();
	// (a view drawn to be laid over something else - a SceneView with a transparent background: where nothing was drawn,
	// nothing - alpha 0 - so what shows it shows only what is in it; the inventory's figure, cut out)
	if (uFrame.weather.z > 0.5)
	{
		// (how much of the pixel and its neighbours was drawn: an edge soft as the picture's own, not stair-stepped)
		ivec2 size = textureSize(sampler2D(uDepth, uLinearClamp), 0);
		ivec2 at = ivec2(vUV * vec2(size));
		float covered = 0.0;
		for (int y = -1; y <= 1; y++)
			for (int x = -1; x <= 1; x++)
				covered += texelFetch(sampler2D(uDepth, uLinearClamp), clamp(at + ivec2(x, y), ivec2(0), size - 1), 0).r > 0.0 ? (x == 0 && y == 0 ? 4.0 : 1.0) : 0.0;
		outColor.a = clamp(covered / 12.0, 0.0, 1.0);
	}
}
