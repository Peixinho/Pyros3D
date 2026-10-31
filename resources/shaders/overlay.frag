#version 450
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 viewProjection; vec4 flags; } uDraw;
layout(set = 1, binding = 1) uniform texture2D uSceneDepth;      // (the prepass's, the picture's size: reversed Z)
layout(set = 1, binding = 2) uniform sampler uPointClamp;
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;
vec3 ToLinear(vec3 c) { vec3 x = max(c, vec3(0.0)); return mix(x / 12.92, pow((x + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), x)); }
void main()
{
	// (hidden where the scene is in front - reversed Z: nearer is larger - with a give of 0.3% of the distance: a grid
	// lifted a hair over a ground at the same height stays over it, and the prepass was jittered)
	if (uDraw.flags.x > 0.5)
	{
		float scene = texelFetch(sampler2D(uSceneDepth, uPointClamp), ivec2(gl_FragCoord.xy), 0).r;
		if (gl_FragCoord.z * 1.003 < scene) discard;
	}
	outColor = vec4(ToLinear(vColor.rgb), vColor.a);     // (the target is sRGB: the store encodes it - the colour as written)
}
