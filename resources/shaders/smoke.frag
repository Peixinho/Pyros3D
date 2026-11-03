#version 450
#include "common.glsl"
// Smoke and fire drawn from their simulations (FluidVolumes: fluid.comp's grids, packed in one buffer): each view ray
// marched through the grids it crosses up to what is solid (the depth prepass), half the picture's size. The smoke
// scatters the sun - its share of it past the smoke toward the sun baked in each cell - and the sky's light, more of it
// from above; the heat glows as a blackbody's at its temperature, and lights the soot round it. Where what the eye
// meets mirrors (the picture's alpha: surface.frag), the mirrored ray is marched on from there as well, so the smoke and
// the fire show in the reflection.
// Out 0: rgb the light gathered, a what of the scene behind still shows. Out 1: the reflection's, already weighed by
// how much the surface mirrors: rgb light, a what of the reflection it hides (smokecomposite.frag lays both).
layout(set = 1, binding = 1) uniform texture2D uDepth;       // (the prepass's: reversed Z, 0 where nothing was drawn)
layout(set = 1, binding = 2, std430) readonly buffer Cells { uvec2 c[]; } uCells;    // (packHalf2x16(smoke, heat), packHalf2x16(light, fuel))
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
layout(set = 1, binding = 4, std140) uniform SmokeData
{
	vec4 uP0;        // -, -, extinction (a metre, at a density of 1), step
	vec4 uP1;        // the detail noise's scale, its strength, -, max steps
	vec4 uSunDir;    // xyz toward the sun, w the lamps
	vec4 uSunColor;
	vec4 uAmbient;
	vec4 uWind;
	vec4 uVolA[8];   // the grid's low corner, a cell's size
	vec4 uVolB[8];   // its cells (x, y, z), where they start in Cells
	vec4 uVolC[8];   // the cloud's colour, its emission (above 0: it burns)
	vec4 uVolD[8];   // x its fade (1 until it fades), y thickness, z kind (0 smoke, 1 fire, 2 blast), w -
	vec4 uLPos[8];   // position, radius
	vec4 uLCol[8];   // colour, cos of the outer cone (-1: point light)
	vec4 uLDir[8];   // spot direction, cos of the inner cone
};
layout(set = 1, binding = 5) uniform texture2D uSkySH;      // (the sky's light all round, 9 SH: skylight.glsl's)
layout(set = 1, binding = 6) uniform texture2D uPicture;    // (the HDR picture: its alpha 1 - how much a surface mirrors)
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outReflect;

#define VOLUMES 8

float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}
float vnoise(vec3 x)
{
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash13(i), hash13(i + vec3(1, 0, 0)), f.x), mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), f.x), mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// The sky's irradiance on a surface facing n (skylight.glsl's SkyIrradiance)
vec3 SkyIrradiance(vec3 n)
{
	vec3 L[9];
	for (int k = 0; k < 9; k++) L[k] = texelFetch(sampler2D(uSkySH, uLinearClamp), ivec2(k, 0), 0).rgb;
	const float c1 = 0.429043, c2 = 0.511664, c3 = 0.743125, c4 = 0.886227, c5 = 0.247708;
	vec3 e = c1 * L[8] * (n.x * n.x - n.y * n.y) + c3 * L[6] * n.z * n.z + c4 * L[0] - c5 * L[6]
		+ 2.0 * c1 * (L[4] * n.x * n.y + L[7] * n.x * n.z + L[5] * n.y * n.z)
		+ 2.0 * c2 * (L[3] * n.x + L[1] * n.y + L[2] * n.z);
	return max(e, vec3(0.0));
}

// Henyey-Greenstein, times 4 pi (1 for light scattered every way alike)
float HG(float c, float g)
{
	float g2 = g * g;
	return (1.0 - g2) / pow(max(1.0 + g2 - 2.0 * g * c, 1e-4), 1.5);
}

// A blackbody's colour at `kelvin`, linear and at most 1 in its brightest channel (Helland's fit to Planck's)
vec3 Blackbody(float kelvin)
{
	float t = kelvin / 100.0;
	vec3 c;
	c.r = t <= 66.0 ? 1.0 : clamp(1.29293618 * pow(t - 60.0, -0.1332047592), 0.0, 1.0);
	c.g = t <= 66.0 ? clamp(0.39008157876 * log(t) - 0.63184144378, 0.0, 1.0) : clamp(1.12989086 * pow(t - 60.0, -0.0755148492), 0.0, 1.0);
	c.b = t >= 66.0 ? 1.0 : (t <= 19.0 ? 0.0 : clamp(0.54320678911 * log(t - 10.0) - 1.19625408914, 0.0, 1.0));
	return SrgbDecode(c);
}

// A grid's cell: smoke, heat, light, fuel
vec4 Cell(int v, ivec3 c)
{
	ivec3 n = ivec3(uVolB[v].xyz);
	c = clamp(c, ivec3(0), n - 1);
	uvec2 u = uCells.c[int(uVolB[v].w) + c.x + n.x * (c.y + n.y * c.z)];
	return vec4(unpackHalf2x16(u.x), unpackHalf2x16(u.y));
}
// ... trilinear, at a world point
vec4 Sample(int v, vec3 p, vec3 jitter)
{
	vec3 f = (p - uVolA[v].xyz) / uVolA[v].w - 0.5 + jitter;
	ivec3 i = ivec3(floor(f));
	vec3 t = f - vec3(i);
	return mix(mix(mix(Cell(v, i), Cell(v, i + ivec3(1, 0, 0)), t.x), mix(Cell(v, i + ivec3(0, 1, 0)), Cell(v, i + ivec3(1, 1, 0)), t.x), t.y),
	           mix(mix(Cell(v, i + ivec3(0, 0, 1)), Cell(v, i + ivec3(1, 0, 1)), t.x), mix(Cell(v, i + ivec3(0, 1, 1)), Cell(v, i + ivec3(1, 1, 1)), t.x), t.y), t.z);
}

// Shared by both rays
float gJitter;
vec3 gAmbTop, gAmbBottom;

// The smoke and fire along a ray from ro (rd its direction) up to tEnd: the light it gathers toward ro, and what of
// what lies behind still shows. maxSteps: what the ray may spend.
vec4 March(vec3 ro, vec3 rd, float tEnd, int maxSteps)
{
	// Which grids does this ray cross, and where
	float hitIn[VOLUMES], hitOut[VOLUMES];
	int count = int(uP0.x);
	float tNear = 1.0e9, tFar = -1.0;
	vec3 safe = vec3(abs(rd.x) < 1e-5 ? 1e-5 : rd.x, abs(rd.y) < 1e-5 ? 1e-5 : rd.y, abs(rd.z) < 1e-5 ? 1e-5 : rd.z);
	vec3 inv = 1.0 / safe;
	for (int v = 0; v < VOLUMES; v++) {
		hitIn[v] = 1.0e9; hitOut[v] = -1.0;
		if (v >= count) continue;
		vec3 lo3 = uVolA[v].xyz, hi3 = uVolA[v].xyz + uVolB[v].xyz * uVolA[v].w;
		vec3 a = (lo3 - ro) * inv, b = (hi3 - ro) * inv;
		vec3 lo = min(a, b), hi = max(a, b);
		float t0 = max(max(lo.x, lo.y), max(lo.z, 0.0));
		float t1 = min(min(hi.x, hi.y), min(hi.z, tEnd));
		if (t1 <= t0) continue;
		hitIn[v] = t0; hitOut[v] = t1;
		tNear = min(tNear, t0); tFar = max(tFar, t1);
	}
	if (tFar < 0.0) return vec4(0.0, 0.0, 0.0, 1.0);

	vec3 L = uSunDir.xyz;
	float cosSun = dot(rd, L);
	// Light toward the eye: most of what smoke scatters goes on forward - the bright rim against the sun - a little back
	float phase = mix(HG(cosSun, 0.45), HG(cosSun, -0.15), 0.3);
	float sigma = uP0.z;
	float time = uFrame.sky.z;

	vec3 light = vec3(0.0);
	float T = 1.0;
	// Steps: a cell of the finest grid crossed or so, more where the ray starts far (the step grows with distance)
	float t = tNear + gJitter * uP0.w;
	for (int s = 0; s < 160; s++) {
		if (s >= maxSteps || t >= tFar) break;
		float dt = uP0.w * max(1.0, t / 25.0);
		vec3 p = ro + rd * (t + 0.5 * dt);
		float smoke = 0.0, heat = 0.0, sun = 0.0, wsum = 0.0, emission = 0.0, burns = 0.0;
		// (each sample a random part of a cell off, a different one each pixel, step and frame: filtered reads of a
		// grid this coarse drew its cells as little blocks - offset, they are a grain the temporal resolve smooths)
		vec3 cellJitter = vec3(hash13(vec3(gl_FragCoord.xy, float(s) + gJitter * 97.0)), hash13(vec3(gl_FragCoord.yx, float(s) * 1.7 + gJitter * 61.0)),
		                       hash13(vec3(gl_FragCoord.xy * 1.3, float(s) * 2.3 + gJitter * 43.0))) - 0.5;
		vec3 albedo = vec3(0.0);
		float yIn = 0.0;
		for (int v = 0; v < VOLUMES; v++) {
			if (v >= count || t + dt < hitIn[v] || t > hitOut[v]) continue;
			vec4 c = Sample(v, p, cellJitter);
			// (thinning to nothing over the box's last cells, sides and top: smoke leaves there, and its edge drew a wall)
			vec3 inCells = (p - uVolA[v].xyz) / uVolA[v].w;
			vec3 toEdge = min(inCells, uVolB[v].xyz - inCells);
			if (uVolD[v].w < 0.5) {     // (an open box only: indoors the room's walls bound it)
			c.x *= smoothstep(0.0, 4.0, min(min(toEdge.x, toEdge.z), uVolB[v].y - inCells.y));
			// (and rounded, its edge broken up: thinned over the box's straight faces alone, a cloud that filled its grid
			// showed the box - flat sides, square corners. Out toward an ellipsoid in the box - the floor kept, smoke lies on
			// it - with the edge moved about by a slow noise)
			vec3 q = inCells / max(uVolB[v].xyz, vec3(1.0)) * 2.0 - 1.0;
			q.y = max(q.y, 0.0);
			float edgeNoise = vnoise(p * 0.35 + vec3(0.0, -time * 0.05, 0.0)) - 0.5;
			c.x *= 1.0 - smoothstep(0.62, 1.04, length(q) + edgeNoise * 0.3);
			}
			float fade = uVolD[v].x;
			float kind = uVolD[v].z;
			float d = c.x * uVolD[v].y * (kind < 0.5 ? fade : mix(0.3, 1.0, fade));
			// soot is dark; a grenade's smoke its own colour
			vec3 col = kind < 0.5 ? uVolC[v].rgb : vec3(0.06, 0.055, 0.05);
			smoke += d;
			albedo += col * d;
			sun += c.z * d;
			wsum += d;
			heat = max(heat, c.y * (kind < 0.5 ? 0.0 : 1.0));
			emission = max(emission, uVolC[v].a);
			burns = max(burns, kind);
			yIn += clamp((p.y - uVolA[v].y) / (uVolB[v].y * uVolA[v].w * 0.6), 0.0, 1.0) * d;
		}
		// the detail the grid is too coarse for: a fine noise eating at the thin smoke more than the thick
		if (smoke > 1e-4) {
			float n = vnoise(p * uP1.x - uWind.xyz * time * 0.5 + vec3(0.0, -time * 0.3, 0.0));
			smoke *= mix(1.0, 0.45 + 1.1 * n, uP1.y * (1.0 - smoothstep(0.4, 2.0, smoke)));
		}
		// Fire: the heat glowing as a blackbody's at its temperature (heat 1 a flame's heart, ~1900 K; the glow fading
		// out below ~900 K), its brightness climbing steeply with it
		if (heat > 0.12 && burns > 0.5) {
			float hot = clamp((heat - 0.12) / 0.88, 0.0, 1.5);
			vec3 glow = Blackbody(mix(850.0, 1950.0, min(hot, 1.0))) * pow(hot, 2.4) * 7.0 * emission;
			light += T * glow * (1.0 - exp(-hot * 3.0 * dt));
		}
		if (smoke > 1e-4) {
			albedo /= wsum;
			sun /= wsum;
			float h = yIn / wsum;
			// the sun past the smoke toward it, the light that has bounced on through the smoke much less dimmed;
			// the sky's light from above, the ground's from below; and soot near a fire lit by it
			// (smoke this thick scatters the light many times over: in the shadow of something - a sign over it, the
			// cloud's own far side - it is a lighter grey, lit by the smoke round it, never near black as a single
			// scatter has it. The share that comes round by those bounces does not care which way the eye looks)
			float direct = sun + 0.35 * sqrt(sun);
			vec3 incoming = uSunColor.rgb * (direct * phase + 0.22 + 0.15 * sqrt(sun))
			              + mix(gAmbBottom, gAmbTop, h) * (0.85 + 0.15 * sun);
			// (soot lit by the heat in it while that is still hot - not by the warmth it keeps after: it went brown)
			if (heat > 0.2) incoming += Blackbody(mix(850.0, 1950.0, clamp(heat, 0.0, 1.0))) * (heat - 0.2) * (heat - 0.2) * 4.0 * emission;
			int lamps = int(uSunDir.w);
			for (int j = 0; j < 8; j++) {
				if (j >= lamps) break;
				vec3 toLamp = uLPos[j].xyz - p;
				float dist = length(toLamp);
				float att = clamp(1.0 - dist / uLPos[j].w, 0.0, 1.0);
				att *= att;
				if (att <= 0.0) continue;
				if (uLCol[j].w > -0.5) {
					float c = dot(-toLamp / max(dist, 1e-4), uLDir[j].xyz);
					att *= smoothstep(uLCol[j].w, uLDir[j].w, c);
				}
				incoming += uLCol[j].rgb * att;
			}
			float a = 1.0 - exp(-smoke * sigma * dt);
			light += T * a * albedo * incoming;
			T *= 1.0 - a;
			if (T < 0.01) { T = 0.0; break; }
		}
		t += dt;
	}
	return vec4(light, T);
}

void main()
{
	// The ray through this pixel, from the eye; how far it goes before it meets what is solid
	vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
	vec3 viewRay = vec3(ndc.x / uFrame.projection.x, ndc.y / uFrame.projection.y, -1.0);
	float rayScale = length(viewRay);
	vec3 rd = normalize(transpose(mat3(uFrame.view)) * viewRay);
	vec3 ro = uFrame.cameraPosition.xyz;
	ivec2 dsize = textureSize(sampler2D(uDepth, uLinearClamp), 0);
	ivec2 dpx = clamp(ivec2(vUV * vec2(dsize)), ivec2(0), dsize - 1);
	float depth = texelFetch(sampler2D(uDepth, uLinearClamp), dpx, 0).r;
	float tScene = depth > 0.0 ? DepthDistance(depth) * rayScale : 1.0e6;

	// (each pixel starts its samples at a different point along the first step, and at another each frame: the
	// banding a fixed step draws turned to grain, and the grain averaged away by the temporal resolve)
	gJitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy + 5.588238 * mod(floor(uFrame.sky.z * 60.0), 64.0), vec2(0.06711056, 0.00583715))));
	// The ambient: the sky's own light from above and the ground's from below, where the scene has a sky (else
	// its ambient, flat) - as radiance, which is what smoke scatters
	gAmbTop = uAmbient.rgb * 1.3; gAmbBottom = uAmbient.rgb * 0.45;
	if (uFrame.frame.z > 0.5)
	{
		gAmbTop = SkyIrradiance(vec3(0.0, 1.0, 0.0)) / 3.14159;
		gAmbBottom = (SkyIrradiance(vec3(0.0, -1.0, 0.0)) + uSunColor.rgb * max(uSunDir.y, 0.0) * 0.25) / 3.14159;
	}

	outColor = March(ro, rd, tScene, int(uP1.w));

	// The reflection: where the surface here mirrors (its alpha), the ray goes on from it, mirrored about its normal
	// (from the depth round it), through what smoke lies that way - half the steps; what of it shows is weighed by
	// how much the surface mirrors, and by what of the surface the smoke in front still lets through
	outReflect = vec4(0.0);
	if (depth > 0.0 && outColor.a > 0.02)
	{
		ivec2 psize = textureSize(sampler2D(uPicture, uLinearClamp), 0);
		float share = 1.0 - texelFetch(sampler2D(uPicture, uLinearClamp), clamp(ivec2(vUV * vec2(psize)), ivec2(0), psize - 1), 0).a;
		if (share > 0.02)
		{
			// (the normal: the depth one texel over each way, the nearer pair - an edge then does not bend it - every
			// point rebuilt from its own texel's centre, the middle one too: this pixel's ray is half a texel off it)
			vec2 texel = 1.0 / vec2(dsize);
			vec3 px[5];
			for (int k = 0; k < 5; k++)
			{
				ivec2 o = k == 0 ? ivec2(1, 0) : (k == 1 ? ivec2(-1, 0) : (k == 2 ? ivec2(0, 1) : (k == 3 ? ivec2(0, -1) : ivec2(0))));
				vec2 uv = (vec2(dpx + o) + 0.5) * texel;
				float dd = texelFetch(sampler2D(uDepth, uLinearClamp), clamp(dpx + o, ivec2(0), dsize - 1), 0).r;
				vec2 n2 = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
				vec3 vr = vec3(n2.x / uFrame.projection.x, n2.y / uFrame.projection.y, -1.0);
				px[k] = ro + normalize(transpose(mat3(uFrame.view)) * vr) * (dd > 0.0 ? DepthDistance(dd) * length(vr) : 1.0e6);
			}
			vec3 here = px[4];
			vec3 dx = abs(dot(px[0] - here, rd)) < abs(dot(here - px[1], rd)) ? px[0] - here : here - px[1];
			vec3 dy = abs(dot(px[2] - here, rd)) < abs(dot(here - px[3], rd)) ? px[2] - here : here - px[3];
			vec3 n = normalize(cross(dy, dx));
			if (dot(n, rd) > 0.0) n = -n;
			vec3 r = reflect(rd, n);
			vec4 seen = March(here + n * 0.05, r, 60.0, int(uP1.w) / 2);
			float w = share * outColor.a;
			outReflect = vec4(seen.rgb * w, (1.0 - seen.a) * w);
		}
	}
}
