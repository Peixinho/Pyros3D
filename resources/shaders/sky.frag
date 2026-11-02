#version 450
#include "common.glsl"
#include "atmosphere.glsl"
// The sky behind everything: the sky-view table and the sun's disc, through the air
layout(set = 1, binding = 1) uniform texture2D uTransmittance;
layout(set = 1, binding = 2) uniform texture2D uSkyView;
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
layout(set = 1, binding = 4, std140) uniform SkyData { mat4 inverseViewProjection; vec4 clouds; vec4 wind; mat4 previousViewProjection; vec4 history; vec4 cloudGrid; } uSky;
layout(set = 1, binding = 5) uniform texture2D uClouds;      // (half resolution: clouds.frag)
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
// The air between the camera and the sun: what the sun's light at the camera is short of, above the air
vec3 TransmittanceAt()
{
	vec2 uv = TransmittanceUV(uFrame.sky.x, normalize(uFrame.skySun.xyz).y);
	return texture(sampler2D(uTransmittance, uLinearClamp), vec2(uv.x, 1.0 - uv.y)).rgb;
}
void main()
{
	vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
	vec4 far = uSky.inverseViewProjection * vec4(ndc, 0.5, 1.0);
	vec3 dir = normalize(far.xyz / far.w - uFrame.cameraPosition.xyz);
	vec3 sunDir = normalize(uFrame.skySun.xyz);
	vec3 E = uFrame.sunRadiance.rgb;     // (the sun's illuminance, already through the air - as the surfaces get it)
	if (uFrame.skySunTop.w > 0.0) E = uFrame.skySunTop.rgb * TransmittanceAt();     // (the sun's own, where a game gave it)
	vec2 uv = SkyViewUV(dir, sunDir);
	vec3 L = texture(sampler2D(uSkyView, uLinearClamp), vec2(uv.x, 1.0 - uv.y)).rgb;     // (the table is absolute radiance)
	// The sun's disc, limb darkened
	float c = dot(dir, sunDir), cr = cos(uFrame.sky.y);
	if (c > cr && dir.y > -0.01)
	{
		float r = sqrt(max(0.0, 1.0 - (1.0 - c) / (1.0 - cr)));
		float solidAngle = 2.0 * PI * (1.0 - cr);
		L += E / solidAngle * (0.4 + 0.6 * r);
	}
	// Night, where a game said the sun is down and turned its light to the moon (RenderGlobals::SkySun): what the air
	// gives back of the moon - a deep blue, paler toward the horizon - and the moon itself, where the light comes from.
	// (The air lit by the sun alone, under the horizon, is black: the clouds the moon lights stood on nothing.)
	vec3 lightDir = normalize(uFrame.sunDirection.xyz);
	float night = uFrame.sunDirection.w * clamp(-sunDir.y * 10.0, 0.0, 1.0) * step(dot(lightDir, sunDir), 0.99);
	if (night > 0.0)
	{
		vec3 M = uFrame.sunRadiance.rgb;
		float h = pow(1.0 - clamp(dir.y, 0.0, 1.0), 4.0);
		L += M * night * mix(0.030, 0.070, h);
		float m = dot(dir, lightDir);
		if (m > 0.99993) L += M * night * 30.0;                                  // (the disc)
		L += M * night * 0.06 * pow(max(m, 0.0), 300.0);                         // (and the glow round it)
	}
	// The clouds in front of it all (and of the sun)
	if (uSky.clouds.x > 0.0)
	{
		vec4 cl = textureLod(sampler2D(uClouds, uLinearClamp), gl_FragCoord.xy * uFrame.viewport.zw, 0.0);
		L = L * cl.a + cl.rgb;
	}
	outColor = vec4(L, 1.0);
}
