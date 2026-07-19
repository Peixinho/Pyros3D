#define varying_in in
#define varying_out out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
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
out vec4 FragColor;

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);

	vec3 ambient;

	ambient.x = texture_2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.y = texture_2D(tSpecular, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.z = texture_2D(tNormal, vec2(Texcoord.x,Texcoord.y)).w;

	vec3 color = texture_2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;

	FragColor=vec4(ambient * color, 1.0);
}
#endif
