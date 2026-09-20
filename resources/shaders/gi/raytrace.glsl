// BVH traversal, GPU side. Deliberately the same algorithm as
// RayScene::Intersect on the CPU - explicit stack, nearest child first,
// same slab test - because that function is the reference this is
// verified against (tools/tests/bvh_gpu.cpp). Being cleverer here would
// make a disagreement ambiguous.
//
// Layout notes, all of which are things std430 gets to decide and we do
// not:
//   - A vec3 in std430 is 16-byte aligned, so the C++ side pads every
//     structure to vec4 boundaries explicitly rather than relying on
//     matching compiler behaviour. Triangles arrive as 9 vec4s and nodes
//     as 2 vec4s; the unpacking below mirrors RayScene's packing exactly.
//   - Indices are uint, and the node's `count` field doubles as the
//     leaf/interior discriminator: non-zero means a leaf.

struct GPUHit {
    float t;
    uint  tri;
    float u, v;
    bool  hit;
};

// Triangle i occupies 9 consecutive vec4s:
//   0,1,2 = v0.xyz, v1.xyz, v2.xyz
//   3,4,5 = n0.xyz, n1.xyz, n2.xyz
//   6     = (materialIndex, 0, 0, 0)
// The remaining two are spare, kept so the stride is a power of two
// multiple and indexing is a shift rather than a multiply-by-nine.
const uint TRI_STRIDE = 9u;

// Node i occupies 2 vec4s:
//   0 = boundsMin.xyz, firstOrLeft (as uintBitsToFloat)
//   1 = boundsMax.xyz, count       (as uintBitsToFloat)
const uint NODE_STRIDE = 2u;

bool p3d_IntersectTriangle(vec3 o, vec3 d, vec3 v0, vec3 v1, vec3 v2,
                           out float t, out float u, out float v)
{
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 p  = cross(d, e2);
    float det = dot(e1, p);
    // No backface rejection: indirect light bounces off whatever it
    // hits, and single-sidedness is a rasterization convention.
    if (abs(det) < 1e-9) return false;
    float invDet = 1.0 / det;
    vec3 tv = o - v0;
    u = dot(tv, p) * invDet;
    if (u < 0.0 || u > 1.0) return false;
    vec3 q = cross(tv, e1);
    v = dot(d, q) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;
    t = dot(e2, q) * invDet;
    return true;
}

// Returns the near distance, or a huge value for a miss. Matching the
// CPU's FLT_MAX sentinel rather than a bool so the caller can order the
// two children by it.
float p3d_IntersectAABB(vec3 o, vec3 invD, vec3 bmin, vec3 bmax, float tMax)
{
    vec3 t1 = (bmin - o) * invD;
    vec3 t2 = (bmax - o) * invD;
    vec3 lo = min(t1, t2);
    vec3 hi = max(t1, t2);
    float tmin = max(max(lo.x, lo.y), lo.z);
    float tmaxv = min(min(hi.x, hi.y), hi.z);
    if (tmaxv >= tmin && tmin < tMax && tmaxv > 0.0)
        return max(tmin, 0.0);
    return 3.402823466e+38;
}
