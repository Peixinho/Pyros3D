/* LIGHTING
 *
 * Include file, and call CalculateLighting in desired pixel
 * using pixel world position, camera world position, pixel normal, pixel color, pixel specular and Shininess value
 *
 * !IMPORTANT!
 * For shadowing, we need to send position in viewspace as last argument
 *
 * Uniforms for Lighting *
 *
 * AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
 * AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));
 * AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
 * AddUniform(Uniform("uShininess", Uniforms::DataType::Float, &Shininess));
 *
 * Uniforms for Shadows *
 * Directional *
 * AddUniform(Uniform("uDirectionalShadowMaps", Uniforms::DataUsage::DirectionalShadowMap));
 * AddUniform(Uniform("uDirectionalDepthsMVP", Uniforms::DataUsage::DirectionalShadowMatrix));
 * AddUniform(Uniform("uDirectionalShadowFar", Uniforms::DataUsage::DirectionalShadowFar));
 * AddUniform(Uniform("uNumberOfDirectionalShadows", Uniforms::DataUsage::NumberOfDirectionalShadows));
 *
 * Point *
 * AddUniform(Uniform("uPointShadowMaps", Uniforms::DataUsage::PointShadowMap));
 * AddUniform(Uniform("uPointDepthsMVP", Uniforms::DataUsage::PointShadowMatrix));
 * AddUniform(Uniform("uNumberOfPointShadows", Uniforms::DataUsage::NumberOfPointShadows));
 *
 * Spot *
 * AddUniform(Uniform("uSpotShadowMaps", Uniforms::DataUsage::SpotShadowMap));
 * AddUniform(Uniform("uSpotDepthsMVP", Uniforms::DataUsage::SpotShadowMatrix));
 * AddUniform(Uniform("uNumberOfSpotShadows", Uniforms::DataUsage::NumberOfSpotShadows));
 *
 */

#ifndef GLSL_LIGHTING
#define GLSL_LIGHTING

    #ifndef MAX_LIGHTS
        #define MAX_LIGHTS 4
    #endif

    #ifdef FRAGMENT

        // Uniforms
        uniform mat4 uLights[MAX_LIGHTS];
        uniform int uNumberOfLights;
        uniform float uShininess;
        uniform vec4 uAmbientLight;

        #if defined(GLES2) || defined(GLLEGACY)
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
        void _buildLightFromMatrix(mat4 Light, inout LIGHT L) 
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

        float _attenuation(vec3 Vertex, vec3 LightPosition, float Radius)
        {
            if (Radius>0.0) {
            float d = distance(Vertex,LightPosition);
            return clamp(1.0 - (1.0/Radius) * d, 0.0, 1.0);
            };
            return 1.0;
        }

        // SpotLight Cones
        float _dualConeSpotLight(vec3 Vertex, vec3 SpotLightPosition, vec3 SpotLightDirection, float cosOutterCone, float cosInnerCone)
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

        void _calculateLighting(vec3 LightVec, vec3 HalfVec, vec3 Normal, float Shininess, out float lightIntensity, out float specularPower)
        {

            float specularLight = 0.0;
            float diffuseLight = max(dot(LightVec,Normal),0.0);
            lightIntensity = max(dot(LightVec,Normal),0.0);
            specularPower = (lightIntensity>0.0?pow(max(dot(HalfVec,Normal),0.0), Shininess):0.0);
        }

        #if defined(DIRECTIONALSHADOW)
            #if defined(GLES2) || defined(GLLEGACY)
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
                    
                    #if defined(GLES2) || defined(GLLEGACY)
                    
                        float shadowSample = DecodeFloatRGBA(texture2D(shadowMap, coord.xy));
                        shadow = shadowSample - coord.z + shadowBias < 0.0 ? 0.0:1.0;
                    
                    #else
                    
                        for (y = -1.5 ; y <=1.5 ; y+=1.0)
                            for (x = -1.5 ; x <=1.5 ; x+=1.0)
                                shadow += shadow2D(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;
                        
                        shadow /= 16.0;
                    
                    #endif
                    
                    return shadow;
                }

                uniform mat4 uDirectionalDepthsMVP[4];
                uniform vec4 uDirectionalShadowFar[4];

                #if defined(GLES2) || defined(GLLEGACY)
                
                uniform sampler2D uDirectionalShadowMaps;
                
                #else
                
                uniform sampler2DShadow uDirectionalShadowMaps;
                
                #endif
                    
        #endif

        #ifdef POINTSHADOW
        
            #if defined(GLES2) || defined(GLLEGACY)
                float PCFPOINT(samplerCube shadowMap, mat4 Matrix1, mat4 Matrix2, float scale, vec4 pos) 
            #else
                #extension GL_EXT_gpu_shader4 : require
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
                    
                    #if defined(GLES2) || defined(GLLEGACY)

                        float shadowSample = DecodeFloatRGBA(textureCube(shadowMap, position_ls.xyz));
                        shadow = shadowSample - depth + shadowBias < 0.0 ? 0.0:1.0;

                    #else       
                    
                        for (y = -1.5 ; y <=1.5 ; y+=1.0)
                            for (x = -1.5 ; x <=1.5 ; x+=1.0)
                                shadow += shadowCube(shadowMap, vec4(position_ls.xyz, depth) + vec4(vec2(x,y) * scale,0.0,0.0)).x;
                        
                        shadow /= 16.0;
                        
                    #endif

                    return shadow;
                }

                uniform int uNumberOfPointShadows;

                #if defined(GLES2) || defined(GLLEGACY)
            
                uniform mat4 uPointDepthsMVP[2];
                uniform samplerCube uPointShadowMaps;

                #else
                
                uniform mat4 uPointDepthsMVP[8];
                uniform samplerCubeShadow uPointShadowMaps[4];
                
                #endif

        #endif

        #ifdef SPOTSHADOW

            #if defined(GLES2) || defined(GLLEGACY)
                float PCFSPOT(sampler2D shadowMap, mat4 sMatrix, float scale, vec4 pos)
            #else
                float PCFSPOT(sampler2DShadow shadowMap, mat4 sMatrix, float scale, vec4 pos)
            #endif
                {
                    vec4 coord = sMatrix * pos;
                    coord.xyz/=coord.w;
                    float shadow = 0.0;
                    float x = 0.0;
                    float y = 0.0;
                    
                    #if defined(GLES2) || defined(GLLEGACY)
                
                        float shadowSample = DecodeFloatRGBA(texture2D(shadowMap, coord.xy));
                        shadow = shadowSample - coord.z + shadowBias < 0.0 ? 0.0:1.0;
                    
                    #else
                        for (y = -1.5 ; y <=1.5 ; y+=1.0)
                            for (x = -1.5 ; x <=1.5 ; x+=1.0)
                                shadow += shadow2D(shadowMap, (coord.xyz + vec3(vec2(x,y) * scale,0.0))).x;
                
                        shadow /= 16.0;
                    
                    #endif
                
                    return shadow;
                }
            
                #if defined(GLES2) || defined(GLLEGACY)
                
                uniform sampler2D uSpotShadowMaps;
                uniform mat4 uSpotDepthsMVP;
                
                #else
                
                uniform sampler2DShadow uSpotShadowMaps[4];
                uniform mat4 uSpotDepthsMVP[4];
                
                #endif
                    
                uniform int uNumberOfSpotShadows; 
        #endif

        void CalculateLighting(vec3 Position, vec3 CameraPos, vec3 Normal, inout vec4 diffuse, inout vec4 specular, vec4 vs_position =vec4(0.0))
        {
            // Fragment Body
            vec4 _diffuse = uAmbientLight;
            vec4 _specular = vec4(0.0,0.0,0.0,1.0);
            vec3 EyeVec = normalize(CameraPos-Position);

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
                    _buildLightFromMatrix(Light,L);
                    if (L.Type == 1.0) 
                    {
                        LightDir = normalize(-L.Direction);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        _calculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef DIRECTIONALSHADOW
                            float DirectionalShadow = 1.0; 
                            if (L.HaveShadowMap) {
                                #if defined(GLES2) || defined(GLLEGACY)
                                shadowBias = max(0.001 * (1.0 - dot(Normal, LightDir)), 0.00001);
                                #endif
                                bool MoreThanOneCascade = (uDirectionalShadowFar[0].y>0.0);
                                if (gl_FragCoord.z<uDirectionalShadowFar[0].x) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.0, uDirectionalDepthsMVP[0],L.PCFTexelSize,vs_position, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].y) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.0, uDirectionalDepthsMVP[1],L.PCFTexelSize,vs_position, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].z) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.0, 0.5, uDirectionalDepthsMVP[2],L.PCFTexelSize,vs_position, MoreThanOneCascade);
                                else if (gl_FragCoord.z<uDirectionalShadowFar[0].w) DirectionalShadow = PCFDIRECTIONAL( uDirectionalShadowMaps, 0.5,0.5, uDirectionalDepthsMVP[3],L.PCFTexelSize,vs_position, MoreThanOneCascade);
                            }

                            _diffuse += vec4(lightIntensity * L.Color.xyz * DirectionalShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * specular.xyz * DirectionalShadow, specularPower * L.Color.w * specular.w);
                        #else
                            _diffuse += lightIntensity * L.Color;
                            _specular += specularPower * L.Color * specular;
                        #endif                    
                    }
                    else if (L.Type == 2.0) 
                    {
                        LightDir = normalize(L.Position - Position);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        attenuation = _attenuation(Position, L.Position, L.Radius);

                        _calculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);
    
                        #ifdef POINTSHADOW
                            float PointShadow = 1.0;
                            if (attenuation>0.0 && L.HaveShadowMap)
                            {
                                PointShadow = 0.0;
                                #if defined(GLES2) || defined(GLLEGACY)
                                shadowBias = max(0.001 * (1.0 - dot(Normal, LightDir)), 0.00001);
                                PointShadow=PCFPOINT(uPointShadowMaps,uPointDepthsMVP[0],uPointDepthsMVP[1],L.PCFTexelSize,vs_position);
                                #else
                                PointShadow+=PCFPOINT(uPointShadowMaps[L.ShadowMap],uPointDepthsMVP[(L.ShadowMap*2)],uPointDepthsMVP[(L.ShadowMap*2+1)],L.PCFTexelSize,vs_position);
                                #endif
                            }
                            _diffuse += vec4(lightIntensity * L.Color.xyz * attenuation * PointShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * attenuation * specular.xyz * PointShadow, specularPower * L.Color.w * specular.w);
                        #else
                            _diffuse += (lightIntensity + specularPower * _specular) * L.Color * attenuation;
                            _specular += specularPower * L.Color * specular * attenuation;
                        #endif
                    
                    }
                    else if (L.Type == 3.0)
                    {
                        LightDir = normalize(L.Position - Position);
                        vec3 HalfVec = normalize(EyeVec + LightDir);
                        vec3 LightVec = LightDir;

                        lightIntensity = specularPower = 0.0;

                        attenuation = _attenuation(Position, L.Position, L.Radius);
                        spotEffect = 1.0 - _dualConeSpotLight(Position, L.Position, L.Direction, L.Cones.x, L.Cones.y);

                        _calculateLighting(LightVec, HalfVec, Normal, uShininess, lightIntensity, specularPower);

                        #ifdef SPOTSHADOW
                            float SpotShadow = 1.0;
                            if (spotEffect>0.0 && attenuation>0.0 && L.HaveShadowMap)
                            {
                                SpotShadow = 0.0;
                                #if defined(GLES2) || defined(GLLEGACY)
                                shadowBias = max(0.001 * (1.0 - dot(Normal, LightDir)), 0.00001);
                                SpotShadow=PCFSPOT(uSpotShadowMaps,uSpotDepthsMVP,L.PCFTexelSize,vs_position);
                                #else
                                SpotShadow+=PCFSPOT(uSpotShadowMaps[L.ShadowMap],uSpotDepthsMVP[L.ShadowMap],L.PCFTexelSize,vs_position);
                                #endif
                            }
                            _diffuse += vec4(lightIntensity * L.Color.xyz * spotEffect * attenuation * SpotShadow, lightIntensity * L.Color.w);
                            _specular += vec4(specularPower * L.Color.xyz * spotEffect * attenuation * specular.xyz * SpotShadow, specularPower * L.Color.w * specular.w);
                        #else
                            _diffuse += lightIntensity * L.Color * spotEffect * attenuation;
                            _specular += specularPower * L.Color * specular * spotEffect * attenuation;
                        #endif
                    }
                }
            }

            diffuse *= _diffuse;
            specular *= _specular;
            
        }

    #endif

#endif