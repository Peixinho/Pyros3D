#version 450
// Auto exposure, the reduction: each texel the sum of the 4x4 below it (weighted log luminance in r, weights in g)
layout(set = 1, binding = 1) uniform texture2D uSource;
layout(set = 1, binding = 2) uniform sampler uPointClamp;
layout(location = 0) out vec4 outColor;
void main()
{
	ivec2 base = ivec2(gl_FragCoord.xy) * 4;
	vec2 sum = vec2(0.0);
	for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) sum += texelFetch(sampler2D(uSource, uPointClamp), base + ivec2(x, y), 0).rg;
	outColor = vec4(sum, 0.0, 1.0);
}
