#version 450
#include "common.glsl"
#include "atmosphere.glsl"
// The atmosphere's transmittance toward the top, by altitude and the cosine of the zenith angle
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
	float altitude, cosZenith;
	TransmittanceParams(vec2(vUV.x, 1.0 - vUV.y), altitude, cosZenith);
	vec3 o = vec3(0.0, kGroundRadius + altitude, 0.0);
	vec3 d = vec3(sqrt(max(0.0, 1.0 - cosZenith * cosZenith)), cosZenith, 0.0);
	float t = RaySphere(o, d, kTopRadius);
	vec3 depth = vec3(0.0);
	const int N = 40;
	for (int i = 0; i < N; i++)
	{
		vec3 p = o + d * (t * (float(i) + 0.5) / float(N));
		depth += MediumAt(length(p) - kGroundRadius).extinction * (t / float(N));
	}
	outColor = vec4(exp(-depth), 1.0);
}
