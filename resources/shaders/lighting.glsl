// Pyros3D the renderer - the surface's light: GGX / Smith height-correlated / Schlick specular and Lambert
// diffuse, from the sun and the frame's point and spot lights.

struct Light
{
	vec4 positionRange;      // xyz world, w: radius (no light past it)
	vec4 radianceType;       // rgb linear, w: 0 point, 1 spot, 2 directional (a light after the sun: directionCosOuter.xyz toward it)
	vec4 directionCosOuter;  // a spot's: xyz the way it points, w: cos of its outer cone
	vec4 cosInner;           // x: cos of a spot's inner cone, y: its first shadow tile (shadows.glsl), < 0 none
};

layout(set = 1, binding = 1, std430) readonly buffer Lights
{
	uvec4 count;             // x: how many lights; after them y 2D shadow casters, then z reflection probes (SceneRenderer::WriteLights)
	Light lights[];
} uLights;

float D_GGX(float NoH, float a)
{
	float a2 = a * a;
	float f = (NoH * a2 - NoH) * NoH + 1.0;
	return a2 / (PI * f * f);
}
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
	float a2 = a * a;
	float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
	float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
	return 0.5 / max(gv + gl, 1e-5);
}
vec3 F_Schlick(float VoH, vec3 f0)
{
	return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0);
}

// What a light coming from l with this radiance gives back toward v
vec3 SurfaceLight(vec3 n, vec3 v, vec3 l, vec3 radiance, vec3 diffuseColor, vec3 f0, float a)
{
	float NoL = clamp(dot(n, l), 0.0, 1.0);
	if (NoL <= 0.0) return vec3(0.0);
	vec3 h = normalize(v + l);
	float NoV = max(dot(n, v), 1e-4);
	float NoH = clamp(dot(n, h), 0.0, 1.0);
	float VoH = clamp(dot(v, h), 0.0, 1.0);
	vec3 F = F_Schlick(VoH, f0);
	vec3 specular = D_GGX(NoH, a) * V_SmithGGXCorrelated(NoV, NoL, a) * F;
	vec3 diffuse = diffuseColor * (1.0 - F) / PI;
	return (diffuse + specular) * radiance * NoL;
}

// Inverse square, windowed to nothing at the radius (as the engine's lights are set up: LinearFalloffIntensity)
float DistanceFalloff(float d, float radius)
{
	float x = d / max(radius, 1e-4);
	float w = clamp(1.0 - x * x * x * x, 0.0, 1.0);
	return w * w / max(d * d, 0.01);
}

vec3 AllLights(vec3 position, vec3 n, vec3 ng, vec3 v, vec3 diffuseColor, vec3 f0, float a, float sunShadow)
{
	vec3 color = vec3(0.0);
	if (uFrame.sunDirection.w > 0.0 && sunShadow > 0.0)
		color += SurfaceLight(n, v, normalize(uFrame.sunDirection.xyz), uFrame.sunRadiance.rgb * sunShadow, diffuseColor, f0, a);
	uint count = uLights.count.x;
	for (uint i = 0u; i < count; i++)
	{
		Light L = uLights.lights[i];
		if (L.radianceType.w > 1.5)
		{
			// (a directional light: from everywhere along it, no falloff; its shadow from its orthographic tile, round the eye -
			// the "light" set back along it by the tile's half size, as LocalShadow measures its bias from it)
			vec3 ld = normalize(L.directionCosOuter.xyz);
			float kd = 1.0;
			if (L.cosInner.y >= 0.0 && uFrame.frame.w != 4.0) kd = LocalShadow(int(L.cosInner.y), false, position, ng, position + ld * L.cosInner.z);
			if (kd > 0.0) color += SurfaceLight(n, v, ld, L.radianceType.rgb * kd, diffuseColor, f0, a);
			continue;
		}
		vec3 toLight = L.positionRange.xyz - position;
		float d = length(toLight);
		if (d >= L.positionRange.w) continue;
		vec3 l = toLight / max(d, 1e-4);
		float k = DistanceFalloff(d, L.positionRange.w);
		if (L.radianceType.w > 0.5)
		{
			float c = dot(-l, normalize(L.directionCosOuter.xyz));
			float cone = clamp((c - L.directionCosOuter.w) / max(L.cosInner.x - L.directionCosOuter.w, 1e-4), 0.0, 1.0);
			k *= cone * cone;
		}
		// (its shadow, where it casts one - not under the debug view without shadows)
		if (k > 0.0 && L.cosInner.y >= 0.0 && uFrame.frame.w != 4.0) k *= LocalShadow(int(L.cosInner.y), L.radianceType.w < 0.5, position, ng, L.positionRange.xyz);
		if (k > 0.0) color += SurfaceLight(n, v, l, L.radianceType.rgb * k, diffuseColor, f0, a);
	}
	return color;
}
