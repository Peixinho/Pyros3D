#if defined(EMSCRIPTEN) || defined(GLES2_DESKTOP) || defined(GLES3_DESKTOP)
   precision mediump float;
#endif
#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
varying vec4 normals;
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
varying vec4 normals;
void main() {
	if (!diffuseIsSet) {diffuse=uColor; diffuseIsSet=true;} else diffuse *= uColor;
	gl_FragData[0]=vec4(diffuse.xyz,1.0);
	gl_FragData[1]=vec4(uSpecular.xyz,1.0);
	gl_FragData[2]=vec4(normals.xyz,1.0);
}
#endif