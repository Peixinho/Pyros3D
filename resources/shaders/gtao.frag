#version 450
#include "common.glsl"
// Ground-truth ambient occlusion (Jimenez et al. 2016), at half resolution: for each pixel, a few slices
// through the view direction; in each the horizon on both sides from the depth, and the cosine-weighted visible
// arc between them, projected on the slice's normal
layout(set = 1, binding = 1) uniform texture2D uDepth;
layout(set = 1, binding = 2) uniform sampler uPointClamp;
layout(location = 0) in vec2 vUV;
// (depth read texel-exact, with no sampler: what a point sampler gave, and in WebGPU a depth texture is not filterable)
float DepthAt(vec2 uv)
{
	ivec2 size = textureSize(sampler2D(uDepth, uPointClamp), 0);
	return texelFetch(sampler2D(uDepth, uPointClamp), clamp(ivec2(uv * vec2(size)), ivec2(0), size - 1), 0).r;
}
layout(location = 0) out vec4 outAO;

const float kRadius = 1.0;           // metres
const int kSlices = 3, kSteps = 6;

// (at the centre of the depth texel the uv falls in: the x and y from the same place as the depth - on a surface
// seen at a grazing angle half a texel is a long way in depth, and samples fell above and below the plane)
vec2 Snap(vec2 uv) { return (floor(uv * uFrame.viewport.xy) + 0.5) * uFrame.viewport.zw; }

vec3 ViewPosition(vec2 uvAny)
{
	vec2 uv = Snap(uvAny);
	float d = DepthAt(uv);
	vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	return ViewPointAt(ndc, DepthDistance(d));
}

void main()
{
	vec2 texel = uFrame.viewport.zw;
	// (this half-resolution pixel's own depth texel, by index: its centre lies on a texel boundary, where a floor()
	// went either way column by column)
	vec2 vUV = (floor(gl_FragCoord.xy) * 2.0 + 0.5) * texel;
	float d0 = DepthAt(vUV);
	if (d0 <= 0.0) { outAO = vec4(1.0); return; }          // (the sky)
	vec3 p = ViewPosition(vUV);
	// The normal from the depth: the smaller of the two differences on each axis (no smearing over edges)
	vec3 px0 = ViewPosition(vUV - vec2(texel.x, 0.0)), px1 = ViewPosition(vUV + vec2(texel.x, 0.0));
	vec3 py0 = ViewPosition(vUV - vec2(0.0, texel.y)), py1 = ViewPosition(vUV + vec2(0.0, texel.y));
	vec3 dx = abs(px1.z - p.z) < abs(p.z - px0.z) ? px1 - p : p - px0;
	vec3 dy = abs(py1.z - p.z) < abs(p.z - py0.z) ? py1 - p : p - py0;
	vec3 n = normalize(cross(dy, dx));
	vec3 v = Orthographic() ? vec3(0.0, 0.0, 1.0) : normalize(-p);

	// The radius on screen, in pixels - a pixel is the same size in x and y, so a step in pixels stays in the
	// slice's plane (in uv it did not, off the axes, and the horizons came out wrong)
	float radiusPixels = kRadius * uFrame.projection.y * 0.5 * uFrame.viewport.y / (Orthographic() ? 1.0 : -p.z);
	if (radiusPixels < 3.0) { outAO = vec4(1.0); return; }
	radiusPixels = min(radiusPixels, 0.25 * uFrame.viewport.y);
	float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	float noise2 = fract(noise * 7.13 + 0.37);

	float visibility = 0.0;
	for (int s = 0; s < kSlices; s++)
	{
		float phi = (float(s) + noise) * PI / float(kSlices);
		vec2 dir = vec2(cos(phi), sin(phi));
		vec3 dir3 = vec3(dir, 0.0);
		vec3 ortho = dir3 - dot(dir3, v) * v;
		vec3 axis = normalize(cross(ortho, v));      // (the slice's axis - unit length, or the projected normal is wrong off the view axis)
		vec3 projN = n - axis * dot(n, axis);
		float projLen = length(projN);
		float cosN = clamp(dot(projN, v) / max(projLen, 1e-4), -1.0, 1.0);
		float nAngle = sign(dot(projN, ortho)) * acos(cosN);
		float h[2];
		for (int side = 0; side < 2; side++)
		{
			float sgn = side == 0 ? 1.0 : -1.0;
			float best = -1.0;
			for (int k = 0; k < kSteps; k++)
			{
				float t = (float(k) + noise2) / float(kSteps);
				// (at least two texels out: a nearer one snapped back along one axis only, off the slice)
				float pixels = max(2.0, t * t * radiusPixels);
				vec2 at = vUV + vec2(dir.x, -dir.y) * sgn * pixels * texel;
				if (any(lessThan(at, vec2(0.0))) || any(greaterThan(at, vec2(1.0)))) break;
				vec3 q = ViewPosition(at) - p;
				float len = length(q);
				if (len < 1e-3 * -p.z) continue;            // (the pixel's own texel: no direction, no horizon)
				float c = dot(q / len, v);
				// (farther than the radius counts less: no halo round objects in front of far ones)
				float falloff = clamp(1.0 - len * len / (kRadius * kRadius * 4.0), 0.0, 1.0);
				best = max(best, mix(-1.0, c, falloff));
			}
			h[side] = acos(clamp(best, -1.0, 1.0)) * sgn;      // (+ toward the slice's direction, - away)
		}
		// (h1 the side away from the direction, h2 toward it - each kept within the hemisphere round the normal)
		float h1 = nAngle + max(h[1] - nAngle, -PI * 0.5);
		float h2 = nAngle + min(h[0] - nAngle, PI * 0.5);
		float arc = 0.25 * (-cos(2.0 * h1 - nAngle) + cosN + 2.0 * h1 * sin(nAngle))
		          + 0.25 * (-cos(2.0 * h2 - nAngle) + cosN + 2.0 * h2 * sin(nAngle));
		visibility += projLen * arc;
	}
	outAO = vec4(clamp(visibility / float(kSlices), 0.0, 1.0));
}
