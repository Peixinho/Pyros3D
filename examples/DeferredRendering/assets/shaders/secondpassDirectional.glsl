#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
void main() {
	gl_Position = vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT
vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;

uniform float uOpacity;
uniform sampler2D tDiffuse;
uniform sampler2D tSpecular;
uniform sampler2D tDepth;
uniform sampler2D tNormal;
uniform vec2 uScreenDimensions;
uniform vec3 uLightDirection;
uniform vec4 uLightColor;
uniform vec2 uNearFar;
uniform mat4 uMatProj;

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
	vec2 screenSpaceRay = vec2(screenPos.x / uMatProj_local[0][0],-screenPos.y / uMatProj_local[1][1]);
	return screenSpaceRay;
}

vec3 getPosViewSpace(float depth_sampled, vec2 uv, vec4 z_info_local, out vec3 vpos, mat4 uMatProj_local, vec4 viewport_transform_local)
{
	vec2 screenSpaceRay = getPosViewSpace(uv, z_info_local, uMatProj_local, viewport_transform_local);

	float lDepth = DecodeNativeDepth(depth_sampled, z_info_local);
	vpos.xy = lDepth * screenSpaceRay;
	vpos.z = -lDepth;

	return vec3(screenSpaceRay, -1.0);
}

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);
	vec4 z_info = vec4(uNearFar.x, uNearFar.y, uNearFar.x*uNearFar.y, uNearFar.x - uNearFar.y);
	vec2 Out = vec2(uScreenDimensions.x, uScreenDimensions.y);
	vec4 vp = vec4(1.0, 1.0, 2.0/Out.x, 2.0/Out.y);
	vec3 v1; // v2, v3;
	vec4 out_dim = vec4(uScreenDimensions.x, uScreenDimensions.y, 1.0/uScreenDimensions.x, 1.0/uScreenDimensions.y);
	vec2 screenCoord = vec2(uScreenDimensions.x*Texcoord.x, uScreenDimensions.y*Texcoord.y);

	getPosViewSpace(texture2D(tDepth, Texcoord).r, screenCoord, z_info, v1, uMatProj, vp);
	/*getPosViewSpace(texture2D(tDepth, Texcoord + vec2(out_dim.z, 0)).r, screenCoord + vec2(1, 0), z_info, v2, uMatProj, vp);
	getPosViewSpace(texture2D(tDepth, Texcoord + vec2(0,out_dim.w)).r, screenCoord + vec2(0, 1), z_info, v3, uMatProj, vp);
	vec3 vViewNormal = normalize(cross(v3 - v1, v2 - v1));*/

	v1.y=-v1.y;

	vec3 vViewNormal = normalize(texture2D(tNormal, Texcoord).xyz);
	vec3 Color = texture2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec4 lightColor = uLightColor;

	vec3 lightDirection = normalize(-uLightDirection);
	float n_dot_l = max(dot(lightDirection, vViewNormal), 0.0);
	vec3 diffuseColor = n_dot_l * lightColor.xyz;

	diffuse = vec4((diffuseColor * Color),1.0);

	vec3 Specular = texture2D(tSpecular, vec2(Texcoord.x,Texcoord.y)).xyz;
	vec3 eyeVec = normalize(-v1);
	vec3 halfVec = normalize(lightDirection + eyeVec);
	float specularPower = (n_dot_l>0.0?pow(max(dot(halfVec,vViewNormal),0.0), 50.0):0.0);
	specular = vec4(specularPower * Specular, 1.0);
	
	gl_FragColor=diffuse + specular;
}
#endif