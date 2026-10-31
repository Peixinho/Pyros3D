#version 450
#include "common.glsl"
#include "atmosphere.glsl"
#include "clouds.glsl"
// Clouds: a slab of them, marched at half resolution through a tileable 3D noise - its base shape cut by the cover,
// worn away by finer noise - each step lit by the sun through the cloud toward it (with a share of the light scattered
// many times, which reaches deeper) and by the sky itself; faded into the sky's own colour with distance.
// Out: rgb the light the clouds send toward the eye, a: how much of what is behind them gets through. DOME: the
// sky-view table's directions instead, the clouds over the sky - what the surfaces mirror and the sky's light is summed from.
layout(set = 1, binding = 3) uniform texture2D uSkyView;
layout(set = 1, binding = 4) uniform sampler uLinearClamp;
layout(set = 1, binding = 6) uniform texture2D uHistory;     // (last frame's clouds: each frame a new jitter, blended into them)
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float Noise01(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

vec3 SkyRadiance(vec3 d)
{
	vec2 uv = SkyViewUV(d, normalize(uFrame.sunDirection.xyz));
	return textureLod(sampler2D(uSkyView, uLinearClamp), vec2(uv.x, 1.0 - uv.y), 0.0).rgb;
}

float HenyeyGreenstein(float c, float g)
{
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(1.0 + g2 - 2.0 * g * c, 1e-4), 1.5));
}

// The clouds along a ray: the light they send back (L) and what gets through them (T); `at` how far off they are
bool March(vec3 eye, vec3 dir, int steps, float jitter, out vec3 L, out float T, out float at)
{
	L = vec3(0.0); T = 1.0; at = 0.0;
	if (uSky.clouds.x <= 0.0) return false;
	float bottom = uSky.clouds.z, top = uSky.clouds.w;
	// Where the ray is in the slab (flat: toward the horizon it goes at most 40 km)
	float t0, t1;
	if (eye.y < bottom) { if (dir.y <= 0.01) return false; t0 = (bottom - eye.y) / dir.y; t1 = (top - eye.y) / dir.y; }
	else if (eye.y > top) { if (dir.y >= -0.01) return false; t0 = (top - eye.y) / dir.y; t1 = (bottom - eye.y) / dir.y; }
	else { t0 = 0.0; t1 = dir.y > 0.01 ? (top - eye.y) / dir.y : dir.y < -0.01 ? (bottom - eye.y) / dir.y : 40000.0; }
	if (t0 > 40000.0) return false;
	t1 = min(t1, t0 + 12000.0);
	float dt = (t1 - t0) / float(steps);
	vec3 sunDir = normalize(uFrame.sunDirection.xyz);
	vec3 sun = uFrame.sunDirection.w > 0.0 ? uFrame.sunRadiance.rgb : vec3(0.0);     // (the sun's illuminance, through the air)
	float c = dot(dir, sunDir);
	// (bright toward the sun - a silver lining - and a little back toward it)
	float phase = mix(HenyeyGreenstein(c, 0.8), HenyeyGreenstein(c, -0.3), 0.25);
	vec3 skyTop = SkyRadiance(vec3(0.0, 1.0, 0.0)), skySide = SkyRadiance(normalize(vec3(-sunDir.x, 0.1, -sunDir.z)));     // (the sky above, and its horizon away from the sun)
	float k = uSky.clouds.y;
	float weightedT = 0.0, weight = 0.0;
	for (int i = 0; i < 128; i++)
	{
		if (i >= steps || T < 0.01) break;
		float t = t0 + (float(i) + jitter) * dt;
		vec3 p = eye + dir * t;
		float h = (p.y - bottom) / (top - bottom);
		float d = Density(p, h, true);
		if (d <= 0.0) continue;
		float sigma = d * k;
		// The sun through the cloud toward it: steps that grow, the cloud's shape only
		float tau = 0.0, step = 60.0;
		vec3 lp = p;
		for (int j = 0; j < 6; j++)
		{
			lp += sunDir * step;
			float lh = (lp.y - bottom) / (top - bottom);
			if (lh > 1.0) break;
			tau += Density(lp, lh, false) * k * step;
			step *= 2.0;
		}
		// The sun's light: scattered once it falls off fast; scattered many times - most of the light inside a deck of
		// cloud - it reaches far deeper and has forgotten where it came from. Three octaves of it (Wrenninge's
		// approximation): each half as bright, extinguished less, its phase less forward. Without them a thick overcast
		// had only the blue sky's light: it read as clear sky; with them it is the bright grey it is.
		float sunLight = 0.0, a = 1.0, b = 1.0, g = 1.0;
		for (int o = 0; o < 3; o++)
		{
			sunLight += a * mix(HenyeyGreenstein(c, 0.8 * g), HenyeyGreenstein(c, -0.3 * g), 0.25) * exp(-tau * b);
			a *= 0.5; b *= 0.35; g *= 0.5;
		}
		// (and the cloud's edges darker than its body - in-scattering builds up inside it: the powder look)
		float powder = 1.0 - exp(-sigma * 120.0);
		vec3 ambient = mix(skySide, skyTop, clamp(h, 0.0, 1.0)) * (0.5 + 0.5 * h);
		// (the light the lit ground sends back up, onto the cloud's underside: a quarter of what falls on it)
		vec3 bounce = sun * (0.25 * max(sunDir.y, 0.0) / PI) * (1.0 - clamp(h, 0.0, 1.0));
		vec3 S = (sun * sunLight * mix(1.0, powder, 0.5) + ambient + bounce) * sigma;
		float stepT = exp(-sigma * dt);
		L += T * (S - S * stepT) / max(sigma, 1e-7);      // (the step's light, integrated through it)
		weightedT += t * T * (1.0 - stepT); weight += T * (1.0 - stepT);
		T *= stepT;
	}
	// Further off, more of the air in front: toward the sky's own colour
	float fade = exp(-t0 / 30000.0);
	L = L * fade + SkyRadiance(dir) * (1.0 - T) * (1.0 - fade);
	at = weight > 1e-4 ? weightedT / weight : t0 + (t1 - t0) * 0.3;
	return true;
}

void main()
{
	vec3 eye = uFrame.cameraPosition.xyz;
#ifdef DOME
	// The sky-view table's directions, clouded: what the surfaces mirror and the sky's light is summed from
	vec3 dir = SkyViewDirection(vec2(vUV.x, 1.0 - vUV.y), normalize(uFrame.sunDirection.xyz));
	vec3 L; float T, at;
	March(eye, dir, 16, 0.5, L, T, at);
	outColor = vec4(SkyRadiance(dir) * T + L, 1.0);
#else
	outColor = vec4(0.0, 0.0, 0.0, 1.0);
	vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
	vec4 far = uSky.inverseViewProjection * vec4(ndc, 0.5, 1.0);
	vec3 dir = normalize(far.xyz / far.w - eye);
	// (a new offset along the ray each frame - the golden ratio's steps: over frames they fill the gaps evenly)
	float jitter = fract(Noise01(gl_FragCoord.xy) + uSky.history.z * 0.61803398875);
	vec3 L; float T, at;
	if (!March(eye, dir, int(uSky.wind.w), jitter, L, T, at)) return;
	vec4 current = vec4(L, T);
	// Into last frame's: where these clouds were then, through last frame's view
	if (uSky.history.x > 0.5)
	{
		vec4 clip = uSky.previousViewProjection * vec4(eye + dir * at, 1.0);
		vec2 prev = vec2(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
		if (clip.w > 0.0 && all(greaterThanEqual(prev, vec2(0.0))) && all(lessThanEqual(prev, vec2(1.0))))
			current = mix(textureLod(sampler2D(uHistory, uLinearClamp), prev, 0.0), current, uSky.history.y);
	}
	outColor = current;
#endif
}
