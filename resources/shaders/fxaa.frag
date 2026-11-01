#version 450
#include "common.glsl"
// Fast approximate anti-aliasing (Lottes' FXAA 3.11, its quality preset's shape): where the picture has an edge - a jump
// in brightness across a pixel - the edge's way found (across rows or columns), walked along each side until its end,
// and the pixel blended across it by how far it is from the nearer end. The anti-aliasing for who wants none of the
// temporal resolve's softness or trails (SceneRenderer::SetAntiAliasing). On the HDR picture: its brightness taken
// against the neighbourhood's (Y / (Y + their mean)), so the thresholds mean the same at any exposure.
layout(set = 1, binding = 1) uniform texture2D uPicture;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float gMean;
float Y(vec3 c) { return dot(max(c, vec3(0.0)), vec3(0.299, 0.587, 0.114)); }
float Luma(vec3 c) { float l = Y(c); return sqrt(l / (l + gMean + 1e-5)); }
vec3 At(vec2 uv) { return textureLod(sampler2D(uPicture, uLinearClamp), uv, 0.0).rgb; }

void main()
{
	vec2 texel = 1.0 / vec2(textureSize(sampler2D(uPicture, uLinearClamp), 0));
	vec3 rgbM = At(vUV);
	// (the neighbourhood's brightness: what this one is measured against)
	gMean = 0.0;
	for (int k = 0; k < 9; k++) gMean += Y(At(vUV + vec2(float(k % 3 - 1), float(k / 3 - 1)) * texel));
	gMean /= 9.0;
	float lM = Luma(rgbM);
	float lN = Luma(At(vUV + vec2(0.0, -texel.y))), lS = Luma(At(vUV + vec2(0.0, texel.y)));
	float lW = Luma(At(vUV + vec2(-texel.x, 0.0))), lE = Luma(At(vUV + vec2(texel.x, 0.0)));
	float lo = min(lM, min(min(lN, lS), min(lW, lE))), hi = max(lM, max(max(lN, lS), max(lW, lE)));
	float range = hi - lo;
	// (no edge to speak of: as it is)
	if (range < max(0.0312, hi * 0.125)) { outColor = vec4(rgbM, 1.0); return; }

	float lNW = Luma(At(vUV + vec2(-texel.x, -texel.y))), lNE = Luma(At(vUV + vec2(texel.x, -texel.y)));
	float lSW = Luma(At(vUV + vec2(-texel.x, texel.y))), lSE = Luma(At(vUV + vec2(texel.x, texel.y)));
	// The edge's way: along rows (a horizontal edge) or columns
	float horizontal = abs(lNW + lNE - 2.0 * lN) + 2.0 * abs(lW + lE - 2.0 * lM) + abs(lSW + lSE - 2.0 * lS);
	float vertical = abs(lNW + lSW - 2.0 * lW) + 2.0 * abs(lN + lS - 2.0 * lM) + abs(lNE + lSE - 2.0 * lE);
	bool isHorizontal = horizontal >= vertical;
	// Which side of the pixel the edge lies on: the steeper of the two neighbours across it
	float l1 = isHorizontal ? lN : lW, l2 = isHorizontal ? lS : lE;
	float g1 = abs(l1 - lM), g2 = abs(l2 - lM);
	bool steeper1 = g1 >= g2;
	float gradient = 0.25 * max(g1, g2);
	float step = isHorizontal ? texel.y : texel.x;
	float lEdge;
	if (steeper1) { step = -step; lEdge = 0.5 * (l1 + lM); } else lEdge = 0.5 * (l2 + lM);
	vec2 uv = vUV;
	if (isHorizontal) uv.y += step * 0.5; else uv.x += step * 0.5;
	// Walked along the edge each way until its brightness leaves the edge's
	vec2 along = isHorizontal ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
	const float kSteps[10] = float[](1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 4.0, 8.0);
	vec2 uv1 = uv - along, uv2 = uv + along;
	float e1 = Luma(At(uv1)) - lEdge, e2 = Luma(At(uv2)) - lEdge;
	bool done1 = abs(e1) >= gradient, done2 = abs(e2) >= gradient;
	for (int i = 1; i < 10 && !(done1 && done2); i++)
	{
		if (!done1) { uv1 -= along * kSteps[i]; e1 = Luma(At(uv1)) - lEdge; done1 = abs(e1) >= gradient; }
		if (!done2) { uv2 += along * kSteps[i]; e2 = Luma(At(uv2)) - lEdge; done2 = abs(e2) >= gradient; }
	}
	float d1 = isHorizontal ? vUV.x - uv1.x : vUV.y - uv1.y, d2 = isHorizontal ? uv2.x - vUV.x : uv2.y - vUV.y;
	bool nearer1 = d1 < d2;
	float dNear = min(d1, d2), length = d1 + d2;
	// (only where the end is on this pixel's side of the edge: else the pixel is the edge's far side, left)
	bool smaller = lM < lEdge;
	bool right = ((nearer1 ? e1 : e2) < 0.0) != smaller;
	float offset = right ? 0.5 - dNear / length : 0.0;
	// ... and a sub-pixel blend for the thinnest features (a line less than a pixel across)
	float average = (2.0 * (lN + lS + lW + lE) + lNW + lNE + lSW + lSE) / 12.0;
	float sub = clamp(abs(average - lM) / range, 0.0, 1.0);
	sub = (-2.0 * sub + 3.0) * sub * sub;
	offset = max(offset, sub * sub * 0.75);
	vec2 final = vUV;
	if (isHorizontal) final.y += offset * step; else final.x += offset * step;
	outColor = vec4(At(final), 1.0);
}
