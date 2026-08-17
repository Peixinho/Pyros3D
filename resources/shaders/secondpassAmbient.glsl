#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif
// Vulkan/SPIR-V needs a static layout(location=) on every attribute/
// varying/output and layout(binding=) on every UBO/sampler, and rejects
// non-opaque uniforms outside a block outright - GL needs none of this.
// VULKAN is predefined by shaderc itself for any Vulkan-target compile.
// Binding 27 - see IMaterial.h's comment on extraUniformsBinding: binding
// points are a single global registry shared with PyrosShader.glsl's own
// UBOs and every IEffect's extra-uniforms block, not per-shader.
#if defined(VULKAN)
#define UBO_BINDING(n) layout(std140, binding = n)
#define SAMPLER_BINDING(n) layout(set = 1, binding = n)
#define IO_LOCATION(n) layout(location = n)
#else
#define UBO_BINDING(n) layout(std140)
#define SAMPLER_BINDING(n)
#define IO_LOCATION(n)
#endif

#ifdef VERTEX
IO_LOCATION(0) attribute_in vec3 aPosition;
IO_LOCATION(1) attribute_in vec3 aNormal;
IO_LOCATION(2) attribute_in vec2 aTexcoord;
void main() {
	gl_Position = vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT

SAMPLER_BINDING(0) uniform sampler2D tDiffuse;
SAMPLER_BINDING(1) uniform sampler2D tSpecular;
SAMPLER_BINDING(2) uniform sampler2D tDepth;
SAMPLER_BINDING(3) uniform sampler2D tNormal;
// PBR metallic/roughness G-buffer attachment - see PyrosShader.glsl's
// FragData_pbr (.r=roughness, .g=metalness).
SAMPLER_BINDING(5) uniform sampler2D tMetallicRoughness;
UBO_BINDING(27) uniform AmbientFragParams {
	vec2 uScreenDimensions;
};

// Fragment Color
IO_LOCATION(0) out vec4 FragColor;

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);

	// Depth clears to 1.0 (far) wherever the G-buffer pass drew no
	// geometry - background/sky pixels. Without this, every second-pass
	// light (this one included) reads whatever garbage sits in the
	// cleared G-buffer there and lights it anyway, brightening the
	// background above DrawBackground()'s own clear colour - found via a
	// Forward-vs-Deferred viewport background colour mismatch that traced
	// back to this shader having no notion of "no geometry here" at all.
	if (texture_2D(tDepth, Texcoord).r >= 1.0) discard;

	vec3 ambient;

	ambient.x = texture_2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.y = texture_2D(tSpecular, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.z = texture_2D(tNormal, vec2(Texcoord.x,Texcoord.y)).w;

	// The alpha channels already hold albedo * uAmbientLight (each
	// G-buffer writer multiplies its own albedo in). Applying `* color`
	// on top of that - which this did - made deferred ambient come out
	// albedo-SQUARED: 5x too dark at albedo 0.2, and most of why a
	// deferred scene looked so much darker than the same scene in
	// forward. Only that second multiply is dropped; (1-metallic) stays
	// here, which deliberately keeps the *existing* packing valid - any
	// shader already generated or embedded in a saved scene keeps
	// meaning exactly what it did, it just stops being squared.
	// Custom materials additionally fold their emissive into these
	// channels (see MaterialCodegen.cpp), which is what gets emissive
	// ADDED after lighting here rather than packed into albedo and then
	// attenuated by N.L the way it used to be.
	float metallic = texture_2D(tMetallicRoughness, vec2(Texcoord.x,Texcoord.y)).g;
	FragColor=vec4(ambient * (1.0 - metallic), 1.0);
}
#endif
