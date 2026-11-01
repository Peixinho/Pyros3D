#version 450
#include "common.glsl"
// The depth prepass for an alpha-cut surface (a leaf, a blade of grass - not its card), or a level of detail mid crossfade: its depth and its move on screen
// only where its base colour's alpha passes the cutoff. With foliage in the prepass, the lit pass rejects a crown's hidden
// leaves before it lights them, and the AO and the temporal resolve see the foliage too.
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 model; mat4 previousModel; vec4 skin; vec4 wind; vec4 uvTransform; vec4 alpha; } uDraw;
layout(set = 1, binding = 3) uniform texture2D uBaseColorMap;
layout(set = 1, binding = 4) uniform sampler uMaterialSampler;
layout(location = 0) in vec4 vCurrent;
layout(location = 1) in vec4 vPrevious;
layout(location = 2) in vec2 vTexcoord;
layout(location = 3) in float vFade;
layout(location = 4) in float vLodCode;
layout(location = 0) out vec2 outVelocity;
void main()
{
	if (texture(sampler2D(uBaseColorMap, uMaterialSampler), vTexcoord).a < uDraw.alpha.x) discard;
	// (a level of detail handing over to the next - alpha.w: + this one going, - the next coming: alphacut.frag's dither)
	if (uDraw.alpha.w != 0.0)
	{
		float dither = LodDither(gl_FragCoord.xy);
		if (uDraw.alpha.w > 0.0 ? dither < uDraw.alpha.w : dither >= -uDraw.alpha.w) discard;
	}
	// (an instance dissolving - thinned away, or past its fade: instancing.glsl - the same dither, the cut's)
	if (InstanceDissolved(vFade, vLodCode, LodDither(gl_FragCoord.xy))) discard;
	vec2 now = vCurrent.xy / vCurrent.w, then = vPrevious.xy / vPrevious.w;
	outVelocity = (now - then) * vec2(0.5, -0.5);
}
