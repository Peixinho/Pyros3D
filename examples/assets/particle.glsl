#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif

// See custommaterialshader.glsl's identical comment. Binding 42 - only
// one block needed, every non-sampler uniform here is VERTEX-only.
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
IO_LOCATION(0) attribute_in vec3 aPosition;
IO_LOCATION(1) attribute_in vec3 aNormal;
IO_LOCATION(2) attribute_in vec2 aTexcoord;
// Per-instance attributes (ParticlesExample.h's ParticleEmitter, a
// separate divisor=1 buffer) - own locations, not sharing aPosition's
// (a single layout(location=) qualifier applied to a comma-separated
// declarator list applies to *every* variable in it, not just the
// first - the exact bug this whole file's rewrite already fixes for
// aPosition/aNormal above, so it can't be reintroduced here either).
IO_LOCATION(3) attribute_in vec4 aParticlePosition;
IO_LOCATION(4) attribute_in vec4 aParticleDetails;
UBO_BINDING(42) uniform ParticleVertParams {
	mat4 uProjectionMatrix;
	mat4 uViewMatrix;
	mat4 uModelMatrix;
	float uTime;
};
IO_LOCATION(0) varying_out vec2 vTexcoord;
IO_LOCATION(1) varying_out float vLifetime;
IO_LOCATION(2) varying_out float vTime;
IO_LOCATION(3) varying_out float vRotation;
IO_LOCATION(4) varying_out vec3 vDirection;
void main()
{
    vRotation = aParticlePosition.w;
    vTexcoord = aTexcoord;
    vDirection = aParticleDetails.xyz;
    vLifetime = aParticleDetails.w;
    vTime = uTime;

    mat4 ModelMatrix = uModelMatrix;
    ModelMatrix[3].xyz += aParticlePosition.xyz;

    mat4 ModelView = (uViewMatrix * ModelMatrix);

    float scale = 1.0;
    scale = (vTime-vLifetime)+0.05;

    ModelView[0][0] = scale; ModelView[0][1] = 0.0; ModelView[0][2] = 0.0;
    ModelView[1][0] = 0.0; ModelView[1][1] = scale; ModelView[1][2] = 0.0;
    ModelView[2][0] = 0.0; ModelView[2][1] = 0.0; ModelView[2][2] = scale;

	gl_Position = uProjectionMatrix * (ModelView * (vec4(aPosition.xyz,1.0)));
}
#endif

#ifdef FRAGMENT
SAMPLER_BINDING(0) uniform sampler2D uTex0;
IO_LOCATION(0) varying_in vec2 vTexcoord;
IO_LOCATION(1) varying_in float vLifetime;
IO_LOCATION(2) varying_in float vTime;
IO_LOCATION(3) varying_in float vRotation;
IO_LOCATION(4) varying_in vec3 vDirection;

// Fragment Color
IO_LOCATION(0) out vec4 FragColor;

void main()
{
    vec2 texcoord = vec2(vTexcoord.x-0.5, vTexcoord.y -0.5);
    float angle = vRotation*vTime;
    mat2 RotationMatrix = mat2( cos(angle), -sin(angle), sin(angle), cos(angle) );
    texcoord *= RotationMatrix;
	// Was texture2D() (a GLES2-only builtin, removed in modern GLSL) -
	// pre-existing inconsistency with this file's own texture_2D
	// portability macro, fixed here since GLES2 was already dropped
	// from the engine (see [[gles2_dropped_proactively]] in memory).
	FragColor = vec4(texture_2D(uTex0, texcoord+0.5));
    FragColor.xyz += clamp(vRotation,0.0,1.0);
    FragColor.w -= (vTime-vLifetime)*.1;
}
#endif
