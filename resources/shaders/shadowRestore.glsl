#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#if defined(GLES3)
	precision highp float;
	precision highp sampler2D;
#endif
#if defined(VULKAN)
#define UBO_BINDING(n) layout(std140, binding = n)
#define SAMPLER_BINDING(n) layout(set = 1, binding = n)
#define IO_LOCATION(n) layout(location = n)
#else
#define UBO_BINDING(n) layout(std140)
#define SAMPLER_BINDING(n)
#define IO_LOCATION(n)
#endif

// A point light's shadow face put back from what its still casters left in it (IRenderer's point
// shadow pass): the same value to the colour, which the lighting reads, and to the depth, which
// what moves is then drawn against. Both pictures are drawn with the same face's settings, so a
// pixel here is the same pixel there - nothing is turned or flipped.

#ifdef VERTEX
IO_LOCATION(0) attribute_in vec3 aPosition;
IO_LOCATION(1) attribute_in vec3 aNormal;
IO_LOCATION(2) attribute_in vec2 aTexcoord;
void main() {
	gl_Position = vec4(aPosition, 1.0);
}
#endif

#ifdef FRAGMENT
SAMPLER_BINDING(0) uniform sampler2D uStillFace;
IO_LOCATION(0) out vec4 FragColor;
void main() {
	float d = texelFetch(uStillFace, ivec2(gl_FragCoord.xy), 0).r;
	FragColor = vec4(d, 0.0, 0.0, 1.0);
	gl_FragDepth = d;
}
#endif
