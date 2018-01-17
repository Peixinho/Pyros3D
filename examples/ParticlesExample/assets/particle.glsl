#ifdef EMSCRIPTEN
precision mediump float;
#endif

#ifdef VERTEX
attribute vec3 aPosition, aNormal;
attribute vec2 aTexcoord;
uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
uniform float uTime;
varying vec2 vTexcoord;
varying float vLifetime;
varying float vTime;
varying float vRotation;
varying vec3 vDirection;
// Particle
attribute vec4 aParticlePosition, aParticleDetails;
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
varying vec2 vTexcoord;
varying float vLifetime;
varying float vTime;
varying float vRotation;
varying vec3 vDirection;
void main()
{
    vec2 texcoord = vec2(vTexcoord.x-0.5, vTexcoord.y -0.5);
    float angle = vRotation*vTime;
    mat2 RotationMatrix = mat2( cos(angle), -sin(angle), sin(angle), cos(angle) );
    texcoord *= RotationMatrix;
	gl_FragColor = vec4(texture2D(uTex0, texcoord+0.5));
    gl_FragColor.xyz += clamp(vRotation,0.0,1.0);
    gl_FragColor.w -= (vTime-vLifetime)*.1;

    // if (vTexcoord.x<0.01 || vTexcoord.x>0.99) { gl_FragColor = vec4(1,0,0,1); gl_FragColor.w = (1.0-(vTime-vLifetime)*.1); }
    // if (vTexcoord.y<0.01 || vTexcoord.y>0.99) { gl_FragColor = vec4(1,0,0,1); gl_FragColor.w = (1.0-(vTime-vLifetime)*.1); }
}
#endif