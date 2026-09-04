#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision highp float;
#endif

#ifdef VERTEX
attribute_in vec3 aPosition, aNormal;
attribute_in vec2 aTexcoord;
varying_out vec4 normals;
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
void main() {
	normals = uViewMatrix * uModelMatrix * vec4(aNormal,0.0);
	//normals = vec4(aTexcoord, 0.0, 0.0);
	gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT
vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;
uniform float uOpacity;
uniform vec4 uColor;
uniform vec4 uSpecular;
varying_in vec4 normals;
layout(location = 0) out vec4 FragData_r;
layout(location = 1) out vec4 FragData_g;
layout(location = 2) out vec4 FragData_b;
void main() {
	if (!diffuseIsSet) {diffuse=uColor; diffuseIsSet=true;} else diffuse *= uColor;
	FragData_r=vec4(diffuse.xyz,1.0);
	FragData_g=vec4(uSpecular.xyz,1.0);
	FragData_b=vec4(normals.xyz,1.0);
}
#endif
