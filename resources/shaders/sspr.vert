#version 450
#include "common.glsl"
// Screen-space planar reflection (SceneRenderer::SetSkyMirrorLevel - a sea's still water, a lake): every pixel of the
// scene above the mirror's plane drawn as a point where its mirror image falls on the screen, its colour last frame's
// picture there. Where several land on one pixel the one nearest the eye wins, as a mirror shows it (the depth below - by
// height above the plane, a palm far behind a ship came out in front of its hull's reflection). Seen from a boat, the
// shore, a ship's hull and the mountains in the water at a glance where marching the screen finds almost nothing.
layout(set = 1, binding = 1) uniform texture2D uDepth;       // (the prepass's: reversed Z)
layout(set = 1, binding = 2) uniform texture2D uPrevious;    // (last frame's picture: the colours)
layout(set = 1, binding = 3) uniform sampler uClamp;
layout(location = 0) out vec4 vColor;     // (rgb its colour, a how far its mirror image is from the eye)
void main()
{
	ivec2 size = textureSize(sampler2D(uDepth, uClamp), 0);
	int index = gl_VertexIndex;
	ivec2 px = ivec2(index % size.x, index / size.x);
	gl_PointSize = 1.0;
	vColor = vec4(0.0);
	gl_Position = vec4(2.0, 2.0, 2.0, 1.0);     // (nothing: off the screen)
	float d = texelFetch(sampler2D(uDepth, uClamp), min(px, size - 1), 0).r;
	if (d <= 0.0) return;                         // (the sky: mirrored as the sky, by the surface itself)
	float level = uFrame.temporal.y;
	vec2 uv = (vec2(px) + 0.5) / vec2(size);
	vec3 vp = ViewPointAt(vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0), DepthDistance(d));
	vec3 q = transpose(mat3(uFrame.view)) * (vp - uFrame.view[3].xyz);
	float above = q.y - level;
	// (under the water, or the water itself - its depth's precision wider far off: the water mirrored into itself showed its
	// own reflection again under the first, a ladder of them down the screen)
	if (above < 0.005 + 0.0005 * length(vp)) return;
	// (nor what lies flat just over it - the water's own surface, its waves: their depth is not exact far off, and a
	// flat thing mirrored nearly onto itself fed last frame's reflection back into this one's - a ladder of them, and the
	// sky's mirror greyed out. Its flatness from the depth's neighbours, a texel on)
	if (above < 1.5)
	{
		// (each axis' nearer neighbour of the two, as the AO does: at a thing's foot the one below is the ground, and a
		// normal across the two came out flat - the bottom of every wall dropped from its reflection, a band at its foot)
		vec3 nb[4];
		ivec2 off[4] = ivec2[](ivec2(2, 0), ivec2(-2, 0), ivec2(0, 2), ivec2(0, -2));
		bool ok = true;
		for (int k = 0; k < 4; k++)
		{
			ivec2 sp = clamp(px + off[k], ivec2(0), size - 1);
			float dk = texelFetch(sampler2D(uDepth, uClamp), sp, 0).r;
			if (dk <= 0.0) { ok = false; break; }
			vec2 uk = (vec2(sp) + 0.5) / vec2(size);
			nb[k] = ViewPointAt(vec2(uk.x * 2.0 - 1.0, 1.0 - uk.y * 2.0), DepthDistance(dk));
		}
		if (ok)
		{
			vec3 ddx = abs(nb[0].z - vp.z) < abs(vp.z - nb[1].z) ? nb[0] - vp : vp - nb[1];
			vec3 ddy = abs(nb[2].z - vp.z) < abs(vp.z - nb[3].z) ? nb[2] - vp : vp - nb[3];
			vec3 nw = transpose(mat3(uFrame.view)) * normalize(cross(ddy, ddx));
			if (abs(nw.y) > 0.45) return;     // (a wave's flank too: its slopes are a good way off flat - a wall's are not)
			// (and a wave's crest off in the distance, where the depth's neighbours are metres apart and its slope is not
			// told well: within a band over the plane growing with the distance, anything not plainly upright left out -
			// mirrored, the far crests came back as a scatter of the sea's colour in blocks. Upright, a wall's foot: kept -
			// left out too, every box on a still mirror stood on a pale band where its foot's image should be)
			if (abs(nw.y) > 0.3 && above < 0.004 * length(vp)) return;
		}
	}
	vec2 puv = PreviousUV(q);
	if (puv.x < 0.0) return;
	vec4 clip = uFrame.viewProjection * vec4(q.x, level - above, q.z, 1.0);
	if (clip.w <= 0.0) return;
	// (as depth: how far its mirror image is from the eye - the nearest drawn over the rest)
	float dist = distance(uFrame.cameraPosition.xyz, vec3(q.x, level - above, q.z));
	vColor = vec4(textureLod(sampler2D(uPrevious, uClamp), puv, 0.0).rgb, max(dist, 0.01));
	gl_Position = vec4(clip.xy / clip.w, clamp(dist / 20000.0, 0.0, 1.0), 1.0);
}
