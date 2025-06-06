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
			shadow += texture(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;
	shadow /= 16.0;
	return shadow;
}

vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;

uniform sampler2D tDiffuse;
uniform sampler2D tSpecular;
uniform sampler2D tDepth;
uniform sampler2D tNormal;
uniform vec2 uScreenDimensions;
uniform vec3 uLightDirection;
uniform vec4 uLightColor;
uniform vec2 uNearFar;
uniform mat4 uMatProj;

uniform sampler2DShadow uShadowMap;
uniform float uPCFTexelSize;
uniform mat4 uDirectionalDepthsMVP[4];
uniform vec4 uDirectionalShadowFar;
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

	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
}
#endif
