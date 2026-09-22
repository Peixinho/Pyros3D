// DDGI probe update, GPU side. Three entry points selected by
// DDGI_STAGE, so the traversal include and the layout below are shared
// rather than triplicated:
//
//   DDGI_STAGE_TRACE      one thread per (probe, ray)   - trace + shade
//   DDGI_STAGE_IRRADIANCE one thread per (probe, texel) - cosine gather
//   DDGI_STAGE_VISIBILITY one thread per (probe, texel) - distance moments
//   DDGI_STAGE_RADIANCE   one thread per (probe, level, texel) - GGX gather
//   DDGI_STAGE_STATS      one thread per probe - relocation statistics
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
// TWICE the atlas: [0, irrTexels) is what this update writes, and
// [irrTexels, 2*irrTexels) is the snapshot multi-bounce feeds back
// from. One buffer rather than two so no new binding point is needed -
// see P_IRRTEXELS.
layout(std430, binding = 6) buffer IrrOut  { vec4 irradiance[]; };
layout(std430, binding = 7) buffer VisOut  { vec4 visibility[]; };  // xy used
layout(std430, binding = 8) buffer Params  { vec4 params[]; };
layout(std430, binding = 9) buffer RadOut  { vec4 radiance[]; };   // prefiltered, levels stacked
// Three vec4 per probe in the batch, written by DDGI_STAGE_STATS and
// read straight back by the CPU, which makes the relocation DECISION -
// see DDGIVolume::ProbeRayStats for why that lives in one place.
layout(std430, binding = 10) buffer StatsOut { vec4 stats[]; };

// params[0] = origin.xyz,          probeCountTotal
// params[1] = spacing.xyz,         raysPerProbe
// params[2] = counts.xyz,          maxRayDistance
// params[3] = skyColor.rgb,        hysteresis
// params[4] = irrRes, visRes, irrProbesPerRow, visProbesPerRow
// params[5] = rotation, lightCount, probeOffset, probesThisDispatch
// params[6] = radRes, radProbesPerRow, radLevels, minRoughness
// params[7] = multiBounce, feedbackNormalBias, irrTexels, unused
// params[8 + probe] = that probe's relocation offset (xyz) and active
//   flag (w). CPU-owned - the decision is made there - so it rides the
//   params upload rather than needing a buffer of its own.

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
#define P_RADRES      params[6].x
#define P_RADPERROW   params[6].y
#define P_RADLEVELS   params[6].z
#define P_MINROUGH    params[6].w
#define P_MULTIBOUNCE params[7].x
#define P_NORMALBIAS  params[7].y
#define P_IRRTEXELS   params[7].z
#define P_PROBEDATA(i) params[8u + (i)]

// Matches DDGIVolume::LevelRoughness. Level 0 is not roughness zero -
// see the comment there; a lobe narrower than the angle between
// neighbouring rays gathers one ray and returns noise.
float p3d_LevelRoughness(uint level, float levels)
{
    if (levels <= 1.0) return 1.0;
    float t = float(level) / (levels - 1.0);
    return P_MINROUGH + (1.0 - P_MINROUGH) * t;
}

// Where the probe actually is: its cell plus whatever offset
// relocation gave it. Mirrors DDGIVolume::ProbePosition - a trace that
// used the unrelocated position would fill the atlas for a place the
// probe is not.
vec3 p3d_ProbePosition(uint probe)
{
    float nx = P_COUNTS.x, ny = P_COUNTS.y;
    float z = floor(float(probe) / (nx * ny));
    float rem = float(probe) - z * nx * ny;
    float y = floor(rem / nx);
    float x = rem - y * nx;
    return P_ORIGIN + P_SPACING * vec3(x, y, z) + P_PROBEDATA(probe).xyz;
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

// Irradiance already in the volume at a point, from the snapshot half
// of the buffer. Mirrors DDGIVolume::SampleIrradianceIn step for step -
// same trilinear weights, same backface term, same Chebyshev test, same
// NEAREST texel lookup (the CPU indexes texels directly, so filtering
// here would be a difference the parity test could not explain away).
vec3 p3d_FeedbackIrradiance(vec3 wp, vec3 n)
{
    vec3 counts = P_COUNTS;
    vec3 g = (wp - P_ORIGIN) / P_SPACING;
    g = clamp(g, vec3(0.0), counts - vec3(1.0));
    vec3 baseF = min(floor(g), counts - vec3(2.0));
    vec3 frac = g - baseF;

    float irrRes = P_IRRRES;
    float visRes = P_VISRES;
    uint irrTile = uint(irrRes) + 2u;
    uint visTile = uint(visRes) + 2u;
    uint irrPerRow = uint(P_IRRPERROW);
    uint visPerRow = uint(P_VISPERROW);
    uint irrWidth = irrPerRow * irrTile;
    uint visWidth = visPerRow * visTile;
    uint base = uint(P_IRRTEXELS);

    // The surface's own normal picks the texel, the same one for every
    // probe - it is the surface's normal, not the probe's.
    vec2 noct = p3d_OctEncodeDir(n);
    uint ix = uint(min(irrRes - 1.0, max(0.0, (noct.x * 0.5 + 0.5) * irrRes)));
    uint iy = uint(min(irrRes - 1.0, max(0.0, (noct.y * 0.5 + 0.5) * irrRes)));

    vec3 sum = vec3(0.0);
    float weightSum = 0.0;

    for (int c = 0; c < 8; c++)
    {
        vec3 offset = vec3(float(c & 1), float((c >> 1) & 1), float((c >> 2) & 1));
        vec3 pc = baseF + offset;
        if (any(greaterThan(pc, counts - vec3(1.0)))) continue;

        vec3 tri3 = mix(vec3(1.0) - frac, frac, offset);
        float weight = tri3.x * tri3.y * tri3.z;
        if (weight <= 0.0) continue;

        uint probe = uint((pc.z * counts.y + pc.y) * counts.x + pc.x);
        // A probe classified as enclosed contributes nothing - the
        // same rule DDGIVolume::GatherProbes applies.
        if (P_PROBEDATA(probe).w <= 0.5) continue;
        vec3 probePos = P_ORIGIN + P_SPACING * pc + P_PROBEDATA(probe).xyz;

        vec3 toProbe = probePos - wp;
        float dist = length(toProbe);
        if (dist > 1e-5) toProbe /= dist;

        float facing = (dot(toProbe, n) + 1.0) * 0.5;
        weight *= facing * facing + 0.2;

        vec2 voct = p3d_OctEncodeDir(-toProbe);
        uint vx = uint(min(visRes - 1.0, max(0.0, (voct.x * 0.5 + 0.5) * visRes)));
        uint vy = uint(min(visRes - 1.0, max(0.0, (voct.y * 0.5 + 0.5) * visRes)));
        uint vpx = probe % visPerRow, vpy = probe / visPerRow;
        vec2 moments = visibility[(vpy * visTile + 1u + vy) * visWidth + vpx * visTile + 1u + vx].xy;
        float mean = moments.x;
        float variance = max(moments.y - mean * mean, 0.0);
        if (dist > mean)
        {
            float diff = dist - mean;
            float cheb = variance / (variance + diff * diff);
            weight *= max(cheb * cheb * cheb, 0.0);
        }
        if (weight <= 1e-6) continue;

        uint ipx = probe % irrPerRow, ipy = probe / irrPerRow;
        uint o = (ipy * irrTile + 1u + iy) * irrWidth + ipx * irrTile + 1u + ix;
        sum += irradiance[base + o].rgb * weight;
        weightSum += weight;
    }
    if (weightSum <= 1e-6) return vec3(0.0);
    return sum / weightSum;
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

    // Multi-bounce. Offset along the normal first, or a hit on a thin
    // wall picks up the lit room on the other side and compounds it
    // once per update - see DDGIVolume::GetFeedbackNormalBias.
    if (P_MULTIBOUNCE > 0.0)
        outgoing += albedo * P_MULTIBOUNCE * p3d_FeedbackIrradiance(point + n * P_NORMALBIAS, n);

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
    // .y carries whether this ray struck a surface from BEHIND, which
    // is the signal relocation reads. Geometric normal from the
    // triangle's winding, not the interpolated vertex normals used for
    // shading: a smooth normal can point back toward the ray on a
    // curved surface seen edge-on and report a backface where there is
    // none.
    float backface = 0.0;
    if (t >= 0.0)
    {
        uint tb = tri * TRI_STRIDE;
        vec3 gn = cross(tris[tb+1u].xyz - tris[tb].xyz, tris[tb+2u].xyz - tris[tb].xyz);
        backface = (dot(gn, dir) > 0.0) ? 1.0 : 0.0;
    }
    rayData[gid * 2u + 0u] = vec4(radiance, 0.0);
    rayData[gid * 2u + 1u] = vec4(distance, backface, 0.0, 0.0);

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

#elif defined(DDGI_STAGE_RADIANCE)
    // The same rays as the irradiance gather, through the GGX
    // distribution instead of a cosine lobe - the specular half of the
    // split sum. No extra tracing: a second ray set for specular would
    // double the cost of the frame and the rays would be the same rays.
    uint res = uint(P_RADRES);
    uint levels = uint(P_RADLEVELS);
    uint perTile = res * res;
    uint perProbe = perTile * levels;
    if (levels == 0u || gid >= uint(P_BATCH) * perProbe) return;
    uint localProbe = gid / perProbe;
    uint rem = gid - localProbe * perProbe;
    uint level = rem / perTile;
    uint texel = rem - level * perTile;
    uint probe = uint(P_OFFSET) + localProbe;
    if (float(probe) >= P_TOTAL) return;

    uint tx = texel % res, ty = texel / res;
    // N = V = R, Karis' approximation - what lets one prefiltered map
    // serve every view direction.
    vec3 n = p3d_OctDecodeTexel(tx, ty, res);

    float rough = p3d_LevelRoughness(level, P_RADLEVELS);
    float a = rough * rough;
    float a2 = a * a;

    uint rays = uint(P_RAYS);
    vec3 sum = vec3(0.0);
    float wsum = 0.0;
    float bestDot = -1.0;
    uint bestRay = 0u;
    for (uint r = 0u; r < rays; r++) {
        vec3 rd = p3d_SphericalFibonacci(r, P_RAYS, P_ROTATION);
        float nDotL = dot(n, rd);
        if (nDotL > bestDot) { bestDot = nDotL; bestRay = r; }
        if (nDotL <= 0.0) continue;
        vec3 h = rd + n;
        float hlen = length(h);
        if (hlen < 1e-6) continue;
        h /= hlen;
        float nDotH = max(0.0, dot(n, h));
        float dd = (nDotH * nDotH) * (a2 - 1.0) + 1.0;
        float ndf = a2 / max(3.14159265358979 * dd * dd, 1e-8);
        float w = ndf * nDotL;
        if (w <= 1e-8) continue;
        sum += rayData[(localProbe * rays + r) * 2u].rgb * w;
        wsum += w;
    }
    if (wsum > 1e-8) sum /= wsum;
    else if (bestDot > 0.0)
        // The lobe fell between rays. Black here would punch a hole in
        // the reflection; the nearest ray is a poor estimate of the
        // right thing rather than a good estimate of nothing.
        sum = rayData[(localProbe * rays + bestRay) * 2u].rgb;

    // Levels are stacked: every probe at level 0, then every probe at
    // level 1. DDGIVolume::RadianceTile does the same arithmetic, and
    // the two have to agree or a rough surface samples a smooth
    // probe's tile.
    uint tile = level * uint(P_TOTAL) + probe;
    uint tsz = res + 2u;
    uint perRow = uint(P_RADPERROW);
    uint px = tile % perRow, py = tile / perRow;
    uint width = perRow * tsz;
    uint ax = px * tsz + 1u + tx, ay = py * tsz + 1u + ty;
    uint o = ay * width + ax;
    vec3 prev = radiance[o].rgb;
    radiance[o] = vec4(mix(sum, prev, P_HYSTERESIS), 0.0);

#elif defined(DDGI_STAGE_STATS)
    // One thread per probe: reduce its rays to the handful of numbers
    // the relocation decision needs, and hand them to the CPU. The
    // decision itself is NOT made here - DDGIVolume::ApplyRelocation
    // makes it for both backends, so the two cannot drift.
    if (gid >= uint(P_BATCH)) return;
    uint probe = uint(P_OFFSET) + gid;
    if (float(probe) >= P_TOTAL) return;

    uint rays = uint(P_RAYS);
    uint backfaces = 0u;
    float closestFront = 1e30;
    vec3 closestFrontDir = vec3(0.0);
    vec3 open = vec3(0.0);

    for (uint r = 0u; r < rays; r++)
    {
        vec4 d = rayData[(gid * rays + r) * 2u + 1u];
        if (d.y > 0.5) { backfaces++; continue; }
        vec3 rd = p3d_SphericalFibonacci(r, P_RAYS, P_ROTATION);
        if (d.x < closestFront) { closestFront = d.x; closestFrontDir = rd; }
        // Summed and weighted, not "the farthest one" - see
        // DDGIVolume::ProbeRayStats::openLength for why picking a
        // single ray put the two backends on different paths.
        open += rd * d.x;
    }

    float openLength = length(open);
    stats[gid * 3u + 0u] = vec4(float(backfaces) / float(rays), closestFront, openLength, 0.0);
    stats[gid * 3u + 1u] = vec4(closestFrontDir, 0.0);
    stats[gid * 3u + 2u] = vec4(openLength > 1e-6 ? open / openLength : vec3(0.0), 0.0);
#endif
}
