#version 450
layout(set = 1, binding = 1) uniform texture2D uTexture;      // (the font atlas, or an image: an editor's viewport)
layout(set = 1, binding = 2) uniform sampler uSampler;
layout(location = 0) in vec2 vTexcoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;
vec3 ToLinear(vec3 c) { vec3 x = max(c, vec3(0.0)); return mix(x / 12.92, pow((x + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), x)); }
// (the target is sRGB - the store encodes - so the style's colours are decoded first: they come out as written; an
// sRGB image is decoded by its sampling and comes out as it is)
void main() { vec4 c = vec4(ToLinear(vColor.rgb), vColor.a) * texture(sampler2D(uTexture, uSampler), vTexcoord); outColor = c; }
