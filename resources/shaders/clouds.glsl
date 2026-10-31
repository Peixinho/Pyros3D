// Pyros3D the renderer - the clouds' shared part: their parameters (the sky's per-frame block), the tiling noise, and
// their density anywhere in the slab - the same for the picture's clouds, the sky-view table's, and their shadows.
layout(set = 1, binding = 1) uniform texture3D uNoise;
layout(set = 1, binding = 2) uniform sampler uNoiseSampler;     // (repeat: the noise tiles)
layout(set = 1, binding = 5, std140) uniform SkyData
{
	mat4 inverseViewProjection;
	vec4 clouds;             // x: cover (0 none), y: extinction per metre at full density, z: the slab's bottom, w: its top (metres)
	vec4 wind;               // xy: the noise's drift (metres), z: the detail's own drift, w: steps along a ray
	mat4 previousViewProjection;
	vec4 history;            // x: 1 where last frame's clouds can be used, y: how much of this frame's goes in, z: the frame (its jitter)
	vec4 cloudGrid;          // the clouds' shadow map: xy its corner (world x, z), z: its size (metres), w: how dark their shadow is
} uSky;

const float kBaseTile = 7000.0;         // (metres the base noise spans before it repeats)

float Remap(float v, float a, float b, float c, float d) { return c + (v - a) / max(b - a, 1e-4) * (d - c); }

// The cloud's density at a point, h its height in the slab (0 bottom, 1 top); the detail only where there is cloud
float Density(vec3 p, float h, bool detail)
{
	vec3 q = (p + vec3(uSky.wind.x, 0.0, uSky.wind.y)) / kBaseTile;
	vec4 n = textureLod(sampler3D(uNoise, uNoiseSampler), q, 0.0);
	float base = n.r;     // (even: a cover of c is about c of the slab's middle)
	// (flat bottoms, rounded tops: thin near the slab's floor, gone toward its ceiling)
	float shape = clamp(h / 0.08, 0.0, 1.0) * clamp((1.0 - h) / 0.45, 0.0, 1.0);
	float d = clamp(Remap(base * shape, 1.0 - uSky.clouds.x, 1.0, 0.0, 1.0), 0.0, 1.0);
	if (detail && d > 0.0)
	{
		vec4 m = textureLod(sampler3D(uNoise, uNoiseSampler), q * 9.0 + vec3(uSky.wind.z, 0.0, 0.0), 0.0);
		float fine = m.g * 0.625 + m.b * 0.25 + m.a * 0.125;
		// (wisps under the cloud, billows above)
		float erode = mix(1.0 - fine, fine, clamp(h * 3.0, 0.0, 1.0));
		d = clamp(Remap(d, erode * 0.4, 1.0, 0.0, 1.0), 0.0, 1.0);
	}
	return d;
}
