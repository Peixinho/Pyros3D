#version 450
#include "common.glsl"
// A scene's skybox - a material's six-face cubemap (GenericShaderMaterial::SetSkyboxMap) - behind everything, by the
// direction looked in: from the view and the projection, the eye's own (no mesh, nothing to follow the camera)
layout(set = 1, binding = 1) uniform textureCube uSkybox;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
	vec2 ndc = vec2(vUV.x * 2.0 - 1.0, 1.0 - vUV.y * 2.0);
	// (orthographic: no eye's spread - a 90-degree one, so the sky still shows round it)
	vec3 v = Orthographic() ? vec3(ndc.x * uFrame.viewport.x / uFrame.viewport.y, ndc.y, -1.0) : vec3(ndc.x / uFrame.projection.x, ndc.y / uFrame.projection.y, -1.0);
	vec3 dir = normalize(transpose(mat3(uFrame.view)) * v);
	outColor = vec4(textureLod(samplerCube(uSkybox, uLinearClamp), dir, 0.0).rgb, 1.0);
}
