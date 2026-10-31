#version 450
#include "common.glsl"
#include "clouds.glsl"
// The clouds' shadow on the ground: a map over the ground round the camera, each texel what of the sun gets through
// the slab above it toward the sun - a few steps (their shadows are soft)
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
	vec3 sunDir = normalize(uFrame.sunDirection.xyz);
	if (uFrame.sunDirection.w <= 0.0 || sunDir.y < 0.03 || uSky.clouds.x <= 0.0) { outColor = vec4(1.0); return; }
	vec2 xz = uSky.cloudGrid.xy + vec2(vUV.x, vUV.y) * uSky.cloudGrid.z;
	float bottom = uSky.clouds.z, top = uSky.clouds.w;
	float t0 = bottom / sunDir.y, t1 = top / sunDir.y, dt = (t1 - t0) / 8.0;
	float tau = 0.0;
	for (int i = 0; i < 8; i++)
	{
		vec3 p = vec3(xz.x, 0.0, xz.y) + sunDir * (t0 + (float(i) + 0.5) * dt);
		tau += Density(p, (p.y - bottom) / (top - bottom), true) * uSky.clouds.y * dt;     // (worn as the clouds seen are: their shadows have their shapes)
	}
	outColor = vec4(exp(-tau));
}
