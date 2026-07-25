#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif
// See secondpassAmbient.glsl's identical comment - binding 32 (not 27,
// AmbientFragParams' own - see IMaterial.h's comment on
// extraUniformsBinding for why these must all be globally distinct).
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

float PCFDIRECTIONAL(sampler2DShadow shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade)
{
	vec4 coord = sMatrix * pos;
	if (MoreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(width,height);
	float shadow = 0.0;
	float x = 0.0;
	float y = 0.0;

	for (y = -1.5 ; y <=1.5 ; y+=1.0)
		for (x = -1.5 ; x <=1.5 ; x+=1.0)
			// texture() on a shadow sampler already returns a scalar float
			// (the depth-compare result) - a trailing .x swizzle on a
			// scalar is invalid GLSL, tolerated on some drivers but
			// rejected outright by macOS's real GL41 compiler (and
			// glslang's Vulkan validation). Pre-existing bug, unrelated to
			// this pass's Vulkan-compat work - fixed here since it was
			// blocking verification of that work (shader link failure ->
			// shaderProgram==0 -> identical symptom either backend).
			shadow += texture(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0)));
	shadow /= 16.0;
	return shadow;
}

vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;

SAMPLER_BINDING(0) uniform sampler2D tDiffuse;
SAMPLER_BINDING(1) uniform sampler2D tSpecular;
SAMPLER_BINDING(2) uniform sampler2D tDepth;
SAMPLER_BINDING(3) uniform sampler2D tNormal;
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

	getPosViewSpace(texture(tDepth, Texcoord).r, screenCoord, z_info, v1, uMatProj, vp);

	vec3 vViewNormal = normalize(texture(tNormal, Texcoord).xyz);
	vec3 color = texture(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec4 lightColor = uLightColor;

	float pcf = 1.0;
	vec4 worldPos = vec4(v1, 1.0);

	if (uHaveShadowmap>0.0)
	{
	    bool MoreThanOneCascade = (uDirectionalShadowFar.y>0.0);
	    if (texture(tDepth, Texcoord).r<uDirectionalShadowFar.x) pcf = PCFDIRECTIONAL(uShadowMap, 0.0, 0.0, uDirectionalDepthsMVP[0],uPCFTexelSize,worldPos, MoreThanOneCascade);
	    else if (texture(tDepth, Texcoord).r<uDirectionalShadowFar.y) pcf = PCFDIRECTIONAL(uShadowMap, 0.5,0.0, uDirectionalDepthsMVP[1],uPCFTexelSize,worldPos, MoreThanOneCascade);
	    else if (texture(tDepth, Texcoord).r<uDirectionalShadowFar.z) pcf = PCFDIRECTIONAL(uShadowMap, 0.0, 0.5, uDirectionalDepthsMVP[2],uPCFTexelSize,worldPos, MoreThanOneCascade);
	    else if (texture(tDepth, Texcoord).r<uDirectionalShadowFar.w) pcf = PCFDIRECTIONAL(uShadowMap, 0.5,0.5, uDirectionalDepthsMVP[3],uPCFTexelSize,worldPos, MoreThanOneCascade);
	}
	vec3 lightDirection = normalize(-uLightDirection);
	float n_dot_l = max(dot(lightDirection, vViewNormal), 0.0);
	vec3 diffuseColor = n_dot_l * lightColor.xyz;

	diffuse = vec4((diffuseColor * color),1.0);

	vec3 Specular = texture(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec3 eyeVec = normalize(-v1);
	vec3 halfVec = normalize(lightDirection + eyeVec);
	float specularPower = (n_dot_l>0.0?pow(max(dot(halfVec,vViewNormal),0.0), 50.0):0.0);
	specular = vec4(specularPower * Specular, 1.0);

	FragColor = (diffuse + specular) * pcf;
}
#endif
