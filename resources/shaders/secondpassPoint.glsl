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
#define UBO_BINDING(n)
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
};
// A varying mat4 occupies 4 consecutive locations (one per column), same
// as a vertex-attribute matrix elsewhere in this engine - locations 0-3.
IO_LOCATION(0) varying_out mat4 vProjectionMatrix;
void main() {
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

SAMPLER_BINDING(0) uniform sampler2D tDiffuse;
SAMPLER_BINDING(1) uniform sampler2D tSpecular;
SAMPLER_BINDING(2) uniform sampler2D tDepth;
SAMPLER_BINDING(3) uniform sampler2D tNormal;
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
	vec3 Specular = texture(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	float lightRadius = uLightRadius;
	vec3 lightPosition = uLightPosition;
	vec4 lightColor = uLightColor;

	vec3 lightDirection = normalize(lightPosition - v1);
	float n_dot_l = max(dot(lightDirection, vViewNormal), 0.0);
	float attenuation = Attenuation(v1, lightPosition, lightRadius);
	vec3 diffuseColor = n_dot_l * lightColor.xyz;
	diffuse = vec4((diffuseColor * color),1.0);

	vec3 eyeVec = normalize(-v1);
	vec3 halfVec = normalize(eyeVec + lightDirection);
	float specularPower = (n_dot_l>0.0?pow(max(dot(halfVec,vViewNormal),0.0), 50.0):0.0);
	specular = vec4(specularPower * Specular, 1.0);

	FragColor = (diffuse + specular) * attenuation;/*# * pcf;*/
}
#endif
