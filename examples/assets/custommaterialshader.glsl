#if defined(GLES2) || defined(GLES3)
precision mediump float;
#endif

// Vulkan/SPIR-V needs a static layout(location=) on every attribute/
// varying/output, layout(binding=) on every UBO/sampler, and rejects
// non-opaque uniforms outside a block outright - GL needs none of this.
// VULKAN is predefined by shaderc itself for any Vulkan-target compile.
// Two separate UBOs (not one) because a single block referenced from
// both VERTEX and FRAGMENT of one program triggers a real
// GL_INVALID_OPERATION on this codebase's target GL41 driver - see
// DeferredRenderer's secondpassPoint.glsl comment for the full story.
// Bindings 35/36 - a single global registry shared with every other
// UBO in the engine, not per-shader (see IMaterial.h's comment on
// extraUniforms[2]).
#if defined(VULKAN)
#define UBO_BINDING(n) layout(std140, binding = n)
#define SAMPLER_BINDING(n) layout(set = 1, binding = n)
#define IO_LOCATION(n) layout(location = n)
#else
#define UBO_BINDING(n)
#define SAMPLER_BINDING(n)
#define IO_LOCATION(n)
#endif

#ifdef VERTEX
IO_LOCATION(0) in vec3 aPosition;
IO_LOCATION(1) in vec3 aNormal;
IO_LOCATION(2) in vec2 aTexcoord;
UBO_BINDING(35) uniform CustomMaterialVertParams {
	mat4 uProjectionMatrix;
	mat4 uViewMatrix;
	mat4 uModelMatrix;
};

void main()
{
	gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT

IO_LOCATION(0) out vec4 FragColor;
UBO_BINDING(36) uniform CustomMaterialFragParams {
	vec4 uColor;
};

void main()
{
	FragColor = uColor;
}
#endif
