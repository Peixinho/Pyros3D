#version 450
#include "common.glsl"
// Auto exposure, last: toward the exposure that puts the scene's average at middle grey, as an eye adapts -
// faster to light than to dark. In: the average (1x1) and the last frame's exposure; out: this frame's (r)
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 params; } uDraw;   // x: compensation (EV), y: 1 auto / 0 manual, z: manual exposure, w: 1 = start over
layout(set = 1, binding = 1) uniform texture2D uAverage;
layout(set = 1, binding = 2) uniform texture2D uPrevious;
layout(set = 1, binding = 3) uniform sampler uPointClamp;
layout(location = 0) out vec4 outColor;
void main()
{
	if (uDraw.params.y < 0.5) { outColor = vec4(uDraw.params.z, 0.0, 0.0, 1.0); return; }
	vec2 s = texelFetch(sampler2D(uAverage, uPointClamp), ivec2(0, 0), 0).rg;
	float avgLum = s.y > 1e-3 ? exp2(s.x / s.y) : 0.18;
	// (middle grey, within five stops down of an exposure of 1; up, in full for two stops, then 60% of each stop more up to
	// six in all - as an eye: a dim room comes up to be seen, and still reads as dim, not grey. A hard stop at two left an
	// interior lit by one lamp a black box)
	float ev = log2(0.18 / max(avgLum, 1e-5));
	if (ev > 2.0) ev = 2.0 + (ev - 2.0) * 0.6;
	float target = exp2(clamp(ev, -5.0, 6.0)) * exp2(uDraw.params.x);
	// (in daylight, no brighter than keeps what the sun lights itself: a field half in shade metered to middle grey burnt its
	// sunlit half white - dry grass in the sun a pale glare with no blade in it. As a camera in the sun is exposed for the sun:
	// a sunlit surface of albedo 0.5 at about 0.7, whatever share of the picture is in shade)
	if (uFrame.frame.z > 0.5 && uFrame.sunDirection.w > 0.0 && uFrame.sunDirection.y > 0.02)     // (with the engine's sky only: its sun and sky are one light, in the same units)
	{
		float sunLum = dot(uFrame.sunRadiance.rgb, vec3(0.2126, 0.7152, 0.0722)) * clamp(uFrame.sunDirection.y * 3.0, 0.0, 1.0);
		// (at most three quarters of a stop under the metered exposure: a scene whose sun is not what lights most of it - a lab's
		// strong light at a grazing angle - went black; at a stop and a half, a narrow street in shade was dark as a cellar)
		if (sunLum > 1e-3) target = max(min(target, 0.7 / (0.5 * sunLum / PI) * exp2(uDraw.params.x)), target * 0.6);
	}
	float previous = texelFetch(sampler2D(uPrevious, uPointClamp), ivec2(0, 0), 0).r;
	if (!(target > 0.0 && target < 1e6)) target = previous > 0.0 && previous < 1e6 ? previous : 1.0;     // (an average gone wrong: kept as it was)
	if (uDraw.params.w > 0.5 || !(previous > 0.0 && previous < 1e6)) { outColor = vec4(target, 0.0, 0.0, 1.0); return; }
	float rate = target < previous ? 3.0 : 1.2;          // (per second: to a brighter scene fast, to a darker one slower)
	float k = 1.0 - exp(-uFrame.frame.x * rate);
	outColor = vec4(exp2(mix(log2(previous), log2(target), k)), 0.0, 0.0, 1.0);
}
