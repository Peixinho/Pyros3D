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

// The sun's shadow cache (IRenderer::SunCacheCopy): a kept map's depth written where the viewport is. A
// pixel takes the source's texel at its own place plus uCopy.xy - less the cascade's corner, where the kept
// map is put into the atlas; the texels the view moved by, where it is moved along. Outside the source, or
// with uCopy.z set (a part to be drawn again), the far plane: nothing there.

#ifdef VERTEX
IO_LOCATION(0) attribute_in vec3 aPosition;
IO_LOCATION(1) attribute_in vec3 aNormal;
IO_LOCATION(2) attribute_in vec2 aTexcoord;
void main() {
	gl_Position = vec4(aPosition, 1.0);
}
#endif

#ifdef FRAGMENT
SAMPLER_BINDING(0) uniform sampler2D uSource;
uniform vec4 uCopy;
void main() {
	float d = 1.0;
	ivec2 p = ivec2(gl_FragCoord.xy) + ivec2(uCopy.xy);
	ivec2 size = textureSize(uSource, 0);
	if (uCopy.z < 0.5 && p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y)
		d = texelFetch(uSource, p, 0).r;
	gl_FragDepth = d;
}
#endif
