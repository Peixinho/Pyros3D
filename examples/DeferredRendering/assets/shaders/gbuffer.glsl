#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
varying vec3 vNormal;
varying vec4 vWorldPosition;
varying vec2 vTexcoord;
void main() {
gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
vNormal = aNormal;
vNormal = normalize((uModelMatrix * vec4(aNormal,0)).xyz);
vWorldPosition=uModelMatrix * vec4(aPosition,1.0);
vTexcoord = aTexcoord;
}
#endif

#ifdef FRAGMENT
vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
vec4 specular = vec4(0.0,0.0,0.0,1.0);
bool diffuseIsSet = false;
uniform float uOpacity;
varying vec3 vNormal;
varying vec4 vWorldPosition;
uniform vec4 uColor;
varying vec2 vTexcoord;
vec2 Texcoord = vTexcoord;
void main() {
vec3 Normal = vNormal;
if (!diffuseIsSet) {diffuse=uColor; diffuseIsSet=true;} else diffuse *= uColor;
gl_FragData[0]=vec4(diffuse.xyz,1.0);
gl_FragData[1]=vec4(specular.xyz,1.0);
gl_FragData[2]=vec4(Normal.xyz,1.0);
gl_FragData[3]=vWorldPosition;
}
#endif