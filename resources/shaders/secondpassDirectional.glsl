#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision highp float;
#endif
// See secondpassAmbient.glsl's identical comment - binding 32 (not 27,
// AmbientFragParams' own - see IMaterial.h's comment on
// extraUniformsBinding for why these must all be globally distinct).
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

// Shadow filtering - the same functions as PyrosShader.glsl's (see there
// for the reasoning) and MaterialCodegen.cpp's; keep all four in step.
// uPCFTexelSize carries ILightComponent::GetShadowFilterPacked(): filter
// radius in texels in the integer part, normal bias / 8 in the fraction.
int ShadowFilterRadius(float packed) { return int(clamp(floor(packed), 0.0, 3.0)); }
float ShadowNormalBiasTexels(float packed) { return fract(packed) * 8.0; }

float ShadowTexelWorld(mat4 M, vec4 pos, float texels)
{
	vec3 rowX = vec3(M[0][0], M[1][0], M[2][0]);
	vec3 rowW = vec3(M[0][3], M[1][3], M[2][3]);
	float w = dot(vec4(M[0][3], M[1][3], M[2][3], M[3][3]), pos);
	return abs(w) / (texels * max(length(rowX - 0.5 * rowW), 1e-8));
}

// The G-buffer normal, turned toward the camera (a double-sided surface
// seen from behind stores the other side's).
vec3 ShadowReceiverNormal(vec3 viewNormal, vec3 viewPos)
{
	return dot(viewNormal, viewPos) > 0.0 ? -viewNormal : viewNormal;
}

float ShadowPCF2D(sampler2DShadow map, vec3 uvz, int K, vec2 size, vec4 rect)
{
	vec2 st = uvz.xy * size - 0.5;
	vec2 base = floor(st);
	vec2 f = st - base;
	float sum = 0.0;
	for (int j = -K; j <= K + 1; j++)
	{
		float wy = (j == -K) ? 1.0 - f.y : ((j == K + 1) ? f.y : 1.0);
		for (int i = -K; i <= K + 1; i++)
		{
			float wx = (i == -K) ? 1.0 - f.x : ((i == K + 1) ? f.x : 1.0);
			vec2 uv = clamp((base + vec2(float(i), float(j)) + 0.5) / size, rect.xy, rect.zw);
			sum += wx * wy * texture(map, vec3(uv, uvz.z));
		}
	}
	float n = float(2 * K + 1);
	return sum / (n * n);
}

vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;

// Cook-Torrance GGX BRDF - duplicated from PyrosShader.glsl's #ifdef PBR
// block (this file has no #include mechanism to share it with).
const float PBR_PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
	return a2 / max(PBR_PI * denom * denom, 1e-6);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculatePBRLighting(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 specTint)
{
	vec3 H = normalize(V + L);
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

	vec3 numerator = NDF * G * F;
	float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;
	// specTint is the G-buffer's specular attachment (see PyrosShader.glsl's
	// FragData_g comment): 1.0 for a PBR material, so that path is bit-for-bit
	// what it was; the classic material's own uSpecular otherwise, playing the
	// same role it plays in ForwardRenderer's Blinn-Phong loop
	// (`_specular += specularPower * L.Color * specular`) - a gate/tint on the
	// highlight, so a material with no SpecularColor usage gets none at all.
	// It multiplies the finished specular term rather than folding into F0:
	// Schlick's F0 + (1-F0)*(1-cos)^5 returns full reflectance at grazing
	// angles no matter how small F0 is, so a specTint-scaled F0 turned "no
	// highlight" into "grazing-only highlight" - a real bright rim/blob on
	// the floor and ceiling, worse than the flat look it replaced.
	vec3 specularTerm = (numerator / denom) * specTint;

	// Tinted too, so the energy taken out of the diffuse lobe matches the
	// specular actually emitted (specTint == 0 -> full diffuse, as forward).
	vec3 kS = F * specTint;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	// The engine's light colour is irradiance/PI, not irradiance - see
	// PyrosShader.glsl's identical comment. Recovering it here is what keeps
	// a deferred surface as bright as the same material under the same light
	// in the classic forward path, which has no 1/PI at all; without it every
	// deferred pixel came out exactly PI (3.14x) too dark.
	vec3 irradiance = radiance * PBR_PI;

	float NdotL = max(dot(N, L), 0.0);
	return (kD * albedo / PBR_PI + specularTerm) * irradiance * NdotL;
}

SAMPLER_BINDING(0) uniform sampler2D tDiffuse;
SAMPLER_BINDING(1) uniform sampler2D tSpecular;
SAMPLER_BINDING(2) uniform sampler2D tDepth;
SAMPLER_BINDING(3) uniform sampler2D tNormal;
// PBR metallic/roughness G-buffer attachment - see PyrosShader.glsl's
// FragData_pbr (.r=roughness, .g=metalness).
SAMPLER_BINDING(5) uniform sampler2D tMetallicRoughness;
UBO_BINDING(32) uniform DirectionalFragParams {
	vec2 uScreenDimensions;
	vec3 uLightDirection;
	vec4 uLightColor;
	vec2 uNearFar;
	mat4 uMatProj;
	float uPCFTexelSize;
	mat4 uDirectionalDepthsMVP[4];
	vec4 uDirectionalShadowFar;
	float uHaveShadowmap;
};

SAMPLER_BINDING(4) uniform sampler2DShadow uShadowMap;

float ShadowCascadeFar(vec4 splits, int c)
{
	if (c == 0) return splits.x;
	if (c == 1) return splits.y;
	if (c == 2) return splits.z;
	return splits.w;
}

float ShadowDirectionalCascade(int c, bool multi, float packed, vec4 pos, vec3 n)
{
	mat4 M = uDirectionalDepthsMVP[c];
	vec2 size = vec2(textureSize(uShadowMap, 0));
	float tile = multi ? size.x * 0.5 : size.x;
	vec4 p = vec4(pos.xyz + n * (ShadowNormalBiasTexels(packed) * ShadowTexelWorld(M, pos, tile)), 1.0);
	vec4 coord = M * p;
	vec4 rect = vec4(0.0, 0.0, 1.0, 1.0);
	if (multi)
	{
		vec2 off = vec2((c == 1 || c == 3) ? 0.5 : 0.0, (c >= 2) ? 0.5 : 0.0);
		coord.xy = coord.xy * 0.5 + off;
		rect = vec4(off + 0.5 / size, off + 0.5 - 0.5 / size);
	}
	return ShadowPCF2D(uShadowMap, coord.xyz, ShadowFilterRadius(packed), size, rect);
}

// Cascade picked from the fragment's linear view depth, cross-faded into
// the next over the last 10% of each and faded out after the last - see
// PyrosShader.glsl's DirectionalShadowFactor. uDirectionalShadowFar holds
// linear view distances now (DirectionalLight::GetCascadeSplits()); this
// copy used to compare window depth against thresholds built from the
// untranslated projection, which picked the wrong cascade on Vulkan.
float DirectionalShadowFactor(float packed, vec4 pos, vec3 n)
{
	vec4 splits = uDirectionalShadowFar;
	bool multi = splits.y > 0.0;
	int count = !multi ? 1 : (splits.w > 0.0 ? 4 : (splits.z > 0.0 ? 3 : 2));
	float depth = -pos.z;
	int c = count;
	for (int i = 3; i >= 0; i--)
		if (i < count && depth < ShadowCascadeFar(splits, i)) c = i;
	if (c >= count) return 1.0;
	float cFar = ShadowCascadeFar(splits, c);
	float t = clamp((depth - cFar * 0.9) / (cFar * 0.1), 0.0, 1.0);
	float s = ShadowDirectionalCascade(c, multi, packed, pos, n);
	if (t > 0.0)
	{
		float next = (c + 1 < count) ? ShadowDirectionalCascade(c + 1, multi, packed, pos, n) : 1.0;
		s = mix(s, next, t);
	}
	return s;
}

// Fragment Color
IO_LOCATION(0) out vec4 FragColor;

// Reconstruct Positions and Normals
float DecodeLinearDepth(float z, vec4 z_info_local)
{
	return z_info_local.x - z * z_info_local.w;
}

float DecodeNativeDepth(float native_z, vec4 z_info_local)
{
	return z_info_local.z / (native_z * z_info_local.w + z_info_local.y);
}

vec2 getPosViewSpace(vec2 uv, vec4 z_info_local, mat4 uMatProj_local, vec4 viewport_transform_local)
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
	vec2 screenSpaceRay = vec2(screenPos.x / uMatProj_local[0][0],screenPos.y / uMatProj_local[1][1]);
	return screenSpaceRay;
}

vec3 getPosViewSpace(float depth_sampled, vec2 uv, vec4 z_info_local, out vec3 vpos, mat4 uMatProj_local, vec4 viewport_transform_local)
{
	vec2 screenSpaceRay = getPosViewSpace(uv, z_info_local, uMatProj_local, viewport_transform_local);

	float lDepth = DecodeNativeDepth(depth_sampled, z_info_local);
	vpos.xy = lDepth * screenSpaceRay;
	vpos.z = -lDepth;

	return vec3(screenSpaceRay, -1);
}

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);
	// See secondpassAmbient.glsl's identical comment - depth clears to
	// 1.0 (far) at background/sky pixels, which this shader would
	// otherwise light using whatever garbage sits in the cleared G-buffer
	// there.
	if (texture(tDepth, Texcoord).r >= 1.0) discard;
	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);
	vec2 Out = vec2(uScreenDimensions.x, uScreenDimensions.y);
	vec4 vp = vec4(1.0, 1.0, 2.0/Out.x, 2.0/Out.y);
	vec3 v1;
	vec4 out_dim = vec4(uScreenDimensions.x, uScreenDimensions.y, 1.0/uScreenDimensions.x, 1.0/uScreenDimensions.y);
	vec2 screenCoord = vec2(uScreenDimensions.x*Texcoord.x, uScreenDimensions.y*Texcoord.y);

	getPosViewSpace(texture(tDepth, Texcoord).r, screenCoord, z_info, v1, uMatProj, vp);

	vec3 vViewNormal = normalize(texture(tNormal, Texcoord).xyz);
	vec3 color = texture(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec3 specTint = texture(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec4 lightColor = uLightColor;

	float pcf = 1.0;
	vec4 worldPos = vec4(v1, 1.0);

	if (uHaveShadowmap>0.0)
	{
		pcf = DirectionalShadowFactor(uPCFTexelSize, worldPos, ShadowReceiverNormal(vViewNormal, v1));
	}
	vec2 mr = texture(tMetallicRoughness, Texcoord).rg;
	float roughness = mr.x;
	float metallic = mr.y;

	vec3 N = vViewNormal;
	vec3 V = normalize(-v1);
	vec3 L = normalize(-uLightDirection);
	vec3 pbrColor = CalculatePBRLighting(N, V, L, lightColor.xyz, color, metallic, roughness, specTint);

	FragColor = vec4(pbrColor, 1.0) * pcf;
}
#endif
