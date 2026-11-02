#version 450
#include "common.glsl"
#include "atmosphere.glsl"
// The sky's radiance as 9 spherical harmonics (bands 0-2), first stage: a 9x16 target - each column one coefficient,
// each row a sixteenth of a Fibonacci sphere of directions through the sky-view table (skyshsum.frag adds the rows)
layout(set = 1, binding = 1) uniform texture2D uTransmittance;
layout(set = 1, binding = 2) uniform texture2D uSkyView;
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float SHBasis(int k, vec3 d)
{
	if (k == 0) return 0.282095;
	if (k == 1) return 0.488603 * d.y;
	if (k == 2) return 0.488603 * d.z;
	if (k == 3) return 0.488603 * d.x;
	if (k == 4) return 1.092548 * d.x * d.y;
	if (k == 5) return 1.092548 * d.y * d.z;
	if (k == 6) return 0.315392 * (3.0 * d.z * d.z - 1.0);
	if (k == 7) return 1.092548 * d.x * d.z;
	return 0.546274 * (d.x * d.x - d.y * d.y);
}

void main()
{
	int k = int(gl_FragCoord.x), row = int(gl_FragCoord.y);
	vec3 sunDir = normalize(uFrame.skySun.xyz);
	const int N = 512;
	vec3 sum = vec3(0.0);
	// The ground's light from below: what the sun and the sky put on it, given back by its albedo (a dry field's, warm) -
	// the sky-view table below the horizon is no ground at all, and without this a face turned from the sun was lit by
	// the blue overhead alone: a rock's shaded side blue, grass in shade flat
	vec3 zenith = textureLod(sampler2D(uSkyView, uLinearClamp), vec2(SkyViewUV(vec3(0.0, 1.0, 0.0), sunDir).x, 1.0 - SkyViewUV(vec3(0.0, 1.0, 0.0), sunDir).y), 0.0).rgb;
	vec3 groundAlbedo = vec3(0.20, 0.18, 0.12);
	vec3 groundL = groundAlbedo * (uFrame.sunRadiance.rgb * max(sunDir.y, 0.0) * uFrame.sunDirection.w + zenith * PI * 0.7) / PI;
	for (int i = row; i < N; i += 16)
	{
		float y = 1.0 - (float(i) + 0.5) / float(N) * 2.0;
		float r = sqrt(max(0.0, 1.0 - y * y));
		float phi = float(i) * 2.39996323;
		vec3 d = vec3(cos(phi) * r, y, sin(phi) * r);
		vec2 uv = SkyViewUV(d, sunDir);
		vec3 L = textureLod(sampler2D(uSkyView, uLinearClamp), vec2(uv.x, 1.0 - uv.y), 0.0).rgb;     // (absolute radiance)
		L = mix(L, groundL, smoothstep(0.0, -0.1, d.y));
		sum += L * SHBasis(k, d);
	}
	outColor = vec4(sum * (4.0 * PI / float(N)), 1.0);
}
