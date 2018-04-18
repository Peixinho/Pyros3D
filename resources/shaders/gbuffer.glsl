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
#if !defined(GLES2)
	layout(location = 0) out vec4 FragData_r;
	layout(location = 1) out vec4 FragData_g;
	layout(location = 2) out vec4 FragData_b;
#endif
void main() {
	if (!diffuseIsSet) {diffuse=uColor; diffuseIsSet=true;} else diffuse *= uColor;
	#if defined(GLES2)
		gl_FragData[0]=vec4(diffuse.xyz,1.0);
		gl_FragData[1]=vec4(uSpecular.xyz,1.0);
		gl_FragData[2]=vec4(normals.xyz,1.0);
	#else
		FragData_r=vec4(diffuse.xyz,1.0);
		FragData_g=vec4(uSpecular.xyz,1.0);
		FragData_b=vec4(normals.xyz,1.0);
	#endif
}
#endif
