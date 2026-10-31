#version 450
#include "common.glsl"
#include "surface_draw.glsl"
#include "shadows.glsl"
#include "lighting.glsl"

layout(set = 1, binding = 2) uniform texture2D uBaseColorMap;       // sRGB (white when none)
layout(set = 1, binding = 3) uniform texture2D uNormalMap;          // tangent space
layout(set = 1, binding = 4) uniform texture2D uMetalRoughMap;      // g roughness, b metallic (glTF)
layout(set = 1, binding = 5) uniform texture2D uEmissionMap;        // sRGB
layout(set = 1, binding = 6) uniform sampler uMaterialSampler;
#include "skylight.glsl"
#include "atmosphere.glsl"
layout(set = 1, binding = 12) uniform texture2D uSkyView;          // (absolute radiance, every direction - sharp reflections)
layout(set = 1, binding = 13) uniform texture2D uSceneDepth;       // (the opaque scene's depth, 1x: what is behind a surface seen through)
layout(set = 1, binding = 14) uniform sampler uLinearClamp;
layout(set = 1, binding = 16) uniform texture2D uPreviousColor;     // (last frame's picture, linear HDR: what the reflections find)

// What a glossy surface mirrors of the picture itself: its reflected ray marched across the screen against the depth
// prepass (perspective-correct depth, the hit refined by halving), the colour there from last frame's picture. rgb the
// light, a how sure (0 where the ray left the screen, came back toward the eye, or found nothing - the sky's then).
// NoV: the view's angle to the surface - at grazing (the far sea) the ray skims the surface and finds the surface
// itself, the next wave: its own last picture mirrored at a Fresnel near 1, plus the sun's highlight, every frame -
// a feedback that grows to white along the horizon. There the sky's reflection (what it would find) takes over.
vec4 ScreenReflection(vec3 p, vec3 r, float roughness, float NoV)
{
	if (uShadow.reflections.x < 0.5 || roughness > 0.4) return vec4(0.0);
	// (ray-traced: what this pixel's ray found - its texture in last frame's picture's place - less sure the rougher)
	if (uShadow.reflections.x > 1.5)
	{
		vec4 found = textureLod(sampler2D(uPreviousColor, uLinearClamp), gl_FragCoord.xy * uFrame.viewport.zw, 0.0);
		return vec4(found.rgb, found.a * (1.0 - smoothstep(0.05, 0.4, roughness)));
	}
	vec3 eye = uFrame.cameraPosition.xyz;
	float reach = uShadow.reflections.y;
	// (a ray coming back toward the eye stops before the near plane)
	vec3 vr = mat3(uFrame.view) * r;
	vec3 vp = (uFrame.view * vec4(p, 1.0)).xyz;
	if (vr.z > 0.0) reach = min(reach, (-uFrame.projection.z * 2.0 - vp.z) / vr.z);
	if (reach <= 0.0) return vec4(0.0);
	// (from a little off the surface: the surface itself is in the depth it is marched against)
	vec3 start = p + r * (0.05 + 0.01 * length(p - eye));
	vec4 c0 = uFrame.viewProjection * vec4(start, 1.0), c1 = uFrame.viewProjection * vec4(p + r * reach, 1.0);
	if (c1.w <= 0.0) return vec4(0.0);
	vec2 s0 = vec2(c0.x / c0.w * 0.5 + 0.5, 0.5 - c0.y / c0.w * 0.5), s1 = vec2(c1.x / c1.w * 0.5 + 0.5, 0.5 - c1.y / c1.w * 0.5);
	// (the ray's distance in front of the eye along the screen line: perspective-correct - 1/distance is linear on the
	// screen - or, orthographic, linear itself)
	float z0 = -(uFrame.view * vec4(start, 1.0)).z, z1 = -(uFrame.view * vec4(p + r * reach, 1.0)).z;
	#define RAY_DEPTH(t) (Orthographic() ? mix(z0, z1, t) : 1.0 / mix(1.0 / z0, 1.0 / z1, t))
	ivec2 size = textureSize(sampler2D(uSceneDepth, uLinearClamp), 0);
	int steps = int(uShadow.reflections.z);
	// (the steps' offset turned each frame, and only a quarter of a step from pixel to pixel: a pixel's own whole offset
	// made the hits a checkerboard the temporal resolve's sharpening turned to a frost along every reflection; one offset
	// for all of them, rings)
	float jitter = fract(0.5 + uShadow.reflections.w * 0.61803398875 + 0.25 * fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))));
	float prevT = 0.0, hitT = -1.0, hitOver = 0.0;
	bool prevInFront = true;
	// (the steps over the part of the ray the screen shows, not all its reach: a ray that leaves the screen after a
	// tenth of it - most, from a floor - spent nine in ten of its steps off it, and the ones on it were a metre apart,
	// stepping over a ball half that size)
	vec2 sd = s1 - s0;
	float tEnd = 1.0;
	if (sd.x > 1e-6) tEnd = min(tEnd, (1.0 - s0.x) / sd.x); else if (sd.x < -1e-6) tEnd = min(tEnd, -s0.x / sd.x);
	if (sd.y > 1e-6) tEnd = min(tEnd, (1.0 - s0.y) / sd.y); else if (sd.y < -1e-6) tEnd = min(tEnd, -s0.y / sd.y);
	tEnd = clamp(tEnd, 0.0, 1.0);
	// (and no finer than a pixel's worth a step: a short ray is not marched 48 times over the same few pixels)
	float pixels = length(sd * tEnd * vec2(size));
	if (pixels < 1.0) return vec4(0.0);
	for (int i = 1; i <= 64; i++)
	{
		if (i > steps) break;
		// (steps closer together near the surface, where a near reflection needs them)
		float t = tEnd * pow((float(i) - 1.0 + jitter) / float(steps), 1.3);
		vec2 uv = mix(s0, s1, t);
		if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;
		float rayDepth = RAY_DEPTH(t);
		float d = texelFetch(sampler2D(uSceneDepth, uLinearClamp), ivec2(uv * vec2(size)), 0).r;
		float sceneDepth = DepthDistance(d);
		// (a hit where the ray went behind the picture between the last step and this one - not where it passes far
		// behind a thing: a thing has a thickness, here a share of its distance)
		// (thin: a ray that passes behind a thing is not a reflection of it - with a metre's thickness a ball's reflection
		// was a pillar, every ray under it that went behind it taken for one that met it; the steps are close enough now)
		float thickness = 0.12 + rayDepth * 0.025;
		bool behind = rayDepth > sceneDepth + 0.02 + rayDepth * 0.003;     // (the depth's own precision, wider far off)
		if (behind && prevInFront && rayDepth < sceneDepth + thickness)
		{
			// (the crossing refined: halve between the last step in front and this one)
			float lo = prevT, hi = t;
			for (int j = 0; j < 5; j++)
			{
				float m = (lo + hi) * 0.5;
				vec2 muv = mix(s0, s1, m);
				float md = DepthDistance(texelFetch(sampler2D(uSceneDepth, uLinearClamp), ivec2(muv * vec2(size)), 0).r);
				if (RAY_DEPTH(m) > md) hi = m; else lo = m;
			}
			hitT = hi;
			// (how far behind the picture it ended, of the thickness it may: the nearer the edge of what it may, the less sure)
			float hd = DepthDistance(texelFetch(sampler2D(uSceneDepth, uLinearClamp), ivec2(mix(s0, s1, hi) * vec2(size)), 0).r);
			hitOver = clamp((RAY_DEPTH(hi) - hd) / thickness, 0.0, 1.0);
			break;
		}
		prevInFront = !behind;
		prevT = t;
	}
	#undef RAY_DEPTH
	if (hitT < 0.0) return vec4(0.0);
	vec2 uv = mix(s0, s1, hitT);
	// How sure: not at the screen's edges, not for rays toward the eye, not on rough surfaces, less far along
	vec2 edge = smoothstep(vec2(0.0), vec2(0.08), uv) * (1.0 - smoothstep(vec2(0.92), vec2(1.0), uv));
	float sure = edge.x * edge.y * (1.0 - smoothstep(0.2, 0.4, roughness)) * (1.0 - smoothstep(0.0, 0.6, vr.z)) * (1.0 - smoothstep(0.75, 1.0, hitT))
		* smoothstep(0.01, 0.06, NoV) * (1.0 - smoothstep(0.35, 1.0, hitOver));
	// (what was there last frame - the picture is last frame's: the hit taken back to where it was in it, or a moving
	// camera's reflections show what was at that place on the screen then - the floor beside a ball, not the ball)
	// (a pixel and a half on along the ray, into what it met: at its edge the picture is the thing and what is behind it
	// blended - the bright floor behind a ball's rim reflected as a frost along every reflection's edge)
	uv = clamp(uv + normalize(sd) * 1.5 / vec2(size), vec2(0.0), vec2(1.0));
	float hd = texelFetch(sampler2D(uSceneDepth, uLinearClamp), ivec2(uv * vec2(size)), 0).r;
	vec3 hv = ViewPointAt(vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0), DepthDistance(max(hd, 1e-7)));
	vec2 puv = PreviousUV(transpose(mat3(uFrame.view)) * (hv - uFrame.view[3].xyz));
	if (puv.x < 0.0) return vec4(0.0);
	return vec4(textureLod(sampler2D(uPreviousColor, uLinearClamp), puv, 0.0).rgb, sure);
}

// How much of a mirror a smooth floor is past what Fresnel gives it: its material's SSR strength (normalRows[1].w - 0 for
// a material that did not ask), less for metal (it mirrors by its Fresnel already) and gone by a roughness of 0.4
float MirrorFloor(float metallic, float roughness)
{
	float strength = uDraw.normalRows[1].w;
	float polish = 1.0 - clamp(roughness / 0.4, 0.0, 1.0);
	return 0.55 * (1.0 - metallic) * strength * strength * polish * polish;
}

// The clouds' shadow on the sun's light (their shadow map, over the ground round the camera)
float CloudShadow(vec3 p)
{
	float deck = uShadow.rain.y > 0.0 ? uShadow.rain.y : 1.0;     // (an overcast deck's weight - shadows.glsl)
	if (uShadow.cloudGrid.w <= 0.0) return deck;
	vec2 uv = (p.xz - uShadow.cloudGrid.xy) * uShadow.cloudGrid.z;
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return deck;
	return deck * mix(1.0, textureLod(sampler2D(uCloudShadow, uLinearClamp), uv, 0.0).r, uShadow.cloudGrid.w);
}
// The sky toward d: the sky-view table where it is sharp, its spherical harmonics as a rough surface blurs it
vec3 SkyReflection(vec3 d, float roughness)
{
	vec2 uv = SkyViewUV(d, normalize(uFrame.sunDirection.xyz));
	vec3 sharp = textureLod(sampler2D(uSkyView, uLinearClamp), vec2(uv.x, 1.0 - uv.y), 0.0).rgb;
	return mix(sharp, SkyRadiance(d), smoothstep(0.08, 0.55, roughness));
}
layout(set = 1, binding = 11) uniform texture2D uAO;       // (half resolution, GTAO)
layout(set = 1, binding = 19) uniform texture2D uProbeAtlas;     // (the reflection probes' pictures: SceneRenderer's probe atlas)

// The scene's DDGI (SceneRenderer::SetGlobalIllumination): its probes' irradiance (filtered), the moments of the
// distances they see (RG32F, read by texel), their offsets; the grid after the reflection probes in the lights buffer
layout(set = 1, binding = 21) uniform texture2D uDDGIIrradiance;
layout(set = 1, binding = 22) uniform texture2D uDDGIVisibility;
layout(set = 1, binding = 23) uniform texture2D uDDGIProbes;
layout(set = 1, binding = 24) uniform texture2D uDDGIRadiance;     // (its reflections: every probe's tile at each roughness level)

vec2 DDGIOctEncode(vec3 d)
{
	float l1 = abs(d.x) + abs(d.y) + abs(d.z);
	if (l1 < 1e-20) return vec2(0.0);
	vec3 n = d / l1;
	if (n.z < 0.0)
	{
		float ox = n.x;
		n.x = (1.0 - abs(n.y)) * (ox >= 0.0 ? 1.0 : -1.0);
		n.y = (1.0 - abs(ox)) * (n.y >= 0.0 ? 1.0 : -1.0);
	}
	return n.xy;
}
// A probe's tile, toward dir, in its atlas's texels (the tiles res + 2 across: a border round each, for the filtering)
vec2 DDGITexel(float probe, vec3 dir, float res, float perRow)
{
	float tile = res + 2.0;
	vec2 at = vec2(mod(probe, perRow), floor(probe / perRow)) * tile;
	return at + vec2(1.0) + (DDGIOctEncode(dir) * 0.5 + 0.5) * res;
}
vec2 DDGIMoments(vec2 texel)
{
	ivec2 size = textureSize(sampler2D(uDDGIVisibility, uLinearClamp), 0);
	vec2 p = texel - 0.5;
	ivec2 a = ivec2(floor(p)); vec2 f = p - vec2(a);
	ivec2 a1 = clamp(a + ivec2(1), ivec2(0), size - 1); a = clamp(a, ivec2(0), size - 1);
	vec2 t = mix(texelFetch(sampler2D(uDDGIVisibility, uLinearClamp), a, 0).rg, texelFetch(sampler2D(uDDGIVisibility, uLinearClamp), ivec2(a1.x, a.y), 0).rg, f.x);
	vec2 b = mix(texelFetch(sampler2D(uDDGIVisibility, uLinearClamp), ivec2(a.x, a1.y), 0).rg, texelFetch(sampler2D(uDDGIVisibility, uLinearClamp), a1, 0).rg, f.x);
	return mix(t, b, f.y);
}
// The indirect light on a surface facing n here, from the eight probes round it - each weighted by whether it can see
// the point (a Chebyshev test against the moments of what it sees that way: a probe behind a wall does not light this
// side) and by facing it - as the old renderer's SampleDDGI. Irradiance over pi: times the albedo. covered: 0 no DDGI
vec3 DDGIIrradiance(vec3 p, vec3 n, out bool covered)
{
	covered = uLights.count.w > 0u;
	if (!covered) return vec3(0.0);
	Light G = uLights.lights[uLights.count.x + uLights.count.y + uLights.count.z];
	vec3 origin = G.positionRange.xyz, spacing = G.radianceType.xyz, counts = G.directionCosOuter.xyz;
	float perRow = G.positionRange.w, irrRes = G.cosInner.x, visRes = G.cosInner.y;
	bool offsets = G.radianceType.w > 0.5;
	vec2 irrSize = vec2(textureSize(sampler2D(uDDGIIrradiance, uLinearClamp), 0));
	// (looked up off the surface by a quarter of the closest spacing: on it, a probe's distance that way is this surface)
	vec3 wp = p + n * (0.25 * min(spacing.x, min(spacing.y, spacing.z)));
	vec3 g = clamp((wp - origin) / spacing, vec3(0.0), counts - vec3(1.0));
	vec3 baseF = min(floor(g), max(counts - vec3(2.0), vec3(0.0)));
	vec3 frac = g - baseF;
	vec3 sum = vec3(0.0);
	float weightSum = 0.0;
	for (int c = 0; c < 8; c++)
	{
		vec3 o = vec3(float(c & 1), float((c >> 1) & 1), float((c >> 2) & 1));
		vec3 pc = baseF + o;
		if (any(greaterThan(pc, counts - vec3(1.0)))) continue;
		vec3 tri = mix(vec3(1.0) - frac, frac, o);
		float weight = tri.x * tri.y * tri.z;
		if (weight <= 0.0) continue;
		float index = (pc.z * counts.y + pc.y) * counts.x + pc.x;
		vec3 probePos = origin + spacing * pc;
		if (offsets)
		{
			vec4 pd = texelFetch(sampler2D(uDDGIProbes, uLinearClamp), ivec2(int(mod(index, perRow)), int(floor(index / perRow))), 0);
			if (pd.w < 0.5) continue;     // (buried in the geometry: no light of its own)
			probePos += pd.xyz;
		}
		vec3 toProbe = probePos - wp;
		float dist = length(toProbe);
		if (dist > 1e-5) toProbe /= dist;
		float facing = (dot(toProbe, n) + 1.0) * 0.5;
		weight *= facing * facing + 0.2;
		vec2 moments = DDGIMoments(DDGITexel(index, -toProbe, visRes, perRow));
		float variance = max(moments.y - moments.x * moments.x, 0.0);
		if (dist > moments.x)
		{
			float d = dist - moments.x;
			float cheb = variance / (variance + d * d);
			weight *= max(cheb * cheb * cheb, 0.0);
		}
		if (weight <= 1e-6) continue;
		sum += textureLod(sampler2D(uDDGIIrradiance, uLinearClamp), DDGITexel(index, n, irrRes, perRow) / irrSize, 0.0).rgb * weight;
		weightSum += weight;
	}
	return weightSum > 1e-6 ? max(sum / weightSum, vec3(0.0)) : vec3(0.0);
}

// What the probes round here see along r, prefiltered for this roughness (the split sum's first half: times the
// environment BRDF) - the same eight probes, chosen the same way, as the irradiance: a wall that blocked the bounce
// but not the reflection would be far harder to see for a bug. As the old renderer's SampleDDGIRadiance. covered: 0
// none (no DDGI, or one without reflections)
vec3 DDGIReflection(vec3 p, vec3 n, vec3 r, float roughness, out bool covered)
{
	covered = false;
	if (uLights.count.w == 0u) return vec3(0.0);
	uint gi = uLights.count.x + uLights.count.y + uLights.count.z;
	Light G = uLights.lights[gi], R = uLights.lights[gi + 1u];
	float levels = R.positionRange.z;
	if (levels < 0.5) return vec3(0.0);
	covered = true;
	vec3 origin = G.positionRange.xyz, spacing = G.radianceType.xyz, counts = G.directionCosOuter.xyz;
	float perRow = G.positionRange.w, visRes = G.cosInner.y, probeTotal = G.directionCosOuter.w;
	float radRes = R.positionRange.x, radPerRow = R.positionRange.y, minR = R.positionRange.w;
	bool offsets = G.radianceType.w > 0.5;
	vec2 radSize = vec2(textureSize(sampler2D(uDDGIRadiance, uLinearClamp), 0));
	vec3 wp = p + n * (0.25 * min(spacing.x, min(spacing.y, spacing.z)));
	vec3 g = clamp((wp - origin) / spacing, vec3(0.0), counts - vec3(1.0));
	vec3 baseF = min(floor(g), max(counts - vec3(2.0), vec3(0.0)));
	vec3 frac = g - baseF;
	// (the two levels this roughness falls between: linear in roughness, as DDGIVolume::LevelRoughness filters them)
	float t = levels > 1.5 ? (clamp(roughness, minR, 1.0) - minR) / max(1.0 - minR, 1e-4) * (levels - 1.0) : 0.0;
	float lo = min(floor(t), levels - 1.0), hi = min(lo + 1.0, levels - 1.0), lt = clamp(t - lo, 0.0, 1.0);
	vec3 sum = vec3(0.0);
	float weightSum = 0.0;
	for (int c = 0; c < 8; c++)
	{
		vec3 o = vec3(float(c & 1), float((c >> 1) & 1), float((c >> 2) & 1));
		vec3 pc = baseF + o;
		if (any(greaterThan(pc, counts - vec3(1.0)))) continue;
		vec3 tri = mix(vec3(1.0) - frac, frac, o);
		float weight = tri.x * tri.y * tri.z;
		if (weight <= 0.0) continue;
		float index = (pc.z * counts.y + pc.y) * counts.x + pc.x;
		vec3 probePos = origin + spacing * pc;
		if (offsets)
		{
			vec4 pd = texelFetch(sampler2D(uDDGIProbes, uLinearClamp), ivec2(int(mod(index, perRow)), int(floor(index / perRow))), 0);
			if (pd.w < 0.5) continue;
			probePos += pd.xyz;
		}
		vec3 toProbe = probePos - wp;
		float dist = length(toProbe);
		if (dist > 1e-5) toProbe /= dist;
		// (by the normal, not the reflection: a probe behind the surface cannot light it however it is looked at)
		float facing = (dot(toProbe, n) + 1.0) * 0.5;
		weight *= facing * facing + 0.2;
		vec2 moments = DDGIMoments(DDGITexel(index, -toProbe, visRes, perRow));
		float variance = max(moments.y - moments.x * moments.x, 0.0);
		if (dist > moments.x)
		{
			float d = dist - moments.x;
			float cheb = variance / (variance + d * d);
			weight *= max(cheb * cheb * cheb, 0.0);
		}
		if (weight <= 1e-6) continue;
		vec3 a = textureLod(sampler2D(uDDGIRadiance, uLinearClamp), DDGITexel(lo * probeTotal + index, r, radRes, radPerRow) / radSize, 0.0).rgb;
		vec3 b = textureLod(sampler2D(uDDGIRadiance, uLinearClamp), DDGITexel(hi * probeTotal + index, r, radRes, radPerRow) / radSize, 0.0).rgb;
		sum += mix(a, b, lt) * weight;
		weightSum += weight;
	}
	return weightSum > 1e-6 ? max(sum / weightSum, vec3(0.0)) : vec3(0.0);
}

// The ambient light on a surface facing n where there is no sky, irradiance over pi (times the albedo), by its mode:
// the flat colour, a gradient ground - horizon - sky by n.y, or the spherical harmonics' irradiance (Ramamoorthi &
// Hanrahan 2001) - as the old renderer's AmbientAt
vec3 AmbientWithoutSky(vec3 n)
{
	float mode = uFrame.ambientSky.w;
	if (mode > 1.5)
	{
		const float c1 = 0.429043, c2 = 0.511664, c3 = 0.743125, c4 = 0.886227, c5 = 0.247708;
		vec3 e = uFrame.ambientSH[8].rgb * (c1 * (n.x * n.x - n.y * n.y)) + uFrame.ambientSH[6].rgb * (c3 * n.z * n.z)
			+ uFrame.ambientSH[0].rgb * c4 - uFrame.ambientSH[6].rgb * c5
			+ uFrame.ambientSH[4].rgb * (2.0 * c1 * n.x * n.y) + uFrame.ambientSH[7].rgb * (2.0 * c1 * n.x * n.z) + uFrame.ambientSH[5].rgb * (2.0 * c1 * n.y * n.z)
			+ uFrame.ambientSH[3].rgb * (2.0 * c2 * n.x) + uFrame.ambientSH[1].rgb * (2.0 * c2 * n.y) + uFrame.ambientSH[2].rgb * (2.0 * c2 * n.z);
		return max(e / PI, vec3(0.0));
	}
	if (mode > 0.5)
	{
		float y = clamp(n.y, -1.0, 1.0);
		return y >= 0.0 ? mix(uFrame.ambientEquator.rgb, uFrame.ambientSky.rgb, y) : mix(uFrame.ambientEquator.rgb, uFrame.ambientGround.rgb, -y);
	}
	return uFrame.ambient.rgb;
}

// A reflection probe's picture at one roughness level k (its block at origin: 256 wide halving to 32, the levels one
// under the other), toward d - bilinear, round in longitude
vec3 ProbeLevel(vec3 d, int k, ivec2 origin)
{
	int lw = max(256 >> k, 32), lh = lw / 2;
	int oy = 0;
	for (int i = 0; i < 6; i++) { if (i < k) oy += max(256 >> i, 32) / 2; }
	vec2 uv = vec2(0.5 + atan(d.x, -d.z) * 0.15915494, acos(clamp(d.y, -1.0, 1.0)) * 0.31830989);
	vec2 p = uv * vec2(float(lw), float(lh)) - 0.5;
	ivec2 a = ivec2(floor(p)); vec2 f = p - vec2(a);
	int x0 = (a.x % lw + lw) % lw, x1 = (x0 + 1) % lw;
	int y0 = clamp(a.y, 0, lh - 1), y1 = clamp(a.y + 1, 0, lh - 1);
	ivec2 o = origin + ivec2(0, oy);
	vec3 t = mix(texelFetch(sampler2D(uProbeAtlas, uLinearClamp), o + ivec2(x0, y0), 0).rgb, texelFetch(sampler2D(uProbeAtlas, uLinearClamp), o + ivec2(x1, y0), 0).rgb, f.x);
	vec3 b = mix(texelFetch(sampler2D(uProbeAtlas, uLinearClamp), o + ivec2(x0, y1), 0).rgb, texelFetch(sampler2D(uProbeAtlas, uLinearClamp), o + ivec2(x1, y1), 0).rgb, f.x);
	return mix(t, b, f.y);
}
// What a reflection probe this point is inside saw where the reflected ray r leaves its box (box projection: a wall is
// reflected where the wall is), at this roughness; cover how much of the point it has (0 none, fading out over its
// blend at the box's faces). The probes come after the lights and the 2D casters, the smallest box first; the first
// that holds the point is its probe - as the old renderer chose them
vec3 ProbeReflection(vec3 p, vec3 r, float roughness, out float cover)
{
	cover = 0.0;
	uint first = uLights.count.x + uLights.count.y;
	for (uint i = 0u; i < uLights.count.z; i++)
	{
		Light P = uLights.lights[first + i];
		vec4 bmin = P.positionRange, bmax = P.radianceType;
		if (any(lessThan(p, bmin.xyz)) || any(greaterThan(p, bmax.xyz))) continue;
		vec3 inside = min(p - bmin.xyz, bmax.xyz - p);
		float edge = min(min(inside.x, inside.y), inside.z);
		cover = bmin.w > 0.0 ? clamp(edge / bmin.w, 0.0, 1.0) : 1.0;
		vec3 safeR = vec3(abs(r.x) > 1e-5 ? r.x : 1e-5, abs(r.y) > 1e-5 ? r.y : 1e-5, abs(r.z) > 1e-5 ? r.z : 1e-5);
		vec3 tf = max((bmax.xyz - p) / safeR, (bmin.xyz - p) / safeR);
		float t = min(min(tf.x, tf.y), tf.z);
		vec3 d = normalize(p + r * t - P.directionCosOuter.xyz);
		int block = int(bmax.w + 0.5);
		ivec2 origin = ivec2((block % 4) * 256, (block / 4) * 272);
		float lv = clamp(roughness, 0.0, 1.0) * 5.0;
		int k = int(floor(lv));
		return mix(ProbeLevel(d, k, origin), ProbeLevel(d, min(k + 1, 5), origin), lv - float(k));
	}
	return vec3(0.0);
}

// What a surface is, for the light: filled from the material's maps, then by a custom material's own Surface()
struct SurfaceData
{
	vec3 position, normal, view;   // world: the shading normal (Surface() may change it), toward the eye
	vec2 uv;
	vec3 albedo; float alpha;
	float metallic, roughness;
	vec3 emission; float occlusion;
};

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexcoord;
layout(location = 3) in vec4 vTint;

layout(location = 0) out vec4 outColor;      // linear HDR

#ifdef CUSTOM_SURFACE
#define P3D_STAGE_FRAGMENT 1
// A material's surface (CustomShaderMaterial::SetSurfaceSource), compiled into this at load; what its text may read
#define uTime (uFrame.sky.z)
#define uCameraPosition (uFrame.cameraPosition.xyz)
#define vWorldPos vec4(vWorldPosition, 1.0)
#define vNormalWorld vNormal
#define texture_2D texture
#define uOpacity (uDraw.baseColor.a)
#define uGlobals (uFrame.globals)
#define P3D_PI PI
//P3D_SURFACE_DECLARATIONS
//P3D_SURFACE
#endif

// The tangent frame from the screen-space derivatives of the position and the texcoord (Schüler, "Normal
// Mapping Without Precomputed Tangents") - no tangent attribute needed. t and b are the texcoord's gradients times
// the determinant dot(dp1, dp2perp); his drops its sign, right where the screen's y goes up (OpenGL) - here it goes
// down (Metal, Vulkan, WebGPU) and the determinant is negative: without it the frame is turned over (normal maps lit
// inside out, parallax sunk in). (A texture mirrored on the mesh is in the gradients already.)
mat3 TangentFrame(vec3 n, vec3 p, vec2 uv)
{
	vec3 dp1 = dFdx(p), dp2 = dFdy(p);
	vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
	vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
	vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
	float det = dot(dp1, dp2perp);
	float invmax = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-20)) * (det < 0.0 ? -1.0 : 1.0);
	return mat3(t * invmax, b * invmax, n);
}
vec3 PerturbNormal(vec3 n, vec3 p, vec2 uv, vec3 mapped) { return normalize(TangentFrame(n, p, uv) * mapped); }

// A 2D light's shadow (Occluder2D): the scene's casting segments after the lights; the ray from the point to the light
// crossing two of them is blocked - one crossing is the caster's own edge on the way out (it would shadow itself). As
// the old renderer's: half-open along the segment, so a corner two share counts once
float Shadow2D(vec2 p, vec2 lightPos)
{
	uint n = uLights.count.y;
	if (n == 0u) return 1.0;
	int crossings = 0;
	vec2 r = lightPos - p;
	for (uint i = 0u; i < n; i++)
	{
		vec4 seg = uLights.lights[uLights.count.x + i].positionRange;
		vec2 s = seg.zw - seg.xy, q = seg.xy - p;
		float denom = r.x * s.y - r.y * s.x;
		if (abs(denom) < 1e-6) continue;
		float t = (q.x * s.y - q.y * s.x) / denom, u = (q.x * r.y - q.y * r.x) / denom;
		if (t > 1e-3 && t < 1.0 - 1e-3 && u >= 0.0 && u < 1.0) crossings++;
	}
	return crossings >= 2 ? 0.0 : 1.0;
}

#ifndef CUSTOM_SURFACE
layout(set = 1, binding = 18) uniform texture2D uHeightMap;         // (r: how deep - white the deepest; parallax)
// Parallax occlusion: the view ray marched into the height field in tangent space (the frame as PerturbNormal's), more
// layers the more grazing, the crossing found between the last two by their depths. The texcoord where the ray meets it.
vec2 ParallaxUV(vec3 n, vec3 p, vec2 uv, vec3 toEye, float depth)
{
	vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
	mat3 tbn = TangentFrame(n, p, uv);
	vec3 v = normalize(vec3(dot(toEye, tbn[0]), dot(toEye, tbn[1]), dot(toEye, n)));
	float layers = mix(32.0, 8.0, clamp(v.z, 0.0, 1.0));
	float layer = 1.0 / layers;
	vec2 step = v.xy / max(v.z, 0.1) * depth * layer;
	// (the gradients of the surface's own texcoord: the loop's would not be continuous)
	vec2 cur = uv;
	float d = 0.0;
	float h = textureGrad(sampler2D(uHeightMap, uMaterialSampler), cur, duv1, duv2).r;
	for (int i = 0; i < 32 && d < h; i++)
	{
		cur -= step;
		h = textureGrad(sampler2D(uHeightMap, uMaterialSampler), cur, duv1, duv2).r;
		d += layer;
	}
	vec2 prev = cur + step;
	float after = h - d;
	float before = textureGrad(sampler2D(uHeightMap, uMaterialSampler), prev, duv1, duv2).r - d + layer;
	float w = after / min(after - before, -1e-5);
	return mix(cur, prev, clamp(w, 0.0, 1.0));
}
#endif

void main()
{
	vec3 ng = normalize(vNormal);
	// (where its normal points away from the eye - the back of a two-sided card, a silhouette's edge - mirrored through
	// the face, not turned round: a picture card's normal leans up so that the sun lights it alike from either side, and
	// turned round it faced the ground and the card's back was a dark slab; straight on, mirroring is turning round)
	{
		vec3 face = normalize(cross(dFdx(vWorldPosition), dFdy(vWorldPosition)));
		if (dot(face, ToEye(vWorldPosition)) < 0.0) face = -face;
		float away = dot(ng, face);
		if (away < 0.0) ng = normalize(ng - 2.0 * away * face);
	}
	// (the maps read where the view meets the height field, with a height map; by the surface's own gradients)
	vec2 uv = vTexcoord;
	const vec2 gx = dFdx(vTexcoord), gy = dFdy(vTexcoord);
#ifndef CUSTOM_SURFACE
	if (uDraw.skin.y > 0.0) uv = ParallaxUV(ng, vWorldPosition, vTexcoord, ToEye(vWorldPosition), uDraw.skin.y);
#endif
#define MAP(t) textureGrad(sampler2D(t, uMaterialSampler), uv, gx, gy)
	vec4 base = MAP(uBaseColorMap) * uDraw.baseColor * vTint;
	vec3 n = ng;
	if (uDraw.surface.w > 0.5)
	{
		vec3 m = MAP(uNormalMap).xyz * 2.0 - 1.0;
		n = PerturbNormal(ng, vWorldPosition, vTexcoord, m);
	}
	vec4 mr = MAP(uMetalRoughMap);
	SurfaceData s;
	s.position = vWorldPosition; s.normal = n; s.view = ToEye(vWorldPosition); s.uv = uv;
	s.albedo = base.rgb; s.alpha = base.a;
	s.metallic = uDraw.surface.y * mr.b; s.roughness = uDraw.surface.x * mr.g;
	s.emission = uDraw.emission.rgb * MAP(uEmissionMap).rgb;
#undef MAP
	s.occlusion = 1.0;
#ifdef CUSTOM_SURFACE
	Surface(s);
#else
	// Wet (GenericShaderMaterial::SetWetness): a soaked surface is darker - water fills its pores, so less light comes
	// back out of it - and glossier, a film over it. Its puddles: of what faces up, a share lies under still water -
	// fixed in the world, not on the texture - a mirror there, its bumps gone, and the ground under it darker still.
	// (and the rain - RenderGlobals::Rain: what is out in it, facing up, soaked and puddled in proportion; what faces
	// sideways only damp - a lit, lacquered material - the paint of a car, say, metal - gets the film, not the darkening)
	float rainUp = uFrame.weather.x > 0.0 ? uFrame.weather.x * smoothstep(0.2, 0.9, ng.y) * RainExposure(vWorldPosition) : 0.0;
	vec4 wetness = vec4(max(uDraw.wet.x, rainUp * 0.8 + (rainUp > 0.0 ? uFrame.weather.x * 0.15 : 0.0)), max(uDraw.wet.y, rainUp > 0.4 ? (rainUp - 0.4) * 0.6 : 0.0), uDraw.wet.y > 0.0 ? uDraw.wet.z : 3.0, 0.0);
	if (uDraw.skin.z > 0.5) wetness = vec4(0.0);     // (the unlit and the 2D: not out in any weather)
	if (wetness.x > 0.0 || wetness.y > 0.0)
	{
		float wet = wetness.x;
		if (wetness.y > 0.0)
		{
			vec2 q = vWorldPosition.xz / wetness.z;
			vec2 i = floor(q), f = fract(q);
			f = f * f * (3.0 - 2.0 * f);
			float h00 = fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453), h10 = fract(sin(dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7))) * 43758.5453);
			float h01 = fract(sin(dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7))) * 43758.5453), h11 = fract(sin(dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);
			float field = mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
			float up = smoothstep(0.85, 0.97, ng.y);
			float puddle = smoothstep(1.0 - wetness.y, 1.0 - wetness.y + 0.08, field) * up;
			s.normal = normalize(mix(s.normal, ng, puddle));
			s.albedo *= 1.0 - 0.25 * puddle;
			s.roughness = mix(s.roughness, 0.02, puddle);
			wet = max(wet, puddle);
		}
		s.albedo *= mix(1.0, 0.55, wet * (1.0 - s.metallic));
		s.roughness = mix(s.roughness, min(s.roughness, 0.12), wet);
	}
#endif
#ifndef NOCUT
	if (uDraw.emission.w > 0.0 && s.alpha < uDraw.emission.w) discard;
#endif     // (NOCUT: an alpha-cut surface's light, at the depth its cut laid down - alphacut.frag)
#ifndef CUSTOM_SURFACE
	// A generic material's own light, as its flags say (skin.z): 1 none - the picture as it is (no Diffuse, PBR or cel
	// shading); 2 2D (Lighting2D) - the lights by distance alone, no angle, a sprite lit by the lights near it. skin.w 1:
	// additive - its colour by its own cover (the pipeline adds it as it is)
	if (uDraw.skin.z > 0.5)
	{
		vec3 c2 = s.albedo;
		if (uDraw.skin.z > 1.5)
		{
			vec3 light = uFrame.sunDirection.w > 0.0 ? uFrame.sunRadiance.rgb : vec3(0.0);
			for (uint i = 0u; i < uLights.count.x; i++)
			{
				Light L = uLights.lights[i];
				if (L.radianceType.w > 1.5) { light += L.radianceType.rgb; continue; }     // (a second sun: as the first)
				float d = length(L.positionRange.xyz - vWorldPosition);
				if (d < L.positionRange.w) light += L.radianceType.rgb * DistanceFalloff(d, L.positionRange.w) * Shadow2D(vWorldPosition.xy, L.positionRange.xy);
			}
			c2 = s.albedo * (uFrame.ambient.rgb + light / PI);
		}
		c2 += s.emission;
		outColor = vec4(uDraw.skin.w > 0.5 ? c2 * s.alpha : c2, s.alpha);
		return;
	}
#endif

	n = normalize(s.normal);
	vec3 v = s.view;
	float roughness = clamp(s.roughness, 0.045, 1.0);
	float metallic = clamp(s.metallic, 0.0, 1.0);
	float a = roughness * roughness;
	vec3 f0 = mix(vec3(0.08 * uDraw.surface.z), s.albedo, metallic);
	vec3 diffuseColor = s.albedo * (1.0 - metallic);

	// (the shadow by the geometric normal: a normal map's bumps would only move the offset about)
	// (ray-traced: the share of the sun the rays found, beside the AO - uAO's green; else the cascades)
	float sunShadow = uFrame.frame.w == 4.0 ? 1.0 : uFrame.weather.y > 0.5
		? textureLod(sampler2D(uAO, uMaterialSampler), gl_FragCoord.xy * uFrame.viewport.zw, 0.0).g
		: SunShadow(vWorldPosition, ng, gl_FragCoord.xy);
	sunShadow *= CloudShadow(vWorldPosition);
	vec3 color = AllLights(vWorldPosition, n, ng, v, diffuseColor, f0, a, sunShadow);
	// Sunlight through a thin surface (GenericShaderMaterial::SetLightThrough - a leaf, a blade of grass): from behind it,
	// shadowed by what is between it and the sun - its shadow looked up from the sun's side, so the leaf does not shadow
	// its own back - brightest looking into the sun. The side of a crown turned from the sun is green, not black.
	// (a generic material's: a custom surface has none - and a second shadow lookup inlined into a large surface was
	// more than Apple's shader compiler service survives: XPC_ERROR_CONNECTION_INTERRUPTED on Erangel's sea)
#ifndef CUSTOM_SURFACE
	if (uDraw.normalRows[0].w > 0.0 && uFrame.sunDirection.w > 0.0)
	{
		vec3 l = normalize(uFrame.sunDirection.xyz);
		float behind = max(dot(-n, l), 0.0);
		float into = pow(max(dot(-v, l), 0.0), 4.0);
		float through = (uFrame.frame.w == 4.0 ? 1.0 : SunShadowCheap(vWorldPosition, -ng)) * CloudShadow(vWorldPosition);
		color += diffuseColor * uFrame.sunRadiance.rgb * through * uDraw.normalRows[0].w * (behind * 0.6 + into * 0.8) / PI;
	}
	// Light under the skin (GenericShaderMaterial::SetSubsurface - skin, wax, a leaf, marble): it enters, travels, comes
	// out nearby tinted by what it went through - the lit side's edge wider and warm, the far side of something thin
	// glowing, a shadow from close by partly seen through. Wrapped diffuse past the terminator, tinted, and a little of
	// the sun through from behind, shadowed from its side.
	if (uDraw.subsurface.w > 0.0 && uFrame.sunDirection.w > 0.0)
	{
		vec3 l = normalize(uFrame.sunDirection.xyz);
		float w = uDraw.subsurface.w;
		float NoL = dot(n, l);
		float past = max(max((NoL + w) / ((1.0 + w) * (1.0 + w)), 0.0) - max(NoL, 0.0), 0.0);     // (what wraps past the terminator)
		float back = pow(max(dot(-v, l), 0.0), 3.0) * max(-NoL, 0.0) * w;
		float lit = mix(sunShadow, 1.0, 0.35 * w);     // (a shadow from close by partly seen through)
		color += diffuseColor * uDraw.subsurface.rgb * uFrame.sunRadiance.rgb * (past * lit + back * 0.5) / PI;
	}
#endif
	// What of the light from all round reaches here (GTAO), and its share for the specular (Lagarde & de Rousiers)
	float ao = (uFrame.frame.w == 2.0 ? 1.0 : textureLod(sampler2D(uAO, uMaterialSampler), gl_FragCoord.xy * uFrame.viewport.zw, 0.0).r) * s.occlusion;
	float NoV = max(dot(n, v), 1e-4);
	float specAO = clamp(pow(NoV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
	if (uFrame.frame.z > 0.5)
	{
		// The sky's light: its irradiance on the surface, and what of it the surface reflects toward the eye
		vec3 r = reflect(-v, n);
		// (what it mirrors: the picture itself where a ray finds it, the sky - with its clouds - where not)
		// (inside a reflection probe's box, what it saw in place of the sky: the room, not the sky through its ceiling)
		vec4 found = ScreenReflection(vWorldPosition, r, roughness, NoV);
		float probeCover;
		vec3 probe = ProbeReflection(vWorldPosition, r, roughness, probeCover);
		// (the sky's, or where the scene's light is solved with reflections, what its probes see; a reflection probe's over it)
		bool giReflects;
		vec3 giReflection = DDGIReflection(vWorldPosition, n, r, roughness, giReflects);
		vec3 mirrored = mix(mix(giReflects ? giReflection : SkyReflection(r, roughness), probe, probeCover), found.rgb, found.a);
		if (uFrame.frame.w == 9.0) { outColor = vec4(0.0, found.a, found.a > 0.0 ? 0.0 : 0.15, 1.0); return; }     // (PYROS_RENDERER_DEBUG=ssr: green where a reflection was found, how sure; blue none)
		if (uFrame.frame.w == 10.0 && uDraw.normalRows[1].w > 0.0) { outColor = vec4(found.rgb * found.a, 1.0); return; }     // (=ssrcolor: what a mirror floor found, the rest drawn as it is)
		// (the scene's solved light, where it has one: its bounces and its sky as the probes saw them, in place of the sky's)
		bool solved;
		vec3 indirect = DDGIIrradiance(vWorldPosition, n, solved);
		color += diffuseColor * (solved ? indirect : SkyIrradiance(n) / PI) * ao;
		// (a polished floor's mirror - the material's SSR strength: more of the reflection than Fresnel gives, over its own
		// colour, where something was found - the old renderer's mirror floor)
		float mirror = MirrorFloor(metallic, roughness) * found.a;
		color = color * (1.0 - mirror) + mirrored * max(EnvBRDFApprox(f0, roughness, NoV), vec3(mirror)) * specAO;
	}
	else
	{
		bool solved;
		vec3 indirect = DDGIIrradiance(vWorldPosition, n, solved);
		color += diffuseColor * (solved ? indirect : AmbientWithoutSky(n)) * ao;     // (the solved light, else the scene's ambient where there is no sky)
		// (and what it mirrors: the picture itself where a ray finds it - the screen's, or the traced ones - over what a
		// reflection probe saw, or the solved light's probes; nothing else - there is no sky to mirror)
		vec3 r = reflect(-v, n);
		vec4 found = ScreenReflection(vWorldPosition, r, roughness, NoV);
		float probeCover;
		vec3 probe = ProbeReflection(vWorldPosition, r, roughness, probeCover);
		bool giReflects;
		vec3 giReflection = DDGIReflection(vWorldPosition, n, r, roughness, giReflects);
		vec3 mirrored = mix(mix(giReflects ? giReflection : vec3(0.0), probe, probeCover), found.rgb, found.a);
		if (uFrame.frame.w == 9.0) { outColor = vec4(0.0, found.a, found.a > 0.0 ? 0.0 : 0.15, 1.0); return; }     // (PYROS_RENDERER_DEBUG=ssr: green where a reflection was found, how sure; blue none)
		if (uFrame.frame.w == 10.0 && uDraw.normalRows[1].w > 0.0) { outColor = vec4(found.rgb * found.a, 1.0); return; }     // (=ssrcolor: what a mirror floor found, the rest drawn as it is)
		float mirror = MirrorFloor(metallic, roughness) * found.a;
		color = color * (1.0 - mirror) + mirrored * max(EnvBRDFApprox(f0, roughness, NoV), vec3(mirror)) * specAO;
	}
	color += s.emission;

	if (uFrame.frame.w == 1.0)
	{
		const vec3 tint[5] = vec3[](vec3(1, 0.3, 0.3), vec3(0.3, 1, 0.3), vec3(0.3, 0.3, 1), vec3(1, 1, 0.3), vec3(1));
		color *= tint[SunCascade(vWorldPosition)];
	}
	// The air between (with the sky): the sky's radiance toward the point - just above the horizon where the point is
	// below it, as the air in front of the ground is lit like the low sky
	if (uFrame.sky.w > 0.0 && uFrame.frame.w == 0.0)
	{
		vec3 toPoint = normalize(vWorldPosition - uFrame.cameraPosition.xyz);
		vec3 skyDir = normalize(vec3(toPoint.x, max(toPoint.y, 0.02), toPoint.z));
		vec2 suv = SkyViewUV(skyDir, normalize(uFrame.sunDirection.xyz));
		vec3 skyL = textureLod(sampler2D(uSkyView, uLinearClamp), vec2(suv.x, 1.0 - suv.y), 0.0).rgb;
		color = AerialPerspective(color, uFrame.cameraPosition.xyz, uFrame.sky.x, vWorldPosition, skyL, uFrame.sky.w) ;
		// (a see-through surface's alpha lets the air behind it through too: its share kept as it was)
	}
	if (uFrame.frame.w == 3.0) color = vec3(ao);
	// The surface's own outputs (SceneRenderer::SetDebugView - a bake reads them back; with the tone curve off the
	// target's sRGB store makes each byte what is said): 6 its albedo, unlit (stored sRGB); 7 its normal in view space,
	// 0..1 (stored as is); 8 its distance through the view's depth range, 0 near to 1 far (stored as is)
	if (uFrame.frame.w >= 6.0 && uFrame.frame.w <= 8.0)     // (albedo, normal, depth - not the reflections' views)
	{
		vec3 vn = normalize(mat3(uFrame.view) * n) * 0.5 + 0.5;
		float far = Orthographic() ? uFrame.projection.w : 1000.0;
		float d01 = clamp((DepthDistance(gl_FragCoord.z) - uFrame.projection.z) / max(far - uFrame.projection.z, 1e-4), 0.0, 1.0);
		color = uFrame.frame.w == 6.0 ? s.albedo : SrgbDecode(uFrame.frame.w == 7.0 ? vn : vec3(d01));
	}
	if (uFrame.frame.w == 5.0) color = vec3(CloudShadow(vWorldPosition));      // (PYROS_RENDERER_DEBUG=cloudshadow)
	outColor = vec4(color, s.alpha);
}
