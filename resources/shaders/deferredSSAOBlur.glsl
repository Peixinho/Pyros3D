#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision highp float;
#endif
// Binding 35 - see deferredSSAO.glsl's identical block.
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
IO_LOCATION(0) out vec4 FragColor;
SAMPLER_BINDING(0) uniform sampler2D tAO;
SAMPLER_BINDING(1) uniform sampler2D tDepth;
UBO_BINDING(35) uniform DeferredSSAOParams {
	vec2 uScreenDimensions;
	vec2 uNearFar;
	mat4 uMatProj;
	float uSSAORadius;
	float uSSAOStrength;
	float uSSAOFalloff;
	float uSSAOSamples;
	float uSSAODirect;
};

float LinearDepth(vec2 uv)
{
	float z = texture_2D(tDepth, uv).r;
	return uNearFar.x * uNearFar.y / (uNearFar.y - z * (uNearFar.y - uNearFar.x));
}

// Bilateral 4x4: exactly one tile of deferredSSAO.glsl's rotation pattern,
// taken at texel centres so neither the AO nor the depth is filtered. A
// tap whose depth differs from the centre's by more than 5% of it is on
// another surface and gets no weight, so a crevice's darkness does not
// bleed onto the object in front of it.
void main() {
	vec2 texel = 1.0 / uScreenDimensions;
	vec2 uv0 = gl_FragCoord.xy * texel;
	float centerZ = LinearDepth(uv0);
	float result = 0.0;
	float wsum = 0.0;
	for (int j = -2; j < 2; j++) {
		for (int i = -2; i < 2; i++) {
			vec2 uv = uv0 + vec2(float(i), float(j)) * texel;
			float w = max(0.0, 1.0 - abs(LinearDepth(uv) - centerZ) / (0.05 * centerZ));
			result += texture_2D(tAO, uv).r * w;
			wsum += w;
		}
	}
	float ao = wsum > 0.0 ? result / wsum : texture_2D(tAO, uv0).r;
	// .r for the ambient pass, .g for the light passes: occlusion is
	// physically an ambient term, but under strong direct light that alone
	// is invisible, so lights take a chosen share of it.
	FragColor = vec4(ao, mix(1.0, ao, uSSAODirect), 1.0, 1.0);
}
#endif
