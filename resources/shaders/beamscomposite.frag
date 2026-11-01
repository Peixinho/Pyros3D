#version 450
#include "common.glsl"
// The air's light laid over the picture (blended, premultiplied: rgb what it adds, a what of the picture it hides - the
// fog's): the half-size march brought to this size bilinearly, each
// of its four texels also weighed by how near the depth it marched to is to this pixel's - so a beam does not bleed
// across what stands in front of it.
layout(set = 1, binding = 1) uniform texture2D uBeams;       // (beams.frag's: rgb the light the air adds, a what it hides)
layout(set = 1, binding = 2) uniform texture2D uDepth;       // (the prepass's: reversed Z)
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
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
	ivec2 isz = textureSize(sampler2D(uBeams, uLinearClamp), 0);
	vec2 sz = vec2(isz);
	vec2 pos = vUV * sz - 0.5;
	float here = DistanceAt(ivec2(vUV * vec2(dsize)), dsize);
	ivec2 corner = ivec2(floor(pos));
	vec2 f = pos - vec2(corner);
	vec4 acc = vec4(0.0);
	float wsum = 0.0;
	for (int j = 0; j < 4; j++)
	{
		ivec2 o = ivec2(j & 1, j >> 1);
		ivec2 tc = clamp(corner + o, ivec2(0), isz - 1);
		float there = DistanceAt(ivec2((vec2(tc) + 0.5) / sz * vec2(dsize)), dsize);
		float w = (o.x == 1 ? f.x : 1.0 - f.x) * (o.y == 1 ? f.y : 1.0 - f.y);
		w = (w + 0.001) / (0.01 + abs(there - here) / min(here, 1.0e5));
		acc += texelFetch(sampler2D(uBeams, uLinearClamp), tc, 0) * w;
		wsum += w;
	}
	outColor = acc / max(wsum, 1e-6);
}
