#if defined(GLES2) || defined(GLES3) 
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

#if defined(GLES2)
	vec4 FragColor;
#else
	out vec4 FragColor;
#endif
uniform vec4 uColor;

void main()
{
	FragColor = uColor;
	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
}
#endif
