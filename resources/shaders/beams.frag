#version 450
#include "common.glsl"
#include "shadows.glsl"
#include "lighting.glsl"
// Light in the air, half the picture's size, each view ray marched to what is solid:
//  - the fog: a height fog (the sun's DirectionalLight::SetVolumetric, or a scene's VolumetricLight) - thick at the
//    ground, thinning with height - lit by the sun through its shadow cascades (light shafts wherever something casts a
//    shadow into it) and by the sky; it hides what is behind it as it goes
//  - the lamps' beams (ILightComponent::SetVolumetricScattering): each point or spot lamp's light scattered toward the
//    eye over the stretch of the ray it reaches - its falloff, cone and shadow
// Henyey-Greenstein lobes (forward: brighter looking toward the light). One compare a step for the shadows: the
// steps' jitter and the temporal resolve smooth it.
// Out: rgb the light the air adds, a what of the scene behind it hides (beamscomposite.frag lays it over the picture).
layout(set = 1, binding = 20) uniform texture2D uDepth;     // (the prepass's: reversed Z, 0 where nothing was drawn)
layout(set = 1, binding = 21) uniform sampler uPointClamp;
layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 params;          // x how many lamps below, y steps (each lamp's, across its reach)
	vec4 lamps[4];        // the lamps' indices in uLights, four a vec4
	vec4 fog0;            // the fog: density at its base (a metre, 0 none), forward scattering g, thinning with height (a metre), base height
	vec4 fog1;            // how far out it is marched (m), steps, the sun's share (scattering), the sky's share (ambient)
	vec4 fogTint;         // rgb the scattered light's colour, w -
} uDraw;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// A point or spot light's shadow at a point in the air: one hardware compare in its tile
float AirShadow(int tile, bool point, vec3 worldPos, vec3 lightPos)
{
	vec3 d = worldPos - lightPos;
	if (point)
	{
		vec3 a = abs(d);
		tile += a.x >= a.y && a.x >= a.z ? (d.x > 0.0 ? 0 : 1) : a.y >= a.z ? (d.y > 0.0 ? 2 : 3) : (d.z > 0.0 ? 4 : 5);
	}
	vec4 rect = uShadow.localRect[tile];
	vec4 clip = uShadow.local[tile] * vec4(worldPos, 1.0);
	if (clip.w <= 0.0) return 1.0;
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || ndc.z < 0.0 || ndc.z > 1.0) return 1.0;
	vec2 size = uShadow.atlas.zw, texel = uShadow.atlas.xy;
	vec2 at = clamp(rect.xy + uv * size, rect.xy + texel, rect.xy + size - texel);
	return textureLod(sampler2DShadow(uShadowAtlas, uShadowCompare), vec3(at, ndc.z), 0.0);
}

float Phase(float c, float g)
{
	g = clamp(g, -0.95, 0.95);
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * 3.14159265 * pow(max(1.0 + g2 - 2.0 * g * c, 1e-4), 1.5));
}

void main()
{
	vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
	vec3 viewRay = vec3(ndc.x / uFrame.projection.x, ndc.y / uFrame.projection.y, -1.0);
	vec3 rd = normalize(transpose(mat3(uFrame.view)) * viewRay);
	vec3 ro = uFrame.cameraPosition.xyz;
	ivec2 dsize = textureSize(sampler2D(uDepth, uPointClamp), 0);
	float depth = texelFetch(sampler2D(uDepth, uPointClamp), clamp(ivec2(vUV * vec2(dsize)), ivec2(0), dsize - 1), 0).r;
	float tScene = depth > 0.0 ? DepthDistance(depth) * length(viewRay) : 1.0e9;
	int steps = int(uDraw.params.y);
	// (each pixel its own start along the first step, and another each frame)
	float jitter = fract(InterleavedGradientNoise(gl_FragCoord.xy) + uFrame.sky.z * 7.31);
	int lamps = int(uDraw.params.x);
	vec3 light = vec3(0.0);
	float T = 1.0;
	// The fog: along the ray to what is solid (or as far out as it is marched), each step's air lit by the sun - through
	// its shadow cascades - and by the sky, and what is behind it dimmed by it
	// (over the sky only along the horizon: the sky is the atmosphere's - its own air scattered in it - and fogged again
	// it went grey with the sun's glow; stopped at the sky outright, the far ground's fog met a clear sky in a seam)
	float fogDensity = uDraw.fog0.x * (depth > 0.0 ? 1.0 : 1.0 - smoothstep(0.0, 0.16, rd.y));
	if (fogDensity > 0.0)
	{
		float tEnd = min(tScene, uDraw.fog1.x);
		int fogSteps = int(uDraw.fog1.y);
		float dt = tEnd / float(max(fogSteps, 1));
		vec3 sunDir = normalize(uFrame.sunDirection.xyz);
		float sunPhase = Phase(dot(rd, sunDir), uDraw.fog0.y) * 4.0 * 3.14159265;     // (1 for light scattered every way alike)
		vec3 sunLight = uFrame.sunDirection.w > 0.0 ? uFrame.sunRadiance.rgb * uDraw.fogTint.rgb * uDraw.fog1.z * sunPhase : vec3(0.0);
		vec3 skyLight = uFrame.ambient.rgb * uDraw.fog1.w;
		for (int s = 0; s < 128; s++)
		{
			if (s >= fogSteps) break;
			vec3 p = ro + rd * (float(s) + jitter) * dt;
			float d = fogDensity * exp(-uDraw.fog0.z * max(p.y - uDraw.fog0.w, 0.0));
			if (d < 1e-6) continue;
			float sun = uFrame.sunDirection.w > 0.0 ? SunShadowCheap(p, vec3(0.0)) : 0.0;
			float a = 1.0 - exp(-d * dt);
			// (what this step scatters toward the eye, from what reaches it, dimmed by the air before it)
			light += T * a * (sunLight * sun + skyLight);
			T *= 1.0 - a;
			if (T < 0.01) break;
		}
	}
	for (int k = 0; k < 16; k++)
	{
		if (k >= lamps) break;
		Light L = uLights.lights[int(uDraw.lamps[k / 4][k % 4])];
		// Only where the ray is inside the lamp's reach, up to what is solid: each lamp its own stretch of the ray,
		// its own steps - wherever the eye is, and none spent in air it does not light
		vec3 oc = ro - L.positionRange.xyz;
		float b = dot(oc, rd), cc = dot(oc, oc) - L.positionRange.w * L.positionRange.w;
		float disc = b * b - cc;
		if (disc <= 0.0) continue;
		float sq = sqrt(disc);
		float t0 = max(-b - sq, 0.0), t1 = min(-b + sq, tScene);
		if (t1 <= t0) continue;
		float dt = (t1 - t0) / float(steps);
		for (int s = 0; s < 64; s++)
		{
			if (s >= steps) break;
			vec3 p = ro + rd * (t0 + (float(s) + jitter) * dt);
			vec3 toLight = L.positionRange.xyz - p;
			float d = length(toLight);
			vec3 l = toLight / max(d, 1e-4);
			float f = DistanceFalloff(d, L.positionRange.w);
			if (L.radianceType.w > 0.5)
			{
				float c = dot(-l, normalize(L.directionCosOuter.xyz));
				float cone = clamp((c - L.directionCosOuter.w) / max(L.cosInner.x - L.directionCosOuter.w, 1e-4), 0.0, 1.0);
				f *= cone * cone;
			}
			if (f <= 0.0) continue;
			if (L.cosInner.y >= 0.0) f *= AirShadow(int(L.cosInner.y), L.radianceType.w < 0.5, p, L.positionRange.xyz);
			// (light going from the lamp to here, turned toward the eye: forward when the eye looks into the lamp)
			light += L.radianceType.rgb * f * Phase(dot(l, rd), L.cosInner.w) * L.cosInner.z * dt * T;     // (behind the fog, dimmed by it - as a whole)
		}
	}
	outColor = vec4(light, 1.0 - T);
}
