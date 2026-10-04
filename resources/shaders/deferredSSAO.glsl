#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision highp float;
#endif
// Binding 35, shared with deferredSSAOBlur.glsl: both declare the identical
// DeferredSSAOParams block, so whichever material's buffer is left bound
// is always the right size for the other.
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
// Screen-space ambient occlusion from the G-buffer, for DeferredRenderer's
// ambient pass. Unlike SSAOEffect (the post-effect version, which only has
// depth) this reads the real view-space normals, so the hemisphere is
// oriented by the surface as shaded - normal maps included - instead of
// one rebuilt from depth differences.
IO_LOCATION(0) out vec4 FragColor;
SAMPLER_BINDING(0) uniform sampler2D tDepth;
SAMPLER_BINDING(1) uniform sampler2D tNormal;
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

float DecodeNativeDepth(float native_z, vec4 z_info_local)
{
	return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);
}

// Same reconstruction, and the same Metal flip, as secondpass*.glsl and
// lastPass.glsl - see lastPass.glsl's getPosViewSpace() for why.
vec3 getPosViewSpace(float depth_sampled, vec2 uv, vec4 z_info_local)
{
	vec4 vp = vec4(1.0, 1.0, 2.0/uScreenDimensions.x, 2.0/uScreenDimensions.y);
	vec2 screenPos = (uv + .5) * vp.zw - vp.xy;
#if defined(METAL)
	screenPos.y = -screenPos.y;
#endif
	vec2 screenSpaceRay = vec2(screenPos.x / uMatProj[0][0], screenPos.y / uMatProj[1][1]);
	float lDepth = DecodeNativeDepth(depth_sampled, z_info_local);
	return vec3(lDepth * screenSpaceRay, -lDepth);
}

// See lastPass.glsl's ClipToUV() - Metal's projection keeps GL's NDC Y
// while its render targets are top-origin.
vec2 ClipToUV(vec4 clipPos) {
	vec2 uv = (clipPos.xy / clipPos.w) * 0.5 + 0.5;
#if defined(METAL)
	uv.y = 1.0 - uv.y;
#endif
	return uv;
}

// A 4x4 ordered set of rotations, one per pixel of each 4x4 screen tile.
// deferredSSAOBlur.glsl averages exactly one such tile, so all sixteen
// rotations meet in every output pixel and the pattern cancels.
const float kRotation[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
	3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

void main() {
	vec2 Texcoord = gl_FragCoord.xy / uScreenDimensions;
	float d0 = texture_2D(tDepth, Texcoord).r;
	if (d0 >= 1.0) { FragColor = vec4(1.0); return; }

	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);
	vec3 P = getPosViewSpace(d0, gl_FragCoord.xy, z_info);
	// Far away the sampling radius is smaller than a pixel and the depth buffer
	// is coarser than the radius: every sample lands on the surface it started
	// from, a few depth steps off, and whole hillsides came out occluded. The
	// effect fades out as the radius shrinks towards a pixel on screen, and
	// pixels past that are not sampled at all.
	float radiusPx = uSSAORadius * abs(uMatProj[1][1]) / max(-P.z, 0.001) * uScreenDimensions.y * 0.5;
	float fade = smoothstep(2.0, 6.0, radiusPx);
	if (fade <= 0.0) { FragColor = vec4(1.0); return; }
	vec3 N = normalize(texture_2D(tNormal, Texcoord).xyz);

	ivec2 q = ivec2(mod(gl_FragCoord.xy, 4.0));
	float ang = (kRotation[q.y * 4 + q.x] + 0.5) * (6.2831853 / 16.0);
	vec3 rvec = vec3(cos(ang), sin(ang), 0.0);
	vec3 T = rvec - N * dot(rvec, N);
	// rvec lies in the view plane; a normal facing straight along it
	// leaves nothing to build a tangent from.
	T = dot(T, T) < 1e-4 ? normalize(cross(N, vec3(0.0, 0.0, 1.0)) + vec3(1e-3, 0.0, 0.0)) : normalize(T);
	mat3 TBN = mat3(T, cross(N, T), N);

	float radius = uSSAORadius;
	// Keeps a flat surface from occluding itself through depth-buffer
	// quantization; grows with distance, where that quantization does.
	// ...which is the square of the distance over the near plane: a 24-bit
	// buffer resolves about z*z / (near * 2^24) metres.
	float bias = 0.03 * radius + 0.0005 * (-P.z) + 3.0 * P.z * P.z / (max(uNearFar.x, 0.0001) * 16777216.0);
	int samples = int(uSSAOSamples);
	float occlusion = 0.0;
	for (int i = 0; i < 32; i++) {
		if (i >= samples) break;
		float fi = float(i) + 0.5;
		float t = fi / float(samples);
		// Cosine-weighted hemisphere on a golden-angle spiral, with the
		// distance on an independent sequence packed towards the centre:
		// close occluders are what contact shadows are made of.
		float phi = fi * 2.39996;
		float sr = sqrt(t);
		vec3 dir = vec3(cos(phi) * sr, sin(phi) * sr, sqrt(1.0 - t));
		float h = fract(fi * 0.618034);
		vec3 samplePos = P + TBN * dir * (radius * mix(0.1, 1.0, h * h));

		vec2 uv = ClipToUV(uMatProj * vec4(samplePos, 1.0));
		float raw = texture_2D(tDepth, uv).r;
		if (raw >= 1.0) continue;
		float sceneZ = -DecodeNativeDepth(raw, z_info);
		// An occluder far in front of this pixel (an object before the
		// floor behind it) is not occluding it: fade its vote out past
		// the radius instead of drawing a dark outline round the object.
		float range = 1.0 - smoothstep(radius, radius + uSSAOFalloff, abs(P.z - sceneZ));
		occlusion += (sceneZ >= samplePos.z + bias ? 1.0 : 0.0) * range;
	}
	float ao = clamp(1.0 - (occlusion / float(samples)) * uSSAOStrength, 0.0, 1.0);
	ao = mix(1.0, ao, fade);
	FragColor = vec4(ao, ao, ao, 1.0);
}
#endif
