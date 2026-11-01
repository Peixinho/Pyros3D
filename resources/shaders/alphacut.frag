#version 450
#include "common.glsl"
#include "surface_draw.glsl"
// An alpha-cut surface's depth - or a level of detail's mid crossfade - laid down in the lit pass ahead of its light (a
// leaf, a blade of grass - not its card):
// the cut is all it does, so a crown's every leaf costs little here. Its light then comes in a second draw at equal
// depth with no cut in it (surface.frag NOCUT) - which a tile-deferred GPU (Apple's) can hide what is behind, as it
// cannot any shader that may discard: the hidden leaves are no longer lit.
layout(set = 1, binding = 2) uniform texture2D uBaseColorMap;
layout(set = 1, binding = 6) uniform sampler uMaterialSampler;
layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexcoord;
layout(location = 3) in vec4 vTint;
layout(location = 9) in float vFade;
layout(location = 10) in float vLodCode;
layout(location = 0) out vec4 outColor;
void main()
{
	float a = texture(sampler2D(uBaseColorMap, uMaterialSampler), vTexcoord).a * uDraw.baseColor.a * vTint.a;
	if (a < uDraw.emission.w) discard;
	// (a level of detail handing over to the next - wet.w: + this one going, - the next coming: the two kept on
	// complementary pixels of one dither, the same in every pass)
	if (uDraw.wet.w != 0.0)
	{
		float dither = LodDither(gl_FragCoord.xy);
		if (uDraw.wet.w > 0.0 ? dither < uDraw.wet.w : dither >= -uDraw.wet.w) discard;
	}
	if (InstanceDissolved(vFade, vLodCode, LodDither(gl_FragCoord.xy))) discard;     // (an instance dissolving: instancing.glsl)
	outColor = vec4(0.0);     // (not written: the pipeline writes no colour)
}
