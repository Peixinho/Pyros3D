// DDGI probe update, GPU side. Three entry points selected by
// DDGI_STAGE, so the traversal include and the layout below are shared
// rather than triplicated:
//
//   DDGI_STAGE_TRACE      one thread per (probe, ray)   - trace + shade
//   DDGI_STAGE_IRRADIANCE one thread per (probe, texel) - cosine gather
//   DDGI_STAGE_VISIBILITY one thread per (probe, texel) - distance moments
//
// Deliberately mirrors DDGIVolume's CPU implementation step for step -
// same Fibonacci directions, same lobe exponents, same blend - because
// that class is the reference this is verified against, and a
// difference that is a deliberate optimisation here is indistinguishable
// from a bug when the two disagree.

layout(std430, binding = 0) buffer Tris    { vec4 tris[]; };
layout(std430, binding = 1) buffer Nodes   { vec4 nodes[]; };
layout(std430, binding = 2) buffer Idx     { uint indices[]; };
layout(std430, binding = 3) buffer Mats    { vec4 materials[]; };   // 2 vec4: albedo, emissive
layout(std430, binding = 4) buffer Lights  { vec4 lights[]; };      // 2 vec4: posOrDir+isPoint, color+range
layout(std430, binding = 5) buffer RayOut  { vec4 rayData[]; };     // 2 vec4: radiance, distance
layout(std430, binding = 6) buffer IrrOut  { vec4 irradiance[]; };  // one vec4 per atlas texel
layout(std430, binding = 7) buffer VisOut  { vec4 visibility[]; };  // xy used
layout(std430, binding = 8) buffer Params  { vec4 params[]; };

// params[0] = origin.xyz,          probeCountTotal
// params[1] = spacing.xyz,         raysPerProbe
// params[2] = counts.xyz,          maxRayDistance
// params[3] = skyColor.rgb,        hysteresis
// params[4] = irrRes, visRes, irrProbesPerRow, visProbesPerRow
// params[5] = rotation, lightCount, probeOffset, probesThisDispatch

#define P_ORIGIN      params[0].xyz
#define P_TOTAL       params[0].w
#define P_SPACING     params[1].xyz
#define P_RAYS        params[1].w
#define P_COUNTS      params[2].xyz
#define P_MAXDIST     params[2].w
#define P_SKY         params[3].rgb
#define P_HYSTERESIS  params[3].w
#define P_IRRRES      params[4].x
#define P_VISRES      params[4].y
#define P_IRRPERROW   params[4].z
#define P_VISPERROW   params[4].w
#define P_ROTATION    params[5].x
#define P_LIGHTCOUNT  params[5].y
#define P_OFFSET      params[5].z
#define P_BATCH       params[5].w

vec3 p3d_ProbePosition(uint probe)
{
    float nx = P_COUNTS.x, ny = P_COUNTS.y;
    float z = floor(float(probe) / (nx * ny));
    float rem = float(probe) - z * nx * ny;
    float y = floor(rem / nx);
    float x = rem - y * nx;
    return P_ORIGIN + P_SPACING * vec3(x, y, z);
}

// Matches DDGIVolume::SphericalFibonacci exactly.
vec3 p3d_SphericalFibonacci(uint index, float count, float rotation)
{
    float i = float(index) + 0.5;
    float phi = 6.283185307179586 * (i * 0.618033988749895 + rotation);
    float cosTheta = 1.0 - 2.0 * i / count;
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Closest hit through the shared BVH walk. Returns t < 0 for a miss.
float p3d_TraceClosest(vec3 o, vec3 d, float tMax, out uint outTri, out float outU, out float outV)
{
    vec3 invD = 1.0 / d;
    float best = tMax;
    float hitT = -1.0;
    outTri = 0u; outU = 0.0; outV = 0.0;
    uint stack[64]; uint depth = 0u; uint cur = 0u;
    while (true) {
        uint nb = cur * NODE_STRIDE;
        vec4 n0 = nodes[nb + 0u];
        vec4 n1 = nodes[nb + 1u];
        uint first = floatBitsToUint(n0.w);
        uint count = floatBitsToUint(n1.w);
        if (count > 0u) {
            for (uint i = 0u; i < count; i++) {
                uint ti = indices[first + i];
                uint tb = ti * TRI_STRIDE;
                float t, u, v;
                if (p3d_IntersectTriangle(o, d, tris[tb].xyz, tris[tb+1u].xyz, tris[tb+2u].xyz, t, u, v)
                    && t > 0.0001 && t < best) {
                    best = t; hitT = t; outTri = ti; outU = u; outV = v;
                }
            }
        } else {
            uint na = first, nf = first + 1u;
            float dn = p3d_IntersectAABB(o, invD, nodes[na*NODE_STRIDE].xyz, nodes[na*NODE_STRIDE+1u].xyz, best);
            float df = p3d_IntersectAABB(o, invD, nodes[nf*NODE_STRIDE].xyz, nodes[nf*NODE_STRIDE+1u].xyz, best);
            if (dn > df) { float tt = dn; dn = df; df = tt; uint ts = na; na = nf; nf = ts; }
            if (dn < 3.0e38) {
                if (df < 3.0e38 && depth < 64u) { stack[depth] = nf; depth++; }
                cur = na; continue;
            }
        }
        if (depth == 0u) break;
        depth--; cur = stack[depth];
    }
    return hitT;
}

// Any-hit, for shadow rays - cheaper than closest-hit because it can
// stop at the first blocker.
bool p3d_TraceOccluded(vec3 o, vec3 d, float tMax)
{
    vec3 invD = 1.0 / d;
    uint stack[64]; uint depth = 0u; uint cur = 0u;
    while (true) {
        uint nb = cur * NODE_STRIDE;
        vec4 n0 = nodes[nb + 0u];
        vec4 n1 = nodes[nb + 1u];
        uint first = floatBitsToUint(n0.w);
        uint count = floatBitsToUint(n1.w);
        if (count > 0u) {
            for (uint i = 0u; i < count; i++) {
                uint ti = indices[first + i];
                uint tb = ti * TRI_STRIDE;
                float t, u, v;
                if (p3d_IntersectTriangle(o, d, tris[tb].xyz, tris[tb+1u].xyz, tris[tb+2u].xyz, t, u, v)
                    && t > 0.0001 && t < tMax) return true;
            }
        } else {
            uint na = first, nf = first + 1u;
            float dn = p3d_IntersectAABB(o, invD, nodes[na*NODE_STRIDE].xyz, nodes[na*NODE_STRIDE+1u].xyz, tMax);
            float df = p3d_IntersectAABB(o, invD, nodes[nf*NODE_STRIDE].xyz, nodes[nf*NODE_STRIDE+1u].xyz, tMax);
            if (dn > df) { float tt = dn; dn = df; df = tt; uint ts = na; na = nf; nf = ts; }
            if (dn < 3.0e38) {
                if (df < 3.0e38 && depth < 64u) { stack[depth] = nf; depth++; }
                cur = na; continue;
            }
        }
        if (depth == 0u) break;
        depth--; cur = stack[depth];
    }
    return false;
}

// Mirrors DDGIVolume::ShadeHit.
vec3 p3d_ShadeHit(uint tri, float t, vec3 o, vec3 d, float u, float v)
{
    uint tb = tri * TRI_STRIDE;
    vec3 point = o + d * t;
    vec3 n = normalize(tris[tb+3u].xyz * (1.0 - u - v) + tris[tb+4u].xyz * u + tris[tb+5u].xyz * v);
    // Face the ray: a probe ray can strike either side of a wall, and
    // lighting the wrong side lights a room through its own wall.
    if (dot(n, d) > 0.0) n = -n;

    uint mi = floatBitsToUint(tris[tb+6u].x);
    vec3 albedo = materials[mi * 2u + 0u].rgb;
    vec3 outgoing = materials[mi * 2u + 1u].rgb;

    for (uint l = 0u; l < uint(P_LIGHTCOUNT); l++) {
        vec4 l0 = lights[l * 2u + 0u];
        vec4 l1 = lights[l * 2u + 1u];
        vec3 toLight; float dist;
        if (l0.w > 0.5) {
            toLight = l0.xyz - point;
            dist = length(toLight);
            if (dist < 1e-5) continue;
            toLight /= dist;
        } else {
            toLight = normalize(-l0.xyz);
            dist = P_MAXDIST;
        }
        float ndotl = dot(n, toLight);
        if (ndotl <= 0.0) continue;
        // Offset along the NORMAL, not the ray - along the ray leaves a
        // grazing hit inside its own triangle and the surface shadows
        // itself, which reads as uniform darkening.
        if (p3d_TraceOccluded(point + n * 1e-3, toLight, dist - 1e-3)) continue;
        float atten = 1.0;
        if (l0.w > 0.5 && l1.w > 0.0) {
            float k = max(0.0, 1.0 - dist / l1.w);
            atten = k * k;
        }
        outgoing += albedo * l1.rgb * (ndotl * atten);
    }
    return outgoing;
}

layout(local_size_x = 64) in;

void main()
{
    uint gid = gl_GlobalInvocationID.x;

#if defined(DDGI_STAGE_TRACE)
    uint rays = uint(P_RAYS);
    if (gid >= uint(P_BATCH) * rays) return;
    uint localProbe = gid / rays;
    uint ray = gid - localProbe * rays;
    uint probe = uint(P_OFFSET) + localProbe;
    if (float(probe) >= P_TOTAL) return;

    vec3 p = p3d_ProbePosition(probe);
    vec3 dir = p3d_SphericalFibonacci(ray, P_RAYS, P_ROTATION);

    uint tri; float u, v;
    float t = p3d_TraceClosest(p, dir, P_MAXDIST, tri, u, v);
    vec3 radiance = (t < 0.0) ? P_SKY : p3d_ShadeHit(tri, t, p, dir, u, v);
    // A miss records "nothing out to here", not zero - zero would claim
    // a wall at the probe itself and fail every Chebyshev test.
    float distance = (t < 0.0) ? P_MAXDIST : t;
    rayData[gid * 2u + 0u] = vec4(radiance, 0.0);
    rayData[gid * 2u + 1u] = vec4(distance, 0.0, 0.0, 0.0);

#elif defined(DDGI_STAGE_IRRADIANCE)
    uint res = uint(P_IRRRES);
    uint perTile = res * res;
    if (gid >= uint(P_BATCH) * perTile) return;
    uint localProbe = gid / perTile;
    uint texel = gid - localProbe * perTile;
    uint probe = uint(P_OFFSET) + localProbe;
    if (float(probe) >= P_TOTAL) return;

    uint tx = texel % res, ty = texel / res;
    vec3 texelDir = p3d_OctDecodeTexel(tx, ty, res);

    uint rays = uint(P_RAYS);
    vec3 sum = vec3(0.0); float wsum = 0.0;
    for (uint r = 0u; r < rays; r++) {
        uint base = (localProbe * rays + r) * 2u;
        vec3 rd = p3d_SphericalFibonacci(r, P_RAYS, P_ROTATION);
        float w = max(0.0, dot(texelDir, rd));
        if (w <= 0.0) continue;
        sum += rayData[base].rgb * w;
        wsum += w;
    }
    if (wsum > 1e-6) sum /= wsum;

    uint tile = res + 2u;
    uint perRow = uint(P_IRRPERROW);
    uint px = probe % perRow, py = probe / perRow;
    uint width = perRow * tile;
    uint ax = px * tile + 1u + tx, ay = py * tile + 1u + ty;
    uint o = ay * width + ax;
    vec3 prev = irradiance[o].rgb;
    irradiance[o] = vec4(mix(sum, prev, P_HYSTERESIS), 0.0);

#elif defined(DDGI_STAGE_VISIBILITY)
    uint res = uint(P_VISRES);
    uint perTile = res * res;
    if (gid >= uint(P_BATCH) * perTile) return;
    uint localProbe = gid / perTile;
    uint texel = gid - localProbe * perTile;
    uint probe = uint(P_OFFSET) + localProbe;
    if (float(probe) >= P_TOTAL) return;

    uint tx = texel % res, ty = texel / res;
    vec3 texelDir = p3d_OctDecodeTexel(tx, ty, res);

    uint rays = uint(P_RAYS);
    float mean = 0.0, mean2 = 0.0, wsum = 0.0;
    for (uint r = 0u; r < rays; r++) {
        uint base = (localProbe * rays + r) * 2u;
        vec3 rd = p3d_SphericalFibonacci(r, P_RAYS, P_ROTATION);
        // cos^4, much tighter than irradiance uses: visibility has to
        // resolve which side of a wall EDGE a direction falls on, and a
        // wide lobe smears the wall's distance into the open direction
        // beside it - which is the leak coming back.
        float w = max(0.0, dot(texelDir, rd));
        w = w * w * w * w;
        if (w <= 1e-6) continue;
        float dd = min(rayData[base + 1u].x, P_MAXDIST);
        mean += dd * w;
        mean2 += dd * dd * w;
        wsum += w;
    }
    if (wsum > 1e-6) { mean /= wsum; mean2 /= wsum; }
    else { mean = P_MAXDIST; mean2 = P_MAXDIST * P_MAXDIST; }

    uint tile = res + 2u;
    uint perRow = uint(P_VISPERROW);
    uint px = probe % perRow, py = probe / perRow;
    uint width = perRow * tile;
    uint ax = px * tile + 1u + tx, ay = py * tile + 1u + ty;
    uint o = ay * width + ax;
    vec2 prev = visibility[o].xy;
    visibility[o] = vec4(mix(vec2(mean, mean2), prev, P_HYSTERESIS), 0.0, 0.0);
#endif
}
