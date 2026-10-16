#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#define MAX_LIGHTS 4
#define MAX_BONES 60
#if defined(GLES3)
	precision highp float;
#endif

// Water: a sea, a lake or a river's surface - the editor's Terrain > River > "Add water" writes this
// into a project, and it is the project's to change from then on.
//
// What it draws comes from where it is: waves and swell that move the surface (the vertex stage
// lays the grid round the eye), the shallows, a shore where the water runs up the sand and thins
// to nothing, foam in plates that open as it leaves the beach, a river's current along its bed,
// ice in winter, reflections. It reads two pictures:
//   shore  the water's depth picture (Terrain > River > Bake depth picture): red the depth, as its
//          square root, green "this is a river", blue and alpha the way the river runs;
//   waves  a normal map of small ripples, tiled.
// And the world's weather, published by the Sky post effect: the sea's state (uGlobals[13]),
// snow and rain (uGlobals[14]), the sun (uGlobals[15]). Without a Sky it is a calm summer sea.
//
// The depth picture's layout, as it was baked (its .bake.json) - change these with the bake:
#define WATER_MAP_SIZE 1024.0       // metres the picture covers, centred on the world's origin
#define WATER_DEPTH_RANGE 12.0      // the depth that is white
#define WATER_DEPTH_ABOVE 0.5       // the ground above the water kept in it
// The water's level is the height of the object it is drawn on.
#define WATER_LEVEL vWaterLevel        // (the vertex stage hands it over: the fragment stage is not given the model matrix)
// A sea is drawn round the eye out to the horizon. A lake or a pool is drawn where its plane is, at its
// own size - put the plane over the basin - and is a fraction as rough: define WATER_BOUNDED for that.
// WATER_ROUGH scales the waves (a sea 1.0; a bounded water 0.25 unless said otherwise).
//#define WATER_BOUNDED
#ifndef WATER_ROUGH
#ifdef WATER_BOUNDED
#define WATER_ROUGH 0.25
#else
#define WATER_ROUGH 1.0
#endif
#endif
// WATER_DEEPEN: how much deeper than it is the water reads, for its colour and its foam. A lake a few metres
// deep and never far from a bank is all surf zone to a sea's shading - pale, cracked foam from side to side;
// read deeper, its middle darkens and the foam keeps to the edge (bounded: 3.5).
#ifndef WATER_DEEPEN
#ifdef WATER_BOUNDED
#define WATER_DEEPEN 3.5
#else
#define WATER_DEEPEN 1.0
#endif
#endif


// (Began as a Material Editor node graph; kept by hand since - nothing regenerates it.)
// Contains both a Forward (real per-fragment lighting) and a Deferred
// G-buffer branch - DEFERRED_GBUFFER picks which one compiles, set by
// the project's Renderer setting at Apply time. SKINNING is defined for
// skinned meshes by CustomShaderMaterial's variant cache.

#ifdef VERTEX
attribute_in vec3 aPosition;
attribute_in vec3 aNormal;
attribute_in vec2 aTexcoord;
#ifdef SKINNING
attribute_in vec4 aBonesID;
attribute_in vec4 aBonesWeight;
uniform mat4 uBoneMatrix[MAX_BONES];
#endif
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
uniform vec3 uCameraPosition;
uniform float uTime;
uniform vec4 uGlobals[16];
vec4 p3d_v4(float x) { return vec4(x); }
vec4 p3d_v4(vec2 v) { return vec4(v, 0.0, 0.0); }
vec4 p3d_v4(vec3 v) { return vec4(v, 0.0); }
vec4 p3d_v4(vec4 v) { return v; }
vec4 p3d_nz(vec4 v) { return mix(v, vec4(1e-5), step(abs(v), vec4(1e-5))); }
// (exact on every graphics card: a hash of whole numbers. The usual fract(sin(x) * 43758.5) and fract(x * 123.34)
// are only as good as the card's arithmetic on big numbers, and gave blocks and stray rectangles on some)
float p3d_ih(ivec3 q) { uint n = uint(q.x) * 1597334673u ^ uint(q.y) * 3812015801u ^ uint(q.z) * 2798796415u; n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16; return float(n >> 8) * (1.0 / 16777216.0); }
float p3d_hash(vec3 p) { return p3d_ih(ivec3(floor(p * 4.0 + 0.5))); }
float p3d_noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(p3d_hash(i + vec3(0.0, 0.0, 0.0)), p3d_hash(i + vec3(1.0, 0.0, 0.0)), f.x),
	               mix(p3d_hash(i + vec3(0.0, 1.0, 0.0)), p3d_hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
	           mix(mix(p3d_hash(i + vec3(0.0, 0.0, 1.0)), p3d_hash(i + vec3(1.0, 0.0, 1.0)), f.x),
	               mix(p3d_hash(i + vec3(0.0, 1.0, 1.0)), p3d_hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
}
varying_out vec2 vTexcoord;
varying_out vec3 vNormal;
varying_out vec3 vNormalWorld;
varying_out vec4 vWorldPos;
varying_out float vWaterLevel;
varying_out vec4 vWorldPositionShadow;
uniform sampler2D shore;
float p3d_vh(vec2 p) { return p3d_ih(ivec3(ivec2(floor(p * 16.0 + 0.5)), 7)); }
float p3d_vn(vec2 p) {
	vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(p3d_vh(i), p3d_vh(i + vec2(1.0, 0.0)), f.x), mix(p3d_vh(i + vec2(0.0, 1.0)), p3d_vh(i + vec2(1.0, 1.0)), f.x), f.y);
}
// (the fragment stage's p3d_sea, its first twelve trains: the swell)
float p3d_seaV(vec2 pos, float t) {
	float iter = 0.0, freq = 1.0, tm = 2.0, weight = 1.0, sumV = 0.0, sumW = 0.0;
	for (int i = 0; i < 12; i++) {
		vec2 d = vec2(sin(iter), cos(iter));
		float x = dot(d, pos) * freq + t * tm;
		float wave = exp(sin(x) - 1.0);
		pos += d * (-wave * cos(x)) * weight * 0.30;
		sumV += wave * weight; sumW += weight;
		weight *= 0.85; freq *= 1.18; tm *= 1.07; iter += 1232.399963;
	}
	return sumV / sumW;
}
void main() {
	vWaterLevel = uModelMatrix[3].y;
	vec4 p3d_localPos = vec4(aPosition, 1.0);
	vec3 p3d_localNormal = aNormal;
#ifdef SKINNING
	mat4 p3d_skin = uBoneMatrix[int(aBonesID.x)] * aBonesWeight.x
	              + uBoneMatrix[int(aBonesID.y)] * aBonesWeight.y
	              + uBoneMatrix[int(aBonesID.z)] * aBonesWeight.z
	              + uBoneMatrix[int(aBonesID.w)] * aBonesWeight.w;
	p3d_localPos = p3d_skin * p3d_localPos;
	p3d_localNormal = (p3d_skin * vec4(aNormal, 0.0)).xyz;
#endif
	vec4 p3d_worldPos = uModelMatrix * p3d_localPos;
	// The sea is a sheet of many vertices drawn round the eye - fine underfoot, coarse at the horizon - and
	// its swell really lifts it: crests stand against the land and each other, and the shore rises and falls.
	{
#ifndef WATER_BOUNDED
		vec2 gn = p3d_worldPos.xz / 6960.0;
		p3d_worldPos.xz = floor(uCameraPosition.xz / 4.0) * 4.0 + sign(gn) * pow(abs(gn), vec2(2.6)) * 3600.0;
#endif
		vec2 suv = (p3d_worldPos.xz + vec2(WATER_MAP_SIZE * 0.5)) / WATER_MAP_SIZE;
		float inMap = step(0.0, min(suv.x, suv.y)) * step(max(suv.x, suv.y), 1.0);
		vec4 shv = textureLod(shore, clamp(suv, vec2(0.002), vec2(0.998)), 0.0);
		float dep = max(mix(WATER_DEPTH_RANGE, shv.r * shv.r * (WATER_DEPTH_RANGE + WATER_DEPTH_ABOVE) - WATER_DEPTH_ABOVE, inMap), 0.0);
		float lift = smoothstep(0.4, 4.0, dep) * (1.0 - clamp(shv.g, 0.0, 1.0) * inMap);
		p3d_worldPos.y += (p3d_seaV(p3d_worldPos.xz * 0.11, uTime * 0.6) - 0.42) * 1.5 * (uGlobals[13].y > 0.001 ? uGlobals[13].y : 1.0) * lift * WATER_ROUGH;
		// The wash: at the sand the water's edge rises and falls a hand's height, and not all along at once - so
		// on a sloping beach it runs up in round tongues, here then there, and draws back. Where it meets the
		// sand is where the two surfaces really cross: a curved line, never a cut.
		{
			// (it is the sea's own waves that push it: as a crest of the swell comes in to this stretch of shore
			// the water runs up the sand there, and as the trough follows it draws back)
			float n2 = p3d_vn(p3d_worldPos.xz * 0.22 + vec2(uTime * 0.03, 0.0));
			float swell = p3d_seaV(p3d_worldPos.xz * 0.11, uTime * 0.6);
			float wash = 0.015 + 0.15 * smoothstep(0.28, 0.66, swell) * (0.65 + 0.35 * n2);
			p3d_worldPos.y += wash * (1.0 - smoothstep(0.10, 0.9, dep)) * inMap * (1.0 - clamp(shv.g, 0.0, 1.0));
		}
		// A river's surface moves too: small waves a few centimetres high, carried downstream with the current
		// (two copies half a beat apart, cross-faded, so nothing stretches), dying out at its banks.
		float riv = clamp(shv.g, 0.0, 1.0) * inMap;
		if (riv > 0.02) {
			vec2 fl = shv.ba * 2.0 - 1.0;
			fl = dot(fl, fl) > 0.01 ? normalize(fl) : vec2(0.0);
			float beat = uTime * 0.22, t0 = fract(beat), t1 = fract(beat + 0.5), w1 = abs(1.0 - 2.0 * t0);
			float h0 = p3d_seaV((p3d_worldPos.xz - fl * t0 * 5.0) * 1.3, uTime * 0.6);
			float h1 = p3d_seaV((p3d_worldPos.xz - fl * t1 * 5.0) * 1.3 + 13.0, uTime * 0.6);
			p3d_worldPos.y += (mix(h0, h1, w1) - 0.42) * 0.14 * riv * smoothstep(0.25, 1.0, dep);
		}
	}
	vec3 p3d_worldNormal = normalize((uModelMatrix * vec4(p3d_localNormal, 0.0)).xyz);
	vTexcoord = aTexcoord;
	vNormalWorld = p3d_worldNormal;
	vNormal = (uViewMatrix * vec4(p3d_worldNormal, 0.0)).xyz;
	vWorldPos = p3d_worldPos;
	vWorldPositionShadow = uViewMatrix * p3d_worldPos;
	gl_Position = uProjectionMatrix * uViewMatrix * p3d_worldPos;
}
#endif

#ifdef FRAGMENT
uniform vec4 uAmbientLight;
uniform vec4 uAmbientSky;
uniform vec4 uAmbientEquator;
uniform vec4 uAmbientGround;
uniform vec4 uAmbientParams;
uniform vec4 uAmbientSH[9];
uniform mat4 uViewMatrix;
uniform mat4 uModelMatrix;
uniform vec3 uCameraPosition;
uniform float uOpacity;
uniform sampler2D shore;
uniform sampler2D waves;
#define WAVE_MEAN vec2(-0.0036, -0.0039)
uniform float uTime;
uniform vec4 uGlobals[16];
vec4 p3d_v4(float x) { return vec4(x); }
vec4 p3d_v4(vec2 v) { return vec4(v, 0.0, 0.0); }
vec4 p3d_v4(vec3 v) { return vec4(v, 0.0); }
vec4 p3d_v4(vec4 v) { return v; }
vec4 p3d_nz(vec4 v) { return mix(v, vec4(1e-5), step(abs(v), vec4(1e-5))); }
// (exact on every graphics card: a hash of whole numbers. The usual fract(sin(x) * 43758.5) and fract(x * 123.34)
// are only as good as the card's arithmetic on big numbers, and gave blocks and stray rectangles on some)
float p3d_ih(ivec3 q) { uint n = uint(q.x) * 1597334673u ^ uint(q.y) * 3812015801u ^ uint(q.z) * 2798796415u; n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16; return float(n >> 8) * (1.0 / 16777216.0); }
float p3d_hash(vec3 p) { return p3d_ih(ivec3(floor(p * 4.0 + 0.5))); }
float p3d_noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(p3d_hash(i + vec3(0.0, 0.0, 0.0)), p3d_hash(i + vec3(1.0, 0.0, 0.0)), f.x),
	               mix(p3d_hash(i + vec3(0.0, 1.0, 0.0)), p3d_hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
	           mix(mix(p3d_hash(i + vec3(0.0, 0.0, 1.0)), p3d_hash(i + vec3(1.0, 0.0, 1.0)), f.x),
	               mix(p3d_hash(i + vec3(0.0, 1.0, 1.0)), p3d_hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
}
#if !defined(DEFERRED_GBUFFER) && !defined(SHADOW_DEPTH)
#define P3D_FORWARD_LIT
#endif
#ifdef P3D_FORWARD_LIT
uniform mat4 uLights[MAX_LIGHTS];
uniform int uNumberOfLights;
uniform sampler2DShadow uDirectionalShadowMaps;
uniform mat4 uDirectionalDepthsMVP[4];
uniform vec4 uDirectionalShadowFar[4];
uniform int uNumberOfDirectionalShadows;
uniform samplerCube uPointShadowMaps[4];
uniform mat4 uPointDepthsMVP[8];
uniform int uNumberOfPointShadows;
uniform sampler2DShadow uSpotShadowMaps[4];
uniform mat4 uSpotDepthsMVP[4];
uniform int uNumberOfSpotShadows;
varying_in vec4 vWorldPositionShadow;
#endif
varying_in vec2 vTexcoord;
varying_in vec3 vNormal;
varying_in vec3 vNormalWorld;
varying_in vec4 vWorldPos;
varying_in float vWaterLevel;

#if defined(DEFERRED_GBUFFER)
layout(location = 0) out vec4 FragData_r;
layout(location = 1) out vec4 FragData_g;
layout(location = 2) out vec4 FragData_b;
layout(location = 3) out vec4 FragData_pbr;
#elif defined(SHADOW_DEPTH)
out vec4 FragColor;
#else
out vec4 FragColor;

struct LIGHT { vec4 Color; vec3 Direction; vec3 Position; float Radius; vec2 Cones; float Type; bool HaveShadowMap; float PCFTexelSize; int ShadowMap; };
void buildLightFromMatrix(mat4 Light, inout LIGHT L) {
	L.Color = Light[0];
	L.Position = vec3(Light[1][0], Light[1][1], Light[1][2]);
	L.Direction = vec3(Light[1][3], Light[2][0], Light[2][1]);
	L.Radius = Light[2][2];
	L.Cones = vec2(Light[2][3], Light[3][0]);
	L.Type = Light[3][1];
	L.HaveShadowMap = (Light[3][3] >= 0.0);
	L.PCFTexelSize = Light[3][2];
	L.ShadowMap = int(Light[3][3]);
}
#ifndef DEFERRED_GBUFFER
int ShadowFilterRadius(float packed) { return int(clamp(floor(packed), 0.0, 3.0)); }
float ShadowNormalBiasTexels(float packed) { return fract(packed) * 8.0; }
float ShadowTexelWorld(mat4 M, vec4 pos, float texels) {
	vec3 rowX = vec3(M[0][0], M[1][0], M[2][0]);
	vec3 rowW = vec3(M[0][3], M[1][3], M[2][3]);
	float w = dot(vec4(M[0][3], M[1][3], M[2][3], M[3][3]), pos);
	return abs(w) / (texels * max(length(rowX - 0.5 * rowW), 1e-8));
}
vec3 ShadowReceiverNormal() {
	vec3 n = normalize(vNormal);
	return dot(n, vWorldPositionShadow.xyz) > 0.0 ? -n : n;
}
float ShadowPCF2D(sampler2DShadow map, vec3 uvz, int K, vec2 size, vec4 rect) {
	vec2 st = uvz.xy * size - 0.5;
	vec2 base = floor(st);
	vec2 f = st - base;
	float sum = 0.0;
	for (int j = -K; j <= K + 1; j++) {
		float wy = (j == -K) ? 1.0 - f.y : ((j == K + 1) ? f.y : 1.0);
		for (int i = -K; i <= K + 1; i++) {
			float wx = (i == -K) ? 1.0 - f.x : ((i == K + 1) ? f.x : 1.0);
			vec2 uv = clamp((base + vec2(float(i), float(j)) + 0.5) / size, rect.xy, rect.zw);
			sum += wx * wy * texture(map, vec3(uv, uvz.z));
		}
	}
	float n = float(2 * K + 1);
	return sum / (n * n);
}
float ShadowCascadeFar(vec4 splits, int c) {
	if (c == 0) return splits.x;
	if (c == 1) return splits.y;
	if (c == 2) return splits.z;
	return splits.w;
}
float ShadowDirectionalCascade(int c, bool multi, float packed, vec4 pos, vec3 n) {
	mat4 M = uDirectionalDepthsMVP[c];
	vec2 size = vec2(textureSize(uDirectionalShadowMaps, 0));
	float tile = multi ? size.x * 0.5 : size.x;
	vec4 coord = M * vec4(pos.xyz + n * (ShadowNormalBiasTexels(packed) * ShadowTexelWorld(M, pos, tile)), 1.0);
	vec4 rect = vec4(0.0, 0.0, 1.0, 1.0);
	if (multi) {
		vec2 off = vec2((c == 1 || c == 3) ? 0.5 : 0.0, (c >= 2) ? 0.5 : 0.0);
		coord.xy = coord.xy * 0.5 + off;
		rect = vec4(off + 0.5 / size, off + 0.5 - 0.5 / size);
	}
	return ShadowPCF2D(uDirectionalShadowMaps, coord.xyz, ShadowFilterRadius(packed), size, rect);
}
float DirectionalShadowFactor(LIGHT L) {
	if (!L.HaveShadowMap || uNumberOfDirectionalShadows <= 0) return 1.0;
	vec4 splits = uDirectionalShadowFar[0];
	bool multi = splits.y > 0.0;
	int count = !multi ? 1 : (splits.w > 0.0 ? 4 : (splits.z > 0.0 ? 3 : 2));
	vec4 pos = vWorldPositionShadow;
	vec3 n = ShadowReceiverNormal();
	float depth = -pos.z;
	int c = count;
	for (int i = 3; i >= 0; i--)
		if (i < count && depth < ShadowCascadeFar(splits, i)) c = i;
	if (c >= count) return 1.0;
	float cFar = ShadowCascadeFar(splits, c);
	float t = clamp((depth - cFar * 0.9) / (cFar * 0.1), 0.0, 1.0);
	float s = ShadowDirectionalCascade(c, multi, L.PCFTexelSize, pos, n);
	if (t > 0.0) {
		float next = (c + 1 < count) ? ShadowDirectionalCascade(c + 1, multi, L.PCFTexelSize, pos, n) : 1.0;
		s = mix(s, next, t);
	}
	return s;
}
float PCFPoint(mat4 m1, mat4 m2, float packed, vec4 pos, vec3 n) {
	vec3 ls = (m2 * pos).xyz;
	float size = float(textureSize(uPointShadowMaps[0], 0).x);
	vec3 a = abs(ls);
	float ma = max(a.x, max(a.y, a.z));
	ls += mat3(m2) * n * (ShadowNormalBiasTexels(packed) * 2.0 * ma / size);
	a = abs(ls);
	ma = max(a.x, max(a.y, a.z));
	// m1 already carries the device's shadow-bias/projection remap, so no
	// extra *0.5+0.5 here. 0.02 is PointLight's default bias scale - this
	// path has no per-light value for it.
	vec4 clip = m1 * vec4(0.0, 0.0, -ma * 0.98, 1.0);
	float depth = clip.z / clip.w;
	vec3 axis, t, b;
	if (a.x >= a.y && a.x >= a.z) { axis = vec3(sign(ls.x), 0.0, 0.0); t = vec3(0.0, 0.0, 1.0); b = vec3(0.0, 1.0, 0.0); }
	else if (a.y >= a.z)          { axis = vec3(0.0, sign(ls.y), 0.0); t = vec3(1.0, 0.0, 0.0); b = vec3(0.0, 0.0, 1.0); }
	else                          { axis = vec3(0.0, 0.0, sign(ls.z)); t = vec3(1.0, 0.0, 0.0); b = vec3(0.0, 1.0, 0.0); }
	vec2 st = (vec2(dot(ls, t), dot(ls, b)) / ma * 0.5 + 0.5) * size - 0.5;
	vec2 base = floor(st);
	vec2 f = st - base;
	int K = ShadowFilterRadius(packed);
	float sum = 0.0;
	for (int j = -K; j <= K + 1; j++) {
		float wy = (j == -K) ? 1.0 - f.y : ((j == K + 1) ? f.y : 1.0);
		for (int i = -K; i <= K + 1; i++) {
			float wx = (i == -K) ? 1.0 - f.x : ((i == K + 1) ? f.x : 1.0);
			vec2 uv = (base + vec2(float(i), float(j)) + 0.5) / size * 2.0 - 1.0;
			sum += wx * wy * ((texture(uPointShadowMaps[0], axis + t * uv.x + b * uv.y).r >= depth) ? 1.0 : 0.0);
		}
	}
	float nk = float(2 * K + 1);
	return sum / (nk * nk);
}
float PCFSpot(mat4 sMatrix, float packed, vec4 pos, vec3 n) {
	vec2 size = vec2(textureSize(uSpotShadowMaps[0], 0));
	vec4 coord = sMatrix * vec4(pos.xyz + n * (ShadowNormalBiasTexels(packed) * ShadowTexelWorld(sMatrix, pos, size.x)), 1.0);
	coord.xyz /= coord.w;
	return ShadowPCF2D(uSpotShadowMaps[0], coord.xyz, ShadowFilterRadius(packed), size, vec4(0.0, 0.0, 1.0, 1.0));
}
float PointShadowFactor(LIGHT L) {
	if (!L.HaveShadowMap || uNumberOfPointShadows <= 0) return 1.0;
	int slot = clamp(L.ShadowMap, 0, 3);
	return PCFPoint(uPointDepthsMVP[slot * 2], uPointDepthsMVP[slot * 2 + 1], L.PCFTexelSize, vWorldPositionShadow, ShadowReceiverNormal());
}
float SpotShadowFactor(LIGHT L) {
	if (!L.HaveShadowMap || uNumberOfSpotShadows <= 0) return 1.0;
	int slot = clamp(L.ShadowMap, 0, 3);
	return PCFSpot(uSpotDepthsMVP[slot], L.PCFTexelSize, vWorldPositionShadow, ShadowReceiverNormal());
}
#endif
float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius) {
	if (Radius > 0.0) {
		float d = distance(Vertex, LightPosition);
		return clamp(1.0 - (1.0 / Radius) * d, 0.0, 1.0);
	}
	return 1.0;
}
float DualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone) {
	if (cosOutterCone > 0.0 || cosInnerCone > 0.0) {
		vec3 to_light = normalize(SpotLightPosition - Vertex);
		float angle = dot(-to_light, normalize(SpotLightDirection));
		float funcX = 1.0 / (cosInnerCone - cosOutterCone);
		float funcY = -funcX * cosOutterCone;
		return clamp(angle * funcX + funcY, 0.0, 1.0);
	}
	return 0.0;
}
const float PBR_PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
	return a2 / max(PBR_PI * denom * denom, 1e-6);
}
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 CalculatePBRLighting(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness) {
	vec3 H = normalize(V + L);
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
	vec3 numerator = NDF * G * F;
	float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;
	vec3 specularTerm = numerator / denom;
	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
	vec3 irradiance = radiance * PBR_PI;
	float NdotL = max(dot(N, L), 0.0);
	return (kD * albedo / PBR_PI + specularTerm) * irradiance * NdotL;
}
#endif

vec3 p3d_SHIrradiance(vec3 n) {
	const float c1 = 0.429043, c2 = 0.511664, c3 = 0.743125, c4 = 0.886227, c5 = 0.247708;
	return uAmbientSH[8].rgb * (c1 * (n.x * n.x - n.y * n.y))
	     + uAmbientSH[6].rgb * (c3 * n.z * n.z)
	     + uAmbientSH[0].rgb * c4
	     - uAmbientSH[6].rgb * c5
	     + uAmbientSH[4].rgb * (2.0 * c1 * n.x * n.y)
	     + uAmbientSH[7].rgb * (2.0 * c1 * n.x * n.z)
	     + uAmbientSH[5].rgb * (2.0 * c1 * n.y * n.z)
	     + uAmbientSH[3].rgb * (2.0 * c2 * n.x)
	     + uAmbientSH[1].rgb * (2.0 * c2 * n.y)
	     + uAmbientSH[2].rgb * (2.0 * c2 * n.z);
}
vec3 p3d_Ambient(vec3 n) {
	if (uAmbientParams.x >= 1.5)
		return max(p3d_SHIrradiance(n) * (1.0 / 3.14159265359), vec3(0.0));
	if (uAmbientParams.x >= 0.5) {
		float y = clamp(n.y, -1.0, 1.0);
		return (y >= 0.0) ? mix(uAmbientEquator.rgb, uAmbientSky.rgb, y)
		                  : mix(uAmbientEquator.rgb, uAmbientGround.rgb, -y);
	}
	return uAmbientLight.rgb;
}
vec3 p3d_PerturbNormal(vec3 N, vec3 p, vec2 uv, vec3 m, float strength) {
	vec3 dp1 = dFdx(p);
	vec3 dp2 = dFdy(p);
	vec2 duv1 = dFdx(uv);
	vec2 duv2 = dFdy(uv);
	vec3 dp2perp = cross(dp2, N);
	vec3 dp1perp = cross(N, dp1);
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	float det = dot(dp1, dp2perp);
	float s = (det < 0.0) ? -1.0 : 1.0;
	float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-20));
	m.xy *= strength;
	return normalize(mat3(T * (invmax * s), B * (invmax * s), N) * m);
}

vec3 p3d_cells(vec2 p) {
	// the nearest two of a scatter of points: x how far in from the cell's edge, y z the cell
	vec2 ci = floor(p), cf = fract(p);
	float d1 = 9.0, d2 = 9.0; vec2 id = vec2(0.0);
	for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
		vec2 o = vec2(float(i), float(j));
		vec2 cid = ci + o;
		vec2 pt = o + vec2(p3d_ih(ivec3(ivec2(floor(cid + 0.5)), 11)), p3d_ih(ivec3(ivec2(floor(cid + 0.5)), 23))) - cf;
		float dd = dot(pt, pt);
		if (dd < d1) { d2 = d1; d1 = dd; id = cid; } else if (dd < d2) d2 = dd;
	}
	return vec3(sqrt(d2) - sqrt(d1), id);
}
// The wave picture read sharp at a glance: a pixel on a sea seen low is long and thin, and the
// ordinary read blurs to its long side - here the long side is cut to twice the short one.
// The wave picture, read so that it never repeats (after I. Quilez): the plane is divided by a slow noise
// into regions, each reading the picture from its own random place, cross-faded where regions meet. And
// read sharp at a glance (a pixel on a sea seen low is long and thin: its long side is cut to twice the short).
vec4 p3d_waves(vec2 uv) {
	vec2 dx = dFdx(uv), dy = dFdy(uv);
	float lx = length(dx) + 1e-9, ly = length(dy) + 1e-9, m = min(lx, ly) * 2.0;
	dx *= min(1.0, m / lx); dy *= min(1.0, m / ly);
	float k = p3d_noise(vec3(uv * 0.37, 3.0)) * 0.65 + p3d_noise(vec3(uv * 1.3, 9.0)) * 0.35;
	float l = k * 9.0, fi = floor(l), ff = l - fi;
	vec2 oa = sin(vec2(3.0, 7.0) * fi), ob = sin(vec2(3.0, 7.0) * (fi + 1.0));
	vec4 ca = textureGrad(waves, uv + oa, dx, dy), cb = textureGrad(waves, uv + ob, dx, dy);
	return mix(ca, cb, smoothstep(0.2, 0.8, ff - 0.1 * dot(ca.xyz - cb.xyz, vec3(1.0))));
}
// Waves as a sum of travelling sharp-crested trains, each dragging the next sideways (after afl_ext's
// procedural ocean): nothing is read from a picture, so nothing can repeat or tile - every place and every
// moment is its own. `iters` falls with distance, so no train is ever finer than a pixel.
float p3d_sea(vec2 pos, float t, int iters) {
	float iter = 0.0, freq = 1.0, tm = 2.0, weight = 1.0, sumV = 0.0, sumW = 0.0;
	for (int i = 0; i < 40; i++) {
		if (i >= iters) break;
		vec2 d = vec2(sin(iter), cos(iter));
		float x = dot(d, pos) * freq + t * tm;
		float wave = exp(sin(x) - 1.0);
		pos += d * (-wave * cos(x)) * weight * 0.30;
		sumV += wave * weight; sumW += weight;
		weight *= 0.85; freq *= 1.18; tm *= 1.07; iter += 1232.399963;
	}
	return sumV / max(sumW, 1e-4);
}
// The same sea and its slope (d height / d pos) in one pass: each train's own derivative,
// wave * cos(x) * freq along its direction. Three passes for a finite difference were most of
// what the water cost - up to 108 trains a pixel where the sea fills the view.
float p3d_seaG(vec2 pos, float t, int iters, out vec2 grad) {
	float iter = 0.0, freq = 1.0, tm = 2.0, weight = 1.0, sumV = 0.0, sumW = 0.0;
	vec2 g = vec2(0.0);
	for (int i = 0; i < 40; i++) {
		if (i >= iters) break;
		vec2 d = vec2(sin(iter), cos(iter));
		float x = dot(d, pos) * freq + t * tm;
		float wave = exp(sin(x) - 1.0);
		float dw = wave * cos(x);
		g += d * (dw * freq * weight);
		pos += d * (-dw) * weight * 0.30;
		sumV += wave * weight; sumW += weight;
		weight *= 0.85; freq *= 1.18; tm *= 1.07; iter += 1232.399963;
	}
	float k = 1.0 / max(sumW, 1e-4);
	grad = g * k;
	return sumV * k;
}
void main() {
	vec3 p3d_N = normalize(vNormalWorld);
	vec3 p3d_V = normalize(uCameraPosition - vWorldPos.xyz);
	vec3 Albedo = vec3(1.0); vec3 Normal = vec3(0.0); float Metallic = 0.0; float Roughness = 0.5; vec3 Emissive = vec3(0.0); float Occlusion = 1.0; float Opacity = 1.0; float Reflection = 0.0; 
	// ---- Water, written again from nothing (the old one is tools/Ocean.old.glsl) ----
	// One idea, the one known to work (three.js' Water): the surface is a photographed wave normal map read
	// four times at four sizes drifting four ways; each point either mirrors the sky or shows the water's own
	// colour by Fresnel; the sun is a specular point on it. Then what this sea adds: its depth (clear over
	// sand, dark over the deep), foam at the shore, a river's calm, winter's ice.
	vec2 wp = vWorldPos.xz;
	float wt = uTime * 0.6;
	float px = max(length(dFdx(wp)), length(dFdy(wp)));
	float fine = 1.0 - smoothstep(0.05, 0.5, px);
	vec2 muv = (wp + vec2(WATER_MAP_SIZE * 0.5)) / WATER_MAP_SIZE;
	vec4 sh = texture_2D(shore, clamp(muv, vec2(0.002), vec2(0.998)));
	float inside = step(0.0, min(muv.x, muv.y)) * step(max(muv.x, muv.y), 1.0);
	float depthRaw = mix(WATER_DEPTH_RANGE, sh.r * sh.r * (WATER_DEPTH_RANGE + WATER_DEPTH_ABOVE) - WATER_DEPTH_ABOVE, inside);
	float depth = max(depthRaw, 0.0) * WATER_DEEPEN;
	float creek = clamp(sh.g, 0.0, 1.0) * inside;
	float winterSea = uGlobals[14].x, rainNow = uGlobals[14].y;
	vec3 sunDir = dot(uGlobals[15].xyz, uGlobals[15].xyz) > 0.5 ? normalize(uGlobals[15].xyz) : normalize(vec3(0.55, 0.58, 0.42));
	float sunUp = smoothstep(-0.05, 0.2, sunDir.y), dayS = smoothstep(0.05, 0.45, sunDir.y);

	// (where the ground is all but at the surface the two fight for the pixel - further off, sooner: the water gives way)
	// (far off, where the two surfaces are too close to tell apart, the water gives way; near to, it is lifted clear)
	if (inside > 0.5 && depthRaw + (vWorldPos.y - WATER_LEVEL) < length(uCameraPosition - vWorldPos.xyz) * 0.0004 - 0.004) discard;
	// the surface
	float pxw = sqrt(max(length(dFdx(wp)) * length(dFdy(wp)), 1e-8));
	const float SEA_SCALE = 0.11;
	// (trains whose wavelength is under about three pixels are left out)
	int iters = int(clamp(log(1.6 / max(pxw * SEA_SCALE, 1e-5)) / log(1.18), 6.0, 36.0));
	float calm = mix(1.0, 0.18, creek) * mix(0.35, 1.0, smoothstep(0.1, 3.0, depth));
	// (gusts and calms: the sea is not the same everywhere)
	calm *= 0.6 + 0.8 * smoothstep(0.25, 0.75, p3d_noise(vec3(wp * 0.006 + wt * 0.004, 1.0)) * 0.6 + p3d_noise(vec3(wp * 0.021, 5.0)) * 0.4);
	float e = max(0.03, pxw * 1.0);
	vec2 sp = wp * SEA_SCALE;
	vec2 seaGrad;
	float h0 = p3d_seaG(sp, wt, iters, seaGrad);
	// (looked down on from high up, a sea is a fine even texture with faint lines of swell - not hills: the big
	// waves' shading is eased as the eye climbs and looks down, or it reads as cloud)
	float fromSky = smoothstep(0.30, 0.70, p3d_V.y) * smoothstep(8.0, 40.0, uCameraPosition.y - WATER_LEVEL);
	// (the scene's settings for its water, set in the editor: each 1 as authored; 0 means none were given)
	vec4 waterSet = vec4(uGlobals[13].x > 0.001 ? uGlobals[13].x : 1.0, 1.0, uGlobals[13].z > 0.001 ? uGlobals[13].z : 1.0, uGlobals[13].w > 0.001 ? uGlobals[13].w : 1.0);
	float waveH = 7.5 * waterSet.x * calm * mix(1.0, 0.22, fromSky) * WATER_ROUGH;
	vec2 slope = seaGrad * SEA_SCALE * waveH;
	// (and the wind's own ripple on top of the waves: a second, much smaller sea, seen only close to)
	float rippleSeen = 1.0 - smoothstep(0.02, 0.14, pxw);
	if (rippleSeen > 0.0) {
		vec2 rp = wp * 1.7 + vec2(31.0, 17.0);          // (the main sea stops at waves a metre long: these go on down to a hand's width)
		float em = 0.035;
		vec2 rg;
		p3d_seaG(rp, wt * 2.2, 9, rg);
		slope += rg * 1.7 * 0.26 * rippleSeen * calm;
	}
	// A river runs: its ripples are carried downstream (the depth picture's blue and alpha say which way).
	// Two copies half a beat apart, each reset as the other is at its clearest, so nothing is ever stretched.
	vec2 flow = vec2(0.0); float streaks = 0.0;
	if (creek > 0.02) {
		flow = sh.ba * 2.0 - 1.0;
		flow = dot(flow, flow) > 0.01 ? normalize(flow) : vec2(0.0);
		float beat = uTime * 0.22, t0 = fract(beat), t1 = fract(beat + 0.5), w1 = abs(1.0 - 2.0 * t0);
		const float RUN = 5.0;                                      // metres a beat: about a metre a second
		vec2 q0 = (wp - flow * t0 * RUN) * 1.3, q1 = (wp - flow * t1 * RUN) * 1.3 + 13.0;
		float er = 0.05;
		float a0 = p3d_sea(q0, wt, 9), b0 = p3d_sea(q1, wt, 9);
		vec2 s0 = vec2(p3d_sea(q0 + vec2(er * 1.3, 0.0), wt, 9) - a0, p3d_sea(q0 + vec2(0.0, er * 1.3), wt, 9) - a0);
		vec2 s1 = vec2(p3d_sea(q1 + vec2(er * 1.3, 0.0), wt, 9) - b0, p3d_sea(q1 + vec2(0.0, er * 1.3), wt, 9) - b0);
		slope = mix(slope, mix(s0, s1, w1) / er * 0.34, creek);
		// (and what floats on it goes with it: thin streaks of foam drawn out along the current)
		// Streamlines: what floats is drawn out along the current. A fine speckle, smeared along the flow by
		// reading it at seven places up and down stream (so the lines follow every bend, with nothing to tear),
		// and carried down at the river's speed.
		vec2 f0 = wp - flow * t0 * RUN, f1 = wp - flow * t1 * RUN + 7.0;
		float k0 = 0.0, k1 = 0.0;
		// (steps finer than the speckle itself, or the line is a string of beads)
		for (int q = -8; q <= 8; q++) {
			vec2 o = flow * (float(q) * 0.14);
			float wq_ = (1.0 - abs(float(q)) / 9.0) * (4.0 / 9.0);
			k0 += p3d_noise(vec3((f0 + o) * 3.4, 1.0)) * wq_;
			k1 += p3d_noise(vec3((f1 + o) * 3.4, 5.0)) * wq_;
		}
		k0 /= 4.0; k1 /= 4.0;
		streaks = smoothstep(0.57, 0.66, mix(k0, k1, w1)) * creek;
	}
	vec3 Nw = normalize(vec3(-slope.x, 1.0, -slope.y));
	vec4 nz = vec4(Nw.x, Nw.z, Nw.y, 1.0);

	// the sun on it, and how much it mirrors
	vec3 sunCol = mix(vec3(1.0, 0.62, 0.36), vec3(1.0), dayS) * sunUp * (1.0 - rainNow);
	float spec = pow(max(dot(p3d_V, normalize(reflect(-sunDir, Nw))), 0.0), 160.0) * 4.0 * (1.0 - 0.6 * creek);
	spec += pow(max(dot(p3d_V, normalize(reflect(-sunDir, Nw))), 0.0), 28.0) * 0.9 * (1.0 - creek);
	// (and a broad shimmer round it: the sun broken over many facets)
	// (no glints picked out cell by cell: they showed as small bright squares in the sun's path)
	// (glints: the bright sky low ahead of the eye, caught by the faces tipped toward it)
	vec3 ahead = normalize(vec3(-p3d_V.x, 0.55, -p3d_V.z));
	float glint = pow(max(dot(p3d_V, normalize(reflect(-ahead, Nw))), 0.0), 30.0) * 4.2 * (1.0 - creek);
	float diff = max(dot(sunDir, Nw), 0.0) * 0.5;
	float theta = max(dot(p3d_V, Nw), 0.0);
	// (near the eye the water's own teal wins and the sky is only its highlights; far off the mirror takes over)
	float refl = 0.05 + mix(mix(0.95, 0.55, dayS), 0.94, smoothstep(0.02, 0.3, px)) * pow(1.0 - theta, mix(2.5, 5.0, dayS));

	// the water's own colour: sand seen through the shallows, teal, then the deep
	float caust = p3d_noise(vec3(wp * 1.3 + Nw.xz * 2.0, wt * 0.9)) * p3d_noise(vec3(wp * 2.9 - 5.0, wt * 1.1 + 3.0));
	vec3 bed = mix(vec3(0.62, 0.58, 0.44), vec3(0.46, 0.46, 0.40), creek) * (0.85 + 1.4 * caust * fine);
	vec3 body = mix(vec3(0.07, 0.66, 0.62), vec3(0.0, 0.38, 0.40), smoothstep(0.3, 4.5, depth));
	// (far off the water's own colour greys with the air)
	body = mix(body, vec3(dot(body, vec3(0.3, 0.5, 0.2))), 0.75 * smoothstep(0.02, 0.22, px) * (1.0 - smoothstep(0.06, 0.40, p3d_V.y)) * (1.0 - creek));
	// (a river is the same water, a shade deeper and greener for its bed and banks; it eases into the sea's colour at its mouth)
	body = mix(body, mix(vec3(0.03, 0.36, 0.36), vec3(0.0, 0.16, 0.20), smoothstep(0.2, 1.5, depth)), creek * 0.9);
	// (light on the bed: soft wandering lines, not patches)
	float cl = 1.0 - abs(p3d_noise(vec3(wp * 0.9 + Nw.xz * 0.6, wt * 0.5)) + p3d_noise(vec3(wp * 2.1 + 4.0, wt * 0.7)) * 0.5 - 0.75) * 4.0;
	body += vec3(0.035, 0.09, 0.08) * smoothstep(0.55, 1.0, cl) * creek * exp(-depth * 0.4) * fine;
	// (the last hand of water is clear: the sand itself, wet, shows through - the sea has no edge of its own colour)
	body = mix(body, bed * vec3(0.40, 0.62, 0.58), exp(-depth * 1.1) * 0.8);
	body = mix(body, vec3(0.36, 0.34, 0.27), exp(-depth * 6.0) * 0.85 * (1.0 - creek));
	vec3 scatter = body * (0.78 + 0.90 * theta) * (0.18 + 0.82 * dayS);
	// (the faces of waves turned from the eye are in their own shade: the dark that gives a sea its shape)
	// (the shade on wave faces turned from the eye: only when looking ALONG the water. Looked down on, 'from the
	// eye' is every way at once, and the shade drew rings bursting out from under the camera)
	float along = 1.0 - smoothstep(0.30, 0.70, p3d_V.y);
	scatter *= 1.0 - 0.62 * along * smoothstep(0.0, 0.5, dot(Nw.xz, normalize(p3d_V.xz + 1e-4)) * 2.2) * (1.0 - creek);
	// (from above the waves show by their own slope to the light instead: one direction for the whole sea)
	scatter *= 1.0 + (1.0 - along) * clamp(dot(Nw.xz, normalize(sunDir.xz + 1e-4)) * 1.2, -0.14, 0.14) * (1.0 - creek);

	// the sky it mirrors
	vec3 Rd = reflect(-p3d_V, Nw); Rd.y = abs(Rd.y);
	vec3 sky = mix(mix(vec3(1.45, 0.92, 0.60), vec3(0.88, 0.91, 0.92), dayS), mix(vec3(0.62, 0.42, 0.44), vec3(0.30, 0.42, 0.48), dayS), pow(Rd.y, mix(1.2, 0.5, dayS))) * (0.2 + 0.8 * sunUp);
	// (toward a low sun the water is a road of its gold)
	sky += vec3(1.6, 0.85, 0.35) * pow(max(dot(normalize(vec3(Rd.x, Rd.y * 0.4, Rd.z)), normalize(vec3(sunDir.x, 0.0, sunDir.z))), 0.0), 6.0) * (1.0 - dayS) * sunUp;
	sky = mix(sky, vec3(0.50, 0.53, 0.56) * (0.2 + 0.8 * sunUp), rainNow);
	refl *= 1.0 - 0.35 * creek;
	vec3 col = mix(sunCol * diff * 0.3 + scatter, vec3(0.1) * sunUp + sky * 0.9 + sunCol * spec, refl);


	// foam: the run-up on the sand, and behind it a net of foam lines thinning out to sea; a river's white edge
	// Foam. At the shore: a soft wash of white that the waves push up the sand and draw back, cracked into a
	// net as it thins, its edge wandering; out at sea only the crests of the steepest waves break.
	float lace = p3d_noise(vec3(wp * 1.1 + vec2(wt * 0.2, 0.0), wt * 0.4)) * 0.55 + p3d_noise(vec3(wp * 4.3, wt * 0.8)) * 0.45;
	float breathe = 0.5 + 0.5 * sin(wt * 1.1 + p3d_noise(vec3(wp * 0.05, 0.0)) * 6.0);
	float push = smoothstep(0.28, 0.66, h0);                       // (the same wave that lifts the water's edge)                         // how far up the wash is just now
	// Foam is lace at every size: a web of fine bubbled lines with holes of clear water, the web closing to a
	// solid white only where the water is a film on the sand. Folded noise summed over five sizes, pulled about
	// by the water under it; how much of the web shows is set by how shallow it is and where the wash stands now.
	vec2 fq = wp + vec2(p3d_noise(vec3(wp * 0.30, wt * 0.12)), p3d_noise(vec3(wp * 0.30 + 7.0, wt * 0.12))) * 3.0 + Nw.xz * 1.2;
	float web = 0.0;
	for (int o = 0; o < 3; o++) {
		float wf = 0.45 * pow(2.3, float(o));
		float rdg = 1.0 - abs(p3d_noise(vec3(fq * wf + float(o) * 5.3, wt * (0.10 + 0.06 * float(o)))) * 2.0 - 1.0);
		web = max(web, smoothstep(0.90 - 0.03 * float(o), 0.985, rdg) * (1.0 - 0.22 * float(o)));
	}
	// Foam as the reference has it: a sheet of white lying on the water at the sand's edge, which CRACKS as it
	// goes out - plates of white with the sea showing in the cracks between them, the cracks widening and the
	// plates shrinking until, a couple of metres of depth out, only a few thin white threads are left.
	// (how far out from the sand, in metres: the depth over the bed's slope - a flat beach and a steep one get
	// the same width of foam)
	float eS = 1.5 / 1024.0;
	// (inside the picture only: unclamped, past its edge the picture repeated, and every 512 m of open sea
	// found the island's coast again - a net of shore foam over the whole ocean, seen from the air)
	vec2 guv = clamp(muv, vec2(eS + 0.002), vec2(0.998 - eS));
	float gx1 = texture_2D(shore, guv + vec2(eS, 0.0)).r, gx0 = texture_2D(shore, guv - vec2(eS, 0.0)).r, gz1 = texture_2D(shore, guv + vec2(0.0, eS)).r, gz0 = texture_2D(shore, guv - vec2(0.0, eS)).r;
	vec2 gradD = vec2(gx1 * gx1 - gx0 * gx0, gz1 * gz1 - gz0 * gz0) * ((WATER_DEPTH_RANGE + WATER_DEPTH_ABOVE) / (3.0 * WATER_MAP_SIZE / 1024.0)) * inside;
	float away = depth / max(length(gradD), 0.004) / waterSet.z;
	vec3 plates = p3d_cells(wp * 1.5 + vec2(p3d_noise(vec3(wp * 1.1, 1.0)), p3d_noise(vec3(wp * 1.1, 5.0))) * 2.4 + vec2(p3d_noise(vec3(wp * 4.0, 2.0)), p3d_noise(vec3(wp * 4.0, 7.0))) * 0.5 + Nw.xz * 0.5 + vec2(wt * 0.05, wt * 0.03));
	float out_ = away + (lace - 0.5) * 1.1 - 0.7 * push;                        // raggedly, breathing with the waves
	// Next the sand it is all but unbroken white; the cracks come in a metre and a half out, between SMALL
	// plates first (a hand across), larger ones only where the sheet is already breaking up.
	float open_ = pow(smoothstep(2.0, 10.0, out_), 1.5);
	float crk = 0.014 + open_ * 0.42;                                          // hairlines at the sand, wide open far out
	float sheet = smoothstep(crk, crk + 0.035, plates.x) * (1.0 - smoothstep(9.5, 13.0, out_)) * mix(0.96, 0.72, smoothstep(2.0, 9.0, out_));
	float threads = web * (1.0 - smoothstep(9.0, 20.0, out_)) * smoothstep(6.0, 11.0, out_) * 0.5;
	float foam = max(sheet, threads) * (1.0 - creek);
	foam *= 0.86 + 0.14 * p3d_noise(vec3(wp * 18.0, wt));
	float crest = smoothstep(0.88, 0.98, h0 + (lace - 0.5) * 0.12) * smoothstep(2.5, 6.0, depth) * smoothstep(0.75, 1.25, calm) * smoothstep(0.55, 0.75, web) * 0.8;
	foam = max(foam, crest * (1.0 - creek));
	foam = max(foam, creek * (1.0 - smoothstep(0.42, 0.80 + 0.20 * breathe, depth)) * (0.75 + 0.25 * lace));
	foam = max(foam, streaks * 0.17 * smoothstep(0.25, 0.8, depth));
	foam *= step(0.0005, depth + inside);
	col = mix(col, vec3(0.80, 0.83, 0.84) * (0.25 + 0.75 * sunUp), foam * 0.92);
	// At the sand the water is a film: foam and water both thin away to nothing, and the wet sand under them
	// shows through - more and more of it toward the very edge.
	{
		float film = depthRaw + (vWorldPos.y - WATER_LEVEL);                              // how thick the water is here
		float thick = smoothstep(0.0, 0.09, film + (lace - 0.5) * 0.02);
		vec3 wetSand = vec3(0.54, 0.50, 0.39) * (0.25 + 0.75 * sunUp);
		col = mix(wetSand, col, mix(0.12, 1.0, thick) + (1.0 - mix(0.12, 1.0, thick)) * creek);
	}

	// winter: a cracked white crust from the shore out, plates breaking away into open water
	float iceOn = 0.0;
	if (winterSea > 0.01) {
		// Ice: a white crust from the shore out, cracked as ice cracks - long wandering lines that branch, fine
		// ones between them - its outer edge ragged, loose pieces of it afloat beyond.
		vec2 iq = wp + vec2(p3d_noise(vec3(wp * 0.09, 2.0)), p3d_noise(vec3(wp * 0.09, 9.0))) * 2.0;
		float reach = depth + (p3d_noise(vec3(wp * 0.05, 6.0)) - 0.5) * 1.4 + (p3d_noise(vec3(wp * 0.6, 8.0)) - 0.5) * 0.5;
		float sheet = (1.0 - smoothstep(1.2, 1.5, reach)) * winterSea;
		float floes = smoothstep(0.56, 0.60, p3d_noise(vec3(iq * 0.35, 4.0))) * (1.0 - smoothstep(1.4, 4.0, reach)) * winterSea;
		float plate = max(sheet, floes);
		float crack = 0.0;
		for (int o = 0; o < 3; o++) {
			float cf = 0.5 * pow(2.6, float(o));
			float rdg = 1.0 - abs(p3d_noise(vec3(iq * cf + float(o) * 7.1, 3.0)) * 2.0 - 1.0);
			crack = max(crack, smoothstep(0.988, 0.998, rdg) * (1.0 - 0.25 * float(o)));
		}
		crack *= fine * 0.6 + 0.4;
		float drift = smoothstep(0.35, 0.65, p3d_noise(vec3(wp * 0.7, 12.0)) * 0.6 + p3d_noise(vec3(wp * 3.1, 15.0)) * 0.4);
		vec3 iceCol = mix(vec3(0.72, 0.78, 0.84), vec3(0.93, 0.94, 0.95), drift) * (1.0 - 0.45 * crack);
		col = mix(col, iceCol, plate);
		iceOn = plate;
		// (the ice thins away at the sand as the water does: without it the crust ended in the steps of the
		// depth picture's own squares)
		{
			float iceEdge = smoothstep(0.0, 0.045, depthRaw + (vWorldPos.y - WATER_LEVEL));
			col = mix(mix(vec3(0.66, 0.63, 0.55) * (0.25 + 0.75 * sunUp), col, mix(0.10, 1.0, iceEdge)), col, 1.0 - plate);
		}
	}
	Albedo = col * mix(1.15, 0.55, dayS);
	float Reflection_ = clamp(clamp(refl, 0.0, 1.0) * waterSet.w, 0.0, 1.0) * (1.0 - foam) * (1.0 - iceOn) * smoothstep(0.1, 0.6, depth);
	// (the surface the renderer mirrors the scene in is the rippled one: what is seen in the water is bent and
	// broken by its waves - more where the water is reflective, so the shallows and foam are lit as before)
	Normal = normalize(mix(vec3(0.0, 1.0, 0.0), Nw, mix(0.12, 0.50, Reflection_) * (1.0 - iceOn)));
	Metallic = 0.0; Roughness = mix(0.41, 0.8, max(foam, iceOn)); Emissive = vec3(0.0);     // (0.42 and under: the renderer mirrors the scene in it)
	// (the shore, its trees and the mountains are seen in the water: most at a glancing look, none in foam or on the wash)
	Reflection = Reflection_;




	vec3 albedo = (vec4(Albedo, 1.0)).xyz;
	float metallic = (vec4(Metallic)).x;
	float roughness = clamp((vec4(Roughness)).x, 0.03, 1.0);
	vec3 emissive = (vec4(Emissive, 0.0)).xyz;
	float occlusion = (vec4(Occlusion)).x;
	float opacity = clamp((vec4(Opacity)).x, 0.0, 1.0) * uOpacity;
	float reflection = clamp((vec4(Reflection)).x, 0.0, 1.0);
	vec3 N = normalize((vec4(dot(Normal, Normal) > 0.0001 ? Normal : p3d_N, 0.0)).xyz);

#if defined(SHADOW_DEPTH)
	FragColor = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
#elif defined(DEFERRED_GBUFFER)
	vec3 normalOut = normalize((uViewMatrix * vec4(N, 0.0)).xyz);
	vec3 color = albedo * occlusion + emissive;
	vec3 ambient = p3d_Ambient(N);
	FragData_r = vec4(color, color.x * ambient.x);
	FragData_g = vec4(1.0, 1.0, 1.0, color.y * ambient.y);
	FragData_b = vec4(normalOut, color.z * ambient.z);
	FragData_pbr = vec4(roughness, metallic, reflection > 0.03 ? 1.0 : 0.0, reflection);
#else
	vec3 Position = vWorldPos.xyz;
	vec3 V = p3d_V;
	vec3 pbrColor = vec3(0.0);
	for (int i = 0; i < MAX_LIGHTS; i++) {
		if (i < uNumberOfLights) {
			LIGHT L;
			buildLightFromMatrix(uLights[i], L);
			vec3 Ldir = vec3(0.0);
			float atten = 1.0;
			float spotEffect = 1.0;
			float shadowFactor = 1.0;
			if (L.Type == 1.0) {
				Ldir = normalize(-L.Direction);
				shadowFactor = DirectionalShadowFactor(L);
			} else if (L.Type == 2.0) {
				Ldir = normalize(L.Position - Position);
				atten = Attenuation(Position, L.Position, L.Radius);
				if (atten > 0.0) shadowFactor = PointShadowFactor(L);
			} else if (L.Type == 3.0) {
				Ldir = normalize(L.Position - Position);
				atten = Attenuation(Position, L.Position, L.Radius);
				spotEffect = 1.0 - DualConeSpotLight(Position, L.Position, L.Direction, L.Cones.x, L.Cones.y);
				if (atten > 0.0 && spotEffect > 0.0) shadowFactor = SpotShadowFactor(L);
			}
			pbrColor += CalculatePBRLighting(N, V, Ldir, L.Color.rgb, albedo, metallic, roughness) * atten * spotEffect * shadowFactor;
		}
	}
	vec3 ambientPBR = albedo * occlusion * (1.0 - metallic) * p3d_Ambient(N);
	vec3 color = pbrColor + ambientPBR + emissive;
	FragColor = vec4(color, opacity);
#endif
}
#endif
