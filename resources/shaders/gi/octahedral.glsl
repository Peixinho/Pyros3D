// Octahedral mapping, GPU side. Mirrors Octahedral.cpp exactly - the
// probe atlases are written by these and read by PyrosShader.glsl's own
// copy, so a divergence puts the irradiance for one direction in the
// texel another direction reads, which looks like probe noise.

vec2 p3d_OctEncodeDir(vec3 d)
{
    float l1 = abs(d.x) + abs(d.y) + abs(d.z);
    if (l1 < 1e-20) return vec2(0.0);
    vec3 n = d / l1;
    if (n.z < 0.0) {
        float ox = n.x;
        n.x = (1.0 - abs(n.y)) * (ox  >= 0.0 ? 1.0 : -1.0);
        n.y = (1.0 - abs(ox))  * (n.y >= 0.0 ? 1.0 : -1.0);
    }
    return n.xy;
}

vec3 p3d_OctDecodeDir(vec2 oct)
{
    vec3 v = vec3(oct.x, oct.y, 1.0 - abs(oct.x) - abs(oct.y));
    if (v.z < 0.0) {
        float ox = v.x;
        v.x = (1.0 - abs(v.y)) * (ox  >= 0.0 ? 1.0 : -1.0);
        v.y = (1.0 - abs(ox))  * (v.y >= 0.0 ? 1.0 : -1.0);
    }
    return normalize(v);
}

// The direction interior texel (tx, ty) represents. The +0.5 is the
// texel CENTRE - without it every probe is biased by half a texel,
// which is a systematic rotation of the whole environment rather than
// noise, and correspondingly hard to spot.
vec3 p3d_OctDecodeTexel(uint tx, uint ty, uint res)
{
    float u = (float(tx) + 0.5) / float(res) * 2.0 - 1.0;
    float v = (float(ty) + 0.5) / float(res) * 2.0 - 1.0;
    return p3d_OctDecodeDir(vec2(u, v));
}
