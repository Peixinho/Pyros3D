// Pyros3D the renderer - shadows, in one depth atlas (reversed Z, compared in hardware): the sun's four cascades in its
// left half (a quadrant of it each), the point and spot lights' tiles in its right half - a spot one, a point light six,
// one per face of its cube. Rotated Poisson discs of bilinear compares; the sun's blended across cascades.

const int kLocalTiles = 64;
const float kLocalTileTexels = 512.0;

layout(set = 1, binding = 7, std140) uniform ShadowData
{
	mat4 cascades[4];        // world -> each cascade's clip space
	vec4 splits;             // far end of each cascade, as a distance along the view
	vec4 params;             // y: on (1) or off, z: shadow distance, w: normal offset in texels
	vec4 texelWorld;         // each cascade's texel, in world units
	vec4 cameraForward;      // xyz the camera's forward, for the distance along the view
	vec4 atlas;              // xy: one texel of the atlas (1 / its size), zw: a local tile's size in it
	vec4 localRect[kLocalTiles];   // each local tile: xy its corner in the atlas, z: tan of half its field of view
	mat4 local[kLocalTiles];       // world -> each local tile's clip space
	vec4 cloudGrid;          // the clouds' shadow map: xy its corner (world x, z), z: 1 / its size, w: how dark their shadow is (0 none)
	vec4 reflections;        // screen-space reflections: x on (1) or off, y: how far a ray goes (metres), z: steps, w: the frame (its jitter)
	vec4 rain;               // x: the local tile that looks straight down round the eye while it rains (-1 none): what is under cover;
	                         // y: what of the sun an overcast deck lets through (1 none)
} uShadow;
layout(set = 1, binding = 15) uniform texture2D uCloudShadow;

layout(set = 1, binding = 8) uniform texture2D uShadowAtlas;
layout(set = 1, binding = 9) uniform samplerShadow uShadowCompare;

const vec2 kPoisson[16] = vec2[](
	vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725), vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
	vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464), vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
	vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420), vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
	vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590), vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

float InterleavedGradientNoise(vec2 p)
{
	return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// How much of the sun one cascade sees at this point: 1 lit, 0 in shadow
float CascadeShadow(int c, vec3 worldPos, vec3 n, float rotation)
{
	vec3 p = worldPos + n * uShadow.texelWorld[c] * uShadow.params.w;
	vec4 clip = uShadow.cascades[c] * vec4(p, 1.0);
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);           // (a texture's first row at the top)
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || ndc.z < 0.0 || ndc.z > 1.0) return 1.0;
	const vec2 size = vec2(0.25, 0.5);                               // (a cascade's quadrant of the atlas's left half)
	vec2 quadrant = vec2(float(c & 1), float(c >> 1)) * size;
	vec2 texel = uShadow.atlas.xy;
	vec2 kernel = 1.5 * texel;                                       // (the disc's radius in the atlas)
	vec2 lo = quadrant + kernel + texel, hi = quadrant + size - kernel - texel;
	float s = sin(rotation), co = cos(rotation);
	mat2 rot = mat2(co, s, -s, co);
	float lit = 0.0;
	for (int i = 0; i < 16; i++)
	{
		vec2 at = clamp(quadrant + uv * size + (rot * kPoisson[i]) * kernel, lo, hi);
		lit += textureLod(sampler2DShadow(uShadowAtlas, uShadowCompare), vec3(at, ndc.z), 0.0);     // (level 0: the atlas has one, and WGSL wants an explicit level off uniform flow)
	}
	return lit / 16.0;
}

// The same, one hardware-filtered tap and no blend between cascades: for light that comes through a leaf, soft
// already - what a crown's every leaf asks for, at a sixteenth of the cost
float SunShadowCheap(vec3 worldPos, vec3 n)
{
	if (uShadow.params.y < 0.5) return 1.0;
	float depth = dot(worldPos - uFrame.cameraPosition.xyz, uShadow.cameraForward.xyz);
	if (depth >= uShadow.params.z) return 1.0;
	int c = 0;
	while (c < 3 && depth > uShadow.splits[c]) c++;
	vec3 p = worldPos + n * uShadow.texelWorld[c] * uShadow.params.w;
	vec4 clip = uShadow.cascades[c] * vec4(p, 1.0);
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || ndc.z < 0.0 || ndc.z > 1.0) return 1.0;
	const vec2 size = vec2(0.25, 0.5);
	vec2 quadrant = vec2(float(c & 1), float(c >> 1)) * size;
	vec2 at = clamp(quadrant + uv * size, quadrant + uShadow.atlas.xy, quadrant + size - uShadow.atlas.xy);
	return textureLod(sampler2DShadow(uShadowAtlas, uShadowCompare), vec3(at, ndc.z), 0.0);
}

int SunCascade(vec3 worldPos)
{
	float depth = dot(worldPos - uFrame.cameraPosition.xyz, uShadow.cameraForward.xyz);
	int c = 0;
	while (c < 3 && depth > uShadow.splits[c]) c++;
	return depth >= uShadow.params.z ? 4 : c;
}

float SunShadow(vec3 worldPos, vec3 n, vec2 fragCoord)
{
	if (uShadow.params.y < 0.5) return 1.0;
	float depth = dot(worldPos - uFrame.cameraPosition.xyz, uShadow.cameraForward.xyz);
	if (depth >= uShadow.params.z) return 1.0;
	int c = 0;
	while (c < 3 && depth > uShadow.splits[c]) c++;
	float rotation = InterleavedGradientNoise(fragCoord) * 6.2831853;
	float lit = CascadeShadow(c, worldPos, n, rotation);
	// (the last tenth of a cascade blends into the next one; the last cascade fades out at the shadow distance)
	float start = c == 0 ? 0.0 : uShadow.splits[c - 1];
	float t = (depth - start) / max(uShadow.splits[c] - start, 1e-3);
	if (t > 0.9)
	{
		float next = c < 3 ? CascadeShadow(c + 1, worldPos, n, rotation) : 1.0;
		lit = mix(lit, next, (t - 0.9) / 0.1);
	}
	return lit;
}

// How much of a point or spot light reaches this point: 1 lit, 0 in shadow. `tile` its first tile (a point light's six
// follow it: +x, -x, +y, -y, +z, -z), `ng` the surface's geometric normal
float LocalShadow(int tile, bool point, vec3 worldPos, vec3 ng, vec3 lightPos)
{
	vec3 d = worldPos - lightPos;
	if (point)
	{
		vec3 a = abs(d);
		tile += a.x >= a.y && a.x >= a.z ? (d.x > 0.0 ? 0 : 1) : a.y >= a.z ? (d.y > 0.0 ? 2 : 3) : (d.z > 0.0 ? 4 : 5);
	}
	vec4 rect = uShadow.localRect[tile];
	// (pushed off the surface along its normal, toward the light, by about a texel at this distance)
	float texelWorld = 2.0 * length(d) * rect.z / kLocalTileTexels;
	vec3 offsetN = dot(ng, d) > 0.0 ? -ng : ng;
	vec4 clip = uShadow.local[tile] * vec4(worldPos + offsetN * texelWorld * 1.5, 1.0);
	if (clip.w <= 0.0) return 1.0;
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || ndc.z < 0.0 || ndc.z > 1.0) return 1.0;
	vec2 size = uShadow.atlas.zw, texel = uShadow.atlas.xy;
	vec2 kernel = 1.5 * texel;
	vec2 lo = rect.xy + kernel + texel, hi = rect.xy + size - kernel - texel;
	float rotation = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;
	float s = sin(rotation), co = cos(rotation);
	mat2 rot = mat2(co, s, -s, co);
	float lit = 0.0;
	for (int i = 0; i < 16; i++)
	{
		vec2 at = clamp(rect.xy + uv * size + (rot * kPoisson[i]) * kernel, lo, hi);
		lit += textureLod(sampler2DShadow(uShadowAtlas, uShadowCompare), vec3(at, ndc.z), 0.0);
	}
	return lit / 16.0;
}

// Out in the rain (1) or under cover (0): what is above this point, as the rain's tile saw it from straight above - a roof,
// a tree's crown, a bridge. Nothing to tell (no tile, outside it): out in it.
float RainExposure(vec3 worldPos)
{
	int tile = int(uShadow.rain.x);
	if (tile < 0) return 1.0;
	vec4 clip = uShadow.local[tile] * vec4(worldPos + vec3(0.0, 0.25, 0.0), 1.0);     // (lifted: a surface is not its own roof)
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || ndc.z < 0.0 || ndc.z > 1.0) return 1.0;
	vec4 rect = uShadow.localRect[tile];
	vec2 size = uShadow.atlas.zw, texel = uShadow.atlas.xy;
	vec2 at = clamp(rect.xy + uv * size, rect.xy + texel, rect.xy + size - texel);
	float open = 0.0;
	for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++)
		open += textureLod(sampler2DShadow(uShadowAtlas, uShadowCompare), vec3(at + vec2(x, y) * texel * 2.0, ndc.z), 0.0);
	return open / 9.0;
}
