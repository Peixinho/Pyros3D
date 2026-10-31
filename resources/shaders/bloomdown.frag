#version 450
// Bloom, down (Jimenez 2014, "Next Generation Post Processing in Call of Duty: Advanced Warfare"): 13 bilinear taps
// in five overlapping boxes; on the first level each box weighted by its brightness (Karis average) - no fireflies
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 sourceTexel; vec4 params; } uDraw;   // params.x: 1 on the first level
layout(set = 1, binding = 1) uniform texture2D uSource;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
vec3 S(vec2 o) { return texture(sampler2D(uSource, uLinearClamp), vUV + o * uDraw.sourceTexel.xy).rgb; }
float KarisWeight(vec3 c) { return 1.0 / (1.0 + dot(c, vec3(0.2126, 0.7152, 0.0722))); }
void main()
{
	vec3 a = S(vec2(-2, -2)), b = S(vec2(0, -2)), c = S(vec2(2, -2));
	vec3 d = S(vec2(-1, -1)), e = S(vec2(1, -1));
	vec3 f = S(vec2(-2, 0)), g = S(vec2(0, 0)), h = S(vec2(2, 0));
	vec3 i = S(vec2(-1, 1)), j = S(vec2(1, 1));
	vec3 k = S(vec2(-2, 2)), l = S(vec2(0, 2)), m = S(vec2(2, 2));
	vec3 box[5] = vec3[]((d + e + i + j) * 0.25, (a + b + f + g) * 0.25, (b + c + g + h) * 0.25, (f + g + k + l) * 0.25, (g + h + l + m) * 0.25);
	const float wt[5] = float[](0.5, 0.125, 0.125, 0.125, 0.125);
	vec3 sum = vec3(0.0); float wsum = 0.0;
	for (int n = 0; n < 5; n++)
	{
		float w = wt[n] * (uDraw.params.x > 0.5 ? KarisWeight(box[n]) : 1.0);
		sum += box[n] * w; wsum += w;
	}
	vec3 result = sum / wsum;
	// (the first level's light capped: the sun's disc is thousands of times the scene and would spread over all of it)
	if (uDraw.params.x > 0.5) { float l = dot(result, vec3(0.2126, 0.7152, 0.0722)); if (l > 32.0) result *= 32.0 / l; }
	outColor = vec4(result, 1.0);
}
