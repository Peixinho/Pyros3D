#define MAX_BONES 60
#define MAX_LIGHTS 4

#ifdef VERTEX

    #ifdef GLES2
        precision mediump float;
    #endif

    #ifdef PHYSICSDEBUG
        attribute vec4 aColor;
        varying vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(SPECULARMAP)
        varying vec2 vTexcoord;
    #endif

    #if defined(BUMPMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING) || defined(TEXTRENDERING)
        varying vec3 vNormal;
    #endif
          
    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        varying vec4 vWorldPosition;
    #endif

    #ifdef BUMPMAPPING
        attribute vec3 aTangent, aBitangent;
        varying mat3 vTangentMatrix;
    #endif

    #ifdef SKINNING
        attribute vec4 aBonesID, aBonesWeight;
        uniform mat4 uBoneMatrix[MAX_BONES];
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        uniform vec3 uCameraPos;
        varying vec3 vCameraPos;
    #endif

    #ifdef REFRACTION
        varying vec3 vTRed, vTGreen, vTBlue;
        varying float vReflectionFactor;
    #endif

    #ifdef SKYBOX
        varying vec3 v3Texcoord;
    #endif

    // Defaults
    attribute vec3 aPosition, aNormal;
    attribute vec2 aTexcoord;
    uniform mat4 uProjectionMatrix, uViewMatrix, uModelMatrix;
    mat4 matAnimation = mat4(1.0);
    void main() {

        #ifdef PHYSICSDEBUG
            vColor = aColor;
        #endif

        #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(SPECULARMAP)
            vTexcoord = aTexcoord;
        #endif

        #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
            #ifndef SKINNING
                vWorldPosition=uModelMatrix * vec4(aPosition,1.0);
            #else
                vWorldPosition=uModelMatrix * (matAnimation * vec4(aPosition,1.0));
            #endif
        #endif

        #if defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
            vCameraPos = uCameraPos;
        #endif

        #ifdef SKYBOX
            v3Texcoord = aPosition.xyz;
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

        gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition,1.0);

        // This Overwrites GL Position
        #ifdef SKINNING
            matAnimation = uBoneMatrix[int(aBonesID.x)] * aBonesWeight.x;
            matAnimation += uBoneMatrix[int(aBonesID.y)] * aBonesWeight.y;
            matAnimation += uBoneMatrix[int(aBonesID.z)] * aBonesWeight.z;
            matAnimation += uBoneMatrix[int(aBonesID.w)] * aBonesWeight.w;
            gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * matAnimation * vec4(aPosition,1.0);
        #endif

        #ifdef TEXTRENDERING
            vNormal = aNormal;
        #endif

        #if defined(BUMPMAPPING) || defined(SKINNING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
            #ifndef SKINNING
                vNormal = normalize((uModelMatrix * vec4(aNormal,0.0)).xyz);
            #else
                vNormal = normalize((uModelMatrix * (matAnimation * vec4(aNormal,0.0))).xyz);
            #endif
        #endif

        #ifdef BUMPMAPPING
            vec3 Tangent = normalize((uModelMatrix * vec4(aTangent,0)).xyz);
            vec3 Binormal = normalize((uModelMatrix * vec4(aBitangent,0)).xyz);
            vTangentMatrix = mat3(Tangent.x, Binormal.x, vNormal.x,Tangent.y, Binormal.y, vNormal.y,Tangent.z, Binormal.z, vNormal.z);
        #endif
    }

#endif

#ifdef FRAGMENT

    #ifdef GLES2
        precision mediump float;
    #endif

    #if defined(DIFFUSE) || defined(CELLSHADING)

        /*  DECODE AND ENCODE FLOAT TO RGBA
        float DecodeFloatRGBA(vec4 rgba)
        {
            return dot(rgba, vec4(1.0,1.0/255.0, 1.0/65025.0, 1.0/160581375.0));
        }
        vec4 EncodeFloatRGBA(float v) {
            vec4 enc = vec4(1.0, 255.0, 65025.0, 160581375.0) * v;
            enc = frac(enc);
            enc -= enc.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);
            return enc;
        }
        */
        
        struct LIGHT 
        {
            vec4 Color;
            vec3 Direction;
            vec3 Position;
            float Radius;
            vec2 Cones;
            float Type;
        };
        void buildLightFromMatrix(mat4 Light, inout LIGHT L) 
        {
            L.Color = Light[0];
            L.Position = vec3(Light[1][0],Light[1][1],Light[1][2]);;
            L.Direction = vec3(Light[1][3],Light[2][0],Light[2][1]);
            L.Radius = Light[2][2];
            L.Cones = vec2(Light[2][3],Light[3][0]);
            L.Type = Light[3][1];
        }

        float Attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
        {
            if (Radius>0.0) {
            float d = distance(Vertex,LightPosition);
            return clamp(1.0 - (1.0/Radius) * sqrt(d*d), 0.0, 1.0);
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

        uniform mat4 uLights[MAX_LIGHTS];
        uniform int uNumberOfLights;
        uniform vec4 uAmbientLight;
        uniform vec4 uKa, uKe, uKd, uKs;
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

    #ifdef PHYSICSDEBUG
        varying vec4 vColor;
    #endif

    #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(SPECULARMAP) || defined(BUMPMAPPING)
        varying vec2 vTexcoord;
    #endif

    #ifdef TEXTURE
        uniform sampler2D uColormap;
    #endif

    #ifdef TEXTRENDERING
        uniform sampler2D uFontmap;
    #endif

    #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        varying vec3 vNormal;
    #endif

    #ifdef BUMPMAPPING
        varying mat3 vTangentMatrix;
        uniform sampler2D uNormalmap;
    #endif

    #ifdef DIRECTIONALSHADOW
        float PCFDIRECTIONAL(sampler2DShadow shadowMap, float width, float height, mat4 sMatrix, float scale, vec4 pos, bool MoreThanOneCascade) 
        {
            vec4 coord = sMatrix * pos;
            if (MoreThanOneCascade) coord.xy = (coord.xy * 0.5) + vec2(width,height);
            float shadow = 0.0;
            float x,y;
            for (y = -1.5 ; y <=1.5 ; y+=1.0)
                for (x = -1.5 ; x <=1.5 ; x+=1.0)
                    shadow += shadow2D(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;
            shadow /= 16.0;
            return shadow;
        }
        uniform float uPCFTexelSize1;
        uniform float uPCFTexelSize2;
        uniform float uPCFTexelSize3;
        uniform float uPCFTexelSize4;
        uniform mat4 uDirectionalDepthsMVP[4];
        uniform vec4 uDirectionalShadowFar[4];
        uniform sampler2DShadow uDirectionalShadowMaps;
    #endif

    #ifdef POINTSHADOW
        #extension GL_EXT_gpu_shader4 : require
        float PCFPOINT(samplerCubeShadow shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos) 
        {
            vec4 position_ls = Matrix2 * vWorldPosition;
            position_ls.xyz/=position_ls.w;
            vec4 abs_position = abs(position_ls);
            float fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));
            vec4 clip = Matrix1 * vec4(0.0, 0.0, fs_z, 1.0);
            float depth = (clip.z / clip.w) * 0.5 + 0.5;
            float shadow = 0.0;
            float x,y;
            for (y = -1.5 ; y <=1.5 ; y+=1.0)
                for (x = -1.5 ; x <=1.5 ; x+=1.0)
                    shadow += shadowCube(shadowMap, vec4(position_ls.xyz, depth) + vec4(vec2(x,y) * scale,0.0,0.0)).x;
            shadow /= 16.0;
            return shadow;
        }
        uniform float uPCFTexelSize1;
        uniform mat4 uPointDepthsMVP[8]
        uniform int uNumberOfPointShadows;
        uniform samplerCubeShadow uPointShadowMaps[4];
    #endif

    #ifdef SPOTSHADOW
        float PCFSPOT(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos) 
        {
            vec4 coord = sMatrix * pos;
            coord.xyz/=coord.w;
            float shadow = 0.0;
            float x,y;
            for (y = -1.5 ; y <=1.5 ; y+=1.0)
               for (x = -1.5 ; x <=1.5 ; x+=1.0)
                    shadow += shadow2D(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;
            shadow /= 16.0;
            return shadow;
        }
        
        uniform sampler2DShadow uSpotShadowMaps[4];
        uniform mat4 uSpotDepthsMVP[4];
        uniform int uNumberOfSpotShadows; 
        uniform float uPCFTexelSize1;
    #endif

    #if defined(DIRECTIONALSHADOW) || defined(POINTSHADOW) || defined(SPOTSHADOW) || defined(SKINNING) || defined(ENVMAP) || defined(DIFFUSE) || defined(CELLSHADING)
        varying vec4 vWorldPosition;
    #endif

    #if defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
        varying vec3 vCameraPos;
    #endif

    #ifdef ENVMAP
        uniform samplerCube uEnvmap;
        uniform float uReflectivity;
    #endif

    #ifdef REFRACTION
        uniform samplerCube uRefractmap;
        varying float vReflectionFactor;
        varying vec3 vTRed, vTGreen, vTBlue;
    #endif

    #ifdef SKYBOX
        varying vec3 v3Texcoord;
        uniform samplerCube uSkyboxmap;
    #endif

    #ifdef SPECULARCOLOR
        uniform vec4 uSpecular;
    #endif

    #ifdef SPECULARMAP
        uniform sampler2D uSpecularmap;
    #endif

    void main() {

        // Default
        mat3 TangentMatrix = mat3(1.0);

        #ifdef COLOR
            if (!diffuseIsSet) 
            {
                diffuse=uColor;
                diffuseIsSet=true;
            } else diffuse *= uColor;
        #endif

        #ifdef PHYSICSDEBUG
            if (!diffuseIsSet) 
            {
                diffuse=vColor;
                diffuseIsSet=true;
            } else diffuse *= vColor;
        #endif

        #if defined(TEXTURE) || defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(SPECULARMAP)
            vec2 Texcoord = vTexcoord;
        #endif

        #ifdef TEXTURE
            if (!diffuseIsSet) 
            {
                diffuse=texture2D(uColormap,Texcoord);
                diffuseIsSet=true;
            } else diffuse *= texture2D(uColormap,Texcoord);
        #endif

        #if defined(TEXTRENDERING) || defined(BUMPMAPPING) || defined(ENVMAP) || defined(REFRACTION) || defined(DIFFUSE) || defined(CELLSHADING)
            vec3 Normal;
            #ifdef BUMPMAPPING
                TangentMatrix = vTangentMatrix;
                Normal = normalize((texture2D(uNormalmap, Texcoord).xyz * 2.0 - 1.0));
            #else
                Normal = vNormal;
            #endif
        #endif

        #ifdef TEXTRENDERING
            if (!diffuseIsSet) 
            {
                diffuse=vec4(Normal*texture2D(uFontmap,Texcoord).a,texture2D(uFontmap,Texcoord).a);
                diffuseIsSet=true;
            } else diffuse *= vec4(Normal*texture2D(uFontmap,Texcoord).a,texture2D(uFontmap,Texcoord).a);
        #endif

        #ifdef DIRECTIONALSHADOW
            float DirectionalShadow = 1.0;
            bool MoreThanOneCascade = (uDirectionalShadowFar[0].y>0.0);
            if (gl_FragCoord.z<uDirectionalShadowFar[0].x) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.0, uDirectionalDepthsMVP[0],uPCFTexelSize1,vWorldPosition, MoreThanOneCascade);
            else if (gl_FragCoord.z<uDirectionalShadowFar[0].y) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.0, uDirectionalDepthsMVP[1],uPCFTexelSize2,vWorldPosition, MoreThanOneCascade);
            else if (gl_FragCoord.z<uDirectionalShadowFar[0].z) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.5, uDirectionalDepthsMVP[2],uPCFTexelSize3,vWorldPosition, MoreThanOneCascade);
            else if (gl_FragCoord.z<uDirectionalShadowFar[0].w) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.5, uDirectionalDepthsMVP[3],uPCFTexelSize4,vWorldPosition, MoreThanOneCascade);
            diffuse.xyz*=vec3(DirectionalShadow+0.5);
        #endif

        #ifdef POINTSHADOW
            float PointShadow = 0.0;
            for (int i=0;i<4;i++)
            {
                if (i<uNumberOfPointShadows) 
                {
                    PointShadow+=PCFSPOT(uPointShadowMaps[i],uPointDepthsMVP[(i*2)],uPointDepthsMVP[(i*2+1)],uPCFTexelSize1,vWorldPosition);
                }
            }
            diffuse.xyz*=vec3(PointShadow+0.5);
        #endif

        #ifdef SPOTSHADOW
            float SpotShadow = 0.0;
            for (int i=0;i<4;i++)
            {
                if (i<2) {
                    SpotShadow+=PCFSPOT(uSpotShadowMaps[i],uSpotDepthsMVP[i],uPCFTexelSize1,vWorldPosition);
                }
            }
            diffuse.xyz*=vec3(SpotShadow+0.5);
        #endif

        #if defined(ENVMAP) || defined(REFRACTION)
            vec3 Reflection = reflect(normalize(vWorldPosition.xyz - (vec4(vCameraPos,1.0)).xyz),normalize(vNormal));
            #ifdef ENVMAP
                diffuse.xyz = diffuse.xyz * (1.0-uReflectivity) + (textureCube(uEnvmap,Reflection)).xyz*uReflectivity;
            #endif
            #ifdef REFRACTION
                vec4 reflectedColor = textureCube( uRefractmap, Reflection);
                vec4 refractedColor = vec4(0,0,0,1);
                refractedColor.x = (textureCube( uRefractmap, vTRed)).x;
                refractedColor.y = (textureCube( uRefractmap, vTGreen)).y;
                refractedColor.z = (textureCube( uRefractmap, vTBlue)).z;
                refractedColor.w = 1.0;
                if (!diffuseIsSet) 
                {
                    diffuse=mix(reflectedColor, refractedColor, vReflectionFactor);
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
            specular = texture2D(uSpecularmap,Texcoord);
        #endif

        #if defined(DIFFUSE) || defined(CELLSHADING)
            // Fragment Body
            vec4 _diffuse = vec4(0.0,0.0,0.0,1.0);
            vec4 _specular = vec4(0.0,0.0,0.0,1.0);
            vec4 finalColor = vec4(0.0,0.0,0.0,1.0);
            
            float _intensity = 0.0;
            
            vec3 Vertex = vWorldPosition.xyz;
            vec3 EyeVec = normalize(vCameraPos-Vertex);
            
            for (int i=0;i<MAX_LIGHTS;i++)
            {
                float attenuation = 1.0;
                float spotEffect = 1.0;
                if (i<uNumberOfLights) {
                    mat4 Light = uLights[i];
                    
                    vec3 LightDir;
                    vec4 LightColor;
                    
                    LIGHT L;
                    buildLightFromMatrix(Light,L);
                    if (L.Type == 1.0) LightDir = normalize(-L.Direction);
                    if (L.Type == 2.0) LightDir = normalize(L.Position - Vertex);
                    if (L.Type == 3.0) LightDir = normalize(L.Position - Vertex);
                    LightColor = L.Color;
                    attenuation = Attenuation(Vertex, L.Position, L.Radius);
                    spotEffect = 1.0 - DualConeSpotLight(Vertex, L.Position, L.Direction, L.Cones.x, L.Cones.y);
                    
                    vec3 HalfVec = TangentMatrix * normalize(EyeVec + LightDir);
                    vec3 LightVec = TangentMatrix * LightDir;
                    
                    float specularLight = 0.0;
                    float diffuseLight = max(dot(LightVec,Normal),0.0);
                    _intensity += max(dot(LightVec,Normal),0.0);
                    
                    #ifdef CELLSHADING
                        float factor = 3.0;
                        if (_intensity > 0.95) factor = 3.0;
                        else if (_intensity > 0.7) factor = 2.0;
                        else if (_intensity > 0.5) factor = 1.0;
                        else if (_intensity > 0.25) factor = 0.8;
                        else factor = 0.5;
                        diffuse.xyz = factor * vec3((diffuse * (uKe + (uAmbientLight * uKa))) + (diffuse * (_diffuse * uKd)));
                    #else
                        if (diffuseLight>0.0) specularLight = pow(max(dot(HalfVec,Normal),0.0),uShininess);
                        _diffuse += LightColor * diffuseLight * attenuation * spotEffect;
                        _specular += LightColor * specularLight * attenuation * spotEffect;
                    #endif
                }
            }
            diffuse.xyz = (diffuse * (uKe + (uAmbientLight * uKa))).xyz + vec3((diffuse * (_diffuse * uKd)) + (specular *(_specular * uKs)));
        #endif

        #ifdef CASTSHADOWS
            diffuse.x=gl_FragCoord.z;
        #endif

        gl_FragColor = diffuse * uOpacity;
    }
    
#endif