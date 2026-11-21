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

const bool CLOUD_SCREEN_JITTER = true;      // (true: the old offset fixed per screen pixel)
float Noise01(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

vec3 SkyRadiance(vec3 d)
{
	vec2 uv = SkyViewUV(d, normalize(uFrame.skySun.xyz));
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
	// (the steps' offset carried by the clouds, not fixed to the screen: each pixel had its own and the clouds drifted
	// under them - their edges sampled differently every frame, they shook as they moved: "not moving smoothly, the
	// shake". Now the offset belongs to the point of the cloud's base the ray enters at, wind and all - a fine smooth
	// noise there, its cells about two pixels at that distance - and a cloud takes its own along as it goes. jitter < 0:
	// this; else the offset given)
	if (jitter < 0.0)
	{
		vec2 cxz = eye.xz + dir.xz * t0 + uSky.wind.xy;
		float cell = max(1.0, t0 * 0.0025);
		vec2 g = cxz / cell, gi = floor(g), gf = g - gi;
		gf = gf * gf * (3.0 - 2.0 * gf);
		float a = Noise01(gi), b = Noise01(gi + vec2(1.0, 0.0)), c0 = Noise01(gi + vec2(0.0, 1.0)), d0 = Noise01(gi + vec2(1.0, 1.0));
		jitter = mix(mix(a, b, gf.x), mix(c0, d0, gf.x), gf.y);
	}
	vec3 sunDir = normalize(uFrame.skySun.xyz);
	// (the sun's illuminance on the clouds, through the air - not what a game's weather leaves of the scene's light on
	// the ground under them: common.glsl's cloudLight; `ground` is that, for what the ground sends back up)
	vec3 sun = uFrame.sunDirection.w > 0.0 ? uFrame.cloudLight.rgb : vec3(0.0);
	vec3 ground = uFrame.sunDirection.w > 0.0 ? uFrame.sunRadiance.rgb : vec3(0.0);
	// (lit from where that light comes from: the moon's at night, not the sun's under the horizon - lit from below,
	// every cloud was a dark underside with a bright rim; the sky's own light, below, still follows the sun)
	vec3 litDir = normalize(uFrame.sunDirection.xyz);
	float c = dot(dir, litDir);
	// (bright toward the sun - a silver lining - and a little back toward it)
	float phase = mix(HenyeyGreenstein(c, 0.8), HenyeyGreenstein(c, -0.3), 0.25);
	vec3 skyTop = SkyRadiance(vec3(0.0, 1.0, 0.0)), skySide = SkyRadiance(normalize(vec3(-sunDir.x, 0.1, -sunDir.z)));     // (the sky above, and its horizon away from the sun)
	// A deck of overcast shuts the clear sky out: inside it and under it the light all round is the deck's own - the
	// daylight that fell on its top (the sun's and the sky's), a sixth of it let down through, scattered until it comes
	// from everywhere and is white. (With the clear sky's light all round instead, a day of rain had a pale blue sky,
	// and everything under it the blue of that sky's light.)
	float deck = uFrame.cloudLight.w;
	vec3 daylight = DeckLight(skyTop);
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
			lp += litDir * step;
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
		vec3 ambient = mix(mix(skySide, skyTop, clamp(h, 0.0, 1.0)) * (0.5 + 0.5 * h), daylight, deck);
		// (the light the lit ground sends back up, onto the cloud's underside: a quarter of what falls on it)
		vec3 bounce = ground * (0.25 * max(litDir.y, 0.0) / PI) * (1.0 - clamp(h, 0.0, 1.0));
		vec3 S = (sun * sunLight * mix(1.0, powder, 0.5) + ambient + bounce) * sigma;
		float stepT = exp(-sigma * dt);
		L += T * (S - S * stepT) / max(sigma, 1e-7);      // (the step's light, integrated through it)
		weightedT += t * T * (1.0 - stepT); weight += T * (1.0 - stepT);
		T *= stepT;
	}
	// Further off, more of the air in front: toward the sky's own colour
	float fade = exp(-t0 / 30000.0);
	// (under a deck that air is lit by the deck: its colour, not the clear sky's)
	L = L * fade + mix(SkyRadiance(dir), daylight, deck) * (1.0 - T) * (1.0 - fade);
	at = weight > 1e-4 ? weightedT / weight : t0 + (t1 - t0) * 0.3;
	return true;
}

void main()
{
	vec3 eye = uFrame.cameraPosition.xyz;
#ifdef DOME
	// The sky-view table's directions, clouded: what the surfaces mirror and the sky's light is summed from
	vec3 dir = SkyViewDirection(vec2(vUV.x, 1.0 - vUV.y), normalize(uFrame.skySun.xyz));
	vec3 L; float T, at;
	March(eye, dir, 16, 0.5, L, T, at);
	outColor = vec4(SkyRadiance(dir) * T + L, 1.0);
#else
	outColor = vec4(0.0, 0.0, 0.0, 1.0);
	vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
	// (a point far along the ray - depth 1e-4 is ~500 m out in this reversed depth - not 0.5, ~10 cm from the eye: that point
	// less the eye lost most of its precision to the eye's distance from the origin, and every sub-millimetre move of a car's
	// camera turned each cloud ray a fraction of a pixel another way - the clouds shook in a race, still in the menus and
	// in pause, where the camera is still. The blue sky's smooth gradient hid the same error)
	vec4 far = uSky.inverseViewProjection * vec4(ndc, 1e-4, 1.0);
	vec3 dir = normalize(far.xyz / far.w - eye);
	// (a new offset along the ray each frame - the golden ratio's steps: over frames they fill the gaps evenly)
	// (the same offset every frame - per pixel only: a new one each frame, blended over 8, left the clouds shimmering on a
	// PC that does not hold a steady frame rate - "the sky shakes"; held, a cloud is the same picture frame after frame)
	float jitter = CLOUD_SCREEN_JITTER ? Noise01(gl_FragCoord.xy) : -1.0;
	vec3 L; float T, at;
	if (!March(eye, dir, int(uSky.wind.w), jitter, L, T, at)) return;
	vec4 current = vec4(L, T);
	// Into last frame's: where these clouds were then, through last frame's view
	if (uSky.history.x > 0.5)
	{
		// (where this cloud was last frame: the wind carried it - its shape now at p was at p + the drift since then
		// (history.w seconds). Found where the point itself was, a drifting cloud's history lagged behind it and every
		// frame's new jitter fought it: the clouds' edges boiled - "the sky shakes")
		vec3 drift = vec3(9.0, 0.0, 3.0) * uSky.history.w;
		vec4 clip = uSky.previousViewProjection * vec4(eye + dir * at + drift, 1.0);
		vec2 prev = vec2(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
		if (clip.w > 0.0 && all(greaterThanEqual(prev, vec2(0.0))) && all(lessThanEqual(prev, vec2(1.0))))
		{
			vec4 h = textureLod(sampler2D(uHistory, uLinearClamp), prev, 0.0);
			if (all(lessThan(abs(h), vec4(65000.0)))) current = mix(h, current, uSky.history.y);     // (a bad history texel: this frame's alone)
		}
	}
	outColor = all(lessThan(abs(current), vec4(65000.0))) ? current : vec4(0.0, 0.0, 0.0, 1.0);     // (gone wrong: no cloud - alpha is what shows through)
#endif
}
