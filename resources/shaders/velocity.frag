#version 450
#include "common.glsl"
// The depth prepass's colour: how far this surface moved on screen since last frame, in the picture's units (0..1)
layout(location = 0) in vec4 vCurrent;
layout(location = 1) in vec4 vPrevious;
layout(location = 5) flat in float vLensCut;
layout(location = 0) out vec2 outVelocity;
void main()
{
	if (vLensCut > 0.5 && InScreenLens(gl_FragCoord.xy)) discard;     // (cut by the screen's lens: a gun at a scope)
	vec2 now = vCurrent.xy / vCurrent.w, then = vPrevious.xy / vPrevious.w;
	outVelocity = (now - then) * vec2(0.5, -0.5);
}
