#version 450
layout(set = 1, binding = 0) uniform texture2D tex;
layout(set = 1, binding = 1) uniform sampler smp;
layout(location = 0) in vec2 vuv;
layout(location = 1) in vec3 vn;
layout(location = 2) in vec4 vtint;
layout(location = 0) out vec4 color;
void main()
{
	float l = max(dot(normalize(vn), normalize(vec3(0.4, 0.8, 0.3))), 0.0) * 0.8 + 0.2;
	color = vec4(texture(sampler2D(tex, smp), vuv).rgb * vtint.rgb * l, 1.0);
}
