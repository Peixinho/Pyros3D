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
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
uniform float uTime;
varying_out vec2 vTexcoord;
varying_out float vLifetime;
varying_out float vTime;
varying_out float vRotation;
varying_out vec3 vDirection;
// Particle
attribute_in vec4 aParticlePosition, aParticleDetails;
void main()
{
    vRotation = aParticlePosition.w;
    vTexcoord = aTexcoord;
    vDirection = aParticleDetails.xyz;
    vLifetime = aParticleDetails.w;
    vTime = uTime;

    mat4 ModelMatrix = uModelMatrix;
    ModelMatrix[3].xyz += aParticlePosition.xyz;

    mat4 ModelView = (uViewMatrix * ModelMatrix);

    float scale = 1.0;
    scale = (vTime-vLifetime)+0.05;

    ModelView[0][0] = scale; ModelView[0][1] = 0.0; ModelView[0][2] = 0.0;
    ModelView[1][0] = 0.0; ModelView[1][1] = scale; ModelView[1][2] = 0.0;
    ModelView[2][0] = 0.0; ModelView[2][1] = 0.0; ModelView[2][2] = scale;
    
	gl_Position = uProjectionMatrix * (ModelView * (vec4(aPosition.xyz,1.0)));
}
#endif

#ifdef FRAGMENT
uniform sampler2D uTex0;
varying_in vec2 vTexcoord;
varying_in float vLifetime;
varying_in float vTime;
varying_in float vRotation;
varying_in vec3 vDirection;

// Fragment Color
#if defined(GLES2)
	vec4 FragColor;	
#else
	out vec4 FragColor;
#endif

void main()
{
    vec2 texcoord = vec2(vTexcoord.x-0.5, vTexcoord.y -0.5);
    float angle = vRotation*vTime;
    mat2 RotationMatrix = mat2( cos(angle), -sin(angle), sin(angle), cos(angle) );
    texcoord *= RotationMatrix;
	FragColor = vec4(texture2D(uTex0, texcoord+0.5));
    FragColor.xyz += clamp(vRotation,0.0,1.0);
    FragColor.w -= (vTime-vLifetime)*.1;

	#if defined(GLES2)
		gl_FragColor = FragColor;
	#endif
    // if (vTexcoord.x<0.01 || vTexcoord.x>0.99) { gl_FragColor = vec4(1,0,0,1); gl_FragColor.w = (1.0-(vTime-vLifetime)*.1); }
    // if (vTexcoord.y<0.01 || vTexcoord.y>0.99) { gl_FragColor = vec4(1,0,0,1); gl_FragColor.w = (1.0-(vTime-vLifetime)*.1); }
}
#endif
