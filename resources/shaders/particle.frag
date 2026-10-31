#version 450
#include "common.glsl"
// Particles' colour: the texture times the colour ramp, faded in and out over its life, and softly where it meets
// what is behind it (the prepass depth); lit ones take the sky's light and the sun's, scattered onward
// (Henyey-Greenstein); additive ones glow. Premultiplied out - the pipeline blends premultiplied or additive.
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 startColor, endColor; vec4 size; vec4 params; vec4 light; } uDraw;
layout(set = 1, binding = 1) uniform texture2D uTexture;
layout(set = 1, binding = 2) uniform sampler uLinear;
layout(set = 1, binding = 3) uniform texture2D uSceneDepth;
layout(set = 1, binding = 4) uniform texture2D uSkySH;
layout(location = 0) in vec2 vTexcoord;
layout(location = 1) in float vAge;
layout(location = 2) in vec3 vWorldPosition;
layout(location = 3) in float vViewDepth;
layout(location = 0) out vec4 outColor;

float Ease(float t, float mode)
{
	if (mode < 0.5) return t;
	if (mode < 1.5) return 0.0;
	if (mode < 2.5) return t * t;
	if (mode < 3.5) return t * (2.0 - t);
	if (mode < 4.5) return t * t * (3.0 - 2.0 * t);
	return t;
}

void main()
{
	vec4 tex = texture(sampler2D(uTexture, uLinear), vTexcoord);
	vec4 color = mix(uDraw.startColor, uDraw.endColor, Ease(vAge, uDraw.params.x)) * tex;
	color.a *= smoothstep(0.0, max(uDraw.params.y, 1e-4), vAge) * (1.0 - smoothstep(uDraw.params.z, 1.0, vAge));
#ifdef HALF
	// (half resolution, no depth target of its own: hidden where the scene is in front, by the full-size prepass depth)
	float d = texelFetch(sampler2D(uSceneDepth, uLinear), ivec2(gl_FragCoord.xy * 2.0), 0).r;
	float sceneZ = d > 0.0 ? DepthDistance(d) : 1e9;
	if (vViewDepth > sceneZ) discard;
	if (uDraw.params.w > 0.0) color.a *= clamp((sceneZ - vViewDepth) / uDraw.params.w, 0.0, 1.0);
#else
	// (soft: fading out over `softness` metres in front of the scene)
	if (uDraw.params.w > 0.0)
	{
		float d = texelFetch(sampler2D(uSceneDepth, uLinear), ivec2(gl_FragCoord.xy), 0).r;
		float sceneZ = d > 0.0 ? DepthDistance(d) : 1e9;
		color.a *= clamp((sceneZ - vViewDepth) / uDraw.params.w, 0.0, 1.0);
	}
#endif
	vec3 rgb = color.rgb;
	if (uDraw.light.x > 0.5)
	{
		// The sky's light from all round (its SH's first band) and the sun's, scattered toward the eye
		vec3 ambient = texelFetch(sampler2D(uSkySH, uLinear), ivec2(0, 0), 0).rgb * 0.282095 * (uFrame.frame.z > 0.5 ? 1.0 : 0.0) + uFrame.ambient.rgb * (uFrame.frame.z > 0.5 ? 0.0 : 1.0);
		vec3 toEye = ToEye(vWorldPosition);
		float g = uDraw.light.y, cosT = dot(-toEye, normalize(uFrame.sunDirection.xyz));
		float phase = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * cosT, 1e-4), 1.5);   // (times 4 pi: 1 for an even scatter)
		vec3 sun = uFrame.sunDirection.w > 0.0 ? uFrame.sunRadiance.rgb / PI * phase * 0.5 : vec3(0.0);
		rgb *= ambient + sun;
	}
	outColor = uDraw.light.z > 0.5 ? vec4(rgb * color.a, 0.0) : vec4(rgb * color.a, color.a);
}
