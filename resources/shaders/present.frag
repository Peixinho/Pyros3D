#version 450
// The finished picture onto the screen - where the scene's own post effects (asset effects: SceneRenderer::
// SetAssetEffects) ran after the tone curve, on the picture in display terms, the tonemap's work went into a texture
// and this brings it to the backbuffer, which encodes it (sRGB, or HDR linear floats)
layout(set = 1, binding = 1) uniform texture2D uPicture;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
	// (the asset effects' display values - tonemap.frag encoded them - linear again for the backbuffer, which encodes)
	outColor = vec4(pow(max(textureLod(sampler2D(uPicture, uLinearClamp), vUV, 0.0).rgb, vec3(0.0)), vec3(2.2)), 1.0);
}
