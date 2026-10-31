#version 450
#include "common.glsl"
#include "atmosphere.glsl"
// The sky-view table from a scene's skybox (all round: SkyViewUV's whole-turn layout, uFrame.ambient.w) - what the
// surfaces mirror and the sky's harmonics are summed from, as from the physical sky's
layout(set = 1, binding = 1) uniform textureCube uSkybox;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
	vec3 dir = SkyViewDirection(vec2(vUV.x, 1.0 - vUV.y), normalize(uFrame.sunDirection.xyz + vec3(0.0, 0.0, 1e-6)));
	outColor = vec4(textureLod(samplerCube(uSkybox, uLinearClamp), dir, 0.0).rgb, 1.0);
}
