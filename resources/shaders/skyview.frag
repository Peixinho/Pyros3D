#version 450
#include "common.glsl"
#include "atmosphere.glsl"
// The sky seen from the camera's altitude, every direction: single scattering with the transmittance table,
// and the multiple scattering as an isotropic share of it (Hillaire's approximation, without its table)
layout(set = 1, binding = 1) uniform texture2D uTransmittance;
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

vec3 TransmittanceTo(float altitude, float cosZenith)
{
	vec2 uv = TransmittanceUV(altitude, cosZenith);
	return textureLod(sampler2D(uTransmittance, uLinearClamp), vec2(uv.x, 1.0 - uv.y), 0.0).rgb;
}

void main()
{
	vec3 sunDir = normalize(uFrame.sunDirection.xyz);
	vec3 dir = SkyViewDirection(vec2(vUV.x, 1.0 - vUV.y), sunDir);
	vec3 o = vec3(0.0, kGroundRadius + max(uFrame.sky.x, 0.001), 0.0);
	float tTop = RaySphere(o, dir, kTopRadius);
	float tGround = RaySphere(o, dir, kGroundRadius);
	float t = (tGround > 0.0) ? tGround : tTop;
	float c = dot(dir, sunDir);
	float pr = RayleighPhase(c), pm = MiePhase(c);
	vec3 L = vec3(0.0), through = vec3(1.0);
	const int N = 32;
	float prevT = 0.0;
	for (int i = 0; i < N; i++)
	{
		// (steps that grow with the distance: more of them close by, where the air is dense)
		float ti = t * pow((float(i) + 0.5) / float(N), 2.0);
		float dt = ti - prevT; prevT = ti;
		vec3 p = o + dir * ti;
		float h = length(p) - kGroundRadius;
		Medium m = MediumAt(h);
		vec3 up = p / length(p);
		vec3 sunThrough = TransmittanceTo(h, dot(sunDir, up));
		// (the sun hidden by the Earth from this point)
		if (RaySphere(p, sunDir, kGroundRadius) > 0.0) sunThrough = vec3(0.0);
		vec3 scatter = m.scatteringRayleigh * pr + m.scatteringMie * pm;
		vec3 multi = (m.scatteringRayleigh + m.scatteringMie) * 0.08;        // (the light scattered more than once)
		vec3 S = sunThrough * scatter + multi * max(dot(sunDir, up) + 0.2, 0.0);
		vec3 stepThrough = exp(-m.extinction * dt);
		L += through * (S - S * stepThrough) / max(m.extinction, vec3(1e-6));
		through *= stepThrough;
	}
	// (the ground below: lit by the sun through the air, seen through it)
	if (tGround > 0.0)
	{
		vec3 p = o + dir * tGround; vec3 up = normalize(p);
		L += through * TransmittanceTo(0.0, dot(sunDir, up)) * max(dot(up, sunDir), 0.0) * kGroundAlbedo / PI;
	}
	// (absolute: per unit of the sun's illuminance above the air, times that - the light at the camera, divided by
	// what the air between leaves of it)
	vec3 sunAtCamera = TransmittanceTo(uFrame.sky.x, sunDir.y);
	outColor = vec4(L * uFrame.sunRadiance.rgb / max(sunAtCamera, vec3(1e-4)), 1.0);
}
