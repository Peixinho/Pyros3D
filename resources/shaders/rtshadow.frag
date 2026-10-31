#version 460
#ifdef RAYQUERY
#extension GL_EXT_ray_query : require
#endif
#include "common.glsl"
// Ray-traced sun shadows (SceneRenderer::SetRayTracedShadows): from each pixel's surface (the depth prepass) a ray toward
// the sun through the scene's triangles (RayScene's BVH, packed as PackForGPU lays it out), anything in the way and it is
// in shadow. Each frame's ray leaves for a different point of the sun's disc, so the temporal resolve gathers them into
// the penumbra the sun's size makes - sharp where the caster touches, soft further from it - which no shadow map has.
// Out: the ambient occlusion it is drawn with (half resolution, brought to this one) and the sun's share, red and green
// - what the surfaces read as their AO (surface.frag, uAO) while it is on.
layout(set = 1, binding = 1) uniform texture2D uDepth;       // (the prepass's: reversed Z, 0 where nothing was drawn)
layout(set = 1, binding = 2) uniform texture2D uAO;          // (GTAO's, half resolution)
layout(set = 1, binding = 3) uniform sampler uLinearClamp;
layout(set = 1, binding = 4, std430) readonly buffer Triangles { vec4 t[]; } uTriangles;     // (9 a triangle: v0 v1 v2, ...)
layout(set = 1, binding = 5, std430) readonly buffer Nodes { vec4 n[]; } uNodes;             // (2 a node: min + first/left, max + count)
layout(set = 1, binding = 6, std430) readonly buffer Indices { uint i[]; } uIndices;         // (a leaf's triangles)
#ifdef RAYQUERY
// (RAYQUERY: the GPU's own ray tracing - the frame's scene, SetRayScene's - in place of the BVH walked here)
layout(set = 2, binding = 0) uniform accelerationStructureEXT uRayScene;     // (its own set: the device's, the frame's scene)
#endif
layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 params;     // x: the sun's angular radius (rad), y: 1 when there are triangles, z: the frame (its noise), w: the longest ray (m)
} uDraw;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float Noise(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

float BoxNear(vec3 o, vec3 invD, vec3 lo, vec3 hi, float tMax)
{
	vec3 t1 = (lo - o) * invD, t2 = (hi - o) * invD;
	vec3 a = min(t1, t2), b = max(t1, t2);
	float near = max(max(a.x, a.y), a.z), far = min(min(b.x, b.y), b.z);
	return (far >= near && near < tMax && far > 0.0) ? max(near, 0.0) : 3.0e38;
}

bool HitsTriangle(vec3 o, vec3 d, uint tri, float tMax)
{
	vec3 v0 = uTriangles.t[tri * 9u].xyz, v1 = uTriangles.t[tri * 9u + 1u].xyz, v2 = uTriangles.t[tri * 9u + 2u].xyz;
	vec3 e1 = v1 - v0, e2 = v2 - v0, p = cross(d, e2);
	float det = dot(e1, p);
	if (abs(det) < 1e-9) return false;
	float inv = 1.0 / det;
	vec3 tv = o - v0;
	float u = dot(tv, p) * inv;
	if (u < 0.0 || u > 1.0) return false;
	vec3 q = cross(tv, e1);
	float v = dot(d, q) * inv;
	if (v < 0.0 || u + v > 1.0) return false;
	float t = dot(e2, q) * inv;
	return t > 1e-3 && t < tMax;
}

// Anything between o and tMax along d: the first hit is enough (a shadow ray asks no more)
bool Occluded(vec3 o, vec3 d, float tMax)
{
#ifdef RAYQUERY
	rayQueryEXT q;
	rayQueryInitializeEXT(q, uRayScene, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 0xFF, o, 1e-3, d, tMax);
	while (rayQueryProceedEXT(q)) {}
	return rayQueryGetIntersectionTypeEXT(q, true) != gl_RayQueryCommittedIntersectionNoneEXT;
#else
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
			for (uint k = 0u; k < count; k++) if (HitsTriangle(o, d, uIndices.i[first + k], tMax)) return true;
		}
		else
		{
			uint a = first, b = first + 1u;
			float da = BoxNear(o, invD, uNodes.n[a * 2u].xyz, uNodes.n[a * 2u + 1u].xyz, tMax);
			float db = BoxNear(o, invD, uNodes.n[b * 2u].xyz, uNodes.n[b * 2u + 1u].xyz, tMax);
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
	return false;
#endif
}

vec3 WorldAt(ivec2 px, ivec2 size)
{
	float d = texelFetch(sampler2D(uDepth, uLinearClamp), clamp(px, ivec2(0), size - 1), 0).r;
	vec2 uv = (vec2(px) + 0.5) / vec2(size);
	vec3 v = ViewPointAt(vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0), DepthDistance(max(d, 1e-7)));
	// (the view is a turn and a move: undone by the turn's transpose)
	return transpose(mat3(uFrame.view)) * (v - uFrame.view[3].xyz);
}

void main()
{
	ivec2 size = textureSize(sampler2D(uDepth, uLinearClamp), 0);
	ivec2 px = clamp(ivec2(vUV * vec2(size)), ivec2(0), size - 1);
	float ao = textureLod(sampler2D(uAO, uLinearClamp), vUV, 0.0).r;
	float depth = texelFetch(sampler2D(uDepth, uLinearClamp), px, 0).r;
	if (depth <= 0.0 || uDraw.params.y < 0.5 || uFrame.sunDirection.w <= 0.0) { outColor = vec4(ao, 1.0, 0.0, 1.0); return; }
	vec3 p = WorldAt(px, size);
	// The surface's facing, from its neighbours (the nearer pair each way: an edge does not bend it)
	vec3 px0 = WorldAt(px - ivec2(1, 0), size), px1 = WorldAt(px + ivec2(1, 0), size);
	vec3 py0 = WorldAt(px - ivec2(0, 1), size), py1 = WorldAt(px + ivec2(0, 1), size);
	vec3 dx = abs(px1.z - p.z) + abs(px1.x - p.x) < abs(p.z - px0.z) + abs(p.x - px0.x) ? px1 - p : p - px0;
	vec3 dy = abs(py1.y - p.y) + abs(py1.z - p.z) < abs(p.y - py0.y) + abs(p.z - py0.z) ? py1 - p : p - py0;
	vec3 n = normalize(cross(dy, dx));
	vec3 toEye = ToEye(p);
	if (dot(n, toEye) < 0.0) n = -n;
	vec3 sun = normalize(uFrame.sunDirection.xyz);
	// (facing away from the sun: in its own shade - no ray needed)
	if (dot(n, sun) <= 0.0) { outColor = vec4(ao, 0.0, 0.0, 1.0); return; }
	// A point of the sun's disc, a different one each pixel and frame
	float r1 = Noise(gl_FragCoord.xy + uDraw.params.z * 5.588238), r2 = Noise(gl_FragCoord.yx * 1.37 + uDraw.params.z * 3.1);
	vec3 up = abs(sun.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 t = normalize(cross(up, sun)), b = cross(sun, t);
	float radius = tan(uDraw.params.x) * sqrt(r1), angle = r2 * 6.2831853;
	vec3 dir = normalize(sun + (t * cos(angle) + b * sin(angle)) * radius);
	// (off the surface by a little more the further it is: the depth's own precision there)
	float dist = length(uFrame.cameraPosition.xyz - p);
	vec3 origin = p + n * (0.01 + dist * 0.0015);
	float lit = Occluded(origin, dir, uDraw.params.w) ? 0.0 : 1.0;
	outColor = vec4(ao, lit, 0.0, 1.0);
}
