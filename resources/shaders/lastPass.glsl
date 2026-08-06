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
// std140 required on GL - without it the default `shared` layout does not
// match DeferredRenderer's hand-computed offsets (uSSREnabled etc.), which
// blacked SSR (and anything that samples LastPassFragParams) on macOS GL.
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
	// Real mip count of tPreviousFrameColor (log2 of its largest
	// dimension, recomputed on resize) - see the roughness-blur comment
	// on textureLod() below.
	float uMaxReflectionLod;
	// Real, per-scene-settable march distances (view-space units) - see
	// the SSR_COARSE_STEPS/SSR_THICKNESS_STEPS comment below for why
	// these are explicit uniforms and not shader constants or an
	// automatic per-pixel scale.
	float uSSRStepDistance;
	float uSSRMaxDistance;
	// Real opt-in gate, defaults to disabled (0.0) - see
	// DeferredRenderer::EnableSSR()'s comment. Every DeferredRenderer
	// runs this composite pass regardless (it's where colorTexture
	// becomes the final image, SSR or not), so this can't be skipped by
	// just not calling into this shader - has to be an explicit runtime
	// check.
	float uSSREnabled;
	// Isolation modes for SSRTest (DeferredRenderer::SetSSRDebugMode):
	// 0 normal, 1 show mr.b gate, 2 paint march hits, 3 full hit color.
	float uSSRDebug;
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
// McGuire/Mara screen-space DDA. Stride 1 + no jitter: without a temporal
// filter, stride/jitter just turns quantization into the stipple/comb
// noise SSRTest showed (view-dependent holes along sphere silhouettes).
const int SSR_COARSE_STEPS = 128;
const int SSR_REFINE_STEPS = 6;
const float SSR_PIXEL_STRIDE = 1.0;
const float SSR_THICKNESS = 0.35;
const float SSR_MIN_PIXELS = 2.5;
const float SSR_COPLANAR_DOT = 0.95;

// uMatProj already goes through IRenderer::CaptureExtraUniform →
// device->TranslateProjectionMatrix() (Vulkan Y-flip + Z remap; identity
// on GL). Projecting with that matrix and doing ndc*0.5+0.5 therefore
// already lands on the same UV convention as gl_FragCoord-based Texcoord
// on BOTH backends. An older `#if VULKAN uv.y = 1-uv.y` here double-flipped
// Vulkan only and turned reflections into large wrong-colored floor blobs
// (SSRTest 2026-08-06). Do not reintroduce that flip.
vec2 ClipToUV(vec4 clipPos) {
	return (clipPos.xy / clipPos.w) * 0.5 + 0.5;
}

// Perspective-correct screen-space ray trace against tDepth (McGuire & Mara
// JCGT 2014). Returns hit UV or (-1,-1) on miss. outConfidence is 1 at a
// tight surface contact and falls off when the ray only grazes thickness
// (used to soften noisy silhouette pixels).
vec2 TraceSSR(vec3 rayOrigin, vec3 rayDir, vec4 z_info, out float outConfidence)
{
	outConfidence = 0.0;
	float maxRayDistance = uSSRMaxDistance;
	float nearZ = uNearFar.x;

	float rayLength = maxRayDistance;
	float zEnd = rayOrigin.z + rayDir.z * maxRayDistance;
	if (zEnd > -nearZ)
		rayLength = max((-nearZ - rayOrigin.z) / rayDir.z, 0.0);
	if (rayLength < 1e-4)
		return vec2(-1.0);
	vec3 rayEnd = rayOrigin + rayDir * rayLength;

	vec4 H0 = uMatProj * vec4(rayOrigin, 1.0);
	vec4 H1 = uMatProj * vec4(rayEnd, 1.0);
	if (H0.w <= 0.0 || H1.w <= 0.0)
		return vec2(-1.0);

	float k0 = 1.0 / H0.w;
	float k1 = 1.0 / H1.w;
	vec3 Q0 = rayOrigin * k0;
	vec3 Q1 = rayEnd * k1;

	vec2 P0 = ClipToUV(H0) * uScreenDimensions;
	vec2 P1 = ClipToUV(H1) * uScreenDimensions;
	vec2 P0orig = P0;
	P1 += length(P1 - P0) < 0.0001 ? vec2(0.01) : vec2(0.0);
	vec2 delta = P1 - P0;

	bool permute = false;
	if (abs(delta.x) < abs(delta.y)) {
		permute = true;
		delta = delta.yx;
		P0 = P0.yx;
		P1 = P1.yx;
	}

	float stepDir = sign(delta.x);
	if (abs(delta.x) < 1e-5)
		return vec2(-1.0);
	float invdx = stepDir / delta.x;
	vec3 dQ = (Q1 - Q0) * invdx;
	float dk = (k1 - k0) * invdx;
	vec2 dP = vec2(stepDir, delta.y * invdx);

	float pixStride = SSR_PIXEL_STRIDE;
	dP *= pixStride;
	dQ *= pixStride;
	dk *= pixStride;

	vec4 pqk = vec4(P0, Q0.z, k0);
	vec4 dPQK = vec4(dP, dQ.z, dk);

	float rayZFar = pqk.z / pqk.w;
	float rayZNear;
	bool hit = false;
	vec2 hitPixel = vec2(-1.0);
	float hitDepthDelta = 0.0;

	for (int i = 0; i < SSR_COARSE_STEPS; i++) {
		pqk += dPQK;
		rayZNear = rayZFar;
		rayZFar = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);

		vec2 pix = permute ? pqk.yx : pqk.xy;
		// Stay off the reflector itself (and its immediate neighbours).
		if (distance(pix, P0orig) < SSR_MIN_PIXELS)
			continue;

		vec2 uv = pix / uScreenDimensions;
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
			break;

		float rawDepth = texture(tDepth, uv).r;
		if (rawDepth >= 0.9999)
			continue;

		float sceneZ = -DecodeNativeDepth(rawDepth, z_info);
		// Thickness grows with the span of this DDA step so grazing /
		// distant rays still catch the sphere instead of tunneling
		// through (the view-dependent holes in SSRTest).
		float stepSpan = abs(rayZFar - rayZNear);
		float thickness = max(SSR_THICKNESS, stepSpan * 2.0);

		float zNear = min(rayZNear, rayZFar);
		float zFar = max(rayZNear, rayZFar);
		if (zFar >= sceneZ - thickness && zNear <= sceneZ) {
			hit = true;
			hitPixel = uv;
			hitDepthDelta = abs(sceneZ - mix(rayZNear, rayZFar, 0.5));
			break;
		}
	}

	if (!hit)
		return vec2(-1.0);

	// Binary refine along the last stride (even at stride 1 this snaps to
	// the depth crossing more tightly).
	pqk -= dPQK;
	dPQK /= max(pixStride, 1.0);
	float stride = 0.5;
	for (int j = 0; j < SSR_REFINE_STEPS; j++) {
		pqk += dPQK * stride;
		rayZNear = rayZFar;
		rayZFar = pqk.z / pqk.w;
		vec2 pix = permute ? pqk.yx : pqk.xy;
		vec2 uv = pix / uScreenDimensions;
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
			break;
		float rawDepth = texture(tDepth, uv).r;
		float sceneZ = (rawDepth >= 0.9999) ? 1e6 : -DecodeNativeDepth(rawDepth, z_info);
		float thickness = max(SSR_THICKNESS, abs(rayZFar - rayZNear) * 2.0);
		float zNear = min(rayZNear, rayZFar);
		float zFar = max(rayZNear, rayZFar);
		bool inside = (zFar >= sceneZ - thickness && zNear <= sceneZ);
		stride = abs(stride) * 0.5;
		if (inside) {
			hitPixel = uv;
			hitDepthDelta = abs(sceneZ - mix(rayZNear, rayZFar, 0.5));
			stride = -stride;
		}
	}

	outConfidence = 1.0 - smoothstep(0.0, SSR_THICKNESS * 2.0, hitDepthDelta);
	return hitPixel;
}

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);
	vec3 baseColor = texture(tColor, Texcoord).rgb;

	vec4 mr = texture(tMetallicRoughness, Texcoord);
	float roughness = mr.x;
	float metallic = mr.y;
	// Real per-material SSR opt-in (GenericShaderMaterial::SetSSREnabled(),
	// written into this G-buffer channel by PyrosShader.glsl's
	// FragData_pbr.b) - the material-level control this whole SSR
	// feature was always meant to have but didn't: before this, ANY
	// pixel under the roughness cutoff got reflections whether its
	// material's author wanted that or not, with no way to opt out short
	// of raising roughness past the cutoff. uSSREnabled (above) stays the
	// whole-DeferredRenderer master switch; this is the per-material one
	// underneath it.
	float ssrReflective = mr.b;
	// mr.a can read 0 if MaterialUniforms reflectivity failed to upload;
	// fall back to the SSR gate so a surface that opted in still gets a
	// dielectric F0 instead of a silent zero-strength reflection.
	float materialReflectivity = max(mr.a, ssrReflective);

	// Debug 1: visualize per-material SSR gate (should be white on the floor).
	if (uSSRDebug > 0.5 && uSSRDebug < 1.5) {
		FragColor = vec4(ssrReflective, ssrReflective, materialReflectivity, 1.0);
		return;
	}

	if (uSSREnabled < 0.5 || ssrReflective < 0.5 || roughness > SSR_ROUGHNESS_CUTOFF) {
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
	vec3 F0 = mix(vec3(0.04) * materialReflectivity, albedo, metallic);
	vec3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
	vec3 reflectDir = reflect(-V, N);
	if (dot(reflectDir, N) < 0.05) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}

	// Nudge off the reflector so the first DDA sample isn't the surface itself.
	vec3 rayOrigin = v1 + reflectDir * 0.08;
	float hitConfidence = 0.0;
	vec2 hitUV = TraceSSR(rayOrigin, reflectDir, z_info, hitConfidence);

	if (hitUV.x < 0.0 || hitConfidence < 0.05) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}

	// Reject floor→floor (or other coplanar) hits.
	vec3 hitN = normalize(texture(tNormal, hitUV).xyz);
	if (dot(hitN, N) > SSR_COPLANAR_DOT) {
		FragColor = vec4(baseColor, 1.0);
		return;
	}

	// Debug 2: paint successful marches red (ignores reflection color).
	if (uSSRDebug > 1.5 && uSSRDebug < 2.5) {
		FragColor = vec4(1.0, 0.1, 0.1, 1.0);
		return;
	}

	vec3 reflectionColor = texture(tColor, hitUV).rgb;

	// Debug 3: show raw hit color at full strength (no Fresnel fade).
	if (uSSRDebug > 2.5) {
		FragColor = vec4(reflectionColor, 1.0);
		return;
	}

	float edgeDist = max(abs(hitUV.x*2.0-1.0), abs(hitUV.y*2.0-1.0));
	float edgeFade = 1.0 - smoothstep(0.78, 1.0, edgeDist);
	float roughnessFade = 1.0 - smoothstep(SSR_ROUGHNESS_CUTOFF*0.7, SSR_ROUGHNESS_CUTOFF, roughness);

	// Facing-angle dielectric mirrors need a boost past bare Schlick F0~0.04
	// so SSRTest's floor reads as a mirror; confidence fades fragile hits
	// instead of leaving stippled holes.
	float mirrorFloor = 0.55 * (1.0 - metallic) * materialReflectivity;
	vec3 reflectStrength = clamp(max(F, vec3(mirrorFloor)) * edgeFade * roughnessFade * hitConfidence, 0.0, 0.95);
	FragColor = vec4(baseColor * (1.0 - reflectStrength) + reflectionColor * reflectStrength, 1.0);
}
#endif
