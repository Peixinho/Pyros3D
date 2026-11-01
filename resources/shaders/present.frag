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
	outColor = vec4(textureLod(sampler2D(uPicture, uLinearClamp), vUV, 0.0).rgb, 1.0);
}
