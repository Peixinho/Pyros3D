// Pyros3D the renderer - the sky's light on a surface, from its 9 spherical harmonics (skysh.frag): the
// irradiance (Ramamoorthi & Hanrahan) for the diffuse, the radiance toward the reflection for the specular,
// scaled by the split-sum's analytic fit (Karis, "Physically Based Shading on Mobile")
layout(set = 1, binding = 10) uniform texture2D uSkySH;

vec3 SkySH(int k) { return texelFetch(sampler2D(uSkySH, uMaterialSampler), ivec2(k, 0), 0).rgb; }

vec3 SkyIrradiance(vec3 n)
{
	const float c1 = 0.429043, c2 = 0.511664, c3 = 0.743125, c4 = 0.886227, c5 = 0.247708;
	vec3 L00 = SkySH(0), L1m1 = SkySH(1), L10 = SkySH(2), L11 = SkySH(3), L2m2 = SkySH(4), L2m1 = SkySH(5), L20 = SkySH(6), L21 = SkySH(7), L22 = SkySH(8);
	// (the basis above is y, z, x for band 1 - SHBasis's order: m = -1 is y, 0 is z, 1 is x)
	vec3 e = c1 * L22 * (n.x * n.x - n.y * n.y) + c3 * L20 * n.z * n.z + c4 * L00 - c5 * L20
		+ 2.0 * c1 * (L2m2 * n.x * n.y + L21 * n.x * n.z + L2m1 * n.y * n.z)
		+ 2.0 * c2 * (L11 * n.x + L1m1 * n.y + L10 * n.z);
	return max(e, vec3(0.0));
}
vec3 SkyRadiance(vec3 d)
{
	vec3 L = SkySH(0) * 0.282095 + 0.488603 * (SkySH(1) * d.y + SkySH(2) * d.z + SkySH(3) * d.x)
		+ 1.092548 * (SkySH(4) * d.x * d.y + SkySH(5) * d.y * d.z + SkySH(7) * d.x * d.z)
		+ 0.315392 * SkySH(6) * (3.0 * d.z * d.z - 1.0) + 0.546274 * SkySH(8) * (d.x * d.x - d.y * d.y);
	return max(L, vec3(0.0));
}
vec3 EnvBRDFApprox(vec3 f0, float roughness, float NoV)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022), c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
	return f0 * AB.x + AB.y;
}
