#version 450
// The depth prepass for an alpha-cut surface (a leaf, a blade of grass - not its card): its depth and its move on screen
// only where its base colour's alpha passes the cutoff. With foliage in the prepass, the lit pass rejects a crown's hidden
// leaves before it lights them, and the AO and the temporal resolve see the foliage too.
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 model; mat4 previousModel; vec4 skin; vec4 wind; vec4 uvTransform; vec4 alpha; } uDraw;
layout(set = 1, binding = 3) uniform texture2D uBaseColorMap;
layout(set = 1, binding = 4) uniform sampler uMaterialSampler;
layout(location = 0) in vec4 vCurrent;
layout(location = 1) in vec4 vPrevious;
layout(location = 2) in vec2 vTexcoord;
layout(location = 0) out vec2 outVelocity;
void main()
{
	if (texture(sampler2D(uBaseColorMap, uMaterialSampler), vTexcoord).a < uDraw.alpha.x) discard;
	vec2 now = vCurrent.xy / vCurrent.w, then = vPrevious.xy / vPrevious.w;
	outVelocity = (now - then) * vec2(0.5, -0.5);
}
