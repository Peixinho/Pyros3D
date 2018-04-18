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

#define MAX_BONES 60
#define MAX_LIGHTS 4

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
        attribute_in vec4 aColor;
        attribute_in float aSize;
        varying_out vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SPECULARMAP)
        varying_out vec2 vTexcoord;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(TEXTRENDERING)
        varying_out vec3 vNormal;
    #endif
          
    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
        varying_out vec4 vWorldPositionShadow;
    #endif

    #if defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(PARALLAXMAPPING)
        varying_out vec4 vWorldPosition;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        attribute_in vec3 aTangent, aBitangent;
        varying_out mat3 vTangentMatrix;
    #endif

    #ifdef SKINNING
        attribute_in vec4 aBonesID, aBonesWeight;
        uniform mat4 uBoneMatrix[MAX_BONES];
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) || defined(DIFFUSE) || defined(CELLSHADING)
        uniform vec3 uCameraPos;
        varying_out vec3 vCameraPos;
    #endif

    #ifdef REFRACTION
        varying_out vec3 vTRed, vTGreen, vTBlue;
        varying_out float vReflectionFactor;
    #endif

    #ifdef SKYBOX
        varying_out vec3 v3Texcoord;
    #endif

    #ifdef DEFERRED_GBUFFER
        varying_out vec4 gbuffer_normals;
    #endif

   #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
       varying_out mat4 vViewMatrix;
   #endif

    // Defaults
    attribute_in vec3 aPosition, aNormal;
    attribute_in vec2 aTexcoord;
    uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
    
    // Instanced
    #ifdef INSTANCED_RENDERING
        attribute_in mat4 aInstancedTransform;
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
    
	// Fragment Color
	#if defined(GLES2)
		vec4 FragColor;	
	#else
		out vec4 FragColor;
	#endif

    #if defined(DIFFUSE) || defined(CELLSHADING)

        #if defined(GLES2)
           float shadowBias;
        #endif

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

        uniform mat4 uLights[MAX_LIGHTS];
        uniform int uNumberOfLights;
        uniform float uShininess;
        uniform float uUseLights;

    #endif

    // Defaults
    vec4 diffuse = vec4(0.0,0.0,0.0,1.0);
    vec4 specular = vec4(0.0,0.0,0.0,1.0);
    bool diffuseIsSet = false;
    uniform float uOpacity;

    #ifdef COLOR
        uniform vec4 uColor;
    #endif

    #ifdef DEBUGRENDERING
        varying_in vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(SPECULARMAP) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        varying_in vec2 vTexcoord;
    #endif

    #ifdef TEXTURE
        uniform sampler2D uColormap;
    #endif

    #ifdef TEXTRENDERING
        uniform sampler2D uFontmap;
    #endif

    #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(PARALLAXMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        varying_in vec3 vNormal;
    #endif

    #if defined(BUMPMAPPING) || defined(PARALLAXMAPPING)
        varying_in mat3 vTangentMatrix;
        #if defined(BUMPMAPPING)
            uniform sampler2D uNormalmap;
        #endif
        #if defined(PARALLAXMAPPING)
            uniform sampler2D uDisplacementmap;
            uniform float uDisplacementHeight;

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
               
               #if defined(GLES2)
               for(int i=0;i<100;i++)
               if (currentLayerDepth < currentDepthMapValue)
               #else
               while (currentLayerDepth < currentDepthMapValue)
               #endif
               {
                   currentTexCoords -= deltaTexCoords;
                   currentDepthMapValue = texture_2D(uDisplacementmap, currentTexCoords).r;
                   currentLayerDepth += layerDepth;
               }
               #if defined(GLES2)
               else break;
               #endif
               
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
    #if defined(GLES2)
            float PCFDIRECTIONAL(sampler2D shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade) 
    #else
            float PCFDIRECTIONAL(sampler2DShadow shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade) 
    #endif
            {
                vec4 coord = sMatrix * pos;
                if (MoreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(width,height);
                float shadow = 0.0;
                float x = 0.0;
                float y = 0.0;
    #if defined(GLES2)
                float shadowSample = DecodeFloatRGBA(texture2D(shadowMap, coord.xy));
                shadow = shadowSample - coord.z + shadowBias < 0.0 ? 0.0:1.0;
    #else
                for (y = -1.5 ; y <=1.5 ; y+=1.0)
                    for (x = -1.5 ; x <=1.5 ; x+=1.0)
                        shadow += texture(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0)));
                shadow /= 16.0;
    #endif
                return shadow;
            }
            uniform mat4 uDirectionalDepthsMVP[4];
            uniform vec4 uDirectionalShadowFar[4];
    #if defined(GLES2)
            uniform sampler2D uDirectionalShadowMaps;
    #else
            uniform sampler2DShadow uDirectionalShadowMaps;
    #endif    
        #endif

        #ifdef POINTSHADOW
    #if defined(GLES2)
            float PCFPOINT(samplerCube shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos) 
    #else
            float PCFPOINT(samplerCubeShadow shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos) 
    #endif
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
                
    #if defined(GLES2)
                float shadowSample = DecodeFloatRGBA(textureCube(shadowMap, position_ls.xyz));
                shadow = shadowSample - depth + shadowBias < 0.0 ? 0.0:1.0;
    #else       
                for (y = -1.5 ; y <=1.5 ; y+=1.0)
                    for (x = -1.5 ; x <=1.5 ; x+=1.0)
                        shadow += texture(shadowMap, vec4(position_ls.xyz, depth) + vec4(vec2(x,y) * scale,0.0,0.0));
                shadow /= 16.0;
    #endif
                return shadow;
            }
            uniform int uNumberOfPointShadows;
    #if defined(GLES2)
            uniform mat4 uPointDepthsMVP[2];
            uniform samplerCube uPointShadowMaps;
    #else
            uniform mat4 uPointDepthsMVP[8];
            uniform samplerCubeShadow uPointShadowMaps[4];
    #endif
        #endif

        #ifdef SPOTSHADOW
    #if defined(GLES2)
            float PCFSPOT(sampler2D shadowMap, mat4 sMatrix, float scale, vec4 pos)
    #else
            float PCFSPOT(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos)
    #endif
            {
                vec4 coord = sMatrix * pos;
                coord.xyz/=coord.w;
                float shadow = 0.0;
    #if defined(GLES2)
                float shadowSample = DecodeFloatRGBA(texture2D(shadowMap, coord.x));
                shadow = shadowSample - coord.z + shadowBias < 0.0 ? 0.0:1.0;
    #else
                        shadow += texture(shadowMap, coord.xyz );
    #endif
                return shadow;
            }
            
    #if defined(GLES2)
            uniform sampler2D uSpotShadowMaps;
            uniform mat4 uSpotDepthsMVP;
    #else
            uniform sampler2DShadow uSpotShadowMaps[4];
            uniform mat4 uSpotDepthsMVP[4];
    #endif
            uniform int uNumberOfSpotShadows; 
#endif

    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW)
        varying_in vec4 vWorldPositionShadow;
    #endif

    #if defined(SKINNING) || defined(ENVMAP) || defined(PARALLAXMAPPING) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        varying_in vec4 vWorldPosition;
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(PARALLAXMAPPING) ||  defined(DIFFUSE) || defined(CELLSHADING)
        varying_in vec3 vCameraPos;
    #endif

    #ifdef ENVMAP
        uniform samplerCube uEnvmap;
        uniform float uReflectivity;
    #endif

    #ifdef REFRACTION
        uniform samplerCube uRefractmap;
        varying_in float vReflectionFactor;
        varying_in vec3 vTRed, vTGreen, vTBlue;
    #endif

    #ifdef SKYBOX
        varying_in vec3 v3Texcoord;
        uniform samplerCube uSkyboxmap;
    #endif

    #ifdef SPECULARCOLOR
        uniform vec4 uSpecular;
    #endif

    #ifdef SPECULARMAP
        uniform sampler2D uSpecularmap;
    #endif

    #ifdef DEFERRED_GBUFFER
        varying_in vec4 gbuffer_normals;
		#if !defined(GLES2)
			layout(location = 0) out vec4 FragData_r;
			layout(location = 1) out vec4 FragData_g;
			layout(location = 2) out vec4 FragData_b;
		#endif
    #endif

   #if defined(DIFFUSE) || defined(CELLSHADING) || defined(DEFERRED_GBUFFER)
       uniform vec4 uAmbientLight;
   #endif

   #if defined(DEFERRED_GBUFFER) && (defined(PARALLAXMAPPING) || defined(BUMPMAPPING))
       varying_in mat4 vViewMatrix;
   #endif

    void main() {

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
                    gbuffer_normals.xyz = vViewMatrix * vec4(Normal,0);
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
                diffuse.xyz = diffuse.xyz * (1.0-uReflectivity) + (textureCube(uEnvmap,Reflection)).xyz*uReflectivity;
            #endif
            #ifdef REFRACTION
                vec4 reflectedColor = textureCube( uRefractmap, Reflection);
                vec4 refractedColor;
                refractedColor.x = (textureCube( uRefractmap, vTRed)).x;
                refractedColor.y = (textureCube( uRefractmap, vTGreen)).y;
                refractedColor.z = (textureCube( uRefractmap, vTBlue)).z;
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
                diffuse=textureCube(uSkyboxmap,v3Texcoord);
                diffuseIsSet=true;
            } else diffuse *= textureCube(uSkyboxmap,v3Texcoord);
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
                                #if defined(GLES2)
                                   shadowBias = max(0.001 * (1.0 - dot(Normal, LightDir)), 0.00001);
                                #endif
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
                                #if defined(GLES2)
                                   shadowBias = max(0.001 * (1.0 - dot(Normal, LightDir)), 0.00001);
                                   PointShadow=PCFPOINT(uPointShadowMaps,uPointDepthsMVP[0],uPointDepthsMVP[1],L.PCFTexelSize,vWorldPositionShadow);
                                #else
                                   PointShadow+=PCFPOINT(uPointShadowMaps[0],uPointDepthsMVP[(L.ShadowMap*2)],uPointDepthsMVP[(L.ShadowMap*2+1)],L.PCFTexelSize,vWorldPositionShadow);
                                #endif
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
                                #if defined(GLES2)
                                   shadowBias = max(0.001 * (1.0 - dot(Normal, LightDir)), 0.00001);
                                   SpotShadow=PCFSPOT(uSpotShadowMaps,uSpotDepthsMVP,L.PCFTexelSize,vWorldPositionShadow);
                                #else
                                   SpotShadow+=PCFSPOT(uSpotShadowMaps[0],uSpotDepthsMVP[L.ShadowMap],L.PCFTexelSize,vWorldPositionShadow);
                                #endif
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
            #if defined(GLES2)
				gl_FragData[0]=vec4(diffuse.xyz,diffuse.x*uAmbientLight.x);
				gl_FragData[1]=vec4(specular.xyz,diffuse.y*uAmbientLight.y);
				gl_FragData[2]=vec4(gbuffer_normals.xyz,diffuse.z*uAmbientLight.z);
			#else
				FragData_r=vec4(diffuse.xyz,diffuse.x*uAmbientLight.x);
				FragData_g=vec4(specular.xyz,diffuse.y*uAmbientLight.y);
				FragData_b=vec4(gbuffer_normals.xyz,diffuse.z*uAmbientLight.z);
			#endif
        #else
            FragColor = vec4(diffuse.xyz,diffuse.w*uOpacity);
			#if defined(GLES2)
				gl_FragColor = FragColor;
			#endif
        #endif
    }
    
#endif
