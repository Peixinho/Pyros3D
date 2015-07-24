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
