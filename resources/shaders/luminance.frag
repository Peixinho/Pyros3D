#version 450
// Auto exposure, first: the log2 luminance of the scene at 64x64 (each texel a bilinear average of four)
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 params; } uDraw;   // x: 1 when what is behind everything is light (a sky)
                                                                                 // y: what is nearer than this depth is not metered (0: all is)
layout(set = 1, binding = 1) uniform texture2D uSource;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(set = 1, binding = 3) uniform texture2D uDepth;     // (the prepass's: reversed Z, 0 where nothing was drawn)
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
	vec3 c = texture(sampler2D(uSource, uLinearClamp), vUV).rgb;
	if (!all(lessThan(abs(c), vec3(65000.0)))) c = vec3(0.0);     // (a bad pixel: not counted - one NaN in the average was the whole picture black)
	float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
	// (the centre counts more: what the eye looks at - a gentle weight, the edges still count)
	vec2 d = vUV - 0.5;
	float w = 1.0 - 0.6 * clamp(dot(d, d) * 4.0, 0.0, 1.0);
	// (nothing there - the background of a scene with no sky, whatever colour it is cleared to - says nothing about how
	// bright the scene is: metered on what was drawn, a lamp-lit room on its walls, not on the dark round it)
	ivec2 size = textureSize(sampler2D(uDepth, uLinearClamp), 0);
	float depth = texelFetch(sampler2D(uDepth, uLinearClamp), clamp(ivec2(vUV * vec2(size)), ivec2(0), size - 1), 0).r;
	if (lum < 1e-4 || (uDraw.params.x < 0.5 && depth <= 0.0)) w = 0.0;
	// (what the eye sits in - a car's cabin, its pillars and dashboard, dark and half the picture - is not what it is adapted
	// to: metered with the rest, the cabin came up toward middle grey and the road, the sky and the cars beyond its glass were
	// a stop over - a pale picture. Reversed Z: nearer is greater)
	if (uDraw.params.y > 0.0 && depth > uDraw.params.y) w = 0.0;
	outColor = vec4(log2(max(lum, 1e-5)) * w, w, 0.0, 1.0);
}
