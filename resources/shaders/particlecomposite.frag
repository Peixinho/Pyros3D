#version 450
// The particles drawn at half resolution, over the picture: their light added, what is behind them dimmed by their cover
// (premultiplied: the pipeline blends one and one minus alpha)
layout(set = 1, binding = 1) uniform texture2D uParticles;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main() { outColor = textureLod(sampler2D(uParticles, uLinearClamp), vUV, 0.0); }
