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

uniform sampler2D tDiffuse;
uniform sampler2D tSpecular;
uniform sampler2D tDepth;
uniform sampler2D tNormal;
uniform vec2 uScreenDimensions;

// Fragment Color
#if defined(GLES2)
	vec4 FragColor;	
#else
	out vec4 FragColor;
#endif

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);

	vec3 ambient;

	ambient.x = texture2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.y = texture2D(tSpecular, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.z = texture2D(tNormal, vec2(Texcoord.x,Texcoord.y)).w;

	vec3 color = texture(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	
	FragColor=vec4(ambient * color, 1.0);

	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
}
#endif
