#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
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

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);

	vec3 ambient;

	ambient.x = texture2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.y = texture2D(tSpecular, vec2(Texcoord.x,Texcoord.y)).w;
	ambient.z = texture2D(tNormal, vec2(Texcoord.x,Texcoord.y)).w;

	vec3 Color = texture2D(tDiffuse, vec2(Texcoord.x,Texcoord.y)).xyz;
	
	gl_FragColor=vec4(ambient * Color, 1.0);
}
#endif