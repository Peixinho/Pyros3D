#version 450
#include "common.glsl"
// The AO blurred along one direction (DrawData), weighted by depth: a separable bilateral blur at half resolution
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 direction; } uDraw;    // xy: one texel of the AO along the blur
layout(set = 1, binding = 1) uniform texture2D uDepth;
layout(set = 1, binding = 2) uniform sampler uPointClamp;
layout(set = 1, binding = 3) uniform texture2D uAO;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outAO;
// (depth read texel-exact, with no sampler: what a point sampler gave, and in WebGPU a depth texture is not filterable)
float DepthAt(vec2 uv)
{
	ivec2 size = textureSize(sampler2D(uDepth, uPointClamp), 0);
	return texelFetch(sampler2D(uDepth, uPointClamp), clamp(ivec2(uv * vec2(size)), ivec2(0), size - 1), 0).r;
}
float LinearDepth(vec2 uv) { return DepthDistance(DepthAt(uv)); }
void main()
{
	float z0 = LinearDepth(vUV);
	float sum = 0.0, wsum = 0.0;
	for (int i = -3; i <= 3; i++)
	{
		vec2 uv = vUV + uDraw.direction.xy * float(i);
		float z = LinearDepth(uv);
		float w = exp(-float(i * i) / 8.0) * exp(-abs(z - z0) / max(z0 * 0.03, 0.02));
		sum += textureLod(sampler2D(uAO, uPointClamp), uv, 0.0).r * w;
		wsum += w;
	}
	outAO = vec4(sum / max(wsum, 1e-4));
}
