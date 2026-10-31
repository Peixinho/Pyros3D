#version 450
#include "common.glsl"

layout(set = 1, binding = 1) uniform texture2D uScene;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(set = 1, binding = 3) uniform texture2D uBloom;       // (half resolution, the bloom chain's top)
layout(set = 1, binding = 4) uniform texture2D uExposure;    // (1x1: this frame's exposure - auto, or as set)
layout(set = 1, binding = 5) uniform texture2D uDepth;       // (the prepass's: reversed Z, 0 where nothing was drawn)

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

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
	vec3 hdr = texture(sampler2D(uScene, uLinearClamp), vUV).rgb;
	float exposure = texelFetch(sampler2D(uExposure, uLinearClamp), ivec2(0, 0), 0).r;
	// (frame.y negative: no curve, no bloom - the light as it is, exposed and clamped: a bake's picture, a colour map)
	if (uFrame.frame.y < 0.0) { outColor = vec4(clamp(hdr * exposure * -uFrame.frame.y, 0.0, 1.0), 1.0); return; }
	// (a scene with no sky: where nothing was drawn, its background as it was set - a backdrop's colour, not light the
	// scene is exposed for: lifted with a dim room's exposure it turned grey, a teal sea pale)
	if (uFrame.frame.z < 0.5)
	{
		ivec2 size = textureSize(sampler2D(uDepth, uLinearClamp), 0);
		float depth = texelFetch(sampler2D(uDepth, uLinearClamp), clamp(ivec2(vUV * vec2(size)), ivec2(0), size - 1), 0).r;
		if (depth <= 0.0) { outColor = vec4(clamp(hdr, 0.0, 1.0), 1.0); return; }
	}
	vec3 bloom = texture(sampler2D(uBloom, uLinearClamp), vUV).rgb;
	hdr = mix(hdr, bloom, 0.03);                                 // (energy kept: bloom is light spread, not added)
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
