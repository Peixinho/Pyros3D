#if defined(GLES3)
	precision mediump float;
#endif

// Plain, unlabeled GLSL - deliberately no UBO_BINDING/IO_LOCATION/
// SAMPLER_BINDING macros and no hand-authored Vulkan treatment at all.
// VulkanRenderDevice::CompileShaderStage() runs every shader through
// SpirvShaderCompiler::AutoFixForVulkan() (see its header comment), which
// auto-assigns layout(location=)/layout(binding=) and auto-wraps every
// loose non-opaque uniform declared below into a synthesized per-stage UBO
// - ParticleSystemMaterial (a CustomShaderMaterial) needs zero manual
// extraUniforms wiring as a result. GL needs none of this either way.
//
// Per-instance attributes (ParticleSystem's own divisor=1 AttributeBuffer,
// see ParticleSystem.cpp) carry everything CPU-integrated per particle -
// world position and normalized age are already computed on the CPU each
// frame, so unlike the old hand-rolled particle.glsl this shader needs
// neither uModelMatrix nor uTime.

#ifdef VERTEX
in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexcoord;

// xyz = current world position (CPU-integrated), w = normalized age [0,1]
in vec4 aParticleData0;
// x = current rotation angle (radians, CPU-integrated), y = per-particle
// random seed [0,1] (drives size jitter here, color jitter in fragment if
// ever needed), z/w reserved
in vec4 aParticleData1;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform float uStartSize;
uniform float uEndSize;
uniform float uSizeRandomJitter;

out vec2 vTexcoord;
out float vAge;

void main()
{
	vTexcoord = aTexcoord;
	vAge = clamp(aParticleData0.w, 0.0, 1.0);

	float size = mix(uStartSize, uEndSize, vAge);
	size *= 1.0 + (aParticleData1.y - 0.5) * 2.0 * uSizeRandomJitter;

	float rotation = aParticleData1.x;
	float c = cos(rotation);
	float s = sin(rotation);
	vec2 rotatedLocal = mat2(c, -s, s, c) * aPosition.xy;

	// Spherical/camera-facing billboard: the particle's local quad offset
	// is added directly in view space, never multiplied through the
	// view's own rotation - this is what makes the quad always face the
	// camera regardless of view angle (equivalent to the classic "zero
	// ModelView's upper-left 3x3 to scale*identity" trick, just derived
	// directly here since there's no per-object ModelMatrix to build in
	// the first place - world position is already known).
	vec3 viewCenter = (uViewMatrix * vec4(aParticleData0.xyz, 1.0)).xyz;
	vec3 viewPos = viewCenter + vec3(rotatedLocal * size, 0.0);

	gl_Position = uProjectionMatrix * vec4(viewPos, 1.0);
}
#endif

#ifdef FRAGMENT
uniform sampler2D uTex0;
uniform vec4 uStartColor;
uniform vec4 uEndColor;
uniform float uFadeInFraction;
uniform float uFadeOutFraction;

in vec2 vTexcoord;
in float vAge;

out vec4 FragColor;

void main()
{
	vec4 tex = texture(uTex0, vTexcoord);
	vec4 color = mix(uStartColor, uEndColor, vAge);

	float fadeIn = smoothstep(0.0, uFadeInFraction, vAge);
	float fadeOut = 1.0 - smoothstep(uFadeOutFraction, 1.0, vAge);

	FragColor = tex * color;
	FragColor.a *= fadeIn * fadeOut;
}
#endif
