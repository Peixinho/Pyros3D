// Pyros3D the renderer - the atmosphere (Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere
// Rendering Technique"): Rayleigh, Mie and ozone over a spherical Earth, in kilometres.

const float kGroundRadius = 6360.0, kTopRadius = 6460.0;
const vec3 kRayleighScattering = vec3(5.802, 13.558, 33.1) * 1e-3;      // per km
const float kRayleighScale = 8.0;
const float kMieScattering = 3.996e-3, kMieAbsorption = 4.4e-3, kMieScale = 1.2, kMieG = 0.8;
const vec3 kOzoneAbsorption = vec3(0.650, 1.881, 0.085) * 1e-3;
const vec3 kGroundAlbedo = vec3(0.3);

struct Medium { vec3 scatteringRayleigh; float scatteringMie; vec3 extinction; };

Medium MediumAt(float altitude)
{
	Medium m;
	float r = exp(-altitude / kRayleighScale), mi = exp(-altitude / kMieScale);
	float ozone = max(0.0, 1.0 - abs(altitude - 25.0) / 15.0);
	m.scatteringRayleigh = kRayleighScattering * r;
	m.scatteringMie = kMieScattering * mi;
	m.extinction = m.scatteringRayleigh + vec3(m.scatteringMie + kMieAbsorption * mi) + kOzoneAbsorption * ozone;
	return m;
}

// Distance along a ray from the centre-relative origin to a sphere of this radius (-1 when missed)
float RaySphere(vec3 o, vec3 d, float radius)
{
	float b = dot(o, d), c = dot(o, o) - radius * radius;
	float h = b * b - c;
	if (h < 0.0) return -1.0;
	h = sqrt(h);
	float t = -b - h;
	if (t < 0.0) t = -b + h;
	return t;
}

float RayleighPhase(float c) { return 3.0 / (16.0 * PI) * (1.0 + c * c); }
float MiePhase(float c)      // Cornette-Shanks
{
	float g2 = kMieG * kMieG;
	return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + c * c)) / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * kMieG * c, 1.5));
}

// The transmittance table: u the cosine of the sun's zenith, v the altitude (both 0..1, sqrt-spaced)
vec2 TransmittanceUV(float altitude, float cosZenith)
{
	return vec2(cosZenith * 0.5 + 0.5, sqrt(clamp(altitude / (kTopRadius - kGroundRadius), 0.0, 1.0)));
}
void TransmittanceParams(vec2 uv, out float altitude, out float cosZenith)
{
	cosZenith = uv.x * 2.0 - 1.0;
	altitude = uv.y * uv.y * (kTopRadius - kGroundRadius);
}

// The sky-view table: u the azimuth from the sun's, v the elevation - spent near the horizon
// (a skybox's - uFrame.ambient.w: not symmetric about the sun - the whole turn, by the absolute azimuth)
vec2 SkyViewUV(vec3 dir, vec3 sunDir)
{
	float elevation = asin(clamp(dir.y, -1.0, 1.0));
	float v = 0.5 + 0.5 * sign(elevation) * sqrt(abs(elevation) / (PI * 0.5));
	if (uFrame.ambient.w > 0.5) return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5, v);
	vec2 dh = normalize(vec2(dir.x, dir.z) + 1e-6), sh = normalize(vec2(sunDir.x, sunDir.z) + 1e-6);
	float azimuth = acos(clamp(dot(dh, sh), -1.0, 1.0));        // (0..pi: the sky is mirror-symmetric about the sun)
	return vec2(azimuth / PI, v);
}
vec3 SkyViewDirection(vec2 uv, vec3 sunDir)
{
	float s = uv.y * 2.0 - 1.0;
	float elevation = sign(s) * s * s * PI * 0.5;
	if (uFrame.ambient.w > 0.5) { float az = (uv.x - 0.5) * 2.0 * PI; return vec3(cos(az) * cos(elevation), sin(elevation), sin(az) * cos(elevation)); }
	float azimuth = uv.x * PI;
	vec2 sh = normalize(vec2(sunDir.x, sunDir.z) + 1e-6);
	vec2 h = vec2(sh.x * cos(azimuth) - sh.y * sin(azimuth), sh.x * sin(azimuth) + sh.y * cos(azimuth));
	return vec3(h.x * cos(elevation), sin(elevation), h.y * cos(elevation));
}

// Aerial perspective: what of a surface's light reaches the eye through the air between (transmittance), and what
// the air adds on the way (in-scattering). The optical depth of the exponential layers is exact for a straight
// path; the light added is the sky's own radiance in that direction (the sky-view table, the whole path's
// in-scattering) times the share of that path's scattering this stretch holds - so the haze is always the sky's
// colour, blue at noon, gold at dusk.
//   from, to: altitudes (km); dist: km; density: 1 the real atmosphere's
vec3 OpticalDepth(float from, float to, float dist, float density)
{
	// (the integral of exp(-h/H) along a straight path from one altitude to the other)
	float dh = to - from;
	float r = abs(dh) > 1e-4 ? kRayleighScale * (exp(-from / kRayleighScale) - exp(-to / kRayleighScale)) / dh : exp(-from / kRayleighScale);
	float m = abs(dh) > 1e-4 ? kMieScale * (exp(-from / kMieScale) - exp(-to / kMieScale)) / dh : exp(-from / kMieScale);
	return (kRayleighScattering * r + vec3((kMieScattering + kMieAbsorption) * m)) * dist * density;
}
vec3 AerialPerspective(vec3 color, vec3 eye, float eyeAltitude, vec3 p, vec3 skyRadiance, float density)
{
	if (density <= 0.0) return color;
	vec3 d = p - eye;
	float dist = length(d) * 0.001;
	if (dist < 1e-4) return color;
	vec3 dir = d / (dist * 1000.0);
	float to = max(eyeAltitude + d.y * 0.001, 0.0);
	vec3 T = exp(-OpticalDepth(eyeAltitude, to, dist, density));
	// (the whole path the sky-view table saw that way: to the top of the air - near the horizon, all of it)
	vec3 Tall = exp(-OpticalDepth(eyeAltitude, eyeAltitude + 100.0, 100.0 / max(dir.y, 0.02), density));
	vec3 share = clamp((1.0 - T) / max(1.0 - Tall, vec3(1e-3)), 0.0, 1.0);
	return color * T + skyRadiance * share;
}
