#version 450
// (the same shader as main.cpp's WGSL - group 0 the draw's data, group 1 the material)
layout(set = 0, binding = 0) uniform Draw { mat4 mvp; vec4 tint; } draw;
layout(location = 0) in vec3 p;
layout(location = 1) in vec3 n;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec2 vuv;
layout(location = 1) out vec3 vn;
layout(location = 2) out vec4 vtint;
void main() { gl_Position = draw.mvp * vec4(p, 1.0); vuv = uv; vn = n; vtint = draw.tint; }
