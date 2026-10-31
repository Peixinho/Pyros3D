#version 460
#ifdef RAYQUERY
#extension GL_EXT_ray_query : require
#endif
#include "common.glsl"
// Ray-traced reflections (SceneRenderer::SetRayTracedReflections): from each pixel's surface (the depth prepass) the
// view's ray mirrored about it, through the scene's triangles (RayScene's BVH - rtshadow.frag's), and what it finds lit
// there: its material's colour by the sun (a second ray: is the sun seen from there), the sky's light and its own glow.
// Unlike the screen's reflections it finds what the screen does not show - behind the camera, under what hides it.
// Out: rgb the light found, a 1 where a ray found something (0: the sky's or the probes' then) - read by surface.frag in
// place of last frame's picture (its uPreviousColor) while it is on.
layout(set = 1, binding = 1) uniform texture2D uDepth;       // (the prepass's: reversed Z, 0 where nothing was drawn)
layout(set = 1, binding = 3) uniform sampler uMaterialSampler;
layout(set = 1, binding = 4, std430) readonly buffer Triangles { vec4 t[]; } uTriangles;     // (9 a triangle: v0 v1 v2 n0 n1 n2 material)
layout(set = 1, binding = 5, std430) readonly buffer Nodes { vec4 n[]; } uNodes;
layout(set = 1, binding = 6, std430) readonly buffer Indices { uint i[]; } uIndices;
layout(set = 1, binding = 7, std430) readonly buffer Materials { vec4 m[]; } uMaterials;     // (2 a material: albedo, emission - RAYQUERY: 2 an instance, by its index in the scene)
layout(set = 1, binding = 9) uniform texture2D uPicture;     // (last frame's picture: what the screen already shows of a hit)
#ifdef RAYQUERY
layout(set = 2, binding = 0) uniform accelerationStructureEXT uRayScene;     // (its own set: the device's, the frame's scene)
#endif
#include "skylight.glsl"
layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 params;     // x: the longest ray (m), y: 1 when there are triangles, z: the materials, w: the frame (its noise)
} uDraw;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float BoxNear(vec3 o, vec3 invD, vec3 lo, vec3 hi, float tMax)
{
	vec3 t1 = (lo - o) * invD, t2 = (hi - o) * invD;
	vec3 a = min(t1, t2), b = max(t1, t2);
	float near = max(max(a.x, a.y), a.z), far = min(min(b.x, b.y), b.z);
	return (far >= near && near < tMax && far > 0.0) ? max(near, 0.0) : 3.0e38;
}

bool Triangle(vec3 o, vec3 d, uint tri, float tMax, out float t, out float u, out float v)
{
	t = 0.0; u = 0.0; v = 0.0;
	vec3 v0 = uTriangles.t[tri * 9u].xyz, v1 = uTriangles.t[tri * 9u + 1u].xyz, v2 = uTriangles.t[tri * 9u + 2u].xyz;
	vec3 e1 = v1 - v0, e2 = v2 - v0, p = cross(d, e2);
	float det = dot(e1, p);
	if (abs(det) < 1e-9) return false;
	float inv = 1.0 / det;
	vec3 tv = o - v0;
	u = dot(tv, p) * inv;
	if (u < 0.0 || u > 1.0) return false;
	vec3 q = cross(tv, e1);
	v = dot(d, q) * inv;
	if (v < 0.0 || u + v > 1.0) return false;
	t = dot(e2, q) * inv;
	return t > 1e-3 && t < tMax;
}

// The nearest triangle along d (anyHit: the first will do - a shadow ray)
bool Trace(vec3 o, vec3 d, float tMax, bool anyHit, out float bestT, out uint bestTri, out vec2 bestUV)
{
	bestT = tMax; bestTri = 0u; bestUV = vec2(0.0);
#ifdef RAYQUERY
	// (the GPU's own: bestTri the instance found - its index in the scene - and no barycentrics needed)
	rayQueryEXT q;
	rayQueryInitializeEXT(q, uRayScene, (anyHit ? gl_RayFlagsTerminateOnFirstHitEXT : 0u) | gl_RayFlagsOpaqueEXT, 0xFF, o, 1e-3, d, tMax);
	while (rayQueryProceedEXT(q)) {}
	if (rayQueryGetIntersectionTypeEXT(q, true) == gl_RayQueryCommittedIntersectionNoneEXT) return false;
	bestT = rayQueryGetIntersectionTEXT(q, true);
	bestTri = uint(rayQueryGetIntersectionInstanceIdEXT(q, true));
	bestUV = rayQueryGetIntersectionBarycentricsEXT(q, true);
	return true;
#else
	bool found = false;
	vec3 invD = 1.0 / d;
	uint stack[48];
	uint depth = 0u, node = 0u;
	if (BoxNear(o, invD, uNodes.n[0].xyz, uNodes.n[1].xyz, tMax) > 1.0e38) return false;
	for (int guard = 0; guard < 4096; guard++)
	{
		vec4 lo = uNodes.n[node * 2u], hi = uNodes.n[node * 2u + 1u];
		uint first = floatBitsToUint(lo.w), count = floatBitsToUint(hi.w);
		if (count > 0u)
		{
			for (uint k = 0u; k < count; k++)
			{
				uint tri = uIndices.i[first + k];
				float t, u, v;
				if (Triangle(o, d, tri, bestT, t, u, v))
				{
					bestT = t; bestTri = tri; bestUV = vec2(u, v); found = true;
					if (anyHit) return true;
				}
			}
		}
		else
		{
			uint a = first, b = first + 1u;
			float da = BoxNear(o, invD, uNodes.n[a * 2u].xyz, uNodes.n[a * 2u + 1u].xyz, bestT);
			float db = BoxNear(o, invD, uNodes.n[b * 2u].xyz, uNodes.n[b * 2u + 1u].xyz, bestT);
			if (da > db) { uint s = a; a = b; b = s; float f = da; da = db; db = f; }
			if (da < 1.0e38)
			{
				if (db < 1.0e38 && depth < 48u) stack[depth++] = b;
				node = a;
				continue;
			}
		}
		if (depth == 0u) break;
		node = stack[--depth];
	}
	return found;
#endif
}

vec3 WorldAt(ivec2 px, ivec2 size)
{
	float d = texelFetch(sampler2D(uDepth, uMaterialSampler), clamp(px, ivec2(0), size - 1), 0).r;
	vec2 uv = (vec2(px) + 0.5) / vec2(size);
	vec3 v = ViewPointAt(vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0), DepthDistance(max(d, 1e-7)));
	return transpose(mat3(uFrame.view)) * (v - uFrame.view[3].xyz);
}

void main()
{
	ivec2 size = textureSize(sampler2D(uDepth, uMaterialSampler), 0);
	ivec2 px = clamp(ivec2(vUV * vec2(size)), ivec2(0), size - 1);
	float depth = texelFetch(sampler2D(uDepth, uMaterialSampler), px, 0).r;
	if (depth <= 0.0 || uDraw.params.y < 0.5) { outColor = vec4(0.0); return; }
	vec3 p = WorldAt(px, size);
	vec3 px0 = WorldAt(px - ivec2(1, 0), size), px1 = WorldAt(px + ivec2(1, 0), size);
	vec3 py0 = WorldAt(px - ivec2(0, 1), size), py1 = WorldAt(px + ivec2(0, 1), size);
	vec3 dx = length(px1 - p) < length(p - px0) ? px1 - p : p - px0;
	vec3 dy = length(py1 - p) < length(p - py0) ? py1 - p : p - py0;
	vec3 n = normalize(cross(dy, dx));
	vec3 v = ToEye(p);
	if (dot(n, v) < 0.0) n = -n;
	vec3 r = reflect(-v, n);
	float dist = length(uFrame.cameraPosition.xyz - p);
	vec3 origin = p + n * (0.01 + dist * 0.0015);
	float t; uint tri; vec2 uv;
	if (!Trace(origin, r, uDraw.params.x, false, t, tri, uv)) { outColor = vec4(0.0); return; }
	vec3 hit = origin + r * t;
	// Where the screen shows what it found - in front, and nothing nearer there - the picture's own light there: its
	// textures, its shadows, all its lights (last frame's: a frame behind, what the screen's reflections read too)
	vec4 clip = uFrame.viewProjection * vec4(hit, 1.0);
	if (clip.w > 0.0)
	{
		vec2 suv = vec2(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
		if (all(greaterThan(suv, vec2(0.002))) && all(lessThan(suv, vec2(0.998))))
		{
			float there = DepthDistance(texelFetch(sampler2D(uDepth, uMaterialSampler), ivec2(suv * vec2(size)), 0).r);
			float hitDist = -(uFrame.view * vec4(hit, 1.0)).z;
			if (abs(there - hitDist) < 0.03 * hitDist + 0.1)
			{
				// (last frame's picture: where the hit was in it)
				vec2 puv = PreviousUV(hit);
				if (puv.x >= 0.0)
				{
					ivec2 psize = textureSize(sampler2D(uPicture, uMaterialSampler), 0);
					outColor = vec4(texelFetch(sampler2D(uPicture, uMaterialSampler), clamp(ivec2(puv * vec2(psize)), ivec2(0), psize - 1), 0).rgb, 1.0);
					return;
				}
			}
		}
	}
	// Off the screen: its material, lit by the sun (if the sun sees it) and the sky - by its smooth normal where the
	// triangles are known here, else as if it faced the ray
#ifdef RAYQUERY
	vec3 hn = -r;
	uint mat = min(tri, uint(max(uDraw.params.z - 1.0, 0.0)));
#else
	vec3 n0 = uTriangles.t[tri * 9u + 3u].xyz, n1 = uTriangles.t[tri * 9u + 4u].xyz, n2 = uTriangles.t[tri * 9u + 5u].xyz;
	vec3 hn = normalize(n0 * (1.0 - uv.x - uv.y) + n1 * uv.x + n2 * uv.y);
	if (dot(hn, r) > 0.0) hn = -hn;
	uint mat = min(floatBitsToUint(uTriangles.t[tri * 9u + 6u].x), uint(max(uDraw.params.z - 1.0, 0.0)));
#endif
	vec3 albedo = uMaterials.m[mat * 2u].rgb, glow = uMaterials.m[mat * 2u + 1u].rgb;
	vec3 light = vec3(0.0);
	if (uFrame.sunDirection.w > 0.0)
	{
		vec3 sun = normalize(uFrame.sunDirection.xyz);
		float ndl = dot(hn, sun);
		float st; uint stri; vec2 suv;
		if (ndl > 0.0 && !Trace(hit + hn * 0.02, sun, uDraw.params.x, true, st, stri, suv)) light += uFrame.sunRadiance.rgb * ndl / PI;
	}
	light += uFrame.frame.z > 0.5 ? SkyIrradiance(hn) / PI : uFrame.ambient.rgb;
	outColor = vec4(albedo * light + glow, 1.0);
}
