#define varying_in in
#define varying_smooth_in smooth in
#define varying_out out
#define varying_smooth_out smooth out
#define attribute_in in
#define texture_2D texture
#define texture_cube texture
#if defined(GLES3)
	precision mediump float;
#endif

#define MAX_BONES 60
#define MAX_LIGHTS 4

// Vulkan/SPIR-V requires every attribute/varying to have a static
// layout(location=N), and every UBO/sampler a static layout(binding=N).
// GL needs neither: it matches varyings by name at link time, and UBO/
// sampler bindings are already assigned at runtime via
// glUniformBlockBinding()/glUniform1i (see
// GLRenderDevice::BindUniformBlockIfPresent and IRenderer's
// uniform-sending loops) - so adding these is never required for GL.
// It was tried unconditionally (any GLSL >= 330/420 "should" tolerate the
// syntax) and reverted: macOS's compatibility-profile GL 4.1 driver
// reserves part of its varying-interpolant budget for legacy built-ins
// (gl_TexCoord[8] and friends, since plain "#version 410" with no "core"
// suffix is a compatibility-profile shader) and throws "Implementation
// limit of 128 varying components exceeded" once an explicit VARYING
// location goes much past single digits - reproduced with a single
// declared vec4 varying at location 14 and nothing else. Since GL never
// needs these qualifiers, the safe fix is to only emit them when actually
// compiling for Vulkan (no such legacy tax there), leaving every GL
// profile's generated source byte-for-byte unchanged. Whoever compiles
// this file for Vulkan must #define VULKAN before #define VERTEX/FRAGMENT,
// the same way GL's #version line is prefixed by BuildShaderSource().
// "Loose" non-block uniforms (uModelMatrix, uOpacity, etc.) are still
// rejected by Vulkan/SPIR-V and are deliberately left as-is - turning them
// into descriptor blocks is real design work that belongs with the actual
// VulkanRenderDevice (Phase 3), not a macro bolted onto the shared GL
// shader here.
#if defined(VULKAN)
	#define IO_LOCATION(n) layout(location = n)
	#define UBO_BINDING(n) layout(std140, binding = n)
	#define SAMPLER_BINDING(n) layout(binding = n)
#else
	#define IO_LOCATION(n)
	#define UBO_BINDING(n) layout(std140)
	#define SAMPLER_BINDING(n)
#endif

// Attribute locations - unique per name, VERTEX-stage input namespace.
// aInstancedTransform is a mat4 and consumes 4 consecutive locations.
#define LOC_aPosition 0
#define LOC_aNormal 1
#define LOC_aTexcoord 2
#define LOC_aColor 3
#define LOC_aSize 4
#define LOC_aTangent 5
#define LOC_aBitangent 6
#define LOC_aBonesID 7
#define LOC_aBonesWeight 8
#define LOC_aInstancedTransform 9

// Varying locations - unique per name, shared between this file's VERTEX
// (out) and FRAGMENT (in) compilations via these same macros, so the two
// separately-compiled stages always agree (required for SPIR-V/Vulkan,
// which matches VS outputs to FS inputs by location, not name). mat3/mat4
// varyings consume 3/4 consecutive locations each.
#define LOC_vColor 0
#define LOC_vTexcoord 1
#define LOC_vNormal 2
#define LOC_vWorldPositionShadow 3
#define LOC_vWorldPosition 4
#define LOC_vTangentMatrix 5
#define LOC_vCameraPos 8
#define LOC_vTRed 9
#define LOC_vTGreen 10
#define LOC_vTBlue 11
#define LOC_vReflectionFactor 12
#define LOC_v3Texcoord 13
#define LOC_gbuffer_normals 14
#define LOC_vViewMatrix 15
#define LOC_vScreenSpaceWorldPosition 19
#define LOC_vPrvScreenSpaceWorldPosition 20

// Existing UBO/sampler bindings - match the fixed runtime binding points
// IRenderer.cpp already uses via glUniformBlockBinding (see
// IRenderer.cpp's BindUniformBlockIfPresent calls: GlobalMatrices=0,
// LightsBlock=1, DirectionalShadowBlock=2, PointShadowBlock=3,
// SpotShadowBlock=4), so an explicit binding here is redundant-but-
// consistent for GL42+/GL45 and gives Vulkan the same layout. Samplers
// continue from 5; the C++ side always explicitly sets each sampler's
// texture unit via glUniform1i every bind, so a compile-time default here
// is likewise redundant-but-harmless for GL, and gives Vulkan a static
// binding to reflect.
#define BIND_GlobalMatrices 0
#define BIND_LightsBlock 1
#define BIND_DirectionalShadowBlock 2
#define BIND_PointShadowBlock 3
#define BIND_SpotShadowBlock 4
#define BIND_uColormap 5
#define BIND_uFontmap 6
#define BIND_uNormalmap 7
#define BIND_uDisplacementmap 8
#define BIND_uDirectionalShadowMaps 9
#define BIND_uPointShadowMaps 10
#define BIND_uSpotShadowMaps 11
#define BIND_uEnvmap 12
#define BIND_uRefractmap 13
#define BIND_uSkyboxmap 14
#define BIND_uSpecularmap 15

// Loose-uniform-turned-UBO bindings. Each block is declared in exactly one
// stage (checked against actual usage: e.g. uCameraPos is only ever read
// in VERTEX - FRAGMENT only sees the interpolated vCameraPos varying), so
// there's no need to worry about a block being declared with a different
// member subset in each stage - same pattern as the existing single-stage
// blocks above (GlobalMatrices is VERTEX-only, LightsBlock/shadow blocks
// are FRAGMENT-only). Blocks that are always relevant regardless of which
// feature flags are active (ObjectMatrixUniforms, MaterialUniforms) are
// declared unconditionally with every member always present - avoids
// combinatorial std140-offset mismatches across this file's many #ifdef
// feature combinations; unused members in an unused code path are legal
// and free in GLSL, same as uOpacity already being unconditional today.
#define BIND_VertexFrameUniforms 16
#define BIND_VelocityFrameUniforms 17
#define BIND_ObjectMatrixUniforms 18
#define BIND_BoneMatrices 19
#define BIND_VelocityObjectUniforms 20
#define BIND_AmbientLightUniforms 21
#define BIND_MaterialUniforms 22

vec4 EncodeFloatRGBA( float v ) {
   vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
   enc = fract(enc);
   enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
   return enc;
}
float DecodeFloatRGBA( vec4 rgba ) {
   return dot( rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0) );
}

#if defined(EMSCRIPTEN)
    #define _highpMat3 highp mat3
    #define _highpMat4 highp mat4
    #define _highpVec3 highp vec3
    #define _highpVec4 highp vec4
#else
    #define _highpMat3 mat3
    #define _highpMat4 mat4
    #define _highpVec3 vec3
    #define _highpVec4 vec4
#endif

_highpMat3 _transpose3(in _highpMat3 inMatrix) {
    _highpVec3 i0 = inMatrix[0];
    _highpVec3 i1 = inMatrix[1];
    _highpVec3 i2 = inMatrix[2];

    _highpMat3 outMatrix = mat3(
                    vec3(i0.x, i1.x, i2.x),
                    vec3(i0.y, i1.y, i2.y),
                    vec3(i0.z, i1.z, i2.z)
                    );

    return outMatrix;
}
_highpMat4 _transpose4(in _highpMat4 inMatrix) {
    _highpVec4 i0 = inMatrix[0];
    _highpVec4 i1 = inMatrix[1];
    _highpVec4 i2 = inMatrix[2];
    _highpVec4 i3 = inMatrix[3];

    _highpMat4 outMatrix = mat4(
                    vec4(i0.x, i1.x, i2.x, i3.x),
                    vec4(i0.y, i1.y, i2.y, i3.y),
                    vec4(i0.z, i1.z, i2.z, i3.z),
                    vec4(i0.w, i1.w, i2.w, i3.w)
                    );

    return outMatrix;
}

#ifdef transpose
    mat3 transpose3(in mat3 m) { return transpose(m); }
    mat4 transpose4(in mat4 m) { return transpose(m); }
#else
    mat3 transpose3(in mat3 m) { return _transpose3(m); }
    mat4 transpose4(in mat4 m) { return _transpose4(m); }
#endif

#ifdef VERTEX

    #ifdef DEBUGRENDERING
        IO_LOCATION(LOC_aColor) attribute_in vec4 aColor;
        IO_LOCATION(LOC_aSize) attribute_in float aSize;
        IO_LOCATION(LOC_vColor) varying_out vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP)
        IO_LOCATION(LOC_vTexcoord) varying_out vec2 vTexcoord;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(TEXTRENDERING)
        IO_LOCATION(LOC_vNormal) varying_out vec3 vNormal;
    #endif

    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
        IO_LOCATION(LOC_vWorldPositionShadow) varying_out vec4 vWorldPositionShadow;
    #endif

    #if defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PARALLAXMAPPING)
        IO_LOCATION(LOC_vWorldPosition) varying_out vec4 vWorldPosition;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        IO_LOCATION(LOC_aTangent) attribute_in vec3 aTangent;
        IO_LOCATION(LOC_aBitangent) attribute_in vec3 aBitangent;
        IO_LOCATION(LOC_vTangentMatrix) varying_out mat3 vTangentMatrix;
    #endif

    #ifdef SKINNING
        IO_LOCATION(LOC_aBonesID) attribute_in vec4 aBonesID;
        IO_LOCATION(LOC_aBonesWeight) attribute_in vec4 aBonesWeight;
        UBO_BINDING(BIND_BoneMatrices) uniform BoneMatrices {
            mat4 uBoneMatrix[MAX_BONES];
        };
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) || defined(DIFFUSE) || defined(CELLSHADING)
        UBO_BINDING(BIND_VertexFrameUniforms) uniform VertexFrameUniforms {
            vec3 uCameraPos;
        };
        IO_LOCATION(LOC_vCameraPos) varying_out vec3 vCameraPos;
    #endif

    #ifdef REFRACTION
        IO_LOCATION(LOC_vTRed) varying_out vec3 vTRed;
        IO_LOCATION(LOC_vTGreen) varying_out vec3 vTGreen;
        IO_LOCATION(LOC_vTBlue) varying_out vec3 vTBlue;
        IO_LOCATION(LOC_vReflectionFactor) varying_out float vReflectionFactor;
    #endif

    #ifdef SKYBOX
        IO_LOCATION(LOC_v3Texcoord) varying_out vec3 v3Texcoord;
    #endif

    #ifdef DEFERRED_GBUFFER
        IO_LOCATION(LOC_gbuffer_normals) varying_out vec4 gbuffer_normals;
    #endif

   #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
       IO_LOCATION(LOC_vViewMatrix) varying_out mat4 vViewMatrix;
   #endif

    // Defaults
    IO_LOCATION(LOC_aPosition) attribute_in vec3 aPosition;
    IO_LOCATION(LOC_aNormal) attribute_in vec3 aNormal;
    IO_LOCATION(LOC_aTexcoord) attribute_in vec2 aTexcoord;
    // uProjectionMatrix/uViewMatrix change once per (frame or shadow pass),
    // not per object, so they're shared via a UBO instead of being resent
    // as individual uniforms on every draw; uModelMatrix is per-object and
    // stays a plain uniform.
    UBO_BINDING(BIND_GlobalMatrices) uniform GlobalMatrices {
        mat4 uProjectionMatrix;
        mat4 uViewMatrix;
    };
    UBO_BINDING(BIND_ObjectMatrixUniforms) uniform ObjectMatrixUniforms {
        mat4 uModelMatrix;
    };

    // Instanced
    #ifdef INSTANCED_RENDERING
        IO_LOCATION(LOC_aInstancedTransform) attribute_in mat4 aInstancedTransform;
    #endif

    #ifdef VELOCITY_RENDERING
        UBO_BINDING(BIND_VelocityFrameUniforms) uniform VelocityFrameUniforms {
            mat4 uPrvProjectionMatrix;
            mat4 uPrvViewMatrix;
        };
        UBO_BINDING(BIND_VelocityObjectUniforms) uniform VelocityObjectUniforms {
            mat4 uPrvModelMatrix;
        };
        IO_LOCATION(LOC_vScreenSpaceWorldPosition) varying_smooth_out vec4 vScreenSpaceWorldPosition;
	IO_LOCATION(LOC_vPrvScreenSpaceWorldPosition) varying_smooth_out vec4 vPrvScreenSpaceWorldPosition;
    #endif

    mat4 matAnimation = mat4(1.0);

    void main() {

        vec3 Position = aPosition;
        mat4 ModelMatrix = uModelMatrix;

        #ifdef INSTANCED_RENDERING
            ModelMatrix *= aInstancedTransform;
        #endif

        #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
            vViewMatrix = uViewMatrix;
        #endif

        #ifdef DEBUGRENDERING
            vColor = aColor;
        #endif

        #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP)
            vTexcoord = aTexcoord;
        #endif

        #if defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING)
            #ifndef SKINNING
                vWorldPosition=ModelMatrix * vec4(Position,1.0);
            #else
                vWorldPosition=ModelMatrix * (matAnimation * vec4(Position,1.0));
            #endif
        #endif

        #ifdef VELOCITY_RENDERING
                vScreenSpaceWorldPosition=uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(Position,1.0);
                vPrvScreenSpaceWorldPosition=uPrvProjectionMatrix * uPrvViewMatrix * uPrvModelMatrix * vec4(Position,1.0);
        #endif


        #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
            #ifndef SKINNING
                vWorldPositionShadow=uViewMatrix * ModelMatrix * vec4(Position,1.0);
            #else
                vWorldPositionShadow=uViewMatrix * ModelMatrix * (matAnimation * vec4(Position,1.0));
            #endif
        #endif

        #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING)
            vCameraPos = uCameraPos;
        #endif

        #ifdef SKYBOX
            v3Texcoord = Position.xyz;
        #endif

        gl_Position = uProjectionMatrix * uViewMatrix * ModelMatrix * vec4(Position,1.0);

        #ifdef DEBUGRENDERING
           if (aSize != 1.0) gl_PointSize = aSize;
        #endif

        // This Overwrites GL Position
        #ifdef SKINNING
            matAnimation = uBoneMatrix[int(aBonesID.x)] * aBonesWeight.x;
            matAnimation += uBoneMatrix[int(aBonesID.y)] * aBonesWeight.y;
            matAnimation += uBoneMatrix[int(aBonesID.z)] * aBonesWeight.z;
            matAnimation += uBoneMatrix[int(aBonesID.w)] * aBonesWeight.w;
            gl_Position = uProjectionMatrix * uViewMatrix * ModelMatrix * matAnimation * vec4(Position,1.0);
        #endif

        #ifdef TEXTRENDERING
            vNormal = aNormal;
        #endif

        #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
            #ifndef SKINNING
                vNormal = normalize((ModelMatrix * vec4(aNormal,0.0)).xyz);
            #else
                vNormal = normalize((ModelMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);
            #endif
        #endif

        #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
            vec3 T = normalize(ModelMatrix * vec4(aTangent, 0)).xyz;
            vec3 B = normalize(ModelMatrix * vec4(aBitangent, 0)).xyz;
            vec3 N = normalize(ModelMatrix * vec4(aNormal, 0)).xyz;
            vTangentMatrix = mat3(T.x, B.x, N.x, T.y, B.y, N.y, T.z, B.z, N.z);
        #endif

        #ifdef REFRACTION
            float fresnelBias, fresnelScale, fresnelPower;
            vec3 etaRatio;
            fresnelBias = 0.9;
            fresnelScale=0.7;
            fresnelPower=1.0;
            etaRatio=vec3(0.943,0.949,0.945);
            vec3 I = normalize(vWorldPosition.xyz - uCameraPos);
            vTRed = refract(I,vNormal,etaRatio.x);
            vTGreen =  refract(I,vNormal,etaRatio.y);
            vTBlue = refract(I,vNormal,etaRatio.z);
            vReflectionFactor = fresnelBias + fresnelScale * pow(1.0+dot(I,vNormal),fresnelPower);
        #endif

        #ifdef DEFERRED_GBUFFER
            #ifdef SKINNING
                gbuffer_normals = uViewMatrix * ModelMatrix * (matAnimation * vec4(aNormal,0.0));
            #else
                gbuffer_normals = uViewMatrix * ModelMatrix * vec4(aNormal,0.0);
            #endif
        #endif
    }

#endif

#ifdef FRAGMENT

	// Fragment Color. When DEFERRED_GBUFFER is also active, FragData_r/g/b
	// below claim locations 0/1/2, so FragColor (unused by the G-buffer
	// pass, but still assigned in main() below) is pinned to location 3
	// instead - this matches what GL's implicit-location auto-assignment
	// already does today when both are declared (it picks a location that
	// doesn't collide with the explicit ones), just made static/explicit
	// so SPIR-V (which requires every output to have one) can compile it.
	#ifdef VELOCITY_RENDERING
		#ifdef DEFERRED_GBUFFER
			IO_LOCATION(3) out vec2 FragColor;
		#else
			IO_LOCATION(0) out vec2 FragColor;
		#endif
	#else
		#ifdef DEFERRED_GBUFFER
			IO_LOCATION(3) out vec4 FragColor;
		#else
			IO_LOCATION(0) out vec4 FragColor;
		#endif
	#endif

    #if defined(DIFFUSE) || defined(CELLSHADING)

        struct LIGHT
        {
            vec4 Color;
            vec3 Direction;
            vec3 Position;
            float Radius;
            vec2 Cones;
            float Type;
            bool HaveShadowMap;
            int ShadowMap;
            float PCFTexelSize;
        };
        void buildLightFromMatrix(mat4 Light, inout LIGHT L)
        {
            L.Color = Light[0];
            L.Position = vec3(Light[1][0],Light[1][1],Light[1][2]);;
            L.Direction = vec3(Light[1][3],Light[2][0],Light[2][1]);
            L.Radius = Light[2][2];
            L.Cones = vec2(Light[2][3],Light[3][0]);
            L.Type = Light[3][1];
            L.HaveShadowMap = (Light[3][3]>=0.0? true : false);
            if (L.HaveShadowMap) {
               L.PCFTexelSize = Light[3][2];
               L.ShadowMap = int(Light[3][3]); // Only for Point and Spot Shadows (Directional have only one shadow map)
            }
        }

        float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
        {
            if (Radius>0.0) {
            float d = distance(Vertex,LightPosition);
            return clamp(1.0 - (1.0/Radius) * d, 0.0, 1.0);
            };
            return 1.0;
        }

        // SpotLight Cones
        float DualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone)
        {
            if (cosOutterCone>0.0 || cosInnerCone>0.0) {
                vec3 to_light = normalize(SpotLightPosition-Vertex);
                float angle = dot(-to_light, normalize(SpotLightDirection));
                float funcX = 1.0/(cosInnerCone-cosOutterCone);
                float funcY = -funcX * cosOutterCone;
                return clamp(angle*funcX+funcY,0.0,1.0);
            }
            return 0.0;
        }

        void CalculateLighting(vec3 LightVec, vec3 HalfVec, vec3 Normal, float Shininess, out float lightIntensity, out float specularPower)
        {

            float specularLight = 0.0;
            float diffuseLight = max(dot(LightVec,Normal),0.0);
            lightIntensity = max(dot(LightVec,Normal),0.0);
            specularPower = (lightIntensity>0.0?pow(max(dot(HalfVec,Normal),0.0), Shininess):0.0);
        }

        // uLights is rebuilt every object (each object only gets its
        // nearby lights), unlike uProjectionMatrix/uViewMatrix above, but
        // it's still shared across every fragment of that object's draw
        // call, so a UBO still saves resending the whole array as
        // individual uniforms across every shader/material switch.
        UBO_BINDING(BIND_LightsBlock) uniform LightsBlock {
            mat4 uLights[MAX_LIGHTS];
        };

    #endif

    // Defaults
    vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
    vec4 specular = vec4(0.0,0.0,0.0,1.0);
    bool diffuseIsSet = false;
    // Every material-scalar/vector uniform PyrosShader.glsl ever needs,
    // in one always-declared block (regardless of which feature flags are
    // active - see the comment on the BIND_* macros above for why).
    UBO_BINDING(BIND_MaterialUniforms) uniform MaterialUniforms {
        vec4 uColor;
        vec4 uSpecular;
        float uOpacity;
        float uShininess;
        float uUseLights;
        float uDisplacementHeight;
        float uReflectivity;
        int uNumberOfLights;
        int uNumberOfPointShadows;
        int uNumberOfSpotShadows;
    };

    #ifdef DEBUGRENDERING
        IO_LOCATION(LOC_vColor) varying_in vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(SPECULARMAP) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        IO_LOCATION(LOC_vTexcoord) varying_in vec2 vTexcoord;
    #endif

    #ifdef TEXTURE
        SAMPLER_BINDING(BIND_uColormap) uniform sampler2D uColormap;
    #endif

    #ifdef TEXTRENDERING
        SAMPLER_BINDING(BIND_uFontmap) uniform sampler2D uFontmap;
    #endif

    #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        IO_LOCATION(LOC_vNormal) varying_in vec3 vNormal;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        IO_LOCATION(LOC_vTangentMatrix) varying_in mat3 vTangentMatrix;
        #if defined(BUMPMAPPING)
            SAMPLER_BINDING(BIND_uNormalmap) uniform sampler2D uNormalmap;
        #endif
        #if defined(PARALLAXMAPPING)
            SAMPLER_BINDING(BIND_uDisplacementmap) uniform sampler2D uDisplacementmap;

            vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
            {
               const float minLayers = 8.0;
               const float maxLayers = 32.0;
               float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));

               float layerDepth = 1.0 / numLayers;
               float currentLayerDepth = 0.0;
               vec2 P = viewDir.xy / viewDir.z * uDisplacementHeight;
               vec2 deltaTexCoords = P / numLayers;

               vec2  currentTexCoords = texCoords;
               float currentDepthMapValue = texture_2D(uDisplacementmap, currentTexCoords).r;

               while (currentLayerDepth < currentDepthMapValue)
               {
                   currentTexCoords -= deltaTexCoords;
                   currentDepthMapValue = texture_2D(uDisplacementmap, currentTexCoords).r;
                   currentLayerDepth += layerDepth;
               }

               vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

               float afterDepth = currentDepthMapValue - currentLayerDepth;
               float beforeDepth = texture_2D(uDisplacementmap, prevTexCoords).r - currentLayerDepth + layerDepth;

               float weight = afterDepth / (afterDepth - beforeDepth);
               vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

               return finalTexCoords;
            }
        #endif
    #endif

#if defined(DIRECTIONALSHADOW)
            float PCFDIRECTIONAL(sampler2DShadow shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade)
            {
                vec4 coord = sMatrix * pos;
                if (MoreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(width,height);
                float shadow = 0.0;
                float x = 0.0;
                float y = 0.0;
                for (y = -1.5 ; y <=1.5 ; y+=1.0)
                    for (x = -1.5 ; x <=1.5 ; x+=1.0)
                        shadow += texture(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0)));
                shadow /= 16.0;
                return shadow;
            }
            // Same batching idea as GlobalMatrices/LightsBlock, for the
            // (up to 4) cascade matrices; uDirectionalShadowFar keeps its
            // declared size but only element [0] is ever written or read -
            // it's a single vec4 whose 4 components are the per-cascade
            // far distances, not a real 4-element array.
            UBO_BINDING(BIND_DirectionalShadowBlock) uniform DirectionalShadowBlock {
                mat4 uDirectionalDepthsMVP[4];
                vec4 uDirectionalShadowFar[4];
            };
            SAMPLER_BINDING(BIND_uDirectionalShadowMaps) uniform sampler2DShadow uDirectionalShadowMaps;
        #endif

        #ifdef POINTSHADOW
            float PCFPOINT(samplerCubeShadow shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos)
            {
                vec4 position_ls = Matrix2 * pos;
                position_ls.xyz/=position_ls.w;
                vec4 abs_position = abs(position_ls);
                float fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));
                vec4 clip = Matrix1 * vec4(0.0, 0.0, fs_z, 1.0);
                float depth = (clip.z / clip.w) * 0.5 + 0.5;
                float shadow = 0.0;
                float x = 0.0;
                float y = 0.0;

                for (y = -1.5 ; y <=1.5 ; y+=1.0)
                    for (x = -1.5 ; x <=1.5 ; x+=1.0)
                        shadow += texture(shadowMap, vec4(position_ls.xyz, depth) + vec4(vec2(x,y) * scale,0.0,0.0));
                shadow /= 16.0;
                return shadow;
            }
            // Samplers can never be members of a uniform block, so
            // uPointShadowMaps stays a plain uniform; only the matrix array
            // moves to a UBO.
            UBO_BINDING(BIND_PointShadowBlock) uniform PointShadowBlock {
                mat4 uPointDepthsMVP[8];
            };
            SAMPLER_BINDING(BIND_uPointShadowMaps) uniform samplerCubeShadow uPointShadowMaps[4];
        #endif

        #ifdef SPOTSHADOW
            float PCFSPOT(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos)
            {
                vec4 coord = sMatrix * pos;
                coord.xyz/=coord.w;
                float shadow = 0.0;
                        shadow += texture(shadowMap, coord.xyz );
                return shadow;
            }

            SAMPLER_BINDING(BIND_uSpotShadowMaps) uniform sampler2DShadow uSpotShadowMaps[4];
            UBO_BINDING(BIND_SpotShadowBlock) uniform SpotShadowBlock {
                mat4 uSpotDepthsMVP[4];
            };
#endif

    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
        IO_LOCATION(LOC_vWorldPositionShadow) varying_in vec4 vWorldPositionShadow;
    #endif

    #if defined(SKINNING) || defined(ENVMAP) || defined(PARALLAXMAPPING) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        IO_LOCATION(LOC_vWorldPosition) varying_in vec4 vWorldPosition;
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING)
        IO_LOCATION(LOC_vCameraPos) varying_in vec3 vCameraPos;
    #endif

    #ifdef ENVMAP
        SAMPLER_BINDING(BIND_uEnvmap) uniform samplerCube uEnvmap;
    #endif

    #ifdef REFRACTION
        SAMPLER_BINDING(BIND_uRefractmap) uniform samplerCube uRefractmap;
        IO_LOCATION(LOC_vReflectionFactor) varying_in float vReflectionFactor;
        IO_LOCATION(LOC_vTRed) varying_in vec3 vTRed;
        IO_LOCATION(LOC_vTGreen) varying_in vec3 vTGreen;
        IO_LOCATION(LOC_vTBlue) varying_in vec3 vTBlue;
    #endif

    #ifdef SKYBOX
        IO_LOCATION(LOC_v3Texcoord) varying_in vec3 v3Texcoord;
        SAMPLER_BINDING(BIND_uSkyboxmap) uniform samplerCube uSkyboxmap;
    #endif

    #ifdef SPECULARMAP
        SAMPLER_BINDING(BIND_uSpecularmap) uniform sampler2D uSpecularmap;
    #endif

    #ifdef DEFERRED_GBUFFER
        IO_LOCATION(LOC_gbuffer_normals) varying_in vec4 gbuffer_normals;
		layout(location = 0) out vec4 FragData_r;
		layout(location = 1) out vec4 FragData_g;
		layout(location = 2) out vec4 FragData_b;
    #endif

   #if defined(DIFFUSE) || defined(CELLSHADING) || defined(DEFERRED_GBUFFER)
       UBO_BINDING(BIND_AmbientLightUniforms) uniform AmbientLightUniforms {
           vec4 uAmbientLight;
       };
   #endif

   #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
       IO_LOCATION(LOC_vViewMatrix) varying_in mat4 vViewMatrix;
   #endif

   #ifdef VELOCITY_RENDERING
	IO_LOCATION(LOC_vScreenSpaceWorldPosition) varying_smooth_in vec4 vScreenSpaceWorldPosition;
	IO_LOCATION(LOC_vPrvScreenSpaceWorldPosition) varying_smooth_in vec4 vPrvScreenSpaceWorldPosition;
   #endif

    void main() {

        // gbuffer_normals is a fragment-stage input (the vertex-interpolated
        // geometric normal); bump/parallax mapping below needs to replace it
        // per-fragment with the normal-mapped result before it reaches
        // FragData_b, which GLSL doesn't allow writing directly - shader
        // inputs are read-only. gbufferNormal is the mutable local that
        // actually gets written; it starts as a copy of the input so
        // non-bump-mapped materials still output the vertex normal unchanged.
        #ifdef DEFERRED_GBUFFER
            vec4 gbufferNormal = gbuffer_normals;
        #endif

        #ifdef COLOR
            if (!diffuseIsSet)
            {
                diffuse=uColor;
                diffuseIsSet=true;
            } else diffuse *= uColor;
        #endif

        #ifdef DEBUGRENDERING
            if (!diffuseIsSet)
            {
                diffuse=vColor;
                diffuseIsSet=true;
            } else diffuse *= vColor;
        #endif

        #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP)
            vec2 Texcoord = vTexcoord;
            #if defined(PARALLAXMAPPING)
                vec3 viewDir = normalize(vTangentMatrix * (vCameraPos-vWorldPosition.xyz));
                Texcoord = ParallaxMapping(Texcoord, viewDir);
                if(Texcoord.x > 1.0 || Texcoord.y > 1.0 || Texcoord.x < 0.0 || Texcoord.y < 0.0)
                    discard;
            #endif
        #endif

        #ifdef TEXTURE
            if (!diffuseIsSet)
            {
                diffuse=texture_2D(uColormap,Texcoord);
                diffuseIsSet=true;
            } else diffuse *= texture_2D(uColormap,Texcoord);
        #endif

        #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
            vec3 Normal;
            #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
                Normal = normalize(transpose3(vTangentMatrix) * (texture_2D(uNormalmap, Texcoord).rgb * 2.0 - 1.0));
                #if defined(DEFERRED_GBUFFER)
                    gbufferNormal.xyz = (vViewMatrix * vec4(Normal,0)).xyz;
                #endif
            #else
                Normal = vNormal;
            #endif
        #endif

        #ifdef TEXTRENDERING
            if (!diffuseIsSet)
            {
                diffuse=vec4(Normal*texture_2D(uFontmap,Texcoord).r,texture_2D(uFontmap,Texcoord).r);
                diffuseIsSet=true;
            } else diffuse *= vec4(Normal*texture_2D(uFontmap,Texcoord).r,texture_2D(uFontmap,Texcoord).r);
        #endif

        #if defined(ENVMAP) || defined(REFRACTION)
            vec3 Reflection = reflect((vWorldPosition.xyz - vCameraPos),normalize(vNormal));
            #ifdef ENVMAP
                diffuse.xyz = diffuse.xyz * (1.0-uReflectivity) + (texture_cube(uEnvmap,Reflection)).xyz*uReflectivity;
            #endif
            #ifdef REFRACTION
                vec4 reflectedColor = texture_cube(uRefractmap, Reflection);
                vec4 refractedColor;
                refractedColor.x = (texture_cube( uRefractmap, vTRed)).x; refractedColor.y = (texture_cube( uRefractmap, vTGreen)).y;
                refractedColor.z = (texture_cube( uRefractmap, vTBlue)).z;
                refractedColor.w = 1.0;
                if (!diffuseIsSet)
                {
                    diffuse = mix(reflectedColor, refractedColor, vReflectionFactor);
                    diffuseIsSet=true;
                } else diffuse *= mix(reflectedColor, refractedColor, vReflectionFactor);
            #endif
        #endif

        #ifdef SKYBOX
            if (!diffuseIsSet)
            {
                diffuse=texture_cube(uSkyboxmap,v3Texcoord);
                diffuseIsSet=true;
            } else diffuse *= texture_cube(uSkyboxmap,v3Texcoord);
        #endif

        #ifdef SPECULARCOLOR
            specular = uSpecular;
        #endif

        #ifdef SPECULARMAP
            specular = texture_2D(uSpecularmap,Texcoord);
        #endif

        #if defined(DIFFUSE) || defined(CELLSHADING)
            // Fragment Body
            vec4 _diffuse = uAmbientLight;
            vec4 _specular = vec4(0.0,0.0,0.0,1.0);

            vec3 Position = vWorldPosition.xyz;
            vec3 EyeVec = normalize(vCameraPos-Position);
            Normal = normalize(Normal);

            float lightIntensityCellShading;

            for (int i=0;i<MAX_LIGHTS;i++)
            {
                float lightIntensity, specularPower;
                float attenuation = 1.0;
                float spotEffect = 1.0;
                if (i<uNumberOfLights)
                {
                    mat4 Light = uLights[i];

                    vec3 LightDir;
                    vec4 LightColor;

                    LIGHT L;
                    buildLightFromMatrix(Light,L);
                    if (L.Type == 1.0)
                    {
                        LightDir = normalize(-L.Direction);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        CalculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef DIRECTIONALSHADOW
                            float DirectionalShadow = 1.0;
                            if (L.HaveShadowMap) {
                                bool MoreThanOneCascade = (uDirectionalShadowFar[0].y>0.0);
                                if (gl_FragCoord.z<uDirectionalShadowFar[0].x) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.0, uDirectionalDepthsMVP[0],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].y) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.0, uDirectionalDepthsMVP[1],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].z) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.5, uDirectionalDepthsMVP[2],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].w) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.5, uDirectionalDepthsMVP[3],L.PCFTexelSize,vWorldPositionShadow, MoreThanOneCascade);
                            }

                            _diffuse += vec4(lightIntensity * L.Color.xyz * DirectionalShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * specular.xyz * DirectionalShadow, specularPower * L.Color.w * specular.w);
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * DirectionalShadow);
                        #else
                            _diffuse += lightIntensity * L.Color;
                            _specular += specularPower * L.Color * specular;
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity);
                        #endif
                    }
                    else if (L.Type == 2.0)
                    {
                        LightDir = normalize(L.Position - Position);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        attenuation = Attenuation(Position, L.Position, L.Radius);

                        CalculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef POINTSHADOW
                            float PointShadow = 1.0;
                            if (attenuation>0.0 && L.HaveShadowMap)
                            {
                                PointShadow = 0.0;
                                PointShadow+=PCFPOINT(uPointShadowMaps[0],uPointDepthsMVP[(L.ShadowMap*2)],uPointDepthsMVP[(L.ShadowMap*2+1)],L.PCFTexelSize,vWorldPositionShadow);
                            }
                            _diffuse += vec4(lightIntensity * L.Color.xyz * attenuation * PointShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * attenuation * specular.xyz * PointShadow, specularPower * L.Color.w * specular.w);
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * attenuation * PointShadow);
                        #else
                            _diffuse += (lightIntensity + specularPower * _specular) * L.Color * attenuation;
                            _specular += specularPower * L.Color * specular * attenuation;
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * attenuation);
                        #endif
                    }
                    else if (L.Type == 3.0)
                    {
                        LightDir = normalize(L.Position - Position);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        attenuation = Attenuation(Position, L.Position, L.Radius);
                        spotEffect = 1.0 - DualConeSpotLight(Position, L.Position, L.Direction, L.Cones.x, L.Cones.y);

                        CalculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef SPOTSHADOW
                            float SpotShadow = 1.0;
                            if (spotEffect>0.0 && attenuation>0.0 && L.HaveShadowMap)
                            {
                                SpotShadow = 0.0;
                                SpotShadow+=PCFSPOT(uSpotShadowMaps[0],uSpotDepthsMVP[L.ShadowMap],L.PCFTexelSize,vWorldPositionShadow);
                            }
                            _diffuse += vec4(lightIntensity * L.Color.xyz * spotEffect * attenuation * SpotShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * spotEffect * attenuation * specular.xyz * SpotShadow, specularPower * L.Color.w * specular.w);
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * spotEffect * attenuation * SpotShadow);
                        #else
                            _diffuse += lightIntensity * L.Color * spotEffect * attenuation;
                            _specular += specularPower * L.Color * specular * spotEffect * attenuation;
                            lightIntensityCellShading = max(lightIntensityCellShading, lightIntensity * spotEffect * attenuation);
                        #endif
                    }
                }
            }
            #if defined(DIFFUSE)
                diffuse = _diffuse * diffuse + _specular * specular;
            #elif defined(CELLSHADING)
                float factor = 3.0;
                if (lightIntensityCellShading > 0.95) factor = 3.0;
                else if (lightIntensityCellShading > 0.7) factor = 2.0;
                else if (lightIntensityCellShading > 0.5) factor = 1.0;
                else if (lightIntensityCellShading > 0.25) factor = 0.8;
                else factor = 0.5;
                diffuse = factor * diffuse;
            #endif
        #endif

        #ifdef CASTSHADOWS
            diffuse = EncodeFloatRGBA(gl_FragCoord.z);
        #endif

        #ifdef DEFERRED_GBUFFER
		FragData_r=vec4(diffuse.xyz,diffuse.x*uAmbientLight.x);
		FragData_g=vec4(specular.xyz,diffuse.y*uAmbientLight.y);
		FragData_b=vec4(gbufferNormal.xyz,diffuse.z*uAmbientLight.z);
	#endif

	#ifdef VELOCITY_RENDERING
		vec2 a = (vScreenSpaceWorldPosition.xy / vScreenSpaceWorldPosition.w) * 0.5 + 0.5;
		vec2 b = (vPrvScreenSpaceWorldPosition.xy / vPrvScreenSpaceWorldPosition.w) * 0.5 + 0.5;
		FragColor = a - b;
	#else
		FragColor = vec4(diffuse.xyz,diffuse.w*uOpacity);
        #endif

    }

#endif
