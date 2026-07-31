#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif
// See secondpassAmbient.glsl's identical comment. Binding 37.
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
void main() {
	gl_Position = vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT

// Fragment Color
IO_LOCATION(0) out vec4 FragColor;

// Material-aware SSR - the whole reason this used to be a trivial
// passthrough is now not true. See DeferredRenderer.h's comment on
// previousFrameColorTexture for why the reflection source is *last*
// frame's color, not this one, and DeferredRenderer.cpp's constructor
// comment for the unit numbering below.
SAMPLER_BINDING(0) uniform sampler2D tDepth;
SAMPLER_BINDING(1) uniform sampler2D tNormal;
SAMPLER_BINDING(2) uniform sampler2D tMetallicRoughness;
SAMPLER_BINDING(3) uniform sampler2D tColor;
SAMPLER_BINDING(4) uniform sampler2D tPreviousFrameColor;
SAMPLER_BINDING(5) uniform sampler2D tDiffuse;
UBO_BINDING(37) uniform LastPassFragParams {
	vec2 uScreenDimensions;
	vec2 uNearFar;
	mat4 uMatProj;
	mat4 uViewMatrixInverse;
	mat4 uPrvProjectionMatrix;
	mat4 uPrvViewMatrix;
};

float DecodeNativeDepth(float native_z, vec4 z_info_local)
{
	return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);
}

// View-space reconstruction - same technique as secondpassDirectional.glsl
// (duplicated, no #include mechanism to share it with).
vec3 getPosViewSpace(float depth_sampled, vec2 uv, vec4 z_info_local, mat4 uMatProj_local, vec4 viewport_transform_local)
{
	vec2 screenPos = (uv + .5) * viewport_transform_local.zw - viewport_transform_local.xy;
	vec2 screenSpaceRay = vec2(screenPos.x / uMatProj_local[0][0], screenPos.y / uMatProj_local[1][1]);
	float lDepth = DecodeNativeDepth(depth_sampled, z_info_local);
	return vec3(lDepth * screenSpaceRay, -lDepth);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Above this roughness, a surface's specular lobe is wide enough that a
// screen-space reflection contributes negligibly next to its diffuse
// response (the same falloff CalculatePBRLighting's own GGX distribution
// already implies elsewhere) - skipping the march entirely here is a
// real, principled early-out, not a quality compromise, and keeps most
// pixels in a typical scene out of the loop below entirely.
const float SSR_ROUGHNESS_CUTOFF = 0.6;
// Coarse march (fixed view-space step) to find any crossing, then a
// short binary-search refine between the last miss and the hit - the
// textbook-correct version of what the old standalone
// ScreenSpaceReflectionEffect approximated with a flat, unrefined
// 256-step march sharing one uniform default across both backends (see
// VULKAN_ROADMAP.md - that fixed step count is what hung on Vulkan).
// ~37 worst-case texture fetches here versus 256, on top of the
// roughness gate above already skipping most pixels.
const int SSR_COARSE_STEPS = 32;
const int SSR_REFINE_STEPS = 5;
const float SSR_STEP_DISTANCE = 0.35;
const float SSR_MAX_DISTANCE = 12.0;

// Real, Vulkan-only bug found via a real report + a pixel-exact GL-vs-
// Vulkan comparison at a frozen camera angle (both backends, same scene,
// same frame): a jagged diagonal seam cut across the floor's SSR
// reflection on Vulkan only, absent on GL. Every OTHER UV this shader
// uses (Texcoord, and every tDepth/tNormal/tMetallicRoughness/tColor/
// tDiffuse sample it drives) comes from gl_FragCoord, which every other
// shader in this codebase also relies on and which was already correct
// on both backends. This ray march is the only place computing a UV by
// manually projecting a position through uMatProj instead of using
// gl_FragCoord - untested territory, and it turns out Vulkan's clip-space
// Y needs an explicit flip here that gl_FragCoord-based sampling
// (handled by the fixed-function rasterizer, not hand-rolled math)
// never needed. Confirmed via before/after screenshot at the same frozen
// angle - seam gone, matches GL.
vec2 ClipToUV(vec4 clipPos) {
	vec2 uv = (clipPos.xy / clipPos.w) * 0.5 + 0.5;
#if defined(VULKAN)
	uv.y = 1.0 - uv.y;
#endif
	return uv;
}

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);
	vec3 baseColor = texture(tColor, Texcoord).rgb;

	vec2 mr = texture(tMetallicRoughness, Texcoord).rg;
	float roughness = mr.x;
	float metallic = mr.y;

	if (roughness > SSR_ROUGHNESS_CUTOFF) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}

	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);
	vec4 vp = vec4(1.0, 1.0, 2.0/uScreenDimensions.x, 2.0/uScreenDimensions.y);
	vec2 screenCoord = vec2(uScreenDimensions.x*Texcoord.x, uScreenDimensions.y*Texcoord.y);

	float centerDepth = texture(tDepth, Texcoord).r;
	vec3 v1 = getPosViewSpace(centerDepth, screenCoord, z_info, uMatProj, vp);

	vec3 N = normalize(texture(tNormal, Texcoord).xyz);
	vec3 V = normalize(-v1);
	vec3 albedo = texture(tDiffuse, Texcoord).rgb;
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
	vec3 reflectDir = reflect(-V, N);

	vec3 rayPos = v1;
	vec3 prevRayPos = v1;
	bool hit = false;

	for (int i = 0; i < SSR_COARSE_STEPS; i++) {
		prevRayPos = rayPos;
		rayPos += reflectDir * SSR_STEP_DISTANCE;

		vec4 clipPos = uMatProj * vec4(rayPos, 1.0);
		if (clipPos.w <= 0.0) break;
		vec2 rayUV = ClipToUV(clipPos);
		if (rayUV.x < 0.0 || rayUV.x > 1.0 || rayUV.y < 0.0 || rayUV.y > 1.0) break;

		float sampleDepth = texture(tDepth, rayUV).r;
		vec3 samplePos = getPosViewSpace(sampleDepth, rayUV * uScreenDimensions, z_info, uMatProj, vp);

		// rayPos.z (view space, more negative = farther, matching
		// getPosViewSpace's own -lDepth) has gone past the surface
		// actually stored at this screen position - a real ray-vs-
		// surface crossing test, not a "closest point ever seen" running
		// minimum (the old effect's actual test - see
		// VULKAN_ROADMAP.md - which isn't a real intersection test).
		if (rayPos.z < samplePos.z && length(rayUV - Texcoord) > 0.005 && distance(samplePos, v1) < SSR_MAX_DISTANCE) {
			hit = true;
			break;
		}
	}

	if (!hit) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}

	// Binary-search refine between the last miss (prevRayPos) and the
	// confirmed hit (rayPos), both already real 3D view-space points.
	vec3 lo = prevRayPos;
	vec3 hi = rayPos;
	for (int i = 0; i < SSR_REFINE_STEPS; i++) {
		vec3 mid = mix(lo, hi, 0.5);
		vec4 clipMid = uMatProj * vec4(mid, 1.0);
		vec2 midUV = ClipToUV(clipMid);
		float sampleDepth = texture(tDepth, midUV).r;
		vec3 samplePos = getPosViewSpace(sampleDepth, midUV * uScreenDimensions, z_info, uMatProj, vp);
		if (mid.z < samplePos.z) hi = mid; else lo = mid;
	}

	vec4 finalClip = uMatProj * vec4(hi, 1.0);
	vec2 hitUV = ClipToUV(finalClip);

	// Reconstruct the hit's world position using the *current* frame's
	// camera, then reproject through *last* frame's camera to sample
	// tPreviousFrameColor.
	float hitDepth = texture(tDepth, hitUV).r;
	vec3 hitView = getPosViewSpace(hitDepth, hitUV * uScreenDimensions, z_info, uMatProj, vp);
	vec4 hitWorld = uViewMatrixInverse * vec4(hitView, 1.0);

	vec4 prevClip = uPrvProjectionMatrix * uPrvViewMatrix * hitWorld;
	if (prevClip.w <= 0.0) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}
	vec2 prevUV = ClipToUV(prevClip);
	if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}

	vec3 reflectionColor = texture(tPreviousFrameColor, prevUV).rgb;

	// Fade near the roughness cutoff and near screen edges, so a
	// reflection doesn't hard-pop when it walks off-screen or roughness
	// crosses the gate above.
	float edgeDist = max(abs(hitUV.x*2.0-1.0), abs(hitUV.y*2.0-1.0));
	float edgeFade = 1.0 - smoothstep(0.7, 1.0, edgeDist);
	float roughnessFade = 1.0 - smoothstep(SSR_ROUGHNESS_CUTOFF*0.7, SSR_ROUGHNESS_CUTOFF, roughness);

	vec3 weighted = reflectionColor * F * edgeFade * roughnessFade;
	FragColor = vec4(baseColor + weighted, 1.0);
}
#endif
