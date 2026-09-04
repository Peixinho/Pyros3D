#if defined(GLES2)
	#define varying_in varying
	#define varying_out varying
	#define attribute_in attribute
	#define texture_2D texture2D
	#define texture_cube textureCube
	precision highp float;
#else
	#define varying_in in
	#define varying_out out
	#define attribute_in in
	#define texture_2D texture
	#define texture_cube texture
	#if defined(GLES3)
		precision highp float;
	#endif
#endif

#ifdef VERTEX
attribute_in vec3 aPosition, aNormal;
attribute_in vec2 aTexcoord;
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
varying_out vec4 vPosition;
void main()
{
	vPosition = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);
	gl_Position = vPosition;
}
#endif

#ifdef FRAGMENT
uniform sampler2D uTexture;
uniform float uTime;
uniform vec4 uColor, uColor2;
varying_in vec4 vPosition;
// Fragment Color
#if defined(GLES2)
	vec4 FragColor;	
#else
	out vec4 FragColor;
#endif
void main()
{
	vec4 color = texture(uTexture, vPosition.xy*6.0);
	float alpha = sin(uTime*5.0);
	if (alpha>0.0) {
		if (color.a>0.0)
			FragColor = vec4(color * uColor);
	}
	else {
		if (color.a==0.0)
		FragColor = vec4(uColor2);
	}

	FragColor.a *= abs(alpha);

	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
}
#endif
