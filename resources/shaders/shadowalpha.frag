#version 450
// An alpha-cut caster's shadow: only where its base colour's alpha passes the cutoff (a leaf, a blade - not its card)
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 model; vec4 uvTransform; vec4 alpha; } uDraw;
layout(set = 1, binding = 1) uniform texture2D uBaseColorMap;
layout(set = 1, binding = 2) uniform sampler uMaterialSampler;
layout(location = 0) in vec2 vTexcoord;
void main()
{
	if (texture(sampler2D(uBaseColorMap, uMaterialSampler), vTexcoord).a < uDraw.alpha.x) discard;
}
