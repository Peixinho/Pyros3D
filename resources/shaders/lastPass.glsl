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

// Fragment Color
#if defined(GLES2)
	vec4 FragColor;	
#else
	out vec4 FragColor;
#endif

uniform sampler2D tColor;
uniform vec2 uScreenDimensions;
void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);
	FragColor = texture(tColor, vec2(Texcoord.x,Texcoord.y));
	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
}
#endif
