#if defined(GLES2)
	#define varying_in varying
	#define varying_out varying
	#define attribute_in attribute
	#define texture_2D texture2D
	#define texture_cube textureCube
	precision mediump float;
#else
	#define varying_in in
	#define varying_out out
	#define attribute_in in
	#define texture_2D texture
	#define texture_cube texture
	#if defined(GLES3)
		precision mediump float;
	#endif
#endif

#ifdef VERTEX
attribute_in vec3 aPosition, aNormal;
attribute_in vec2 aTexcoord;
attribute_in mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
void main() {
	gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT

float PCFSPOT(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos)
{
	vec4 coord = sMatrix * pos;
	coord.xyz/=coord.w;
	float shadow = 0.0;
	float x = 0.0;
	float y = 0.0;
	for (y = -1.5 ; y <=1.5 ; y+=1.0)
		for (x = -1.5 ; x <=1.5 ; x+=1.0)
			shadow += texture(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;
	shadow /= 16.0;
	return shadow;
}

float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
{
	float d = distance(Vertex,LightPosition);
	return clamp(1.0 - (1.0/Radius)*d, 0.0, 1.0);
}

float DualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone)
{
    if (cosOutterCone>0.0 || cosInnerCone>0.0) {
        vec3 to_light = normalize(SpotLightPosition-Vertex);
        float angle = dot(-to_light, normalize(SpotLightDirection));
        float funcX = 1.0/(cosInnerCone-cosOutterCone);
        float funcY = -funcX * cosOutterCone;
        return clamp(angle*funcX+funcY,0.0,1.0);
    }
    return 0.0;
}

vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;

uniform sampler2D tDiffuse;
uniform sampler2D tSpecular;
uniform sampler2D tDepth;
uniform sampler2D tNormal;
uniform vec2 uScreenDimensions;
uniform vec3 uLightPosition;
uniform vec3 uLightDirection;
uniform float uLightRadius;
uniform float uOutterCone;
uniform float uInnerCone;
uniform vec4 uLightColor;
uniform vec2 uNearFar;
uniform mat4 uMatProj;

uniform mat4 uSpotDepthsMVP;
uniform sampler2DShadow uShadowMap;
uniform float uPCFTexelSize;
uniform float uHaveShadowmap;

// Fragment Color
#if defined(GLES2)
	vec4 FragColor;	
#else
	out vec4 FragColor;
#endif

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
	vec3 Specular = texture(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	float lightRadius = uLightRadius;
	vec3 lightPosition = uLightPosition;
	vec4 lightColor = uLightColor;

	vec3 lightDirection = normalize(-uLightDirection);
	float n_dot_l = max(dot(lightDirection, vViewNormal), 0.0);
	float attenuation = Attenuation(v1, lightPosition, lightRadius);
	float innerCone = uInnerCone;
	float outterCone = uOutterCone;
	float spotEffect = 1.0 - DualConeSpotLight(v1, lightPosition, lightDirection, outterCone, innerCone);

	vec3 eyeVec = normalize(-v1);
	vec3 halfVec = normalize(eyeVec + lightDirection);
	float specularPower = (n_dot_l>0.0?pow(max(dot(halfVec,vViewNormal),0.0), 50.0):0.0);
	
	float pcf = 1.0;
	vec4 worldPos = vec4(v1, 1.0);

	if (uHaveShadowmap>0.0)
		pcf = PCFSPOT(uShadowMap, uSpotDepthsMVP, uPCFTexelSize, worldPos);

	vec3 diffuseColor = spotEffect * attenuation * n_dot_l * lightColor.xyz;
	diffuse = vec4((diffuseColor * color),1.0);
	specular = vec4(specularPower * spotEffect * attenuation * Specular, 1.0);
	
	FragColor = (diffuse + specular) * pcf;

	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
}
#endif
