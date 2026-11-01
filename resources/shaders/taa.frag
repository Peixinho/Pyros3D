#version 450
#include "common.glsl"
// Temporal anti-aliasing: each frame drawn a sub-pixel off (the projection jittered), blended into the last frame's
// result where that pixel's surface was then (reprojected through the depth prepass, the matrices without their
// jitter: a still camera moves nothing). Last frame's sampled sharp (Catmull-Rom), clamped to what this frame's 3x3
// neighbourhood allows (in YCoCg, by its spread: a disoccluded or changed pixel takes this frame's), and the blend
// weighted by brightness (a lone bright sample does not flicker).
layout(set = 1, binding = 1) uniform texture2D uCurrent;      // (this frame, resolved: linear HDR)
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(set = 1, binding = 3) uniform texture2D uHistory;      // (last frame's result)
layout(set = 1, binding = 4) uniform texture2D uDepth;        // (the depth prepass, reversed Z)
layout(set = 1, binding = 5, std140) uniform TaaData
{
	mat4 inverseViewProjection;      // (this frame's, without its jitter)
	mat4 previousViewProjection;     // (last frame's, without its jitter)
	mat4 viewProjection;             // (this frame's, without its jitter)
	vec4 params;                     // x: 1 where last frame's result can be used, y: how much of this frame goes in; zw: this
	                                 // frame's jitter (uv) - where it drew a point; with fewer pixels drawn than resolved (a render
	                                 // scale), the frame is read there, between its pixels
	vec4 reactive;                   // x: 1 the full-size particle layer drawn this frame, y: the half-size one
} uTaa;
layout(set = 1, binding = 6) uniform texture2D uVelocity;     // (the prepass's: each surface's move on screen; 1e4 where none was drawn)
// The particles' layers (premultiplied: rgb their light, a their cover) - full size and half: where they are, this frame
// taken over the history, which knows nothing of how they moved (they draw no motion) and would smear and tint a spark
layout(set = 1, binding = 7) uniform texture2D uParticles;
layout(set = 1, binding = 8) uniform texture2D uHalfParticles;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float Luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
vec3 Weigh(vec3 c) { return c / (1.0 + Luma(c)); }                 // (the bright brought into range)
// (back out of range - kept below 1 first: the sharpen below extrapolates, and over a high-contrast flicker - a moving
// dither, a plant dissolving - it could reach 1 or past it, where this divided by nothing or went negative: black)
vec3 Unweigh(vec3 c) { c = max(c, vec3(0.0)); float l = Luma(c); if (l > 0.98) c *= 0.98 / l; return c / max(1.0 - Luma(c), 1e-4); }
vec3 ToYCoCg(vec3 c) { return vec3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }
vec3 FromYCoCg(vec3 c) { return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

// (last frame's colour brought inside the box this frame's neighbourhood allows - along the line to its centre, not
// each channel on its own: clamped apart, a blade's green and the ground's took a hue neither had, yellow-brown)
vec3 ClipToBox(vec3 h, vec3 bmin, vec3 bmax)
{
	vec3 c = 0.5 * (bmin + bmax), e = max(0.5 * (bmax - bmin), vec3(1e-5));
	vec3 v = h - c;
	vec3 a = abs(v / e);
	float m = max(a.x, max(a.y, a.z));
	return m > 1.0 ? c + v / m : h;
}

// Catmull-Rom from five bilinear taps (the corners' weights are small enough to drop): sharp where bilinear blurs
vec3 History(vec2 uv, vec2 size)
{
	vec2 pos = uv * size, center = floor(pos - 0.5) + 0.5, f = pos - center;
	vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f), w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
	vec2 w12 = w1 + w2, t0 = (center - 1.0) / size, t3 = (center + 2.0) / size, t12 = (center + w2 / w12) / size;
	vec3 c = textureLod(sampler2D(uHistory, uLinearClamp), vec2(t12.x, t0.y), 0.0).rgb * w12.x * w0.y
		+ textureLod(sampler2D(uHistory, uLinearClamp), vec2(t0.x, t12.y), 0.0).rgb * w0.x * w12.y
		+ textureLod(sampler2D(uHistory, uLinearClamp), t12, 0.0).rgb * w12.x * w12.y
		+ textureLod(sampler2D(uHistory, uLinearClamp), vec2(t3.x, t12.y), 0.0).rgb * w3.x * w12.y
		+ textureLod(sampler2D(uHistory, uLinearClamp), vec2(t12.x, t3.y), 0.0).rgb * w12.x * w3.y;
	float wsum = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
	return max(c / wsum, vec3(0.0));
}

void main()
{
	ivec2 size = textureSize(sampler2D(uCurrent, uLinearClamp), 0);
	ivec2 outSize = textureSize(sampler2D(uHistory, uLinearClamp), 0);
	// (drawn at fewer pixels than this resolves to - a render scale: this pixel's place among the frame's)
	bool upscale = outSize.x != size.x || outSize.y != size.y;
	ivec2 px = upscale ? clamp(ivec2(vUV * vec2(size)), ivec2(0), size - 1) : ivec2(gl_FragCoord.xy);
	// This frame's neighbourhood: its mean and spread, and the nearest depth among it (edges move with what is in front)
	vec3 m1 = vec3(0.0), m2 = vec3(0.0), center = vec3(0.0);
	float nearest = 0.0;
	ivec2 nearestAt = px;
	for (int y = -1; y <= 1; y++)
		for (int x = -1; x <= 1; x++)
		{
			ivec2 q = clamp(px + ivec2(x, y), ivec2(0), size - 1);
			vec3 c = Weigh(Sane(texelFetch(sampler2D(uCurrent, uLinearClamp), q, 0).rgb));
			vec3 yc = ToYCoCg(c);
			m1 += yc; m2 += yc * yc;
			if (x == 0 && y == 0) center = c;
			float d = texelFetch(sampler2D(uDepth, uLinearClamp), q, 0).r;
			if (d > nearest) { nearest = d; nearestAt = q; }
		}
	vec3 mean = m1 / 9.0, sigma = sqrt(max(m2 / 9.0 - mean * mean, vec3(0.0)));
	// (upscaling: this frame at this very point - between its pixels, where its jitter put the picture)
	if (upscale) center = Weigh(textureLod(sampler2D(uCurrent, uLinearClamp), vUV + uTaa.params.zw, 0.0).rgb);
	if (uTaa.params.x < 0.5) { outColor = vec4(Unweigh(center), 1.0); return; }
	// Where this pixel's surface was last frame: by its own move where the prepass drew it (a thing that moves with the
	// camera - a vehicle, a weapon - is still on screen); else through the depth (the sky - none - as far as can be)
	vec2 moved = texelFetch(sampler2D(uVelocity, uLinearClamp), nearestAt, 0).rg;
	vec2 prev;
	if (abs(moved.x) < 1000.0) prev = vUV - moved;
	else
	{
		vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
		vec4 world = uTaa.inverseViewProjection * vec4(ndc, max(nearest, 1e-6), 1.0);
		vec4 prevClip = uTaa.previousViewProjection * vec4(world.xyz / world.w, 1.0);
		if (prevClip.w <= 0.0) { outColor = vec4(Unweigh(center), 1.0); return; }
		prev = vec2(prevClip.x / prevClip.w * 0.5 + 0.5, 0.5 - prevClip.y / prevClip.w * 0.5);
	}
	if (any(lessThan(prev, vec2(0.0))) || any(greaterThan(prev, vec2(1.0)))) { outColor = vec4(Unweigh(center), 1.0); return; }
	// Last frame's, held to what this neighbourhood allows
	vec3 history = ToYCoCg(Weigh(Sane(History(prev, vec2(outSize)))));
	// (the spread's box, not the neighbourhood's own lowest and highest: held to those, last frame's could never settle on
	// fine detail - grass, a texture's grain - and the picture followed this frame's jitter, the whole scene shaking a
	// pixel; the sharp fetch's ringing below black is caught where it is turned back, Unweigh)
	history = ClipToBox(history, mean - 1.25 * sigma, mean + 1.25 * sigma);
	// (more of this frame where things move fast: their history is resampled every frame, and softens)
	float speed = length((vUV - prev) * vec2(outSize));
	float blend = mix(uTaa.params.y, 0.35, clamp(speed / 12.0, 0.0, 1.0));
	// (where particles are: this frame's - by how much of the pixel they make, their cover or their light)
	float reactive = 0.0;
	if (uTaa.reactive.x > 0.5)
	{
		vec4 p = texelFetch(sampler2D(uParticles, uLinearClamp), px, 0);
		reactive = max(p.a, Luma(p.rgb) / (Luma(p.rgb) + Luma(Unweigh(center)) * 0.5 + 1e-4));
	}
	if (uTaa.reactive.y > 0.5)
	{
		vec4 p = textureLod(sampler2D(uHalfParticles, uLinearClamp), vUV, 0.0);
		reactive = max(reactive, max(p.a, Luma(p.rgb) / (Luma(p.rgb) + Luma(Unweigh(center)) * 0.5 + 1e-4)));
	}
	blend = mix(blend, 1.0, clamp(reactive * 1.5, 0.0, 0.95));
	// (less of this frame where it disagrees with what the picture held - a speck the size of a pixel, a flower's petal,
	// in one frame's samples and not the next: taken at the usual share it blinked, lit a frame or few at a time. Its
	// history held to the neighbourhood already, a real change agrees with it and goes in as before)
	{
		float lc = Luma(center), lh = Luma(FromYCoCg(history));
		// (only a pixel suddenly brighter than its history - a sparkle: held back over grass's every disagreement, its
		// detail was averaged away, the field one flat colour)
		float disagree = clamp((lc - lh * 1.4) / max(lc, 0.05), 0.0, 1.0);
		blend = mix(blend, blend * 0.08, smoothstep(0.0, 0.5, disagree) * (1.0 - clamp(speed / 6.0, 0.0, 1.0)));
	}
	vec3 result = mix(FromYCoCg(history), center, blend);
	// A light sharpen, by the neighbourhood's own contrast (what the resolve's filtering takes back)
	vec3 meanRGB = FromYCoCg(mean);
	float contrast = clamp(Luma(sigma) * 8.0, 0.0, 1.0);
	result = max(result + (result - meanRGB) * 0.25 * (1.0 - contrast), vec3(0.0));
	outColor = vec4(Sane(Unweigh(result)), 1.0);
}
