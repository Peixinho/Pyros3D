#version 450
// The sky's spherical harmonics, second stage: each coefficient the sum of its sixteen rows (skysh.frag)
layout(set = 1, binding = 1) uniform texture2D uPartial;
layout(set = 1, binding = 2) uniform sampler uPointClamp;
layout(location = 0) out vec4 outColor;
void main()
{
	int k = int(gl_FragCoord.x);
	vec3 sum = vec3(0.0);
	for (int r = 0; r < 16; r++) sum += texelFetch(sampler2D(uPartial, uPointClamp), ivec2(k, r), 0).rgb;
	outColor = vec4(sum, 1.0);
}
