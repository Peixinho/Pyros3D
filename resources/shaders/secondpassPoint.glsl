#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif
// See secondpassAmbient.glsl's identical comment. Two separate UBOs here
// (not one combined block referenced from both stages) - empirically,
// declaring a single uniform block used in both the VERTEX and FRAGMENT
// compilation units of one program triggered a real GL_INVALID_OPERATION
// at glDrawElements() time on this machine's Metal-backed GL driver (not
// just a style preference - confirmed via lldb backtrace pointing straight
// at this shader's draw call, gone once split back into two bindings).
// uProjectionMatrix/uViewMatrix/uModelMatrix are only read in VERTEX, the
// rest only in FRAGMENT - matches PyrosShader.glsl's own "declared in
// exactly one shader stage" discipline. Bindings 33/38 - see IMaterial.h's
// comment on extraUniforms[2]/ExtraUniformsBlock::binding for why these
// must all be globally distinct.
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
UBO_BINDING(33) uniform PointVertParams {
	mat4 uProjectionMatrix;
	mat4 uViewMatrix;
	mat4 uModelMatrix;
	float uUseFullscreenQuad;
};
// A varying mat4 occupies 4 consecutive locations (one per column), same
// as a vertex-attribute matrix elsewhere in this engine - locations 0-3.
IO_LOCATION(0) varying_out mat4 vProjectionMatrix;
void main() {
	// Real fix for point lights vanishing when the camera is near/inside
	// the light's radius - see DeferredRenderer.cpp's comment on
	// uUseFullscreenQuad. With no depth test, the sphere proxy's own
	// vertices going behind the near clip plane makes the GPU discard
	// the whole primitive before rasterization, before CullFace matters.
	// When the C++ side detects this and swaps in a real full-screen
	// quad mesh, skip the sphere's MVP transform entirely and emit that
	// quad's own already-correct clip-space positions directly, exactly
	// like secondpassAmbient.glsl/secondpassDirectional.glsl's identical
	// full-screen-quad passes do.
	if (uUseFullscreenQuad > 0.5)
		gl_Position = vec4(aPosition, 1.0);
	else
		gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
	vProjectionMatrix = uProjectionMatrix;
}
#endif

#ifdef FRAGMENT

float PCFPOINT(samplerCubeShadow shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos)
{
	vec4 position_ls = Matrix2 * pos;
	position_ls.xyz/=position_ls.w;
	vec4 abs_position = abs(position_ls);
	float fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));
	vec4 clip = Matrix1 * vec4(0.0, 0.0, fs_z, 1.0);
	float depth = (clip.z / clip.w) * 0.5 + 0.5;
	float shadow = 0.0;
	float x = 0.0;
	float y = 0.0;

	for (y = -1.5 ; y <=1.5 ; y+=1.0)
		for (x = -1.5 ; x <=1.5 ; x+=1.0)
			// See secondpassDirectional.glsl's comment on the pre-existing
			// scalar-swizzle bug - fixed here too.
			shadow += texture(shadowMap, vec4(position_ls.xyz, depth - scale*5.0) + vec4(vec2(x,y) * scale,0.0,0.0));
	shadow /= 16.0;
	return shadow;
}

float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
{
	float d = distance(Vertex,LightPosition);
	return clamp(1.0 - (1.0/Radius)*d, 0.0, 1.0);
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

	float NdotL = max(dot(N, L), 0.0);
	return (kD * albedo / PBR_PI + specularTerm) * radiance * NdotL;
}

SAMPLER_BINDING(0) uniform sampler2D tDiffuse;
SAMPLER_BINDING(1) uniform sampler2D tSpecular;
SAMPLER_BINDING(2) uniform sampler2D tDepth;
SAMPLER_BINDING(3) uniform sampler2D tNormal;
// PBR metallic/roughness G-buffer attachment - see PyrosShader.glsl's
// FragData_pbr (.r=roughness, .g=metalness).
SAMPLER_BINDING(5) uniform sampler2D tMetallicRoughness;
UBO_BINDING(38) uniform PointFragParams {
	vec2 uScreenDimensions;
	vec3 uLightPosition;
	float uLightRadius;
	vec4 uLightColor;
	vec2 uNearFar;
	mat4 uPointDepthsMVP[2];
	float uPCFTexelSize;
	float uHaveShadowmap;
};
IO_LOCATION(0) varying_in mat4 vProjectionMatrix;

SAMPLER_BINDING(4) uniform samplerCubeShadow uShadowMap;

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
	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);
	vec2 Out = vec2(uScreenDimensions.x, uScreenDimensions.y);
	vec4 vp = vec4(1.0, 1.0, 2.0/Out.x, 2.0/Out.y);
	vec3 v1;
	vec4 out_dim = vec4(uScreenDimensions.x, uScreenDimensions.y, 1.0/uScreenDimensions.x, 1.0/uScreenDimensions.y);
	vec2 screenCoord = vec2(uScreenDimensions.x*Texcoord.x, uScreenDimensions.y*Texcoord.y);

	getPosViewSpace(texture(tDepth, Texcoord).r, screenCoord, z_info, v1, vProjectionMatrix, vp);

	float pcf = 1.0;
	vec4 worldPos = vec4(v1, 1.0);

	if (uHaveShadowmap>0.0)
		pcf = PCFPOINT(uShadowMap, uPointDepthsMVP[0], uPointDepthsMVP[1], uPCFTexelSize, worldPos);

	vec3 vViewNormal = normalize(texture(tNormal, Texcoord).xyz);
	vec3 color = texture(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec3 specTint = texture(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	float lightRadius = uLightRadius;
	vec3 lightPosition = uLightPosition;
	vec4 lightColor = uLightColor;

	float attenuation = Attenuation(v1, lightPosition, lightRadius);

	vec2 mr = texture(tMetallicRoughness, Texcoord).rg;
	float roughness = mr.x;
	float metallic = mr.y;

	vec3 N = vViewNormal;
	vec3 V = normalize(-v1);
	vec3 L = normalize(lightPosition - v1);
	vec3 pbrColor = CalculatePBRLighting(N, V, L, lightColor.xyz, color, metallic, roughness, specTint);

	FragColor = vec4(pbrColor, 1.0) * attenuation;/*# * pcf;*/
}
#endif
