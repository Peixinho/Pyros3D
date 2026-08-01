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
// Coarse march to find any crossing, then a short binary-search refine
// between the last miss and the hit - the textbook-correct version of
// what the old standalone ScreenSpaceReflectionEffect approximated with
// a flat, unrefined 256-step march sharing one uniform default across
// both backends (see VULKAN_ROADMAP.md - that fixed step count is what
// hung on Vulkan). ~37 worst-case texture fetches here versus 256, on
// top of the roughness gate above already skipping most pixels.
const int SSR_COARSE_STEPS = 32;
const int SSR_REFINE_STEPS = 5;
// uSSRStepDistance/uSSRMaxDistance (below, in LastPassFragParams) are
// real *uniforms*, not hardcoded constants - DeferredRenderer.cpp
// defaults them to 0.35/12.0 (this shader's original, proven-correct
// values for a human-scale scene) but a scene built at a very different
// scale (a tabletop diorama, a city block) genuinely needs different
// absolute march distances and can set its own via
// DeferredRenderer::SetSSRDistances(). An earlier version of this
// shader tried to make that automatic by scaling every distance by the
// current pixel's own view-space depth (abs(v1.z)) instead - it looked
// principled but was a real, shipped regression: a ray marching toward
// a *farther* surface (e.g. the floor stretching away from a nearby
// sphere) used a step size derived from the *near* pixel it started
// at, so thickness (below) ended up wrong for everything past the
// first few steps and reflections landed near the horizon instead of
// under the object casting them. Reverted to a real, explicit,
// per-scene-settable value instead of an automatic one that turned out
// to need a correctness guarantee ("thickness stays valid however far
// the ray has travelled") this shader doesn't actually have.
// How many step-lengths "behind" the sampled surface still counts as a
// real hit - the actual thickness test. Without this (the shader's
// original version), the hit test was `rayPos.z < samplePos.z` alone:
// true for a ray that has gone *any* distance past a surface, which
// treats every piece of geometry as infinitely thick going away from
// the camera. A ray that should pass cleanly behind a thin object (a
// leaf, a railing, a sheet of glass) and continue on to hit whatever
// real surface is farther back instead stops at the thin object's near
// face and reflects it - a real, visible false-positive this shader had
// no defense against at all.
const float SSR_THICKNESS_STEPS = 2.0;

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

// Same clip-to-UV math as ClipToUV() but *without* the Vulkan Y-flip -
// real, found-via-reproduction fix for a second, independent Y-axis bug
// (separate from the seam ClipToUV()'s own flip already fixed): every
// getPosViewSpace() call reconstructs a view-space position from a
// (depth, uv) pair, and its X/Y math implicitly assumes whatever uv
// convention the caller used - v1's own call (below, in main()) feeds it
// screenCoord = uScreenDimensions*Texcoord, gl_FragCoord's *unflipped*
// convention. Every other call in this shader fed it a ClipToUV()-derived
// (flipped, on Vulkan) uv instead - a real mismatch invisible to the ray
// march's own hit test (getPosViewSpace()'s Z output depends only on the
// sampled depth value, not uv, so hit detection - and the shapes/
// silhouettes visible in earlier debug screenshots - looked completely
// correct) but corrupted the X/Y of every reconstructed position used for
// anything past that: hitWorld's reprojection sampled a Y-shifted part of
// tPreviousFrameColor (confirmed empirically - bypassing reprojection
// and sampling tColor directly at hitUV gave the right color; going
// through the normal getPosViewSpace(hitUV,...) round-trip did not), and
// the main loop's `distance(samplePos, v1) < maxDistance` check compared
// a Y-corrupted samplePos against a correct v1. Texture *sampling*
// (texture(tDepth, hitUV) etc) still needs ClipToUV()'s flip - it's
// correct there, matching gl_FragCoord-based lookups. Reconstruction
// needs this instead, matching v1's own convention.
vec2 ClipToReconstructionUV(vec4 clipPos) {
	return (clipPos.xy / clipPos.w) * 0.5 + 0.5;
}

// Standard interleaved-gradient-noise hash (Jimenez 2014) - real fix for
// the coarse march's jagged/noisy-looking hit boundary. With a fixed
// SSR_COARSE_STEPS stride, every pixel's ray crosses a given surface at
// the same quantized set of distances, so the hit/miss boundary lines up
// into a visible staircase pattern (worst right at a reflection's silhouette
// edge) - found by temporarily bypassing this shader's Fresnel term during
// debugging, which otherwise mostly hides it at non-grazing angles.
// Jittering *where the march starts* (below) by a per-pixel fraction of one
// step turns that shared, structured quantization into per-pixel noise
// instead - the standard single-frame mitigation used by most ray-marched
// SSR implementations that don't have a temporal accumulation buffer to
// average it out further (this engine doesn't - see VULKAN_ROADMAP.md).
// Doesn't change what a ray can hit, only exactly where along its path each
// step lands, so it's a pure quality fix with no correctness implications.
float InterleavedGradientNoise(vec2 screenPos)
{
	const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * fract(dot(screenPos, magic.xy)));
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
	// Same uReflectivity value that already blends in env-map reflections
	// (see PyrosShader.glsl's uEnvmap mix()) - reused here as a dielectric
	// F0 scale (see F0 below), not a flat post-multiply on the final
	// reflection color. A flat multiply was tried first and is wrong: SSR
	// strength is already fully determined by Fresnel (from F0) and
	// roughness in a real PBR pipeline - there's no separate "how
	// reflective" scalar for that in the physics. The one place real
	// engines *do* add an artist dial here (Unreal's "Specular"
	// parameter, similarly a legacy holdover from before physically-
	// based texturing) folds it into F0 itself, since dielectric F0 (4%
	// reflectance at normal incidence, by convention) isn't derived from
	// BaseColor/Metallic the way a metal's is - there's real headroom for
	// an artist override there that doesn't exist post-Fresnel. A
	// material needs *both* SetSSREnabled(true) (the gate above) and a
	// real SetReflectivity() value for a *dielectric* surface to show
	// SSR - metals don't need it (see F0's mix() below: metallic=1
	// selects albedo as F0 regardless, same as any other PBR metal
	// workflow - a metal's reflectivity isn't a separate artist knob,
	// it's just its own color).
	float materialReflectivity = mr.a;

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
	// Dielectric F0 scaled by materialReflectivity (see its declaration
	// comment) instead of the flat vec3(0.04) this used before - at
	// reflectivity=1.0 this is exactly that same 0.04 (the standard
	// dielectric baseline, unchanged from before this feature existed),
	// scaling down toward 0 as the material's author dials it down.
	// Metals ignore it entirely (mix() selects albedo at metallic=1) -
	// correct, a metal's Fresnel response is its own color, not a
	// separate adjustable quantity.
	vec3 F0 = mix(vec3(0.04) * materialReflectivity, albedo, metallic);
	vec3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
	vec3 reflectDir = reflect(-V, N);

	// Per-scene-settable march distances - see the SSR_COARSE_STEPS
	// comment above on why these are real uniforms, not shader constants.
	float stepDistance = uSSRStepDistance;
	float thickness = stepDistance * SSR_THICKNESS_STEPS;
	float maxDistance = uSSRMaxDistance;

	// Jitter the first step by a per-pixel fraction of stepDistance - see
	// InterleavedGradientNoise()'s comment. Applied once, before the loop,
	// not re-added every iteration: that would just shift the whole ray by
	// a constant offset every step, changing nothing about the
	// quantization (still a fixed stride between samples) - jittering only
	// the start decorrelates *where in each pixel's stride* the first
	// sample falls, which is what actually breaks up the shared pattern.
	float jitter = InterleavedGradientNoise(gl_FragCoord.xy);
	vec3 rayPos = v1 + reflectDir * (stepDistance * jitter);
	vec3 prevRayPos = v1;
	// Real, found-via-reproduction fix: the surface Z the *previous* step
	// was compared against, tracked so a hit requires an actual local
	// sign change (was in front of a surface, now behind it) rather than
	// just "is my current step's Z coincidentally within `thickness` of
	// whatever real geometry happens to be at this step's screen
	// position" - the shader's old test only checked the latter, with no
	// memory of the previous step at all. That let a ray flying past an
	// object's silhouette (screen position jumping from "near the object"
	// to "far background" between two consecutive coarse steps) register
	// a false hit purely from a Z coincidence at the new, unrelated
	// location - confirmed via a real repro: a categorical debug (color
	// per known sphere) showed two small, disconnected extra reflection
	// blobs on the floor, each landing exactly on a real sphere's
	// position - impossible for an actual flat-mirror reflection (a
	// convex object's mirror image in a flat mirror is always a single
	// connected region, never two separate ones), so these were
	// confirmed false positives, not real geometry. v1 was itself
	// reconstructed from the current pixel's own real depth, so it's a
	// valid starting "previous surface Z".
	float prevSampleZ = v1.z;
	bool hit = false;

	for (int i = 0; i < SSR_COARSE_STEPS; i++) {
		prevRayPos = rayPos;
		rayPos += reflectDir * stepDistance;

		vec4 clipPos = uMatProj * vec4(rayPos, 1.0);
		if (clipPos.w <= 0.0) break;
		vec2 rayUV = ClipToUV(clipPos);
		if (rayUV.x < 0.0 || rayUV.x > 1.0 || rayUV.y < 0.0 || rayUV.y > 1.0) break;

		float sampleDepth = texture(tDepth, rayUV).r;
		vec3 samplePos = getPosViewSpace(sampleDepth, ClipToReconstructionUV(clipPos) * uScreenDimensions, z_info, uMatProj, vp);

		// rayPos.z (view space, more negative = farther, matching
		// getPosViewSpace's own -lDepth) has gone past the surface
		// actually stored at this screen position - a real ray-vs-
		// surface crossing test, not a "closest point ever seen" running
		// minimum (the old effect's actual test - see
		// VULKAN_ROADMAP.md - which isn't a real intersection test).
		// The extra `rayPos.z > samplePos.z - thickness` half is the real
		// thickness test this shader didn't have before: without it, any
		// ray that has gone even slightly past a surface counts as a
		// hit forever afterward, treating every piece of geometry as
		// infinitely thick facing away from the camera - a ray that
		// should pass behind a thin object (a railing, a leaf, a pane of
		// glass) and keep going to whatever's really behind it instead
		// stops at the thin object's near face and reflects that.
		// `prevRayPos.z >= prevSampleZ` is the real crossing requirement -
		// see prevSampleZ's comment above.
		if (prevRayPos.z >= prevSampleZ && rayPos.z < samplePos.z && rayPos.z > samplePos.z - thickness && length(rayUV - Texcoord) > 0.005 && distance(samplePos, v1) < maxDistance) {
			hit = true;
			break;
		}
		prevSampleZ = samplePos.z;
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
		vec3 samplePos = getPosViewSpace(sampleDepth, ClipToReconstructionUV(clipMid) * uScreenDimensions, z_info, uMatProj, vp);
		if (mid.z < samplePos.z) hi = mid; else lo = mid;
	}

	vec4 finalClip = uMatProj * vec4(hi, 1.0);
	vec2 hitUV = ClipToUV(finalClip);

	// Reconstruct the hit's world position using the *current* frame's
	// camera, then reproject through *last* frame's camera to sample
	// tPreviousFrameColor.
	float hitDepth = texture(tDepth, hitUV).r;
	vec3 hitView = getPosViewSpace(hitDepth, ClipToReconstructionUV(finalClip) * uScreenDimensions, z_info, uMatProj, vp);
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

	// Roughness-based blur, real fix for the previous version's binary
	// sharp-mirror-or-nothing look: tPreviousFrameColor now has real
	// mipmaps (generated every frame after the blit that populates it -
	// see DeferredRenderer.cpp), and sampling a higher LOD is the
	// standard cheap approximation of glossy-reflection cone tracing
	// used by most production SSR implementations (pre-filtered mip
	// chain instead of per-pixel importance sampling). Linear roughness
	// -> LOD isn't physically exact (real GGX lobe-to-mip mapping is a
	// steeper curve) but reads correctly: 0 roughness stays mip 0 (sharp
	// mirror), roughness approaching the cutoff pulls in a visibly
	// blurred reflection instead of just fading opacity.
	float reflectionLod = roughness * uMaxReflectionLod;
	vec3 reflectionColor = textureLod(tPreviousFrameColor, prevUV, reflectionLod).rgb;

	// Fade near the roughness cutoff and near screen edges, so a
	// reflection doesn't hard-pop when it walks off-screen or roughness
	// crosses the gate above.
	float edgeDist = max(abs(hitUV.x*2.0-1.0), abs(hitUV.y*2.0-1.0));
	float edgeFade = 1.0 - smoothstep(0.7, 1.0, edgeDist);
	float roughnessFade = 1.0 - smoothstep(SSR_ROUGHNESS_CUTOFF*0.7, SSR_ROUGHNESS_CUTOFF, roughness);

	// Real fix for reflections washing out to white at grazing angles
	// (found from a real screenshot at a shallow camera angle - the floor's
	// mirror image of colored spheres looked like flat white/gray blobs
	// instead of carrying their color): this used to be pure
	// `baseColor + reflectionColor*F*fades`, additive with no energy
	// conservation. Fresnel correctly approaches (1,1,1) at grazing
	// incidence for *any* material - that's real physics, not a bug - but
	// adding a near-full-strength reflection on top of the surface's
	// already-fully-lit diffuse baseColor double-counts energy, and the
	// resulting HDR sum reliably clips toward white once TonemapEffect's
	// ACES curve compresses it. A real surface's specular reflection
	// *replaces* the fraction of diffuse response Fresnel says didn't
	// scatter diffusely - baseColor needs to fade out by the same amount
	// the reflection fades in, not stay at full strength underneath it.
	// materialReflectivity's contribution is already baked into F via F0
	// above (a real PBR-consistent artist dial on dielectric reflectance,
	// not a second, physically-meaningless multiplier stacked on top of
	// Fresnel) - F alone is the correct, complete reflection strength.
	vec3 reflectStrength = F * edgeFade * roughnessFade;
	FragColor = vec4(baseColor * (1.0 - reflectStrength) + reflectionColor * reflectStrength, 1.0);
}
#endif
