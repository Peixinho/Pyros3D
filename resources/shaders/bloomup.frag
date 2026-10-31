#version 450
// Bloom, up: a 3x3 tent of the smaller level, added onto this one (the pipeline blends additively)
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 sourceTexel; vec4 params; } uDraw;   // params.x: the radius in texels
layout(set = 1, binding = 1) uniform texture2D uSource;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
vec3 S(vec2 o) { return texture(sampler2D(uSource, uLinearClamp), vUV + o * uDraw.sourceTexel.xy * uDraw.params.x).rgb; }
void main()
{
	vec3 c = S(vec2(0, 0)) * 4.0
		+ (S(vec2(-1, 0)) + S(vec2(1, 0)) + S(vec2(0, -1)) + S(vec2(0, 1))) * 2.0
		+ (S(vec2(-1, -1)) + S(vec2(1, -1)) + S(vec2(-1, 1)) + S(vec2(1, 1)));
	outColor = vec4(c / 16.0, 1.0);
}
