#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
void main() {
	gl_Position = vec4(aPosition,1.0);
}
#endif

#ifdef FRAGMENT

uniform sampler2D tColor;
uniform vec2 uScreenDimensions;

void main() {
	vec2 Texcoord = vec2(gl_FragCoord.x/uScreenDimensions.x, gl_FragCoord.y/uScreenDimensions.y);
	gl_FragColor = texture2D(tColor, vec2(Texcoord.x,Texcoord.y));
}
#endif