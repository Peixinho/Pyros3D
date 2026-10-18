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
//! param float uCloudShadow 0.45 0.0 1.0 Cloud shadows
//! param float uSnow 0.0 0.0 1.0 Snow
//! param float uExposure 1.16 0.2 2.0 Exposure
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
	vec2 ndc = vec2(vTexcoord.x * 2.0 - 1.0, 1.0 - vTexcoord.y * 2.0);
	vec3 dir = normalize(uCamFwd.xyz + uCamRight.xyz * (ndc.x * uLens.x * uLens.y) + uCamUp.xyz * (ndc.y * uLens.x));
	vec3 sun = normalize(uSunDir.w > 0.5 ? uSunDir.xyz : uSunTo.xyz);
	float day = smoothstep(-0.10, 0.28, sun.y);
	float low = (1.0 - smoothstep(0.0, 0.42, abs(sun.y))) * smoothstep(-0.18, 0.05, sun.y);
	vec3 hazeColour = mix(horizonColour(sun, uRain), vec3(1.0, 0.62, 0.34) * (0.4 + 0.6 * day + low), (0.30 + 0.5 * low) * pow(max(dot(dir, sun), 0.0), 3.0) * (1.0 - uRain));

	// (with an environment, the air takes the colour of its horizon that way)
	if (uEnv.x > 0.5) hazeColour = envAt(normalize(vec3(dir.x, 0.03, dir.z)));
	if (d >= 1.0 && uEnv.x > 0.5) {
		c = envAt(dir);
	} else if (d >= 1.0) {
		c = skyColour(dir, sun, uRain);
		// clouds: where this ray meets a level 1100 m over the sea
		if (dir.y > 0.012) {
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
	if (uRain > 0.01) {
		// rain: slanted streaks at three depths, falling fast
		float streak = 0.0;
		for (int i = 0; i < 3; i++) {
			float sc = 70.0 + float(i) * 60.0;
			vec2 q = vec2(vTexcoord.x * uLens.y * sc + vTexcoord.y * sc * 0.12, vTexcoord.y * sc * 0.03 - uTime * (2.4 + float(i) * 0.9));
			float col = floor(q.x);
			float y = fract(q.y + hash(vec2(col, float(i))) * 7.0);
			streak += step(0.86, hash(vec2(col, 3.0 + float(i)))) * smoothstep(0.0, 0.05, y) * smoothstep(0.34, 0.05, y) * (1.0 - abs(fract(q.x) - 0.5) * 2.0) * (0.55 - float(i) * 0.14);
		}
		c = mix(c, vec3(0.80, 0.84, 0.88) * (0.25 + 0.75 * day), clamp(streak, 0.0, 1.0) * uRain * 0.55 * (1.0 - uSnow));
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
