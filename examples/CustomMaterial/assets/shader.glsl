#ifdef EMSCRIPTEN
precision mediump float;
#endif

#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;

void main()
{
	gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT
uniform vec4 uColor;

void main()
{
	gl_FragColor = uColor;
}
#endif