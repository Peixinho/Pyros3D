//============================================================================
// Name        : SkyEffect.cpp
// Author      : Duarte Peixinho
// Description : The source of the built-in post effect "Sky" (PostEffectChain):
//               a dome by the sun's height with clouds, sunsets and stars;
//               height haze; cloud shadows; rain and snow in the air; a film
//               grade. Written as an asset effect (CustomEffect header), so its
//               parameters are the editor's sliders like any other's.
//============================================================================

namespace p3d {

	const char* SkyEffectSource()
	{
		// (two literals: a compiler may cap one at 16 KB)
		return
R"PYROSSKY(//! effect Sky
//! input LastRTT
//! input Depth
//! input Environment
//! input Extra
//! input Shelter
//! param float uNear 0.2 Camera near
//! param float uFar 4000.0 Camera far
//! param vec4 uCamPos 0,50,0,0 Camera position
//! param vec4 uCamFwd 0,0,-1,0 Camera forward
//! param vec4 uCamRight 1,0,0,0 Camera right
//! param vec4 uCamUp 0,1,0,0 Camera up
//! param vec4 uLens 0.52,1.7778,0,0 tan(half fov), aspect
//! param vec4 uSunDir 0.55,0.58,0.42,0 To the sun (w 1: a script says; else the scene's sun light is followed)
//! param vec4 uSunTo 0.55,0.58,0.42,1 The scene's sun (filled by the engine)
//! publish uSnow 14 0
//! publish uRain 14 1
//! publish uSunTo 15
//! param float uWaterWaves 1.0 0.1 2.5 Sea state: wave strength
//! param float uWaterSwell 1.0 0.1 3.0 Sea state: swell height
//! param float uWaterFoam 1.0 0.2 3.0 Sea state: foam reach from the shore
//! param float uWaterMirror 1.0 0.05 1.8 Sea state: reflections
//! publish uWaterWaves 13 0
//! publish uWaterSwell 13 1
//! publish uWaterFoam 13 2
//! publish uWaterMirror 13 3
//! param float uTime 0.0 Seconds
//! param float uHaze 0.0012 0.0 0.01 Haze per metre at sea level
//! param float uHazeFall 0.012 0.0 0.1 How fast the haze thins with height
//! param float uHazeMax 0.92 0.0 1.0 Most haze
//! param float uSea 0.0 -500.0 2000.0 Haze floor (sea level)
//! publish uSea 12 0
//! param float uCloudCover 0.52 0.0 1.0 Cloud cover
//! param float uRain 0.0 0.0 1.0 Rain
//! param vec4 uShelter 0,0,1,0 Rain shelter: the grid's corner x, z, its cell, its side (filled by weather.shelter; side 0: none)
//! param vec4 uRainWind 0.16,0.07,0,0 Rain: how far the wind leans it (x, z per metre fallen)
//! param float uCloudShadow 0.45 0.0 1.0 Cloud shadows
//! param float uCloudVolume 0.0 0.0 1.0 Volumetric clouds (1: marched through a slab of 3D noise, lit by the sun through themselves; 0: the flat layer)
//! param float uCloudPass -1.0 Volumetric clouds: 1 in the half-resolution pass that marches them (set by the chain)
//! param float uCloudSteps 32.0 8.0 96.0 Volumetric clouds: steps along a ray
//! param float uSnow 0.0 0.0 1.0 Snow
//! param float uExposure 1.16 0.2 2.0 Exposure
//! param float uSkyBright 1.0 0.3 3.0 Sky brightness (the dome by day; the land stays as it is)
//! param float uSkyPale 0.0 0.0 1.0 Sky paleness (the dome by day toward the horizon's white)
//! param float uSaturation 0.70 0.0 1.5 Saturation
//! param float uContrast 0.98 0.5 1.5 Contrast
//! param vec4 uEnv 0,0,0,0 Environment: loaded, turn (set by the engine)
//! param float uEnvIntensity 1.0 0.0 8.0 Environment brightness

// The sky, the air and the grade, in one pass over the frame. Built in (post effect "Sky");
// the sun is the scene's - the directional light marked as one, its hour and all - unless a
// script names another (uSunDir with w = 1). The weather is published to every shader, for
// materials that answer it: snow and rain (uGlobals[14].x, .y), the way to the sun (uGlobals[15])
// and the sea's state - waves, swell, foam, reflections - (uGlobals[13], 1 = as made) - and the sea level (uGlobals[12].x), where a
// natural ground lays its beach.
//
// Where nothing was drawn there is sky: a dome that pales to the horizon, a sun with
// its glow, and clouds - a layer of drifting noise a kilometre up, lit from the sun's
// side, thinning to haze at the horizon. Where something was drawn there is air
// between it and the eye: thickest at the sea and thinning with height, the colour of
// the horizon, warmer toward the sun. Then the picture is graded as film is: a
// shoulder for the highlights, a little contrast, a little less colour.

// (exact on every graphics card: a hash of whole numbers. The usual fract(sin(x) * 43758.5) and fract(x * 123.34)
// are only as good as the card's arithmetic on big numbers, and gave blocks and stray rectangles on some)
// The scene's HDR environment (EnvironmentMap), when it has one: the panorama in the direction
// d. RGBA32F, which not every card filters, so the four texels are blended here.
vec3 envAt(vec3 d) {
	float cy = cos(uEnv.y), sy = sin(uEnv.y);
	vec3 i = vec3(d.x * cy - d.z * sy, d.y, d.x * sy + d.z * cy);     // back into the image's own frame
	ivec2 size = textureSize(uTex2, 0);
	vec2 uv = vec2(0.5 + atan(i.x, -i.z) * 0.15915494, acos(clamp(i.y, -1.0, 1.0)) * 0.31830989);
	vec2 p = uv * vec2(size) - 0.5;
	ivec2 a = ivec2(floor(p)); vec2 f = p - vec2(a);
	int x0 = (a.x % size.x + size.x) % size.x, x1 = (x0 + 1) % size.x;
	int y0 = clamp(a.y, 0, size.y - 1), y1 = clamp(a.y + 1, 0, size.y - 1);
	vec3 t = mix(texelFetch(uTex2, ivec2(x0, y0), 0).rgb, texelFetch(uTex2, ivec2(x1, y0), 0).rgb, f.x);
	vec3 b = mix(texelFetch(uTex2, ivec2(x0, y1), 0).rgb, texelFetch(uTex2, ivec2(x1, y1), 0).rgb, f.x);
	return mix(t, b, f.y) * uEnvIntensity;
}

float p3d_ih(ivec3 q) { uint n = uint(q.x) * 1597334673u ^ uint(q.y) * 3812015801u ^ uint(q.z) * 2798796415u; n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16; return float(n >> 8) * (1.0 / 16777216.0); }
float hash(vec2 p) { return p3d_ih(ivec3(ivec2(floor(p * 16.0 + 0.5)), 7)); }
float vnoise(vec2 p) {
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}
float fbm(vec2 p) {
	float a = 0.5, s = 0.0;
	for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(17.1, 9.3); a *= 0.5; }
	return s;
}

// Volumetric clouds: a slab of air 1200-2800 m over the sea holding 3D noise - big shapes,
// eaten at their edges by finer noise, rounded at the bottom and wisped at the top - marched
// along the view ray. At each step the light that reaches it is what a few steps toward the sun
// let through (Beer's law, with the "powder" darkening of a cloud's edge seen against the
// light), scattered toward the eye forward more than back; under that the sky's light from
// above. Exact integer hashing, as the rest of this sky.
float vnoise3(vec3 p) {
	vec3 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	ivec3 q = ivec3(i);
	float a = p3d_ih(q), b = p3d_ih(q + ivec3(1, 0, 0)), c = p3d_ih(q + ivec3(0, 1, 0)), d = p3d_ih(q + ivec3(1, 1, 0));
	float e = p3d_ih(q + ivec3(0, 0, 1)), g = p3d_ih(q + ivec3(1, 0, 1)), h = p3d_ih(q + ivec3(0, 1, 1)), k = p3d_ih(q + ivec3(1, 1, 1));
	return mix(mix(mix(a, b, f.x), mix(c, d, f.x), f.y), mix(mix(e, g, f.x), mix(h, k, f.x), f.y), f.z);
}
const float CLOUD_BOTTOM = 1200.0, CLOUD_TOP = 2800.0;
float cloudDensity(vec3 wp, bool fine) {
	float h = (wp.y - uSea - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
	if (h < 0.0 || h > 1.0) return 0.0;
	vec3 wind = vec3(uTime * 6.0, 0.0, uTime * 2.5);
	vec3 p = (wp + wind) * 0.00038;
	// (the light's own march asks only for the big shape: a third of the cost, and the edges it misses do not shade much)
	float shape = fine ? vnoise3(p) * 0.6 + vnoise3(p * 2.1 + 5.3) * 0.3 + vnoise3(p * 4.3 + 1.7) * 0.1
	                   : vnoise3(p) * 0.6 + vnoise3(p * 2.1 + 5.3) * 0.3 + 0.05;
	float cc = mix(uCloudCover, 0.98, uRain);
	float cover = clamp((shape - (1.0 - cc)) / max(cc, 0.05), 0.0, 1.0);
	// (round underneath, thinning to wisps at the top)
	cover *= smoothstep(0.0, 0.12, h) * (1.0 - smoothstep(0.55, 1.0, h));
	if (fine && cover > 0.0) {
		float detail = vnoise3((wp + wind * 1.6) * 0.0031) * 0.65 + vnoise3((wp + wind * 1.6) * 0.0072) * 0.35;
		cover = clamp(cover - detail * 0.32 * (1.0 - cover), 0.0, 1.0);
	}
	return cover * 0.012;
}
float hg(float c, float g) { return (1.0 - g * g) / (12.5663706 * pow(max(1.0 + g * g - 2.0 * g * c, 1e-4), 1.5)); }
// over `background` (the sky behind): returns the colour, and how much of the background shows
vec4 marchClouds(vec3 dir, vec3 sun, float day, float low, vec3 hazeColour) {
	if (dir.y < 0.01) return vec4(0.0, 0.0, 0.0, 1.0);
	float b = CLOUD_BOTTOM + uSea - uCamPos.y, t = CLOUD_TOP + uSea - uCamPos.y;
	float t0 = max(b / dir.y, 0.0), t1 = t / dir.y;
	if (t1 <= 0.0) return vec4(0.0, 0.0, 0.0, 1.0);
	t1 = min(t1, t0 + 24000.0);
	int steps = int(clamp(uCloudSteps, 8.0, 96.0));
	float dt = (t1 - t0) / float(steps);
	float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))) + uTime * 0.618);
	float cosT = dot(dir, sun);
	float phase = mix(hg(cosT, 0.65), hg(cosT, -0.2), 0.3);
	vec3 sunLight = mix(vec3(1.0, 0.62, 0.36), vec3(1.0, 0.97, 0.92), day) * (0.15 + 0.85 * day) * 14.0;
	vec3 ambient = mix(vec3(0.34, 0.42, 0.56), vec3(0.62, 0.70, 0.84), day) * (0.08 + 0.92 * day) * 0.9;
	float T = 1.0; vec3 lit = vec3(0.0); float tFirst = -1.0;
	// (through empty air two steps at a time, on the big shape alone; inside a cloud, one, in detail)
	float tt = t0 + jitter * dt;
	for (int i = 0; i < 96; i++) {
		if (tt >= t1 || T < 0.02) break;
		vec3 wp = uCamPos.xyz + dir * tt;
		if (cloudDensity(wp, false) <= 0.0) { tt += dt * 2.0; continue; }
		float sigma = cloudDensity(wp, true);
		tt += dt;
		if (sigma <= 0.0) continue;
		if (tFirst < 0.0) tFirst = tt - dt;
		// the light toward the sun, through the cloud itself: four steps, longer as they go
		float od = 0.0, ls = 60.0;
		vec3 lp = wp;
		for (int j = 0; j < 4; j++) { lp += sun * ls; od += cloudDensity(lp, false) * ls; ls *= 2.0; }
		float sunT = exp(-od) * (1.0 - exp(-od * 2.0) * 0.5 + 0.5 * exp(-od * 2.0));
		float powder = 1.0 - exp(-sigma * dt * 2.0);
		vec3 S = (sunLight * sunT * phase * mix(1.0, powder * 2.0, 0.35) + ambient) * sigma;
		float stepT = exp(-sigma * dt);
		lit += T * S * (1.0 - stepT) / max(sigma, 1e-6);
		T *= stepT;
	}
	// a low sun lights their undersides gold and rose; far off they go into the haze
	lit = mix(lit, lit * vec3(1.0, 0.72, 0.55), low * 0.6);
	lit = mix(lit, vec3(0.36, 0.38, 0.41) * (0.25 + 0.75 * day) * (1.0 - T), uRain * 0.8);
	if (tFirst > 0.0) lit = mix(lit, hazeColour * (1.0 - T), 1.0 - exp(-tFirst * 0.00006));
	return vec4(lit, T);
}

// The sky by where the sun is: blue at noon, gold and rose as it comes down, dark after.
// `rain` 0..1 greys it all.
vec3 horizonColour(vec3 sun, float rain) {
	float day = smoothstep(-0.10, 0.28, sun.y);
	float low = 1.0 - smoothstep(0.0, 0.42, abs(sun.y));
	vec3 c = mix(vec3(0.05, 0.07, 0.12), vec3(0.80, 0.85, 0.88), day);
	c = mix(c, vec3(1.0, 0.62, 0.34), low * 0.75 * smoothstep(-0.18, 0.05, sun.y));
	return mix(c, vec3(0.50, 0.53, 0.56) * (0.25 + 0.75 * day), rain);
}
vec3 skyColour(vec3 dir, vec3 sun, float rain) {
	float up = max(dir.y, 0.0);
	float day = smoothstep(-0.10, 0.28, sun.y);
	float low = (1.0 - smoothstep(0.0, 0.42, abs(sun.y))) * smoothstep(-0.18, 0.05, sun.y);
	vec3 zenith = mix(vec3(0.010, 0.016, 0.040), vec3(0.30, 0.44, 0.66), day);
	zenith = mix(zenith, vec3(0.20, 0.26, 0.48), low * 0.6);
	vec3 horizon = horizonColour(sun, 0.0);
	float s = max(dot(dir, sun), 0.0);
	// (the glow of a low sun spreads along the horizon, not up the dome)
	horizon = mix(horizon, vec3(1.0, 0.50, 0.20), low * pow(s, 3.0) * 0.8);
	// (a dome made brighter, or paler, by day: a real sky through a day's air is a pale bright blue)
	zenith = mix(zenith, horizon, uSkyPale * day) * mix(1.0, uSkyBright, day);
	horizon *= mix(1.0, uSkyBright, day * 0.5);
	vec3 c = mix(horizon, zenith, pow(up, mix(0.52, 0.36, low)));
	c += vec3(1.0, 0.86, 0.62) * (0.10 * pow(s, 6.0) + 0.30 * pow(s, 64.0)) * (0.3 + 0.7 * day + low);
	c += mix(vec3(1.0, 0.55, 0.25), vec3(1.0, 0.96, 0.88), day) * smoothstep(0.9993, 0.9997, s) * 6.0;
	c = mix(c, horizon * vec3(0.93, 0.95, 0.98), smoothstep(0.0, -0.06, dir.y));
	// stars, when it is dark
	if (day < 0.5 && dir.y > 0.0) {
		vec2 sp = dir.xz / (dir.y + 0.35) * 220.0;
		float st = step(0.9975, hash(floor(sp))) * (1.0 - smoothstep(0.0, 0.5, day * 2.0));
		c += vec3(st) * 0.9;
	}
	return mix(c, vec3(0.42, 0.45, 0.48) * (0.20 + 0.80 * day), rain * 0.92);
}
)PYROSSKY"
R"PYROSSKY(
void main() {
	vec3 c = texture_2D(uTex0, vTexcoord).rgb;
	float d = texture_2D(uTex1, vTexcoord).r;
	// A lens: sharp where it is focused (some twenty metres off), a little soft nearer and
	// further - what is at arm's length and what is at the horizon are not both in focus.
	if (d < 1.0) {
		float zl = (uNear * uFar) / (d * (uNear - uFar) + uFar);
		float coc = clamp(abs(zl - 22.0) / max(zl, 22.0), 0.0, 1.0);
		// (what is far is only a little soft - and from the air, where everything is far, not at all)
		coc = smoothstep(0.35, 1.0, coc) * (zl < 22.0 ? 0.0 : 0.0004 * (1.0 - smoothstep(60.0, 200.0, uCamPos.y - uSea)));
		if (coc > 0.0002) {
			vec3 sum = c; float n = 1.0;
			for (int i = 0; i < 8; i++) {
				float a = float(i) * 2.399963 + 0.7;
				vec2 o = vec2(cos(a), sin(a)) * coc * sqrt((float(i) + 1.0) / 8.0) * vec2(1.0, uLens.y);
				// (only what is itself out of focus bleeds: a sharp tree is not smeared into soft hills)
				float dz = texture_2D(uTex1, vTexcoord + o).r;
				float w = dz < 1.0 ? 1.0 : 0.0;
				sum += texture_2D(uTex0, vTexcoord + o).rgb * w; n += w;
			}
			c = sum / n;
		}
	}
	// (the picture's v counts down from the top on Metal and Vulkan, up from the bottom on GL)
#if defined(METAL) || defined(VULKAN)
	vec2 uvTop = vTexcoord;
#else
	vec2 uvTop = vec2(vTexcoord.x, 1.0 - vTexcoord.y);
#endif
	vec2 ndc = vec2(uvTop.x * 2.0 - 1.0, 1.0 - uvTop.y * 2.0);
	vec3 dir = normalize(uCamFwd.xyz + uCamRight.xyz * (ndc.x * uLens.x * uLens.y) + uCamUp.xyz * (ndc.y * uLens.x));
	vec3 sun = normalize(uSunDir.w > 0.5 ? uSunDir.xyz : uSunTo.xyz);
	float day = smoothstep(-0.10, 0.28, sun.y);
	float low = (1.0 - smoothstep(0.0, 0.42, abs(sun.y))) * smoothstep(-0.18, 0.05, sun.y);
	vec3 hazeColour = mix(horizonColour(sun, uRain), vec3(1.0, 0.62, 0.34) * (0.4 + 0.6 * day + low), (0.30 + 0.5 * low) * pow(max(dot(dir, sun), 0.0), 3.0) * (1.0 - uRain));

	// (with an environment, the air takes the colour of its horizon that way - the sky's broad colour
	// there, not one texel of it: a picture's sun is twenty-five times its sky, and read straight it
	// stood as a column of fire over everything in its direction, whatever the height looked at.
	// Spread over a fan of the horizon, and never brighter than a few times the sky's own.)
	if (uEnv.x > 0.5)
	{
		vec2 h = normalize(vec2(dir.x, dir.z) + vec2(1e-5, 0.0));
		vec3 sum = vec3(0.0);
		for (int i = 0; i < 9; i++)
		{
			float a = (float(i) - 4.0) * 0.11;                     // some twenty-five degrees either side
			vec2 r = vec2(h.x * cos(a) - h.y * sin(a), h.x * sin(a) + h.y * cos(a));
			float up = 0.02 + 0.05 * float(i % 3);
			sum += envAt(normalize(vec3(r.x, up, r.y)));
		}
		hazeColour = sum / 9.0;
		float lum = dot(hazeColour, vec3(0.2126, 0.7152, 0.0722));
		float cap = 3.0 * max(dot(envAt(vec3(0.0, 1.0, 0.0)), vec3(0.2126, 0.7152, 0.0722)), 0.05);
		if (lum > cap) hazeColour *= cap / lum;
	}
	if (uCloudPass > 0.5)
	{
		// the half-resolution pass: only the clouds' light and the sky's share through them, where sky shows
		FragColor = (d >= 1.0) ? marchClouds(dir, sun, day, low, hazeColour) : vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	if (d >= 1.0 && uEnv.x > 0.5) {
		c = envAt(dir);
	} else if (d >= 1.0) {
		c = skyColour(dir, sun, uRain);
		if (uCloudVolume > 0.5)
		{
			// (marched at half resolution by the pass before - its light and how much of the sky
			// shows through - and laid over the sky here; marched here, where nothing did)
			vec4 cl = uCloudPass < -0.5 ? marchClouds(dir, sun, day, low, hazeColour) : texture_2D(uTex3, vTexcoord);
			c = c * cl.a + cl.rgb;
		}
		// clouds: where this ray meets a level 1100 m over the sea
		else if (dir.y > 0.012) {
			float t = (1100.0 + uSea - uCamPos.y) / dir.y;
			vec2 p = (uCamPos.xz + dir.xz * t) * 0.00042 + vec2(uTime * 0.004, uTime * 0.0015);
			float shape = fbm(p) * 0.70 + fbm(p * 3.1 + 11.0) * 0.30;
			float cc = mix(uCloudCover, 0.98, uRain);
			float cover = smoothstep(1.0 - cc - 0.06, 1.0 - cc + 0.22, shape);
			// lit from the sun's side: the same shape, sampled a little toward the sun, is what shades it
			float toward = fbm(p + sun.xz * 0.045) * 0.70 + fbm((p + sun.xz * 0.045) * 3.1 + 11.0) * 0.30;
			float lit = clamp(0.62 + (shape - toward) * 3.2, 0.0, 1.0);
			vec3 cloud = mix(vec3(0.62, 0.68, 0.78), vec3(1.0, 0.99, 0.96), lit) * (0.10 + 0.90 * day);
			// a low sun lights them from underneath, gold then rose
			cloud = mix(cloud, mix(vec3(0.42, 0.30, 0.38), vec3(1.0, 0.62, 0.36), lit), low * 0.85);
			cloud = mix(cloud, vec3(0.36, 0.38, 0.41) * (0.25 + 0.75 * day) * (0.7 + 0.5 * lit), uRain);
			// (a snow sky is not one grey: banks of darker cloud with paler gaps)
			cloud *= 1.0 - 0.30 * uSnow;
			cloud = mix(cloud, cloud * (0.50 + 0.60 * lit), uSnow * 0.8);
			float far = 1.0 - exp(-t * 0.00007);              // toward the horizon they go into the haze
			cloud = mix(cloud, hazeColour, far * 0.9);
			c = mix(c, cloud, cover * 0.94 * (1.0 - smoothstep(0.012, 0.07, 0.07 - dir.y + 0.012) * 0.0));
		}
	} else {
		float z = (uNear * uFar) / (d * (uNear - uFar) + uFar);
		float dist = z / max(dot(dir, normalize(uCamFwd.xyz)), 0.05);
		// the air between: its density falls off with height, so the amount along the ray has a closed form
		float h0 = uCamPos.y - uSea;
		float dy = dir.y * dist;
		float k = uHazeFall;
		float along = abs(dy) > 0.01 ? (exp(-k * h0) - exp(-k * (h0 + dy))) / (k * dy) : exp(-k * h0);
		// the clouds' shadows, drifting over land and sea: what stands between this place and the sun
		if (sun.y > 0.05) {
			vec3 world = uCamPos.xyz + dir * dist;
			float up2 = (1100.0 + uSea - world.y) / sun.y;
			vec2 cp2 = (world.xz + sun.xz * up2) * 0.00042 + vec2(uTime * 0.004, uTime * 0.0015);
			float sh2 = fbm(cp2) * 0.70 + fbm(cp2 * 3.1 + 11.0) * 0.30;
			float cc2 = mix(uCloudCover, 0.98, uRain);
			float cov2 = smoothstep(1.0 - cc2 - 0.06, 1.0 - cc2 + 0.22, sh2);
			c *= 1.0 - cov2 * uCloudShadow * (1.0 - uRain) * smoothstep(0.05, 0.25, sun.y);
		}
		float f = 1.0 - exp(-dist * uHaze * (1.0 + uRain * 7.0) * max(along, 0.0));
		c = mix(c, hazeColour, clamp(f, 0.0, 1.0) * uHazeMax);
	}

	// (winter: what is green goes dull and dark - leaves thin, grass dies back)
	if (uSnow > 0.01 && d < 1.0) {
		float green = smoothstep(0.0, 0.10, c.g - max(c.r, c.b) * 0.92);
		c = mix(c, vec3(dot(c, vec3(0.3, 0.5, 0.2))) * vec3(0.62, 0.64, 0.58), green * uSnow * 0.75);
	}
	if (uRain > 0.01 && uSnow < 0.99) {
		// Rain: drops that are somewhere - each falls through a place in the air round the eye, a
		// streak as long as it falls in the blink a picture is (the wind leaning it), thin as a pixel
		// far off and thicker passing the eye, behind whatever is nearer, and none under a roof (the
		// shelter grid: what is over each metre of ground round the eye). Looking down, they come
		// at you end on - dots.
		float reachTo = d < 1.0 ? ((uNear * uFar) / (d * (uNear - uFar) + uFar)) / max(dot(dir, normalize(uCamFwd.xyz)), 0.05) : 1.0e6;
		vec3 fall = normalize(vec3(uRainWind.x, -1.0, uRainWind.y));
		vec3 fp = fall - dir * dot(fall, dir);
		float flen = length(fp);
		vec3 fn = fp / max(flen, 1.0e-4);
		float pix = 2.0 * uLens.x / float(textureSize(uTex0, 0).y);       // a pixel, as an angle
		float rn = 0.0;
		for (int i = 0; i < 9; i++) {
			float t = 0.9 * pow(1.45, float(i));                 // shells from a metre to twenty-five
			if (t > reachTo) break;
			float s = t * 0.20;
			vec3 q = (uCamPos.xyz + dir * t - fall * (uTime * 9.0)) / s;      // (falling at 9 m/s)
			vec3 cell = floor(q);
			float h1 = hash(cell.xz + cell.y * 17.0 + float(i) * 3.1), h2 = hash(cell.xz * 1.7 + cell.y * 5.0 + 11.0 + float(i)), h3 = hash(cell.zx + cell.y * 29.0 + 23.0);
			if (h3 > 0.25 + 0.55 * uRain) continue;
			vec3 at = cell + 0.2 + 0.6 * vec3(h1, h2, fract(h1 * 7.31 + h2 * 3.7));
			vec3 v = (at - q) * s;                               // metres from this pixel's point to the drop
			float along = dot(v, dir);
			if (abs(along) > s * 0.5) continue;
			vec3 vp = v - dir * along;
			float u = dot(vp, fn);
			float across = length(vp - fn * u);
			float halfLen = (0.13 + 0.09 * h2) * flen;            // half the streak, as it is seen
			float w = max(pix * (t + along) * 0.75, 0.0016);
			float a = (1.0 - smoothstep(w * 0.45, w, across)) * (1.0 - smoothstep(halfLen * 0.55, halfLen + w, abs(u)));
			if (a <= 0.0) continue;
			// under something: none
			if (uShelter.w > 0.5) {
				vec3 wp = uCamPos.xyz + dir * (t + along) + fn * u;
				vec2 g = (wp.xz - uShelter.xy) / uShelter.z;
				if (g.x >= 0.0 && g.y >= 0.0 && g.x < uShelter.w && g.y < uShelter.w) {
					vec4 sh = texture_2D(uTex4, (floor(g) + 0.5) / uShelter.w);
					float sv = floor(sh.r * 255.0 + 0.5) * 256.0 + floor(sh.g * 255.0 + 0.5);
					if (sv > 0.5 && wp.y < -100.0 + sv / 65535.0 * 2100.0) continue;
				}
			}
			rn = max(rn, a * mix(0.45, 0.26, float(i) / 8.0) * (0.6 + 0.4 * h1));
		}
		// (a drop is the sky bent through it: lighter than what is behind, by day; a glint by night)
		vec3 rainCol = mix(horizonColour(sun, 1.0) * 1.35, vec3(0.86, 0.89, 0.93), 0.35) * (0.18 + 0.82 * day);
		c = mix(c, max(c * 1.08, rainCol), clamp(rn, 0.0, 1.0) * clamp(uRain * 1.6, 0.0, 1.0) * (1.0 - uSnow));
	}
	if (uSnow > 0.01) {
		// Snow: flakes that are somewhere - each has a place in the air, falls and drifts there,
		// is big when it passes the eye and a speck far off, and goes behind whatever is nearer.
		float reachTo = d < 1.0 ? ((uNear * uFar) / (d * (uNear - uFar) + uFar)) / max(dot(dir, normalize(uCamFwd.xyz)), 0.05) : 1.0e6;
		float fl = 0.0;
		for (int i = 0; i < 9; i++) {
			float t = 1.3 * pow(1.6, float(i));                 // shells from under a metre to thirty
			if (t > reachTo) break;
			float s = t * 0.21;
			vec3 q = uCamPos.xyz + dir * t;
			q.y += uTime * (1.0 + 0.22 * mod(float(i), 3.0));
			q.xz += vec2(0.42, 0.16) * uTime;
			q /= s;
			vec3 cell = floor(q);
			float h1 = hash(cell.xz + cell.y * 17.0 + float(i) * 3.1), h2 = hash(cell.xz * 1.7 + cell.y * 5.0 + 11.0), h3 = hash(cell.zx + cell.y * 29.0 + 23.0);
			vec3 at = cell + 0.25 + 0.5 * vec3(h1, h2, h3);
			at.x += 0.10 * sin(uTime * 1.3 + h1 * 6.283);
			vec3 v = (at - q) * s;
			float along = dot(v, dir);
			float off = length(v - dir * along) / t;            // how far off this pixel's line of sight, as an angle
			float r = max((0.004 + 0.006 * h2) / t, 0.0010);
			float w = (1.0 - smoothstep(r * 0.45, r, off)) * (1.0 - smoothstep(0.16, 0.30, abs(along) / s)) * step(0.35, h3);
			fl = max(fl, w * mix(0.95, 0.55, float(i) / 8.0));
		}
		c = mix(c, vec3(0.97, 0.98, 1.0) * (0.35 + 0.65 * day), clamp(fl, 0.0, 1.0) * uSnow * 0.92);
	}
	// (the eye opens as the light goes: a low sun's land is gold, not dim)
	c *= uExposure * (d < 1.0 ? 1.0 + 0.75 * low * (1.0 - uRain) : 1.0);
	float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
	c = mix(vec3(l), c, uSaturation * (1.0 + 0.7 * low * (1.0 - uRain)));
	c = (c - 0.42) * uContrast + 0.42;
	// a shoulder: above this the rest of the way to white is taken slowly
	float sh = 0.74;
	vec3 over = max(c - sh, 0.0);
	c = min(c, vec3(sh)) + (1.0 - sh) * (1.0 - exp(-over / (1.0 - sh)));
	FragColor = vec4(max(c, 0.0), 1.0);
}
)PYROSSKY";
	}

}
