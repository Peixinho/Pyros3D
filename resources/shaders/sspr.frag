#version 450
// (the mirrored point's colour, and its distance from the eye - SceneRenderer's planar reflection)
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;
void main() { outColor = vColor; }     // (a: its distance - 0 where nothing landed)
