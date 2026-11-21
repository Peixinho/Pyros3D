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
// (the frame's lights, as lighting.glsl has them: a lit particle takes the point and spot lights too)
struct PLight { vec4 positionRange; vec4 radianceType; vec4 directionCosOuter; vec4 cosInner; };
layout(set = 1, binding = 5, std430) readonly buffer PLights { uvec4 count; PLight lights[]; } uPLights;
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
#if defined(HALF) || defined(SCREEN)
	// (a layer of their own, no depth target: hidden where the scene is in front, by the prepass depth - full size, or
	// half resolution's, a texel of it two of the depth's)
#ifdef HALF
	float d = texelFetch(sampler2D(uSceneDepth, uLinear), ivec2(gl_FragCoord.xy * 2.0), 0).r;
#else
	float d = texelFetch(sampler2D(uSceneDepth, uLinear), ivec2(gl_FragCoord.xy), 0).r;
#endif
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
	// (none within half a metre of the eye, all from a metre and a half: a raindrop falling past the lens - inside a
	// car's cockpit, lit by its cabin lamp - drew as a column the screen's height for a frame)
	color.a *= smoothstep(0.5, 1.5, vViewDepth);
	if (color.a <= 0.0) discard;
	vec3 rgb = color.rgb;
	if (uDraw.light.x > 0.5)
	{
		// The sky's light from all round (its SH's first band) and the sun's, scattered toward the eye
		vec3 ambient = texelFetch(sampler2D(uSkySH, uLinear), ivec2(0, 0), 0).rgb * 0.282095 * (uFrame.frame.z > 0.5 ? 1.0 : 0.0) + uFrame.ambient.rgb * (uFrame.frame.z > 0.5 ? 0.0 : 1.0);
		vec3 toEye = ToEye(vWorldPosition);
		float g = uDraw.light.y, cosT = dot(-toEye, normalize(uFrame.sunDirection.xyz));
		float phase = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * cosT, 1e-4), 1.5);   // (times 4 pi: 1 for an even scatter)
		vec3 sun = uFrame.sunDirection.w > 0.0 ? uFrame.sunRadiance.rgb / PI * phase * 0.5 : vec3(0.0);
		// the point and spot lights (no shadows: a mist in a beam is lit where the beam is): each by its falloff and cone,
		// scattered toward the eye as the sun's is - forward most (a headlight seen through a car's spray glows in it)
		vec3 local = vec3(0.0);
		uint n = min(uPLights.count.x, 256u);
		for (uint i = 0u; i < n; i++)
		{
			PLight L = uPLights.lights[i];
			if (L.radianceType.w > 1.5) continue;
			vec3 toL = L.positionRange.xyz - vWorldPosition;
			float dl = length(toL);
			if (dl >= L.positionRange.w) continue;
			vec3 l = toL / max(dl, 1e-4);
			float x = dl / max(L.positionRange.w, 1e-4);
			float w = clamp(1.0 - x * x * x * x, 0.0, 1.0);
			float k = w * w / max(dl * dl, 0.01);
			if (L.radianceType.w > 0.5)
			{
				float c = dot(-l, normalize(L.directionCosOuter.xyz));
				float cone = clamp((c - L.directionCosOuter.w) / max(L.cosInner.x - L.directionCosOuter.w, 1e-4), 0.0, 1.0);
				k *= cone * cone;
			}
			if (k <= 0.0) continue;
			float ct = dot(-toEye, -l);
			float ph = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * ct, 1e-4), 1.5);
			local += L.radianceType.rgb * k / PI * ph * 0.5;
		}
		// (its share of the lamps' light - ParticleSystem::SetLocalLightGain, light.x past 1: rain a trace, spray a part)
		local *= max(uDraw.light.x - 1.0, 0.0);
		rgb *= ambient + sun + local;
	}
	// (in the haze layer, as the scene behind it is: a far burst grey with the distance; a glow only dimmed by it)
	if (uFrame.haze.x > 0.0)
	{
		float keep = exp(-HazeDepth(uFrame.cameraPosition.xyz, vWorldPosition));
		vec3 hazeLight = texelFetch(sampler2D(uSkySH, uLinear), ivec2(0, 0), 0).rgb * 0.282095 * 1.2;
		rgb = uDraw.light.z > 0.5 ? rgb * keep : rgb * keep + hazeLight * (1.0 - keep);
	}
	outColor = uDraw.light.z > 0.5 ? vec4(rgb * color.a, 0.0) : vec4(rgb * color.a, color.a);
}
