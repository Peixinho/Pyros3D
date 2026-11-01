#version 450
#include "common.glsl"
// The smoke laid over the picture (blended, premultiplied: out rgb is what it adds, out a what of the picture it
// hides): the half-size march brought to this size bilinearly, each of its four texels also weighed by how near the
// depth it marched to is to this pixel's - so smoke does not bleed across what stands in front of it. And a screen flash (a
// stun grenade's): the whole picture washed toward a colour.
layout(set = 1, binding = 1) uniform texture2D uSmoke;       // (smoke.frag's: rgb light, a what of the scene still shows)
layout(set = 1, binding = 2) uniform texture2D uDepth;       // (the prepass's: reversed Z)
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
layout(set = 1, binding = 4) uniform texture2D uSmokeReflect;  // (smoke.frag's along the mirrored rays: rgb light, a what of the reflection it hides)
layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 flash;      // rgb the colour, a how much (0 none, 1 nothing but it)
	vec4 params;     // x: 1 when there is smoke to lay (else only the flash)
} uDraw;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float DistanceAt(ivec2 px, ivec2 size)
{
	float d = texelFetch(sampler2D(uDepth, uLinearClamp), clamp(px, ivec2(0), size - 1), 0).r;
	return d > 0.0 ? DepthDistance(d) : 1.0e6;
}

void main()
{
	ivec2 dsize = textureSize(sampler2D(uDepth, uLinearClamp), 0);
	ivec2 isz = textureSize(sampler2D(uSmoke, uLinearClamp), 0);
	vec2 sz = vec2(isz);
	vec2 pos = vUV * sz - 0.5;
	float here = DistanceAt(ivec2(vUV * vec2(dsize)), dsize);
	// The 2x2 half-size texels round this pixel (the march's grain moves each frame, and the temporal resolve
	// after this averages it: no wider filter, which only blurs the billows)
	ivec2 corner = ivec2(floor(pos));
	vec2 f = pos - vec2(corner);
	vec4 acc = vec4(0.0), accR = vec4(0.0);
	float wsum = 0.0;
	for (int j = 0; j < 4; j++)
	{
		ivec2 o = ivec2(j & 1, j >> 1);
		ivec2 tc = clamp(corner + o, ivec2(0), isz - 1);
		float there = DistanceAt(ivec2((vec2(tc) + 0.5) / sz * vec2(dsize)), dsize);
		float w = (o.x == 1 ? f.x : 1.0 - f.x) * (o.y == 1 ? f.y : 1.0 - f.y);
		w = (w + 0.001) / (0.01 + abs(there - here) / min(here, 1.0e5));
		acc += texelFetch(sampler2D(uSmoke, uLinearClamp), tc, 0) * w;
		accR += texelFetch(sampler2D(uSmokeReflect, uLinearClamp), tc, 0) * w;
		wsum += w;
	}
	vec4 smoke = uDraw.params.x > 0.5 ? acc / max(wsum, 1e-6) : vec4(0.0, 0.0, 0.0, 1.0);
	vec4 mirrored = uDraw.params.x > 0.5 ? accR / max(wsum, 1e-6) : vec4(0.0);
	// The reflection's smoke into the picture first (the picture taken for what it mirrors, a mirror floor's
	// being nearly all its reflection), the smoke in front over that, then all washed toward the flash: as one
	// premultiplied blend - picture x what shows + what is added
	float a = clamp(uDraw.flash.a, 0.0, 1.0);
	float shows = smoke.a * (1.0 - mirrored.a) * (1.0 - a);
	outColor = vec4((smoke.rgb + smoke.a * mirrored.rgb) * (1.0 - a) + uDraw.flash.rgb * a, 1.0 - shows);
}
