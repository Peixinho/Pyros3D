#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision highp float;
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
	// Real, per-scene-settable march parameters - see the
	// SSR_COARSE_STEPS/SSR_PIXEL_STRIDE comment below for why these are
	// explicit uniforms and not shader constants or an automatic
	// per-pixel scale.
	//
	// uSSRStepDistance is the coarse DDA's stride in SCREEN PIXELS (it was
	// declared here but never read at all until the island SSR demo needed
	// reflections to reach further than 128px; the march has been a
	// pixel-space DDA since the McGuire/Mara rewrite, so a view-space step
	// no longer has anything to scale). Clamped up to SSR_PIXEL_STRIDE
	// below, so the old room-scale values (SSRTest's 0.35, the
	// constructor's 0.22) all land on stride 1 and march exactly as they
	// did before. uSSRMaxDistance stays view-space: it only clips the
	// ray's far end, it cannot buy reach.
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
	// uv is gl_FragCoord-derived, so its Y origin follows the backend:
	// bottom-left on GL (matching NDC Y up) and top-left on Vulkan
	// (matching NDC Y down) - either way the line above lands on the
	// right NDC Y already. Metal is the mismatch: top-left gl_FragCoord
	// but NDC Y *up*, so the mapping comes out negated and the
	// reconstructed view-space ray points the wrong way vertically -
	// deferred lighting slid along Y with the camera instead of staying
	// put on the geometry.
#if defined(METAL)
	screenPos.y = -screenPos.y;
#endif
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
// McGuire/Mara screen-space DDA. The coarse stride is whichever is larger
// of uSSRStepDistance (floored at 1px) and the stride that lets
// SSR_COARSE_STEPS reach the ray's projected end. It used to be the
// former alone, so a reflection physically could not travel further than
// SSR_COARSE_STEPS*stride pixels: SSRTest (stride 1) drew 128px stubs
// under each sphere and nothing beyond. Never below 1: no temporal filter
// runs after this, so a sub-pixel stride only burns steps, and jitter
// would turn quantization into the stipple/comb noise SSRTest showed.
// The refine loop bisects the last stride, so a long stride costs
// precision only on thin geometry, not on where the hit lands.
const int SSR_COARSE_STEPS = 128;
const int SSR_REFINE_STEPS = 8;
const float SSR_PIXEL_STRIDE = 1.0;
const float SSR_MAX_PIXEL_STRIDE = 24.0;
// View-space units. The floor on the depth slab a ray may be behind a
// surface and still count as hitting it; the slab also grows with the
// z-span of one coarse step and with distance (depth precision and
// object size both scale with it).
const float SSR_THICKNESS = 1.0;
const float SSR_THICKNESS_PER_UNIT = 0.02;
const float SSR_MIN_PIXELS = 2.5;
const float SSR_COPLANAR_DOT = 0.95;
// Glossy reflection blur: pixels of spread per pixel travelled at
// roughness 1, its cap, and the taps spent on it.
const float SSR_GLOSSY_SPREAD = 0.35;
const float SSR_GLOSSY_MAX_PX = 28.0;
const int SSR_GLOSSY_TAPS = 8;

// uMatProj already goes through IRenderer::CaptureExtraUniform →
// device->TranslateProjectionMatrix() (Vulkan Y-flip + Z remap; identity
// on GL). Projecting with that matrix and doing ndc*0.5+0.5 therefore
// already lands on the same UV convention as gl_FragCoord-based Texcoord
// on BOTH backends. An older `#if VULKAN uv.y = 1-uv.y` here double-flipped
// Vulkan only and turned reflections into large wrong-colored floor blobs
// (SSRTest 2026-08-06). Do not reintroduce that flip.
// Metal is a third case, not the Vulkan one above: its
// TranslateProjectionMatrix() deliberately does NOT flip Y (Metal's NDC Y
// points up, same as GL - see that function's comment), so ndc*0.5+0.5
// comes out in GL's bottom-origin convention while its render targets are
// top-origin like Vulkan's. Nothing upstream compensates, so the traced
// ray walked the depth buffer mirrored about the horizon and essentially
// never registered a hit - SSRTest rendered with no reflections at all on
// Metal while GL showed them. This is the same flip secondpass*.glsl's
// getPosViewSpace() and SSAOEffect's kernel-sample reprojection need, for
// the same reason, and it is deliberately NOT the Vulkan flip that was
// removed on 2026-08-06.
vec2 ClipToUV(vec4 clipPos) {
	vec2 uv = (clipPos.xy / clipPos.w) * 0.5 + 0.5;
#if defined(METAL)
	uv.y = 1.0 - uv.y;
#endif
	return uv;
}

float SceneZAt(vec2 uv, vec4 z_info)
{
	float rawDepth = texture(tDepth, uv).r;
	return (rawDepth >= 0.9999) ? -1e6 : -DecodeNativeDepth(rawDepth, z_info);
}

// Perspective-correct screen-space ray trace against tDepth (McGuire & Mara
// JCGT 2014). Returns hit UV or (-1,-1) on miss. outConfidence is 1 at a
// tight surface contact and falls off as the ray only grazes the slab.
// outHitPixels is the screen distance travelled, for the glossy blur.
vec2 TraceSSR(vec3 rayOrigin, vec3 rayDir, vec4 z_info, out float outConfidence, out float outHitPixels)
{
	outConfidence = 0.0;
	outHitPixels = 0.0;
	float maxRayDistance = uSSRMaxDistance;
	float nearZ = uNearFar.x;

	// Clip the ray at the near plane so its end projects to a finite pixel.
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

	// Clip the screen-space segment to the viewport, so the stride below
	// is spent on pixels that exist rather than on the off-screen tail of
	// a 500-unit ray.
	vec2 dir2 = P1 - P0;
	float tMax = 1.0;
	if (dir2.x > 0.0) tMax = min(tMax, (uScreenDimensions.x - P0.x) / dir2.x);
	if (dir2.x < 0.0) tMax = min(tMax, -P0.x / dir2.x);
	if (dir2.y > 0.0) tMax = min(tMax, (uScreenDimensions.y - P0.y) / dir2.y);
	if (dir2.y < 0.0) tMax = min(tMax, -P0.y / dir2.y);
	tMax = max(tMax, 0.0);
	P1 = P0 + dir2 * tMax;
	Q1 = mix(Q0, Q1, tMax);
	k1 = mix(k0, k1, tMax);

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

	float reachStride = abs(delta.x) / float(SSR_COARSE_STEPS);
	float pixStride = clamp(max(uSSRStepDistance, reachStride), SSR_PIXEL_STRIDE, SSR_MAX_PIXEL_STRIDE);
	dP *= pixStride;
	dQ *= pixStride;
	dk *= pixStride;

	vec4 pqk = vec4(P0, Q0.z, k0);
	vec4 dPQK = vec4(dP, dQ.z, dk);

	float rayZPrev = pqk.z / pqk.w;
	bool hit = false;
	float hitThickness = SSR_THICKNESS;

	for (int i = 0; i < SSR_COARSE_STEPS; i++) {
		pqk += dPQK;
		float rayZ = pqk.z / pqk.w;
		float rayZNear = rayZPrev;
		rayZPrev = rayZ;

		vec2 pix = permute ? pqk.yx : pqk.xy;
		// Stay off the reflector itself (and its immediate neighbours).
		if (distance(pix, P0orig) < SSR_MIN_PIXELS)
			continue;

		vec2 uv = pix / uScreenDimensions;
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
			break;

		float sceneZ = SceneZAt(uv, z_info);
		if (sceneZ < -1e5)
			continue;

		float thickness = max(SSR_THICKNESS, abs(rayZ - rayZNear) * 2.0)
			+ (-sceneZ) * SSR_THICKNESS_PER_UNIT;
		float zMin = min(rayZNear, rayZ);
		float zMax = max(rayZNear, rayZ);
		// The step's z-range reaches behind the surface, but not so far
		// behind it that the ray went round the back of the object.
		if (zMin <= sceneZ && zMax >= sceneZ - thickness) {
			hit = true;
			hitThickness = thickness;
			break;
		}
	}

	if (!hit)
		return vec2(-1.0);

	// Bisect the last stride: lo is in front of the depth buffer, hi is
	// behind it. The hit is the first sample behind.
	vec4 lo = pqk - dPQK;
	vec4 hi = pqk;
	for (int j = 0; j < SSR_REFINE_STEPS; j++) {
		vec4 mid = (lo + hi) * 0.5;
		vec2 pix = permute ? mid.yx : mid.xy;
		float sceneZ = SceneZAt(pix / uScreenDimensions, z_info);
		if (mid.z / mid.w <= sceneZ)
			hi = mid;
		else
			lo = mid;
	}

	vec2 hitPix = permute ? hi.yx : hi.xy;
	vec2 hitUV = hitPix / uScreenDimensions;
	float depthDelta = abs(SceneZAt(hitUV, z_info) - hi.z / hi.w);
	// Relative to the slab the hit was accepted with - an absolute window
	// here rejected every hit further than a few units from the camera,
	// which on the island was the whole island.
	outConfidence = 1.0 - smoothstep(0.5, 1.0, depthDelta / hitThickness);
	outHitPixels = distance(hitPix, P0orig);
	return hitUV;
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
	float hitPixels = 0.0;
	vec2 hitUV = TraceSSR(rayOrigin, reflectDir, z_info, hitConfidence, hitPixels);

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

	// A rough surface's lobe spreads the reflection out with distance, so
	// widen a disk around the hit by roughness * distance travelled. A
	// single tap made every material under the cutoff a perfect mirror,
	// sharp edges and all, with a hard step where the cutoff fades it.
	// Taps that land on the sky or back on the reflector's own plane keep
	// the centre colour instead of bleeding it in.
	vec3 reflectionColor = texture(tColor, hitUV).rgb;
	float blurPx = min(roughness * roughness * hitPixels * SSR_GLOSSY_SPREAD, SSR_GLOSSY_MAX_PX);
	if (blurPx > 0.75) {
		vec3 sum = reflectionColor;
		float wsum = 1.0;
		for (int t = 0; t < SSR_GLOSSY_TAPS; t++) {
			// Golden-angle spiral: even coverage with no fixed pattern.
			float r = sqrt((float(t) + 0.5) / float(SSR_GLOSSY_TAPS));
			float a = float(t) * 2.39996;
			vec2 tapUV = hitUV + vec2(cos(a), sin(a)) * r * blurPx / uScreenDimensions;
			float w = (texture(tDepth, tapUV).r >= 0.9999
				|| dot(normalize(texture(tNormal, tapUV).xyz), N) > SSR_COPLANAR_DOT) ? 0.0 : 1.0;
			sum += texture(tColor, tapUV).rgb * w;
			wsum += w;
		}
		reflectionColor = sum / wsum;
	}

	// Debug 3: show raw hit color at full strength (no Fresnel fade).
	if (uSSRDebug > 2.5) {
		FragColor = vec4(reflectionColor, 1.0);
		return;
	}

	float edgeDist = max(abs(hitUV.x*2.0-1.0), abs(hitUV.y*2.0-1.0));
	float edgeFade = 1.0 - smoothstep(0.78, 1.0, edgeDist);
	float roughnessFade = 1.0 - smoothstep(SSR_ROUGHNESS_CUTOFF*0.7, SSR_ROUGHNESS_CUTOFF, roughness);
	// Fade out as the hit nears the march limit, not cut off at it.
	float hitDistance = length(getPosViewSpace(texture(tDepth, hitUV).r, hitUV * uScreenDimensions, z_info, uMatProj, vp) - v1);
	float distanceFade = 1.0 - smoothstep(0.75, 1.0, hitDistance / uSSRMaxDistance);

	// Facing-angle dielectric mirrors need a boost past bare Schlick F0~0.04
	// so SSRTest's floor reads as a mirror; confidence fades fragile hits
	// instead of leaving stippled holes.
	float mirrorFloor = 0.55 * (1.0 - metallic) * materialReflectivity;
	vec3 reflectStrength = clamp(max(F, vec3(mirrorFloor)) * edgeFade * roughnessFade * distanceFade * hitConfidence, 0.0, 0.95);
	FragColor = vec4(baseColor * (1.0 - reflectStrength) + reflectionColor * reflectStrength, 1.0);
}
#endif
