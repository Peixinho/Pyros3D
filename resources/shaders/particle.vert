#version 450
#include "common.glsl"
// Particles: a camera-facing quad for each, built from the vertex index (six a quad), its data from a per-instance
// stream the CPU simulation fills each frame. Size and colour ramps as the particle system's description says.
layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 startColor, endColor;   // linear
	vec4 size;                   // x start, y end, z random jitter, w size ease
	vec4 params;                 // x colour ease, y fade-in fraction, z fade-out fraction, w softness (m)
	vec4 light;                  // x lit, y forward scatter (g), z 1 additive, w stretch (seconds of motion drawn)
} uDraw;

layout(location = 0) in vec4 aData0;     // xyz world position, w normalized age
layout(location = 1) in vec4 aData1;     // x rotation, y random seed, z 0 dead, else 1 + speed; w its way (packed)

layout(location = 0) out vec2 vTexcoord;
layout(location = 1) out float vAge;
layout(location = 2) out vec3 vWorldPosition;
layout(location = 3) out float vViewDepth;

// The ease modes the scene data uses (Core/Math/Easing.h): linear, step, in, out, both
float Ease(float t, float mode)
{
	if (mode < 0.5) return t;
	if (mode < 1.5) return 0.0;
	if (mode < 2.5) return t * t;
	if (mode < 3.5) return t * (2.0 - t);
	if (mode < 4.5) return t * t * (3.0 - 2.0 * t);
	return t;
}

vec3 UnpackDirection(float packed)
{
	float q = floor(packed + 0.5);
	vec2 o = vec2(floor(q / 1024.0), mod(q, 1024.0)) / 1023.0 * 2.0 - 1.0;
	vec3 d = vec3(o.x, 1.0 - abs(o.x) - abs(o.y), o.y);
	if (d.y < 0.0) d.xz = (1.0 - abs(d.zx)) * vec2(d.x >= 0.0 ? 1.0 : -1.0, d.z >= 0.0 ? 1.0 : -1.0);
	return normalize(d);
}

void main()
{
	const vec2 corners[6] = vec2[](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
	vec2 corner = corners[gl_VertexIndex % 6];
	vTexcoord = vec2(corner.x + 0.5, 0.5 - corner.y);
	vAge = clamp(aData0.w, 0.0, 1.0);
	vWorldPosition = aData0.xyz;
	float size = mix(uDraw.size.x, uDraw.size.y, Ease(vAge, uDraw.size.w));
	size *= (1.0 + (aData1.y - 0.5) * 2.0 * uDraw.size.z) * (aData1.z > 0.5 ? 1.0 : 0.0);
	float c = cos(aData1.x), s = sin(aData1.x);
	vec2 local = mat2(c, -s, s, c) * corner * size;
	// (facing the camera: the corner added in view space)
	vec3 viewCenter = (uFrame.view * vec4(aData0.xyz, 1.0)).xyz;
	float speed = max(aData1.z - 1.0, 0.0);
	if (uDraw.light.w > 0.0 && speed > 0.0)
	{
		// Stretched: the streak it travelled over `stretch` seconds, as seen - along its motion across the screen, as
		// wide as it is, its head where the particle is
		vec3 move = mat3(uFrame.view) * UnpackDirection(aData1.w) * (speed * uDraw.light.w);
		// (its motion across the screen: in view space, perspective's foreshortening of a move toward the eye kept)
		vec2 across = move.xy - viewCenter.xy * (move.z / min(viewCenter.z, -1e-3));
		float trail = length(across);
		vec2 axis = trail > 1e-5 ? across / trail : vec2(0.0, 1.0);
		local = axis * (corner.y * (size + trail)) + vec2(-axis.y, axis.x) * (corner.x * size);
		viewCenter.xy -= axis * (trail * 0.5);
	}
	vec3 viewPos = viewCenter + vec3(local, 0.0);
	vViewDepth = -viewPos.z;
	gl_Position = ProjectView(viewPos);
}
