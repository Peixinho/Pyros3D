#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
void main() {
	gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT

#extension GL_EXT_gpu_shader4 : require
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
			shadow += shadowCube(shadowMap, vec4(position_ls.xyz, depth - scale*3.0) + vec4(vec2(x,y) * scale,0.0,0.0)).x;
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

uniform sampler2D tDiffuse;
uniform sampler2D tSpecular;
uniform sampler2D tDepth;
uniform sampler2D tNormal;
uniform vec2 uScreenDimensions;
uniform vec3 uLightPosition;
uniform float uLightRadius;
uniform vec4 uLightColor;
uniform vec2 uNearFar;
uniform mat4 uMatProj;

uniform samplerCubeShadow uShadowMap;
uniform mat4 uPointDepthsMVP[2];
uniform vec4 uPCFTexelSize;
uniform float uHaveShadowmap;

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
	getPosViewSpace(texture2D(tDepth, Texcoord).r, screenCoord, z_info, v1, uMatProj, vp);

	float pcf = 1.0;
	vec4 worldPos = vec4(v1, 1.0);

	if (uHaveShadowmap>0.0)
		pcf = PCFPOINT(uShadowMap, uPointDepthsMVP[0], uPointDepthsMVP[1], uPCFTexelSize.x, worldPos);

	vec3 vViewNormal = normalize(texture2D(tNormal, Texcoord).xyz);
	vec3 Color = texture2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec3 Specular = texture2D(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	float lightRadius = uLightRadius;
	vec3 lightPosition = uLightPosition;
	vec4 lightColor = uLightColor;

	vec3 lightDirection = normalize(lightPosition - v1);
	float n_dot_l = max(dot(lightDirection, vViewNormal), 0.0);
	float attenuation = Attenuation(v1, lightPosition, lightRadius);
	vec3 diffuseColor = attenuation * n_dot_l * lightColor.xyz * pcf;
	diffuse = vec4((diffuseColor * Color),1.0);

	vec3 eyeVec = normalize(-v1);
	vec3 halfVec = normalize(eyeVec + lightDirection);
	float specularPower = (n_dot_l>0.0?pow(max(dot(halfVec,vViewNormal),0.0), 50.0):0.0);
	specular = vec4(specularPower * attenuation * Specular * pcf, 1.0);
	
	gl_FragColor = diffuse;
}
#endif